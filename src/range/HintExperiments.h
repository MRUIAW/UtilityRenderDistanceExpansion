#pragma once
// TEMPORARY 0.3.1 experiment. All roots/audits disappear with this one switch.
#ifdef URDE_HINT_EXPERIMENT
#include <unordered_set>
#include "range/Commands.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/command/SoftEnum.h"
#include "ll/api/service/Bedrock.h"
#include "mc/network/packet/AvailableCommandsPacket.h"
#include "mod/UrdeMod.h"

namespace urde::hint {
enum class Action { status, set, reload };
enum class Idle { status, reload };
enum class Key { alpha, beta };
enum class SoftAction { status, set, reload };
enum class SoftKey { alpha, beta };
enum class Reordered { reload, set, status };
struct IdleParams { Idle action; };
struct SetParams { Key setting; ll::command::Optional<float> value; };
struct VanillaParams { Key setting; };
template <class ActionType, class KeyType = Key>
struct ActionParams { ActionType action; ll::command::Optional<KeyType> setting; ll::command::Optional<float> value; };
struct ActionVanilla { Action action; Key setting; };
struct SoftParams {
    ll::command::SoftEnum<SoftAction> action;
    ll::command::Optional<ll::command::SoftEnum<SoftKey>> setting;
    ll::command::Optional<float> value;
};
inline std::string enumText(auto value) { return std::string(magic_enum::enum_name(value)); }
template <class Params>
void echo(CommandOutput& output, std::string const& root, std::string action, Params const& params) {
    if (params.setting) {
        if constexpr (std::is_enum_v<std::remove_cvref_t<decltype(*params.setting)>>)
            action += " setting=" + enumText(*params.setting);
        else action += " setting=" + std::string(*params.setting);
    }
    if (params.value) action += " value=" + commandNumberText(*params.value);
    output.success("TEMPORARY " + root + " parsed " + action);
}
inline void addSet(ll::command::CommandHandle& command, std::string root, bool compact) {
    auto number = command.overload<SetParams>();
    (void)number.text("set").required("setting");
    if (compact) (void)number.modify(compactSetting);
    number.optional("value").execute([root](CommandOrigin const&, CommandOutput& out, SetParams const& p) {
        out.success("TEMPORARY " + root + " parsed set setting=" + enumText(p.setting)
            + (p.value ? " value=" + commandNumberText(*p.value) : " value=<missing>"));
    });
    auto vanilla = command.overload<VanillaParams>();
    (void)vanilla.text("set").required("setting");
    if (compact) (void)vanilla.modify(compactSetting);
    vanilla.text("vanilla").execute([root](CommandOrigin const&, CommandOutput& out, VanillaParams const& p) {
        out.success("TEMPORARY " + root + " parsed set setting=" + enumText(p.setting) + " vanilla");
    });
}
inline void literal(ll::command::CommandHandle& command, std::string const& root, bool compact) {
    command.overload<EmptyParams>().text("status").execute([root](CommandOrigin const&, CommandOutput& out) {
        out.success("TEMPORARY " + root + " parsed status");
    });
    addSet(command,root,compact);
    command.overload<EmptyParams>().text("reload").execute([root](CommandOrigin const&, CommandOutput& out) {
        out.success("TEMPORARY " + root + " parsed reload");
    });
}
template <class A>
void shared(ll::command::CommandHandle& command, std::string root) {
    command.overload<ActionParams<A>>().required("action").optional("setting").optional("value")
        .execute([root](CommandOrigin const&, CommandOutput& out, ActionParams<A> const& p) { echo(out,root,enumText(p.action),p); });
}
// Read-only registry and AvailableCommands packet audit, once at registration.
// This distinguishes loss before serialization from client hint rendering.
inline void audit(bool client, std::string const& prefix) {
    auto registry = ll::service::getCommandRegistry(client);
    if (!registry) return;
    auto& log = UrdeMod::getInstance().getSelf().getLogger();
    auto realRoot = std::string(client ? "urdec" : "urde");
    std::unordered_set<std::string> enumNames;
    for (auto const& [name,signature] : registry->mSignatures) {
        if (name != realRoot && !name.starts_with(prefix)) continue;
        size_t row = 0;
        for (auto const& overload : signature.overloads) {
            size_t column = 0;
            for (auto const& parameter : overload.params) {
                auto enumName = parameter.mEnumNameOrPostfix ? parameter.mEnumNameOrPostfix : "";
                if (*enumName) enumNames.insert(enumName);
                std::string values;
                if (auto it = registry->mEnumLookup.find(enumName); it != registry->mEnumLookup.end())
                    for (auto const& [textIndex,number] : registry->mEnums[it->second].values) {
                        if (textIndex < registry->mEnumValues.size()) values += registry->mEnumValues[textIndex] + ",";
                    }
                if (auto it = registry->mSoftEnumLookup.find(enumName); it != registry->mSoftEnumLookup.end())
                    for (auto const& text : registry->mSoftEnums[it->second].mValues) values += text + ",";
                log.info("[URDE temporary command hint] registry root={} row={} column={} enum={} options={} values={}",
                         name,row,column++,enumName,static_cast<unsigned>(parameter.mOptions),values);
            }
            ++row;
        }
    }
    auto packet = registry->serializeAvailableCommands();
    // Log tables by name: packet parseSymbol encoding is deliberately not
    // assumed to be identical to CommandRegistry::Symbol's internal flags.
    for (size_t index = 0; index < packet.mEnums->size(); ++index) {
        auto const& entry = packet.mEnums.get()[index];
        if (!enumNames.contains(entry.name.get())) continue;
        std::string values;
        for (auto textIndex : entry.values.get())
            if (textIndex < packet.mEnumValues->size()) values += packet.mEnumValues.get()[textIndex] + ",";
        log.info("[URDE temporary command hint] packet hard_enum={} index={} values={}",entry.name.get(),index,values);
    }
    for (size_t index = 0; index < packet.mSoftEnums->size(); ++index) {
        auto const& entry = packet.mSoftEnums.get()[index];
        if (!enumNames.contains(entry.name.get())) continue;
        std::string values;
        for (auto const& text : entry.values.get()) values += text + ",";
        log.info("[URDE temporary command hint] packet soft_enum={} index={} values={}",entry.name.get(),index,values);
    }
    for (auto const& command : packet.mCommands.get()) {
        if (command.name.get() != realRoot && !command.name->starts_with(prefix)) continue;
        size_t row = 0;
        for (auto const& overload : command.overloads.get()) {
            size_t column = 0;
            for (auto const& parameter : overload.params.get())
                log.info("[URDE temporary command hint] packet root={} row={} column={} name={} symbol={} optional={} options={}",
                         command.name.get(),row,column++,parameter.name.get(),parameter.parseSymbol,
                         parameter.optional,static_cast<unsigned>(parameter.paramOptions));
            ++row;
        }
    }
}
inline void registerExperiments(ll::command::CommandRegistrar& registrar, bool client) {
    auto prefix = std::string(client ? "urdechint" : "urdehint");
    auto permission = client ? CommandPermissionLevel::Any : CommandPermissionLevel::GameDirectors;
    auto root = [&](int n) -> ll::command::CommandHandle& {
        return registrar.getOrCreateCommand(prefix+std::to_string(n),"TEMPORARY URDE hint experiment: echo only",permission);
    };
    literal(root(1),prefix+"1",false);
    auto& v2 = root(2);
    shared<Action>(v2,prefix+"2");
    v2.overload<ActionVanilla>().required("action").required("setting").text("vanilla")
        .execute([prefix](CommandOrigin const&, CommandOutput& out, ActionVanilla const& p) {
            out.success("TEMPORARY " + prefix + "2 parsed " + enumText(p.action) + " setting=" + enumText(p.setting) + " vanilla");
        });
    auto& v3 = root(3);
    v3.overload<IdleParams>().required("action").execute([prefix](CommandOrigin const&, CommandOutput& out, IdleParams const& p) {
        out.success("TEMPORARY " + prefix + "3 parsed " + enumText(p.action));
    });
    addSet(v3,prefix+"3",false);
    root(4).overload<SoftParams>().required("action").optional("setting").optional("value")
        .execute([prefix](CommandOrigin const&, CommandOutput& out, SoftParams const& p) { echo(out,prefix+"4",p.action,p); });
    literal(root(5),prefix+"5",true); // only key expansion differs from v1
    shared<Reordered>(root(6),prefix+"6"); // reverse action order, no competing longer row
    try { audit(client,prefix); }
    catch (std::exception const& error) {
        UrdeMod::getInstance().getSelf().getLogger().warn("[URDE temporary command hint] audit failed: {}",error.what());
    }
}
}
#else
namespace urde::hint {
inline void registerExperiments(auto&, bool) {}
}
#endif
