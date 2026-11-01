#include "range/Side.h"
#include "range/Commands.h"
#include "range/Config.h"
#include "range/Limits.h"
#include "range/Scope.h"
#include "range/HookInstall.h"
#include "mod/UrdeMod.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/command/ServerCommandRegisterEvent.h"
#include "ll/api/event/server/ServerStoppingEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"
#include "mc/deps/ecs/WeakEntityRef.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/chunk/ChunkSource.h"
#include "mc/world/level/chunk/ChunkTickOffsetManager.h"
#include "mc/world/level/dimension/ActorReplication.h"

namespace urde::side {
namespace {
struct Config {
    int actorInterestChunks = 8;
    std::vector<ChunkPos> offsets;
    Config() { rebuild(); }
    void validate() const { validateRange(float(actorInterestChunks), MaxChunks, true); }
    void rebuild() {
        validate();
        offsets.clear();
        if (!actorInterestChunks) return;
        offsets.reserve(diskCount(actorInterestChunks));
        for (int x = -actorInterestChunks; x <= actorInterestChunks; ++x)
            for (int z = -actorInterestChunks; z <= actorInterestChunks; ++z)
                if (x*x + z*z <= actorInterestChunks*actorInterestChunks) offsets.emplace_back(x,z);
    }
    void set(std::string const& key, std::string const& value) {
        if (key != "actorInterestChunks") throw std::invalid_argument("Key: actorInterestChunks");
        actorInterestChunks = int(parseRange(value, MaxChunks, true));
        rebuild();
    }
    nlohmann::json encode() const { return {{"actorInterestChunks", encodeRange(float(actorInterestChunks))}}; }
    static Config decode(nlohmann::json const& json) {
        if (!json.is_object() || json.size() != 1) throw std::invalid_argument("Expected only actorInterestChunks");
        Config result;
        result.actorInterestChunks = int(readRange(json,"actorInterestChunks",MaxChunks,true));
        result.rebuild();
        return result;
    }
};
ConfigStore<Config> config;
std::atomic<bool> enabled = false;
std::atomic<uint64_t> generation = 0;
struct TickContext { Level const* level; uint64_t generation; std::shared_ptr<Config const> snapshot; };
struct ReplicationContext { TickContext const* tick; ChunkSource const* source; };
thread_local TickContext* tickContext = nullptr;
thread_local ReplicationContext* replicationContext = nullptr;
ll::event::ListenerPtr commandListener, stopListener;
bool installed = false;

using Tick = void (Level::*)();
using Replicate = void (ActorReplication::*)(std::vector<WeakEntityRef> const&, Level const&, ChunkSource&);
using Sorted = std::vector<ChunkPos> (Level::*)(std::vector<ChunkPos> const&) const;
using Expand = std::vector<ChunkPos> (*)(std::vector<ChunkPos> const&, std::vector<ChunkPos> const&);
static_assert(std::is_same_v<decltype(&Level::tick), Tick>);
static_assert(std::is_same_v<decltype(&Level::$tick), Tick>);
static_assert(std::is_same_v<decltype(&ActorReplication::processReplicationForPlayers), Replicate>);
static_assert(std::is_same_v<decltype(&Level::getSortedPositionsFromClientOffsets), Sorted>);
static_assert(std::is_same_v<decltype(&Level::$getSortedPositionsFromClientOffsets), Sorted>);
static_assert(std::is_same_v<decltype(&ChunkTickOffsetManager::getSortedPositionsFromClientOffsets), Expand>);

LL_TYPE_INSTANCE_HOOK(LevelTick, ll::memory::HookPriority::Normal, Level, &Level::$tick, void) {
    auto* level = thisFor<Level>();
    auto live = ll::service::getLevel();
    if (!enabled || !live || &*live != level || level->isClientSide()) {
        Scope clear(tickContext, static_cast<TickContext*>(nullptr));
        return origin();
    }
    TickContext context{level, generation.load(), config.accept()};
    Scope scope(tickContext, &context);
    origin();
}
LL_TYPE_INSTANCE_HOOK(Replication, ll::memory::HookPriority::Normal, ActorReplication,
    &ActorReplication::processReplicationForPlayers, void,
    std::vector<WeakEntityRef> const& players, Level const& level, ChunkSource& source) {
    auto* tick = tickContext;
    bool valid = enabled && tick && tick->level == &level && tick->generation == generation
              && source.mLevel == &level && !source.mShuttingDown->load();
    ReplicationContext context{tick, &source};
    Scope scope(replicationContext, valid ? &context : nullptr);
    origin(players, level, source);
}
LL_TYPE_INSTANCE_HOOK(SortedPositions, ll::memory::HookPriority::Normal, Level,
    &Level::$getSortedPositionsFromClientOffsets, std::vector<ChunkPos>, std::vector<ChunkPos> const& centers) {
    auto* level = thisFor<Level>();
    auto* context = replicationContext;
    if (enabled && context && context->tick == tickContext && context->tick->level == level
        && context->tick->generation == generation && context->source->mLevel == level) {
        auto const& snapshot = *context->tick->snapshot;
        // Protect the native integer center+offset arithmetic, even at extreme coords.
        bool safe = std::all_of(centers.begin(), centers.end(), [&](ChunkPos const& center) {
            auto r = snapshot.actorInterestChunks;
            return int64_t(center.x)-r >= INT_MIN && int64_t(center.x)+r <= INT_MAX
                && int64_t(center.z)-r >= INT_MIN && int64_t(center.z)+r <= INT_MAX;
        });
        if (snapshot.actorInterestChunks && safe)
            return ChunkTickOffsetManager::getSortedPositionsFromClientOffsets(centers, snapshot.offsets);
    }
    return origin(centers);
}
std::string describe(Config const& value) {
    return "actorInterestChunks=" + rangeText(float(value.actorInterestChunks)) + " offsets="
         + std::to_string(value.actorInterestChunks ? value.offsets.size() : 57);
}
std::string status() {
    auto active = config.active();
    return "server requested: " + describe(*config.desired()) + "; effective(last owner tick): "
         + (active && enabled ? describe(*active) : "native/inactive")
         + "; horizontal chunk disk, resident chunks only; experimental terrain=false (client only)";
}
void commands() {
    auto& command = ll::command::CommandRegistrar::getServerInstance().getOrCreateCommand(
        "urde", "URDE server actor-interest range", CommandPermissionLevel::GameDirectors);
    registerControls(command, status,
        [](auto const& key, auto const& value) { return config.change(key,value) + status(); },
        [] { return config.reload() + status(); });
}
}
bool load(std::filesystem::path const& dir) {
    try { config.load(dir / "server.json"); return true; }
    catch (std::exception const& error) { UrdeMod::getInstance().getSelf().getLogger().error("{}",error.what()); return false; }
}
bool enable() {
    if (installed) return true;
    for (auto symbol : {
        "?$tick@Level@@QEAAXXZ",
        "?processReplicationForPlayers@ActorReplication@@QEAAXAEBV?$vector@VWeakEntityRef@@V?$allocator@VWeakEntityRef@@@std@@@std@@AEBVLevel@@AEAVChunkSource@@@Z",
        "?$getSortedPositionsFromClientOffsets@Level@@QEBA?AV?$vector@VChunkPos@@V?$allocator@VChunkPos@@@std@@@std@@AEBV23@@Z",
        "?getSortedPositionsFromClientOffsets@ChunkTickOffsetManager@@SA?AV?$vector@VChunkPos@@V?$allocator@VChunkPos@@@std@@@std@@AEBV23@0@Z"
    }) if (!requireSymbol(symbol)) return false;
    // Fail closed: no partial feature and never substitute an address.
    if (!installHook<LevelTick>("Level::tick")) return false;
    if (!installHook<Replication>("ActorReplication::processReplicationForPlayers")) { LevelTick::unhook(); return false; }
    if (!installHook<SortedPositions>("Level::getSortedPositionsFromClientOffsets")) { Replication::unhook(); LevelTick::unhook(); return false; }
    installed = true;
    ++generation;
    enabled = true;
    auto& bus = ll::event::EventBus::getInstance();
    commandListener = bus.emplaceListener<ll::event::ServerCommandRegisterEvent>([](auto&) { commands(); });
    stopListener = bus.emplaceListener<ll::event::ServerStoppingEvent>([](auto&) { ++generation; config.clearEffective(); });
    if (ll::service::getCommandRegistry(false)) commands();
    return true;
}
bool disable() {
    enabled = false;
    ++generation;
    config.clearEffective();
    auto& bus = ll::event::EventBus::getInstance();
    if (commandListener) bus.removeListener(commandListener);
    if (stopListener) bus.removeListener(stopListener);
    commandListener = stopListener = nullptr;
    if (installed) { SortedPositions::unhook(); Replication::unhook(); LevelTick::unhook(); installed = false; }
    return true;
}
}
