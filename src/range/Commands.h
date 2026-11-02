#pragma once
#include <functional>
#include <string>
#include "range/CommandValues.h"
#include "ll/api/command/CommandHandle.h"
#include "mc/server/commands/CommandOutput.h"

namespace urde {
// External linkage required by command reflection.
template <class Key, class Number>
struct SetNumberParams { Key key; Number value; };
template <class Key>
struct SetVanillaParams { Key key; UrdeVanilla value; };
template <class Key, class Value>
struct SetEnumParams { Key key; Value value; };

// LL 26.51.9 maps std::string to Id, not to a numeric-capable free token.
// Assert the grammar/enum metadata on which this registration depends.
static_assert(ll::command::ParamTraits<std::string>::parseRuleValue() == CommandRegistry::HardNonTerminal::Id);
static_assert(ll::command::ParamTraits<float>::parseRuleValue() == CommandRegistry::HardNonTerminal::Val);
static_assert(ll::command::ParamTraits<int>::parseRuleValue() == CommandRegistry::HardNonTerminal::Int);
static_assert(ll::command::ParamTraits<UrdeClientKey>::dataType() == CommandParameterDataType::Enum);
static_assert(ll::command::ParamTraits<UrdeServerKey>::dataType() == CommandParameterDataType::Enum);
static_assert(ll::command::ParamTraits<UrdeClientShapeKey>::dataType() == CommandParameterDataType::Enum);
static_assert(ll::command::ParamTraits<UrdeClientMarginKey>::dataType() == CommandParameterDataType::Enum);
static_assert(ll::command::ParamTraits<UrdeEntityShape>::dataType() == CommandParameterDataType::Enum);
static_assert(ll::command::ParamTraits<UrdeVanilla>::options() == CommandParameterOption::EnumAutocompleteExpansion);
static_assert(magic_enum::enum_count<UrdeClientKey>() == 3);
static_assert(magic_enum::enum_count<UrdeClientMarginKey>() == 1);
static_assert(magic_enum::enum_count<UrdeClientShapeKey>() == 1);
static_assert(magic_enum::enum_count<UrdeEntityShape>() == 2);
static_assert(magic_enum::enum_count<UrdeServerKey>() == 1);
static_assert(magic_enum::enum_count<UrdeVanilla>() == 1);
static_assert(magic_enum::enum_name(UrdeClientKey::entity_range) == "entity_range");
static_assert(magic_enum::enum_name(UrdeClientKey::block_entity_range) == "block_entity_range");
static_assert(magic_enum::enum_name(UrdeClientKey::terrain_detail) == "terrain_detail");
static_assert(magic_enum::enum_name(UrdeClientMarginKey::replication_margin) == "replication_margin");
static_assert(magic_enum::enum_name(UrdeClientShapeKey::entity_shape) == "entity_shape");
static_assert(magic_enum::enum_name(UrdeEntityShape::circle) == "circle");
static_assert(magic_enum::enum_name(UrdeEntityShape::square) == "square");
static_assert(magic_enum::enum_name(UrdeServerKey::actor_interest_chunks) == "actor_interest_chunks");
static_assert(magic_enum::enum_name(UrdeVanilla::vanilla) == "vanilla");
static_assert(ll::reflection::member_count_v<SetNumberParams<UrdeClientKey,float>> == 2);
static_assert(ll::reflection::member_count_v<SetNumberParams<UrdeServerKey,int>> == 2);
static_assert(ll::reflection::member_count_v<SetNumberParams<UrdeClientMarginKey,float>> == 2);
static_assert(ll::reflection::member_count_v<SetEnumParams<UrdeClientShapeKey,UrdeEntityShape>> == 2);

template <class Key, class Number>
inline void registerControls(ll::command::CommandHandle& command,
                             std::function<std::string()> status,
                             std::function<std::string(std::string const&, std::string const&)> set,
                             std::function<std::string()> reload) {
    static_assert((std::is_same_v<Key,UrdeClientKey> && std::is_same_v<Number,float>)
               || (std::is_same_v<Key,UrdeServerKey> && std::is_same_v<Number,int>));
    command.overload().text("status").execute([status](CommandOrigin const&, CommandOutput& output) {
        output.success(status());
    });
    command.overload<SetNumberParams<Key,Number>>().text("set").required("key").required("value")
        .execute([set](CommandOrigin const&, CommandOutput& output, SetNumberParams<Key,Number> const& params) {
            try { output.success(set(std::string(magic_enum::enum_name(params.key)), commandNumberText(params.value))); }
            catch (std::exception const& error) { output.error(error.what()); }
        });
    command.overload<SetVanillaParams<Key>>().text("set").required("key").required("value")
        .execute([set](CommandOrigin const&, CommandOutput& output, SetVanillaParams<Key> const& params) {
            try { output.success(set(std::string(magic_enum::enum_name(params.key)), "vanilla")); }
            catch (std::exception const& error) { output.error(error.what()); }
        });
    if constexpr (std::is_same_v<Key,UrdeClientKey>) {
        command.template overload<SetNumberParams<UrdeClientMarginKey,float>>().text("set")
            .required("key").required("value")
            .execute([set](CommandOrigin const&, CommandOutput& output,
                           SetNumberParams<UrdeClientMarginKey,float> const& params) {
                try { output.success(set(std::string(magic_enum::enum_name(params.key)), commandNumberText(params.value))); }
                catch (std::exception const& error) { output.error(error.what()); }
            });
        command.template overload<SetEnumParams<UrdeClientShapeKey,UrdeEntityShape>>().text("set")
            .required("key").required("value")
            .execute([set](CommandOrigin const&, CommandOutput& output,
                           SetEnumParams<UrdeClientShapeKey,UrdeEntityShape> const& params) {
                try {
                    output.success(set(std::string(magic_enum::enum_name(params.key)),
                                       std::string(magic_enum::enum_name(params.value))));
                } catch (std::exception const& error) { output.error(error.what()); }
            });
    }
    command.overload().text("reload").execute([reload](CommandOrigin const&, CommandOutput& output) {
        try { output.success(reload()); }
        catch (std::exception const& error) { output.error(error.what()); }
    });
}
}
