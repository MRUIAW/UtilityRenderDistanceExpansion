#pragma once
#include <functional>
#include <string>
#include "range/CommandValues.h"
#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/Optional.h"
#include "mc/server/commands/CommandOutput.h"

namespace urde {
// Share action and side-key enum identities across all overloads so there is
// one metadata value set at each position; exact Bedrock UI merging needs a live
// check. Optional fields handle bare actions and incomplete set explicitly.
template <class Key, class Number>
struct NumberParams { UrdeAction action; ll::command::Optional<Key> key; ll::command::Optional<Number> value; };
template <class Key, class Value>
struct ChoiceParams { UrdeAction action; Key key; Value value; };
static_assert(ll::command::ParamTraits<float>::parseRuleValue() == CommandRegistry::HardNonTerminal::Val);
static_assert(ll::command::ParamTraits<int>::parseRuleValue() == CommandRegistry::HardNonTerminal::Int);
template <class T, size_t Count>
constexpr bool hintedEnum = ll::command::ParamTraits<T>::dataType() == CommandParameterDataType::Enum
    && ll::command::ParamTraits<T>::options() == CommandParameterOption::EnumAutocompleteExpansion
    && magic_enum::enum_count<T>() == Count;
static_assert(hintedEnum<UrdeAction,3> && hintedEnum<UrdeClientKey,6> && hintedEnum<UrdeServerKey,2>);
static_assert(hintedEnum<UrdeVanilla,1> && hintedEnum<UrdeEntityShape,2>);
static_assert(magic_enum::enum_name(UrdeAction::reload) == "reload");
static_assert(ll::reflection::member_count_v<NumberParams<UrdeClientKey,float>> == 3);
using SetControl = std::function<std::string(std::string const&, std::string const&)>;

template <class Key, class Value>
void registerChoice(ll::command::CommandHandle& command, SetControl set) {
    command.overload<ChoiceParams<Key,Value>>().required("action").required("key").required("value")
        .execute([set](CommandOrigin const&, CommandOutput& output, ChoiceParams<Key,Value> const& params) {
            if (params.action != UrdeAction::set) return output.error("status/reload take no key or value");
            try {
                auto key = std::string(magic_enum::enum_name(params.key));
                if constexpr (std::is_same_v<Value,UrdeEntityShape>) {
                    if (key != "entity_shape") throw std::invalid_argument("circle/square require entity_shape");
                } else if (key == "entity_shape" || key == "replication_margin") {
                    throw std::invalid_argument("vanilla is not supported for " + key);
                }
                output.success(set(key,std::string(magic_enum::enum_name(params.value))));
            } catch (std::exception const& error) { output.error(error.what()); }
        });
}
template <class Key, class Number>
void registerControls(ll::command::CommandHandle& command, std::function<std::string()> status,
                      SetControl set, std::function<std::string()> reload) {
    static_assert((std::is_same_v<Key,UrdeClientKey> && std::is_same_v<Number,float>)
               || (std::is_same_v<Key,UrdeServerKey> && std::is_same_v<Number,int>));
    command.overload<NumberParams<Key,Number>>().required("action").optional("key").optional("value")
        .execute([status,set,reload](CommandOrigin const&, CommandOutput& output, NumberParams<Key,Number> const& params) {
            try {
                if (params.action != UrdeAction::set) {
                    if (params.key || params.value) return output.error("status/reload take no key or value");
                    return output.success(params.action == UrdeAction::status ? status() : reload());
                }
                if (!params.key) return output.error("set requires a key and value: set <key> <value>");
                if (!params.value) return output.error("set requires a value: set <key> <value>");
                auto key = std::string(magic_enum::enum_name(*params.key));
                if (key == "entity_shape") return output.error("entity_shape requires circle or square");
                output.success(set(key,commandNumberText(*params.value)));
            } catch (std::exception const& error) { output.error(error.what()); }
        });
    registerChoice<Key,UrdeVanilla>(command,set);
    if constexpr (std::is_same_v<Key,UrdeClientKey>) registerChoice<Key,UrdeEntityShape>(command,set);
}
}
