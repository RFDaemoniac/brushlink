#include "Element.hpp"
#include "Parameter.hpp"

namespace Command
{

std::queue<Parameter *> Element::GetParams()
{
	std::queue<Parameter *> params;
	if (left_parameter)
	{
		params.push(left_parameter.get());
	}
	for (auto & param : parameters)
	{
		params.push(param.get());
	}
	return params;
} 

// helper for GetAllowedTypes, AppendArgument
// bool is whether parameters are satisfied
std::pair<std::vector<Parameter*>, bool> GetActiveParameters(Element & element)
{
	// left parameters are never considered active
	// and if this element exists its left parameter is assumed to be satisfied

	// @Feature permutable
	std::vector<Parameter *> active;
	int first_active_param = 0;
	for (int index = element.parameters.size() - 1; index >= 0 ; index--)
	{
		// arguments to parameters before the most recent explicit can't be revisited
		Element * argument = element.parameters[index]->GetLastArgument();
		if (argument != nullptr
			&& argument->IsExplicitBranch())
		{
			first_active_param = index;
			break;
		}
	}

	for(int index = first_active_param; index < element.parameters.size(); index++)
	{
		active.push_back(element.parameters[index].get());
		if (!element.parameters[index]->IsSatisfied())
		{
			return {active, false};
		}
	}

	return {active, true};
}


std::string Element::GetPrintString(std::string line_prefix) const
{
	std::string print_string = line_prefix + name.value + "\n";
	line_prefix += "    ";
	if (left_parameter)
	{
		// @Cleanup left parameter should probably only be prefixed on first line
		// with the direct name
		print_string += left_parameter->GetPrintString(line_prefix + "<");
	}
	for (auto & parameter : parameters)
	{
		print_string += parameter->GetPrintString(line_prefix);
	}
	return print_string;
}

ErrorOr<Variant> Element::Evaluate(Context & context) const
{
	// default just passes through last parameter value
	// or Success if there are no parameter values
	Variant value{Success{}};
	if (left_parameter)
	{
		value = CHECK_RETURN(left_parameter->Evaluate(context));
	}
	for (auto && param : parameters)
	{
		value = CHECK_RETURN(param->Evaluate(context));
	}
	return value;
}

bool Element::IsSatisfied() const
{
	// aren't left parameters assumed to be satisfied?
	// if they aren't, should they be part of active parameters? no
	if (left_parameter
		&& !left_parameter->IsSatisfied())
	{
		return false;
	}
	for (auto param & : parameters)
	{
		if (!parameter.IsSatisfied())
		{
			return false;
		}
	}
	return true;
}

bool Element::IsExplicitBranch() const override
{
	if (implicit == Implicit::None)
	{
		return true;
	}
	// @Feature LeftParam do we need to check left parameter here?
	if(left_parameter
		&& left_parameter->IsExplicitBranch())
	{
		return true;
	}
	for (auto& parameter : parameters)
	{
		if (parameter->IsExplicitBranch())
		{
			return true;
		}
	}
	return false;
}

void Element::GetAllowedTypes(AllowedTypes & allowed) const
{
	auto [ active, satisfied ] = GetActiveParameters(*this);

	for (auto * param : active)
	{
		param->GetAllowedTypes(allowed);
		if (!param->IsSatisfied())
		{
			return;
		}
	}
	if (satisfied)
	{
		// this is a left argument
		allowed.Append({Type(), true});
	}
	return;
}

ErrorOr<bool> Element::AppendArgument(Context & context, value_ptr<Element>&& next, int &skip_count)
{
	auto [ active, satisfied ] = GetActiveParameters(*this);
	for (auto * param : active)
	{
		auto result = CHECK_RETURN(param->AppendArgument(context, next, skip_count));
		if (result)
		{
			return true;
		}
	}

	if (!satisfied)
	{
		return Error("Can't append because a preceding parameter has not been satisfied");
	}

	// left parameters have lower priority than right parameters
	// because after left parameters are applied you can't access any right parameters

	// the requirements for left parameters are kind of ridiculous
	// @Feature LeftParam types don't need to be ==, just acceptable to parent
	// but it should probably be equal unless there's a very good reason not to
	if (next->left_parameter == nullptr
		|| next->left_parameter->GetLastArgument() != nullptr
		|| next->type != type
		|| !Contains(next->left_parameter->GetAllowedTypes(), type)
		|| location_in_parent == nullptr)
	{
		return false;
	}

	if (skip_count > 0)
	{
		skip_count--;
		return false;
	}

	if (this != location_in_parent->get())
	{
		return Error("location_in_parent has a mismatched pointer");
	}
	// caching because SetArgument will change this;
	auto * loc = location_in_parent;
	int no_skips = 0;
	value_ptr<Element> boxed_this { loc->release() };
	// append isn't quite the right function because we don't want to
	// have this become a right child of next's left_parameter, right?
	// or does this open up implied left parameters? is that okay?
	auto result = CHECK_RETURN(next->left_parameter->AppendArgument(
		context,
		std::move(boxed_this),
		no_skips));
	if (!result)
	{
		// is this an error after std::move? implication is that the move failed
		loc->reset(boxed_this.release());
		return Error("Unexpectedly failed to append to left");
	}
	next->location_in_parent = loc;
	loc->reset(next.release());

	return true;
}

ErrorOr<Removal> Element::RemoveLastExplicitElement()
{
	bool had_explicit_child = false;
	for (int index = parameters.size() - 1; index >= 0 ; index--)
	{
		if (parameters[index]->IsExplicitBranch())
		{
			Removal removal = CHECK_RETURN(parameters[index]->RemoveLastExplicitElement());
			if (removal == Removal::None)
			{
				return Error("Couldn't remove an explicit argument");
			}
			return removal;
		}
	}
	return Removal::None;
}

GetNamedValue::GetNamedValue()
	: Element{
		"Get",
		Variant_Type::Any,
		{}, // no left parameter
		{Parameter_Basic<false, false>{
			{{"name"}},
			Variant_Type::ValueName,
			nullptr,
			{}
		}}
	}
{ }

GetNamedValue::GetNamedValue(ValueName name)
	: Element{
		"Get",
		Variant_Type::Any,
		{}, // no left parameter
		{Parameter_Implied{
			{"name"},
			{new Literal<ValueName>{name}}}
		}
	}
{ }

ErrorOr<Variant> GetNamedValue::Evaluate(Context & context) const
{
	ValueName name = CHECK_RETURN(parameters.front()->template EvaluateAs<ValueName>(context));
	auto values = CHECK_RETURN(context.GetNamedValue(name));
	if (values.size() == 1)
	{
		return values.front();
	}
	else
	{
		return Error("Named value " + name.value + " was expected to contain exactly one value");
	}
}

ErrorOr<std::vector<Variant>> GetNamedValue::EvaluateRepeatable(Context & context) const
{
	ValueName name = CHECK_RETURN(parameters.front()->template EvaluateAs<ValueName>(context));
	return context.GetNamedValue(name);
}

ErrorOr<Variant> ElementFunction::Evaluate(Context & context) const override
{
	Context child_context = context.MakeChild(Scoped{true});
	// using child context during evaluation here makes previous arguments available to later ones
	// @Bug when to call EvaluateRepeatable? can we store those?

	std::queue<Parameter *> params = GetParams();
	for(int index = 0; !params.empty(); index++)
	{
		Parameter * param = params.pop();
		ValueName name { param->name ? param->name.value() : "arg_" + str(index) };
		// arguments are always evaluated repeatable
		// and collapsed back to single variant at point of use if there is only one
		child_context.arguments[name] = CHECK_RETURN(param->EvaluateRepeatable(context));
	}
	Variant value = CHECK_RETURN(implementation->Evaluate(child_context));
	// @Feature recursion
	while (child_context.recurse)
	{
		child_context.recurse = false;
		value = CHECK_RETURN(implementation->Evaluate(child_context));
	}
	return value;
}

} // namespace Command
