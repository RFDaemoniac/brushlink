#include "CommandElement.hpp"

#include "ContainerExtensions.hpp"

namespace Command
{
// bool is whether parameters are satisfied
std::pair<std::vector<CommandParameter*>, bool> GetActiveParameters(CommandElement & element)
{
	// @Feature permutable
	std::vector<CommandParameter*> active;
	int first_param = 0;
	for (int index = element.parameters.size() - 1; index >= 0 ; index--)
	{
		// arguments to parameters before the most recent explicit can't be revisited
		CommandElement * argument = element.parameters[index]->GetLastArgument();
		if (argument != nullptr
			&& argument->IsExplicitOrHasExplicitChild())
		{
			first_param = index;
			break;
		}
	}

	for(int index = first_param; index < element.parameters.size(); index++)
	{
		active.push_back(element.parameters[index].get());

		if (element.parameters[index]->IsRequired()
			&& !element.parameters[index]->IsSatisfied())
		{
			return {active, false};
		}
	}

	return {active, true};
}

void CommandElement::GetAllowedArgumentTypes(AllowedTypes & allowed)
{
	auto [ active, satisfied ] = GetActiveParameters(*this);

	for (auto * param : active)
	{
		CommandElement * argument = param->GetLastArgument();
		if (argument != nullptr)
		{
			// this argument could be the last explicit argument
			// or any implicit argument
			argument->GetAllowedArgumentTypes(allowed);
			if (!argument->ParametersSatisfied())
			{
				return;
			}
			// intentional fall through here for Repeatable params
		}

		auto param_allowed = param->GetAllowedTypes();
		for (auto&& type : param_allowed)
		{
			allowed.Append({type});
		}
		for (auto&& type : GetAllowedWithImplied(Set<ElementType::Enum>{param_allowed.begin(), param_allowed.end()}))
		{
			allowed.Append({type});
		}
	}

	if (satisfied)
	{
		// this is a left argument
		allowed.Append({Type(), true});
	}

	return;
}

ErrorOr<bool> CommandElement::AppendArgument(CommandContext & context, value_ptr<CommandElement>&& next, int & skip_count)
{
	auto [ active, satisfied ] = GetActiveParameters(*this);
	for (auto * param : active)
	{
		CommandElement * argument = param->GetLastArgument();
		if (argument != nullptr)
		{
			// this is an implicit argument, recurse on its parameters
			bool result = CHECK_RETURN(argument->AppendArgument(context, std::move(next), skip_count));
			if (result)
			{
				return true;
			}
			// intentional fallthrough for Repeatable parameters
		}
		
		auto allowed_types = param->GetAllowedTypes();
		// without implied types for same type take priority
		// and we only want to decrement skip count at most once per parameter
		if (Contains(allowed_types, next->Type()))
		{
			if (skip_count == 0)
			{
				CHECK_RETURN(param->SetArgument(context, std::move(next)));
				return true;
			}
			skip_count--;
		}
		else
		{
			for(auto&& arg_type : allowed_types)
			{
				if (CommandContext::allowed_with_implied.count(arg_type) <= 0
					|| CommandContext::allowed_with_implied.at(arg_type).count(next->Type()) <= 0)
				{
					continue;
				}
				if (skip_count > 0)
				{
					// if we match in one way or 10 ways, we still skip once
					skip_count--;
					break;
				}
				
				ElementName implied_name = CommandContext::implied_elements.at(next->Type()).at(arg_type);
				auto implied_element = CHECK_RETURN(context.GetNewCommandElement(implied_name.value));
				implied_element->implicit = Implicit::Parent;
				int no_skips = 0;
				bool result = CHECK_RETURN(implied_element->AppendArgument(context, std::move(next), no_skips));
				if (!result)
				{
					return Error("Could not append new element to implied element");
				}
				CHECK_RETURN(param->SetArgument(context, std::move(implied_element)));
				return true;
			}
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
	if (next->left_parameter != nullptr
		&& next->left_parameter->GetLastArgument() == nullptr
		&& next->Type() == Type() 
		&& Contains(next->left_parameter->GetAllowedTypes(), Type())
		&& location_in_parent != nullptr)
	{
		if (skip_count == 0)
		{
			// @Feature LeftParam how to get location in parent?
			// should we have a pointer to the owning parameter?
			if (this != location_in_parent->get())
			{
				return Error("location_in_parent has a mismatched pointer");
			}
			// caching because SetArgument will change this;
			auto * loc = location_in_parent;

			CHECK_RETURN(next->left_parameter->SetArgument(context, loc->release()));
			// if types don't match we should instead have a function in parameter do this
			// should maybe do all of this in the parameter...
			// could also consider passing in parent as function argument
			loc->reset(next.release());
			loc->get()->location_in_parent = loc;

			return true;
		}
		else
		{
			skip_count--;
		}
	}

	return false;
}

ErrorOr<bool> CommandElement::RemoveLastExplicitElement()
{
	bool had_explicit_child = false;
	for (int index = parameters.size() - 1; index >= 0 ; index--)
	{
		// earlier arguments to parameters can't be revisited
		CommandElement * argument = parameters[index]->GetLastArgument();
		// can the last argument of a parameter be implicit but
		// earlier ones explicit?
		// if so then we would need GetLastExplicitArgument
		if (argument != nullptr
			&& argument->IsExplicitOrHasExplicitChild())
		{
			had_explicit_child = true;
			bool should_remove_argument = CHECK_RETURN(argument->RemoveLastExplicitElement());
			if (should_remove_argument)
			{
				bool removed = parameters[index]->RemoveLastArgument();
				if (!removed)
				{
					return Error("Expected to RemoveLastArgument but couldn't");
				}
				break;
			}
			else
			{
				break;
			}
		}
	}
	bool should_remove_this = [&]{
		if (!had_explicit_child)
		{
			// we are a leaf, and should be removed
			return true;
		}
		if (implicit == Implicit::Parent
			&& !IsExplicitOrHasExplicitChild())
		{
			// we are an implicit parent with no explicit children
			return true;
		}
		return false;
	}();

	if (!should_remove_this)
	{
		return false;
	}

	bool swap_with_left = [&]{
		if (left_parameter == nullptr)
		{
			return false;
		}
		if (left_parameter->HasExplicitArgOrChild())
		{
			return true;
		}
		if (left_parameter->GetLastArgument() != nullptr
			&& left_parameter->GetLastArgument()->implicit == Implicit::Child)
		{
			// left_parameters that are implicit children
			// are implicit children of this elements parent, not this
			return true;
		}
		return false;
	}();

	if (!swap_with_left)
	{
		// this tells the parent function call to remove us
		return true;
	}

	// swap out left parameter to remove ourselves
	if (this != location_in_parent->get())
	{
		return Error("location_in_parent has a mismatched pointer");
	}
	auto * left_element = left_parameter->GetLastArgument();
	if (left_element != left_element->location_in_parent->get())
	{
		return Error("location_in_parent for left_parameter has a mismatched pointer");
	}

	// self_container will delete this when it goes out of scope on function return
	value_ptr<CommandElement> self_container;
	location_in_parent->swap(self_container);
	// if types don't match we should instead have a function in parameter do this
	// should maybe do all of this in the parameter...
	location_in_parent->swap(*(left_element->location_in_parent));
	left_element->location_in_parent = location_in_parent;

	// no need for parent to remove because we did the removal ourselves by swapping
	return false;
}

bool CommandElement::IsExplicitOrHasExplicitChild()
{
	if (implicit == Implicit::None)
	{
		return true;
	}
	// @Feature LeftParam do we need to check left parameter here?
	for (auto& parameter : parameters)
	{
		if (parameter->HasExplicitArgOrChild())
		{
			return true;
		}
	}
	return false;
}

bool CommandElement::ParametersSatisfied()
{
	if (left_parameter
		&& !left_parameter->IsSatisfied())
	{
		return false;
	}
	for (auto& parameter : parameters)
	{
		if (!parameter->IsSatisfied())
		{
			return false;
		}
	}
	return true;
}

std::string CommandElement::GetPrintString(std::string line_prefix)
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

ErrorOr<Value> EmptyCommandElement::Evaluate(CommandContext & context)
{
	if (left_parameter)
	{
		CHECK_RETURN(left_parameter->Evaluate(context));
	}
	for (auto && param : parameters)
	{
		CHECK_RETURN(param->Evaluate(context));
	}
	return Value{Success()};
}

ErrorOr<Value> SelectorCommandElement::Evaluate(CommandContext & context)
{
	// an alternative format for this could be to allow elements
	// to have private parameters that don't return in the get allowed types
	// and don't prevent the user from moving on
	// but then would throw errors during evaluation

	// context dependent defaults should happen here
	// maybe CommandElements should have a pointer to their parent?
	// we had considered that for access to actors and everything
	
	UnitGroup units = CHECK_RETURN(parameters[0]->EvaluateAs<UnitGroup>(context));
	std::vector<Filter> filters = CHECK_RETURN(parameters[1]->EvaluateAsRepeatable<Filter>(context));

	for (auto& filter : filters)
	{
		units = filter(units);
	}

	int count = 0;
	if (parameters[2]->GetLastArgument() != nullptr)
	{
		// group size is optional and doesn't have a default
		// so we can only evaluate it if it has an argument
		GroupSize size = CHECK_RETURN(parameters[2]->EvaluateAs<GroupSize>(context));
		count = size(units).value;
	}
	else if (parameters[3]->GetLastArgument() != nullptr)
	{
		// if GroupSize is not set but Superlative is, target a single unit
		count = 1;
	}
	else
	{
		// default, with no GroupSize or Superlative set, just take all the units
		count = units.members.size();
	}

	// only apply superlative if we have too many units
	if (units.members.size() > count)
	{
		// superlatives should have a default value, so even if there isn't 
		// a last argument, we should be able to evaluate
		Superlative superlative = CHECK_RETURN(parameters[3]->EvaluateAs<Superlative>(context));

		units = superlative(units, Number(count));
	}
	
	return Value{units};
}

} // namespace Command
