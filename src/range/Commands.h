#pragma once
#include <functional>
#include <string>
#include "range/CommandValues.h"
#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/Optional.h"
#include "mc/server/commands/CommandOutput.h"

namespace ll::command {
// Each package registers only its own side. A short name plus no expansion
// keeps usage as <setting: setting>, rather than spelling out every key.
template <> struct enum_name<urde::UrdeClientKey> { static constexpr char const* value = "setting"; };
template <> struct enum_name<urde::UrdeServerKey> { static constexpr char const* value = "setting"; };
}
namespace urde {
struct EmptyParams {};
template <class Key, class Number>
struct NumberParams { Key setting; ll::command::Optional<Number> value; };
template <class Key>
struct VanillaParams { Key setting; };
struct ShapeParams { UrdeEntityShape shape; };
static_assert(ll::command::ParamTraits<float>::parseRuleValue() == CommandRegistry::HardNonTerminal::Val);
static_assert(ll::command::ParamTraits<int>::parseRuleValue() == CommandRegistry::HardNonTerminal::Int);
static_assert(magic_enum::enum_count<UrdeClientKey>() == 5 && magic_enum::enum_count<UrdeServerKey>() == 2);
using SetControl = std::function<std::string(std::string const&, std::string const&)>;

// Do not use LL26.60.2 deoption(): it uses logical ! instead of bitwise ~.
// Explicitly clear the one display-expansion bit, retaining all other options.
inline void compactSetting(CommandParameterData& data) {
    data.mOptions = static_cast<CommandParameterOption>(static_cast<uchar>(data.mOptions)
        & ~static_cast<uchar>(CommandParameterOption::EnumAutocompleteExpansion));
}
template <class Key, class Number>
void registerControls(ll::command::CommandHandle& command, std::function<std::string()> status,
                      SetControl set, std::function<std::string()> reload) {
    static_assert((std::is_same_v<Key,UrdeClientKey> && std::is_same_v<Number,float>)
               || (std::is_same_v<Key,UrdeServerKey> && std::is_same_v<Number,int>));
    // Literal action layout is provisional until the separate probe roots have
    // been compared live. Keep this registration in one shared function.
    command.overload<EmptyParams>().text("status")
        .execute([status](CommandOrigin const&, CommandOutput& output) { output.success(status()); });
    command.overload<EmptyParams>().text("reload")
        .execute([reload](CommandOrigin const&, CommandOutput& output) {
            try { output.success(reload()); } catch (std::exception const& error) { output.error(error.what()); }
        });
    command.overload<NumberParams<Key,Number>>().text("set").required("setting").modify(compactSetting).optional("value")
        .execute([set](CommandOrigin const&, CommandOutput& output, NumberParams<Key,Number> const& params) {
            if (!params.value) return output.error("set requires a value: set <key> <value>");
            try { output.success(set(std::string(magic_enum::enum_name(params.setting)),commandNumberText(*params.value))); }
            catch (std::exception const& error) { output.error(error.what()); }
        });
    command.overload<VanillaParams<Key>>().text("set").required("setting").modify(compactSetting).text("vanilla")
        .execute([set](CommandOrigin const&, CommandOutput& output, VanillaParams<Key> const& params) {
            try {
                auto key = std::string(magic_enum::enum_name(params.setting));
                if (key == "replication_margin") return output.error("vanilla is not supported for replication_margin");
                output.success(set(key,"vanilla"));
            } catch (std::exception const& error) { output.error(error.what()); }
        });
    if constexpr (std::is_same_v<Key,UrdeClientKey>)
        command.overload<ShapeParams>().text("set").text("entity_shape").required("shape")
            .execute([set](CommandOrigin const&, CommandOutput& output, ShapeParams const& params) {
                try { output.success(set("entity_shape",std::string(magic_enum::enum_name(params.shape)))); }
                catch (std::exception const& error) { output.error(error.what()); }
            });
}
}
