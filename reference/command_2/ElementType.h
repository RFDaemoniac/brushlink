#ifndef BRUSHLINK_COMMAND_TYPES_HPP
#define BRUSHLINK_COMMAND_TYPES_HPP

#include <vector>
#include <unordered_map>
#include <map>
#include <list>

#include "BuiltinTypedefs.h"
#include "NamedType.hpp"
#include "ErrorOr.hpp"

using namespace Farb;

namespace Command
{

// todo: interactive visualizations

namespace ElementType
{
enum Enum
{
	Command =              1 << 0,

	Condition =            1 << 1,
	Action =               1 << 2,

	Selector =             1 << 3,
	Set =                  1 << 4, // base, group, crew, set?
	Filter =               1 << 5, // predicate?
	Group_Size =           1 << 6,
	Superlative =          1 << 7,

	Location =             1 << 8,
	Point =                1 << 9,
	Line =                 1 << 10,
	Area =                 1 << 11,
	Direction =            1 << 12,

	Unit_Type =            1 << 13,
	Attribute_Type =       1 << 14, // drop _Type?
	Ability_Type =         1 << 15, // skill?
	Resource_Type =        1 << 16,

	Number =               1 << 17,
	// used for Number literal construction
	Digit =                1 << 18,

	//Name
	//Letter

	// @Cleanup maybe combine into single type Instruction? Punctuation?
	// would require handling Allowed slightly differently
	Skip =                 1 << 19,
	Termination =          1 << 20,
	Cancel =               1 << 21,
	Undo =                 1 << 22,
	Redo =                 1 << 23,
	// RepeatLastCommand
	// Begin_Word? End_Word?

	Mouse_Input =          1 << 24,

	Parameter_Reference =  1 << 25

	// these are probably not appropriate types because of the way fields are compared
	// but really I need to think about how to compare fields more thoroughly
	// which probably means not using a field, and using sets instead
	// or these become flags

	//Default_Value =        1 << 18,
	//Implied =              1 << 19,

	// has no parameters and is not context dependent
	//Literal =              1 << 20,

	// User_Defined =         1 << 22
};

} // namespace ElementType

const ElementType::Enum kNullElementType = static_cast<ElementType::Enum>(0);

inline ElementType::Enum operator|(ElementType::Enum a, ElementType::Enum b)
{
	return static_cast<ElementType::Enum>(static_cast<int>(a) | static_cast<int>(b));
}

inline ElementType::Enum operator&(ElementType::Enum a, ElementType::Enum b)
{
	return static_cast<ElementType::Enum>(static_cast<int>(a) & static_cast<int>(b));
}

const std::vector<std::pair<ElementType::Enum, std::string> > element_type_mapping{{
	{ ElementType::Command, "Command"},
	{ ElementType::Condition, "Condition"},
	{ ElementType::Action, "Action"},
	{ ElementType::Selector, "Selector"},
	{ ElementType::Set, "Set"},
	{ ElementType::Filter, "Filter"},
	{ ElementType::Group_Size, "Group_Size"},
	{ ElementType::Superlative, "Superlative"},
	{ ElementType::Location, "Location"},
	{ ElementType::Point, "Point"},
	{ ElementType::Line, "Line"},
	{ ElementType::Area, "Area"},
	{ ElementType::Direction, "Direction"},
	{ ElementType::Unit_Type, "Unit_Type"},
	{ ElementType::Attribute_Type, "Attribute_Type"},
	{ ElementType::Ability_Type, "Ability_Type"},
	{ ElementType::Resource_Type, "Resource_Type"},
	{ ElementType::Number, "Number"},
	{ ElementType::Digit, "Digit"},
	{ ElementType::Skip, "Skip"},
	{ ElementType::Undo, "Undo"},
	{ ElementType::Redo, "Redo"},
	{ ElementType::Termination, "Termination"},
	{ ElementType::Cancel, "Cancel"},
	{ ElementType::Mouse_Input, "Mouse_Input"},
	{ ElementType::Parameter_Reference, "Parameter_Reference"},
}};

inline std::string ToString(ElementType::Enum type)
{
	for (auto && pair : element_type_mapping)
	{
		if (pair.first == type)
		{
			return pair.second;
		}
	}
	return "Uknown Type";
}

inline ElementType::Enum FromString(const std::string & as_string)
{
	for (auto && pair : element_type_mapping)
	{
		if (pair.second == as_string)
		{
			return pair.first;
		}
	}
	return kNullElementType;
}

// rmf todo: split out ElementFlags from ElementType


struct ElementNameTag
{
	static HString GetName() { return "Command::ElementName"; }
};
using ElementName = NamedType<HString, ElementNameTag>;

struct ElementToken
{
	// this is a bit field of flags, aka a set of types
	// but I think it should actually only be a single one
	ElementType::Enum type;
	ElementName name;
	bool has_left_param;
	// could also consider bool is_literal for combining literals

	ElementToken(ElementType::Enum type, ElementName name, bool has_left_param = false)
		: name(name)
		, type(type)
		, has_left_param(has_left_param)
	{ }

	bool IsType(ElementType::Enum other)
	{
		return static_cast<uint>(other) & static_cast<uint>(type);
	}
};


namespace OccurrenceFlags
{
	enum Enum
	{
		Optional =   1 << 0,
		Permutable = 1 << 1, // unimplemented
		Repeatable = 1 << 2,
		Implied =    1 << 3, // used to automatically instantiate default_value
	};
}

inline OccurrenceFlags::Enum operator|(OccurrenceFlags::Enum a, OccurrenceFlags::Enum b)
{
	return static_cast<OccurrenceFlags::Enum>(static_cast<int>(a) | static_cast<int>(b));
}

} // namespace Command

namespace std
{
template<>
class hash<Set<Command::ElementType::Enum>>
{
public:
	size_t operator()(const Set<Command::ElementType::Enum> & set) const
	{
		// @Cleanup if we ever change ElementType to not be <<
		size_t sum;
		for(auto type : set)
		{
			sum += static_cast<size_t>(type);
		}
		return sum;
	}
};
} // namespace std

#endif // BRUSHLINK_COMMAND_TYPES_HPP
