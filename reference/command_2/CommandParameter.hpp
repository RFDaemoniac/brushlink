#ifndef COMMAND_PARAMETER_HPP
#define COMMAND_PARAMETER_HPP

#include <type_traits>

#include "ErrorOr.hpp"
#include "ElementType.h"
#include "CommandContext.h"

namespace Command
{

struct CommandElement;

struct CommandParameter
{
	CommandParameter() = default;

	CommandParameter(const CommandParameter & other) { }

	// this is intended to only be used by value_ptr internals
	virtual CommandParameter * clone() const
	{
		// @Incomplete why can't we use abstract classes with value_ptr?
		return nullptr;
	}

	virtual ~CommandParameter() = default;

	ErrorOr<Success> SetArgument(CommandContext & context, value_ptr<CommandElement>&& argument);

	template<typename T>
	ErrorOr<T> EvaluateAs(CommandContext & context) const
	{
		if constexpr(std::is_same<T, Value>::value)
		{
			return Evaluate(context);
		}
		else
		{
			Value value = CHECK_RETURN(Evaluate(context));
			if (!std::holds_alternative<T>(value))
			{
				// @Bug Location
				return Error("Element is of unexpected type");
			}
			return std::get<T>(value);
		}
	}

	template<typename T>
	ErrorOr<std::vector<T> > EvaluateAsRepeatable(CommandContext & context) const
	{
		std::vector<Value> values = CHECK_RETURN(EvaluateRepeatable(context));
		std::vector<T> ret;
		for (auto& value : values)
		{
			if (!std::holds_alternative<T>(value))
			{
				return Error("Element is of unexpected type");
			}
			ret.push_back(std::get<T>(value));
		}
		return ret;
	}

	virtual std::string GetPrintString(std::string line_prefix) const = 0;

	virtual bool IsRequired() const = 0;

	// todo: should this take into consideration the current state of parameters?
	// should probably have a separate function for that
	virtual std::vector<ElementType::Enum> GetAllowedTypes() const = 0;

	virtual bool IsSatisfied() const = 0;

	virtual bool HasExplicitArgOrChild() const = 0;

	virtual CommandElement * GetLastArgument() = 0;

	virtual bool RemoveLastArgument() = 0;

	virtual ErrorOr<Success> SetArgumentInternal(CommandContext & context, value_ptr<CommandElement>&& argument) = 0;

	virtual ErrorOr<Value> Evaluate(CommandContext & context) const = 0;

	virtual ErrorOr<std::vector<Value> > EvaluateRepeatable(CommandContext & context) const
	{
		Value value = CHECK_RETURN(Evaluate(context));
		return std::vector<Value>{value};
	}
};

// @Incomplete is this even necessary? or should we instead have identity defaults
value_ptr<CommandParameter> Param(
	CommandContext & context,
	ElementType::Enum type,
	ElementName default_value,
	OccurrenceFlags::Enum flags);

value_ptr<CommandParameter> ParamImplied(
	CommandContext& context,
	value_ptr<CommandElement>&& default_value);

inline value_ptr<CommandParameter> ParamImplied(
	CommandContext& context,
	ElementName name)
{
	return ParamImplied(context, std::move(context.GetNewCommandElement(name.value).GetValue()));
}


inline value_ptr<CommandParameter> Param(CommandContext & context, ElementType::Enum type, ElementName default_value)
{
	return Param(context, type, default_value, OccurrenceFlags::Optional);
}

inline value_ptr<CommandParameter> Param(CommandContext & context, ElementType::Enum type, OccurrenceFlags::Enum flags = static_cast<OccurrenceFlags::Enum>(0x0))
{
	// @Incomplete optional without default value
	return Param(context, type, "", flags);
}

struct ParamSingleRequired : CommandParameter
{
	const ElementType::Enum type;
	value_ptr<CommandElement> argument;

	ParamSingleRequired(ElementType::Enum type)
		: type(type)
	{ }

	ParamSingleRequired(const ParamSingleRequired & other)
		: CommandParameter(other)
		, type(other.type)
		, argument(other.argument)
	{ }

	// this is intended to only be used by value_ptr internals
	virtual CommandParameter * clone() const override
	{
		return new ParamSingleRequired(*this);
	}

	std::string GetPrintString(std::string line_prefix) const override;

	std::vector<ElementType::Enum> GetAllowedTypes() const override
	{
		if (argument.get() == nullptr)
		{
			return {type};
		}
		else
		{
			return {};
		}
	}

	bool IsRequired() const override { return true; }

	bool IsSatisfied() const override;

	bool HasExplicitArgOrChild() const override;

	CommandElement * GetLastArgument() override { return argument.get(); }

	bool RemoveLastArgument() override;

	ErrorOr<Success> SetArgumentInternal(CommandContext & context, value_ptr<CommandElement>&& argument) override;

	ErrorOr<Value> Evaluate(CommandContext & context) const override;
};

struct ParamSingleOptional : ParamSingleRequired
{
	// @Incomplete should optional without default value be possible?
	ElementName default_value;

	ParamSingleOptional(ElementType::Enum type, ElementName default_value)
		: ParamSingleRequired(type)
		, default_value(default_value)
	{
		// todo: check default value is of correct type
	}

	ParamSingleOptional(const ParamSingleOptional & other)
		: ParamSingleRequired(other)
		, default_value(other.default_value)
	{ }

	// this is intended to only be used by value_ptr internals
	virtual CommandParameter * clone() const override
	{
		return new ParamSingleOptional(*this);
	}

	std::string GetPrintString(std::string line_prefix) const override;

	bool IsRequired() const override { return false; }

	bool IsSatisfied() const override;

	ErrorOr<Value> Evaluate(CommandContext & context) const override;
};

struct ParamSingleImpliedOptions : ParamSingleRequired
{
	// @Incomplete should optional without default value be possible?
	std::vector<value_ptr<CommandElement>> implied_options;

	ParamSingleImpliedOptions(ElementType::Enum type, std::vector<value_ptr<CommandElement>>&& implied_options);

	ParamSingleImpliedOptions(const ParamSingleImpliedOptions & other)
		: ParamSingleRequired(other)
		, implied_options(other.implied_options)
	{ }

	// this is intended to only be used by value_ptr internals
	virtual CommandParameter * clone() const override
	{
		return new ParamSingleImpliedOptions(*this);
	}

	std::string GetPrintString(std::string line_prefix) const override;

	std::vector<ElementType::Enum> GetAllowedTypes() const override;

	bool IsRequired() const override;

	bool IsSatisfied() const override;

	ErrorOr<Success> SetArgumentInternal(CommandContext & context, value_ptr<CommandElement>&& argument) override;

	ErrorOr<Value> Evaluate(CommandContext & context) const override;
};

struct ParamRepeatableRequired : CommandParameter
{
	const ElementType::Enum type;
	std::vector<value_ptr<CommandElement> > arguments;

	ParamRepeatableRequired(ElementType::Enum type)
		: type(type)
	{ }

	ParamRepeatableRequired(const ParamRepeatableRequired & other)
		: CommandParameter(other)
		, type(other.type)
		, arguments(other.arguments)
	{ }

	// this is intended to only be used by value_ptr internals
	virtual CommandParameter * clone() const override
	{
		return new ParamRepeatableRequired(*this);
	}

	std::string GetPrintString(std::string line_prefix) const override;

	std::vector<ElementType::Enum> GetAllowedTypes() const override { return {type}; }

	bool IsRequired() const override { return true; }

	bool IsSatisfied() const override;

	bool HasExplicitArgOrChild() const override;

	CommandElement * GetLastArgument() override;

	bool RemoveLastArgument() override;

	// todo: should this be passed in as a unique_ptr?
	ErrorOr<Success> SetArgumentInternal(CommandContext & context, value_ptr<CommandElement>&& argument) override;

	ErrorOr<Value> Evaluate(CommandContext & context) const override;

	ErrorOr<std::vector<Value> > EvaluateRepeatable(CommandContext & context) const override;
};

struct ParamRepeatableOptional : ParamRepeatableRequired
{
	ElementName default_value;

	ParamRepeatableOptional(ElementType::Enum type, ElementName default_value)
		: ParamRepeatableRequired(type)
		, default_value(default_value)
	{
		// todo: assert default value is of correct type
	}

	ParamRepeatableOptional(const ParamRepeatableOptional & other)
		: ParamRepeatableRequired(other)
		, default_value(other.default_value)
	{ }

	// this is intended to only be used by value_ptr internals
	virtual CommandParameter * clone() const override
	{
		return new ParamRepeatableOptional(*this);
	}

	std::string GetPrintString(std::string line_prefix) const override;

	bool IsRequired() const override { return false; }

	bool IsSatisfied() const override;

	ErrorOr<Value> Evaluate(CommandContext & context) const override;

	ErrorOr<std::vector<Value> > EvaluateRepeatable(CommandContext & context) const override;
};

struct OneOf : CommandParameter
{
	std::vector<value_ptr<CommandParameter> > possibilities;
	int chosen_index;

	OneOf(std::vector<value_ptr<CommandParameter> > possibilities)
		: possibilities(possibilities)
		, chosen_index(-1)
	{ }

	OneOf(const OneOf & other)
		: CommandParameter(other)
		, possibilities(other.possibilities)
		, chosen_index(other.chosen_index)
	{ }

	// this is intended to only be used by value_ptr internals
	virtual CommandParameter * clone() const override
	{
		return new OneOf(*this);
	}


	std::string GetPrintString(std::string line_prefix) const override;

	std::vector<ElementType::Enum> GetAllowedTypes() const override;

	bool IsRequired() const override;

	bool IsSatisfied() const override;

	bool HasExplicitArgOrChild() const override;

	CommandElement * GetLastArgument() override;

	bool RemoveLastArgument() override;

	ErrorOr<Success> SetArgumentInternal(CommandContext & context, value_ptr<CommandElement>&& argument) override;

	ErrorOr<Value> Evaluate(CommandContext & context) const override;

	ErrorOr<std::vector<Value> > EvaluateRepeatable(CommandContext & context) const override;
};

} // namespace Command

#endif // COMMAND_PARAMETER_HPP
