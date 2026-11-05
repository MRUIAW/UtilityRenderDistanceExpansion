#pragma once
#include <functional>
#include <string>
#include "range/CommandValues.h"
#include "ll/api/command/CommandHandle.h"
#include "mc/server/commands/CommandOutput.h"

namespace magic_enum::customize {
template <>
constexpr customize_t enum_name<urde::UrdeBoolean>(urde::UrdeBoolean value) noexcept {
    switch (value) {
    case urde::UrdeBoolean::true_: return "true";
    case urde::UrdeBoolean::false_: return "false";
    default: return invalid_tag;
    }
}
}

namespace urde {
// External linkage required by LL's reflected parameters. Group by value kind
// to retain contextual hard-enum hints without a separate row for every key.
struct ActionParams { UrdeAction action; };
template <class Key, class Value>
struct SetParams { UrdeSet action; Key key; Value value; };

// LL26.60.2: strings use Id (which rejects bare numbers); keep native Val/Int
// parsing and expanded hard enums. No free-string catch-all obscures the hints.
static_assert(ll::command::ParamTraits<std::string>::parseRuleValue() == CommandRegistry::HardNonTerminal::Id);
static_assert(ll::command::ParamTraits<float>::parseRuleValue() == CommandRegistry::HardNonTerminal::Val);
static_assert(ll::command::ParamTraits<int>::parseRuleValue() == CommandRegistry::HardNonTerminal::Int);
template <class T, size_t Count>
constexpr bool hintedEnum = ll::command::ParamTraits<T>::dataType() == CommandParameterDataType::Enum
    && ll::command::ParamTraits<T>::options() == CommandParameterOption::EnumAutocompleteExpansion
    && magic_enum::enum_count<T>() == Count;
static_assert(hintedEnum<UrdeAction,3> && hintedEnum<UrdeSet,1>);
static_assert(hintedEnum<UrdeClientNumberKey,4> && hintedEnum<UrdeClientVanillaKey,4>);
static_assert(hintedEnum<UrdeServerNumberKey,2> && hintedEnum<UrdeIntervalKey,1>);
static_assert(hintedEnum<UrdeClientShapeKey,1> && hintedEnum<UrdePlayersKey,1>);
static_assert(hintedEnum<UrdeEntityShape,2> && hintedEnum<UrdeBoolean,2> && hintedEnum<UrdeVanilla,1>);
static_assert(magic_enum::enum_name(UrdeBoolean::true_) == "true");
static_assert(magic_enum::enum_name(UrdeBoolean::false_) == "false");
static_assert(magic_enum::enum_name(UrdeAction::reload) == "reload");
static_assert(ll::reflection::member_count_v<ActionParams> == 1);
static_assert(ll::reflection::member_count_v<SetParams<UrdeClientNumberKey,float>> == 3);
static_assert(ll::reflection::member_count_v<SetParams<UrdeServerNumberKey,int>> == 3);

using SetControl = std::function<std::string(std::string const&, std::string const&)>;
template <class Key, class Value>
inline void registerSet(ll::command::CommandHandle& command, SetControl set) {
    command.overload<SetParams<Key,Value>>().required("action").required("key").required("value")
        .execute([set](CommandOrigin const&, CommandOutput& output, SetParams<Key,Value> const& params) {
            try {
                auto value = [&] {
                    if constexpr (std::is_enum_v<Value>) return std::string(magic_enum::enum_name(params.value));
                    else return commandNumberText(params.value);
                }();
                output.success(set(std::string(magic_enum::enum_name(params.key)),value));
            } catch (std::exception const& error) { output.error(error.what()); }
        });
}

template <class Key, class Number>
inline void registerControls(ll::command::CommandHandle& command,
                             std::function<std::string()> status,
                             SetControl set,
                             std::function<std::string()> reload) {
    static_assert((std::is_same_v<Key,UrdeClientKey> && std::is_same_v<Number,float>)
               || (std::is_same_v<Key,UrdeServerKey> && std::is_same_v<Number,int>));
    // A single three-value enum explicitly advertises every root action. The
    // longer overloads accept only set, so status/reload never take extra args.
    command.overload<ActionParams>().required("action")
        .execute([status,reload](CommandOrigin const&, CommandOutput& output, ActionParams const& params) {
            try {
                switch (params.action) {
                case UrdeAction::status: output.success(status()); break;
                case UrdeAction::reload: output.success(reload()); break;
                case UrdeAction::set: output.error("Usage: set <key> <value>"); break;
                }
            } catch (std::exception const& error) { output.error(error.what()); }
        });
    if constexpr (std::is_same_v<Key,UrdeClientKey>) {
        registerSet<UrdeClientNumberKey,float>(command,set);
        registerSet<UrdeIntervalKey,int>(command,set);
        registerSet<UrdeClientVanillaKey,UrdeVanilla>(command,set);
        registerSet<UrdeClientShapeKey,UrdeEntityShape>(command,set);
    } else {
        registerSet<UrdeServerNumberKey,int>(command,set);
        registerSet<UrdeServerNumberKey,UrdeVanilla>(command,set);
    }
    registerSet<UrdePlayersKey,UrdeBoolean>(command,set);
}
}
