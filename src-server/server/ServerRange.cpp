#include "range/Side.h"
#include "range/Commands.h"
#include "range/CommandErrorOutput.h"
#include "range/Config.h"
#include "range/Limits.h"
#include "range/Replication.h"
#include "mod/UrdeMod.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/command/ServerCommandRegisterEvent.h"
#include "ll/api/event/server/ServerStoppingEvent.h"
#include "ll/api/service/Bedrock.h"

namespace urde::side {
namespace {
struct Config : replication::Range {
    Config() { rebuild(32); }
    void validate() const { validateRange(float(actorInterestChunks), MaxServerChunks, true); movement.validate(); }
    void set(std::string const& key, std::string const& value) {
        if (key == "actor_interest_chunks") rebuild(int(parseRange(value, MaxServerChunks, true)));
        else if (movement::Settings::isKey(key)) movement.set(key,value);
        else throw std::invalid_argument("Unknown server setting: " + key);
    }
    nlohmann::json encode() const {
        auto json = movement.encode();
        json["actor_interest_chunks"] = encodeRange(float(actorInterestChunks));
        return json;
    }
    static Config decode(nlohmann::json json) {
        json = migrateKeys(std::move(json),{{"actor_interest_chunks","actorInterestChunks"}});
        if (!json.is_object()) throw std::invalid_argument("Expected server config object");
        for (auto const& [key,value] : json.items())
            if (key != "actor_interest_chunks" && !movement::Settings::isConfigKey(key))
                throw std::invalid_argument("Unknown server config key: " + key);
        Config result;
        result.rebuild(int(readRange(json,"actor_interest_chunks",MaxServerChunks,true)));
        result.movement = movement::Settings::decode(json,4);
        return result;
    }
};
ConfigStore<Config> config;
std::atomic<bool> enabled = false;
ll::event::ListenerPtr commandListener, commandErrorListener, stopListener;
bool installed = false;
std::string describe(Config const& value) {
    return "actor_interest_chunks=" + rangeText(float(value.actorInterestChunks)) + " offsets="
         + std::to_string(value.actorInterestChunks ? value.offsets.size() : 57) + " " + value.movement.describe();
}
std::string status() {
    auto active = config.active();
    return "server requested: " + describe(*config.desired()) + "; effective(last owner tick): "
         + (active && enabled ? describe(*active) : "native/inactive")
          + "; movement(last_server_tick): " + replication::effectiveMovement()
          + "; horizontal chunk disk, resident chunks only; experimental terrain=false (client only)";
}
void commands() {
    auto& command = ll::command::CommandRegistrar::getServerInstance().getOrCreateCommand(
        "urde", "URDE server actor-interest range", CommandPermissionLevel::GameDirectors);
    registerControls<UrdeServerKey,int>(command, status,
        [](auto const& key, auto const& value) { return config.change(key,value) + status(); },
        [] { return config.reload() + status(); });
}
}
bool load(std::filesystem::path const& dir) {
    try {
        config.load(dir / "server.json", [] {
            UrdeMod::getInstance().getSelf().getLogger().info("Ignoring legacy full_rate_players; use full_rate_types (default minecraft:player). Next save removes the legacy key.");
        });
        return true;
    }
    catch (std::exception const& error) { UrdeMod::getInstance().getSelf().getLogger().error("{}",error.what()); return false; }
}
bool enable() {
    if (installed) return true;
    if (!replication::enable([]() -> std::shared_ptr<replication::Range const> { return config.accept(); })) return false;
    installed = true;
    enabled = true;
    auto& bus = ll::event::EventBus::getInstance();
    commandListener = bus.emplaceListener<ll::event::ServerCommandRegisterEvent>([](auto&) { commands(); });
    commandErrorListener = bus.emplaceListener<ll::event::ExecutedCommandEvent>([](auto& event) { explainCommandMisuse(event,false); });
    stopListener = bus.emplaceListener<ll::event::ServerStoppingEvent>([](auto&) { replication::invalidate(); config.clearEffective(); });
    if (ll::service::getCommandRegistry(false)) commands();
    return true;
}
bool disable() {
    enabled = false;
    replication::disable();
    config.clearEffective();
    auto& bus = ll::event::EventBus::getInstance();
    if (commandListener) bus.removeListener(commandListener);
    if (commandErrorListener) bus.removeListener(commandErrorListener);
    if (stopListener) bus.removeListener(stopListener);
    commandListener = commandErrorListener = stopListener = nullptr;
    installed = false;
    return true;
}
}
