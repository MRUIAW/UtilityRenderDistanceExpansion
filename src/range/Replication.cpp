#include "range/Replication.h"
#include "range/Limits.h"
#include "range/Scope.h"
#include "range/HookInstall.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"
#include "mc/deps/ecs/WeakEntityRef.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/chunk/ChunkSource.h"
#include "mc/world/level/chunk/ChunkTickOffsetManager.h"
#include "mc/world/level/dimension/ActorReplication.h"

namespace urde::replication {
namespace {
std::atomic<bool> enabled = false;
std::atomic<uint64_t> generation = 0;
std::atomic<uint64_t> observedGeneration = UINT64_MAX;
std::atomic<int> radius = 0;
Provider provide = nullptr; // installed/removed with hooks, never changed by commands
bool installed = false;
struct TickContext { Level const* level; uint64_t generation; std::shared_ptr<Range const> snapshot; };
struct ReplicationContext { TickContext const* tick; ChunkSource const* source; };
thread_local TickContext* tickContext = nullptr;
thread_local ReplicationContext* replicationContext = nullptr;
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
    // On client LL getLevel() obtains ServerInstance's Minecraft, NOT ClientLevel.
    // Remote joins therefore cannot supply this matching non-client Level.
    auto live = ll::service::getLevel();
    auto epoch = generation.load();
    auto snapshot = enabled && live && &*live == level && !level->isClientSide() ? provide() : nullptr;
    if (!snapshot || epoch != generation) {
        Scope clear(tickContext, static_cast<TickContext*>(nullptr));
        return origin();
    }
    TickContext context{level, epoch, std::move(snapshot)};
    Scope scope(tickContext, &context);
    radius = context.snapshot->actorInterestChunks;
    observedGeneration = epoch;
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
}
void Range::rebuild(int value) {
    validateRange(float(value), MaxChunks, true);
    actorInterestChunks = value;
    offsets.clear();
    if (!value) return;
    offsets.reserve(diskCount(value));
    for (int x = -value; x <= value; ++x)
        for (int z = -value; z <= value; ++z)
            if (x*x + z*z <= value*value) offsets.emplace_back(x,z);
}
bool enable(Provider provider) {
    if (installed) return true;
    for (auto symbol : {
        "?$tick@Level@@QEAAXXZ",
        "?processReplicationForPlayers@ActorReplication@@QEAAXAEBV?$vector@VWeakEntityRef@@V?$allocator@VWeakEntityRef@@@std@@@std@@AEBVLevel@@AEAVChunkSource@@@Z",
        "?$getSortedPositionsFromClientOffsets@Level@@QEBA?AV?$vector@VChunkPos@@V?$allocator@VChunkPos@@@std@@@std@@AEBV23@@Z",
        "?getSortedPositionsFromClientOffsets@ChunkTickOffsetManager@@SA?AV?$vector@VChunkPos@@V?$allocator@VChunkPos@@@std@@@std@@AEBV23@0@Z"
    }) if (!requireSymbol(symbol)) return false;
    provide = provider;
    if (!installHook<LevelTick>("Level::tick")) return false;
    if (!installHook<Replication>("ActorReplication::processReplicationForPlayers")) { LevelTick::unhook(); return false; }
    if (!installHook<SortedPositions>("Level::getSortedPositionsFromClientOffsets")) { Replication::unhook(); LevelTick::unhook(); return false; }
    installed = true;
    invalidate();
    enabled = true;
    return true;
}
void invalidate() { ++generation; radius = 0; }
bool active() { return enabled && observedGeneration == generation; }
int effectiveRadius() { return observedGeneration == generation ? radius.load() : 0; }
void disable() {
    enabled = false;
    invalidate();
    if (installed) { SortedPositions::unhook(); Replication::unhook(); LevelTick::unhook(); installed = false; }
}
}
