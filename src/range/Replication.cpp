#include "range/Replication.h"
#include "range/Limits.h"
#include "range/Scope.h"
#include "range/HookInstall.h"
#include "range/MovementDiagnostics.h"
#include "range/MovementPairs.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"
#include "mc/deps/ecs/WeakEntityRef.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/Tick.h"
#include "mc/world/level/chunk/ChunkSource.h"
#include "mc/world/level/chunk/ChunkTickOffsetManager.h"
#include "mc/world/level/dimension/ActorReplication.h"
#include "mc/network/SpatialActorNetworkData.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/player/Player.h"
#include <array>
#include <chrono>

namespace urde::replication {
namespace {
std::atomic<bool> enabled = false;
std::atomic<uint64_t> generation = 0;
std::atomic<uint64_t> observedGeneration = UINT64_MAX;
std::atomic<int> radius = 0;
std::atomic<std::shared_ptr<Range const>> observedPolicy;
Provider provide = nullptr; // installed/removed with hooks, never changed by commands
bool installed = false;
struct PairState {
    uint64_t generation;
    movement::PairTable<> table;
    bool vanilla = false;
    uint64_t pruned = 0;
    explicit PairState(uint64_t epoch) : generation(epoch) {}
};
// Invalidation drops publication immediately, even when called off the server
// thread. An in-flight tick owns its old table until its stack unwinds; generation
// checks prevent further use. No cross-thread table mutation or per-call locks.
std::atomic<std::shared_ptr<PairState>> pairState;
struct TickContext {
    Level const* level;
    uint64_t generation;
    std::shared_ptr<Range const> snapshot;
    std::shared_ptr<PairState> pairs;
};
struct SendContext {
    SpatialActorNetworkData const* receiver;
    TickContext const* tick;
    uint64_t now;
    std::optional<movement::Settings::Selection> selection;
};
struct ReplicationContext { TickContext const* tick; ChunkSource const* source; };
thread_local TickContext* tickContext = nullptr;
thread_local ReplicationContext* replicationContext = nullptr;
thread_local SendContext* sendContext = nullptr;
using Tick = void (Level::*)();
using Replicate = void (ActorReplication::*)(std::vector<WeakEntityRef> const&, Level const&, ChunkSource&);
using Sorted = std::vector<ChunkPos> (Level::*)(std::vector<ChunkPos> const&) const;
using Expand = std::vector<ChunkPos> (*)(std::vector<ChunkPos> const&, std::vector<ChunkPos> const&);
using Interval = std::optional<uint64> (SpatialActorNetworkData::*)(Player&) const;
using SendUpdate = void (SpatialActorNetworkData::*)(bool,bool,bool,bool);
static_assert(std::is_same_v<decltype(&SpatialActorNetworkData::_getOptimizationUpdateInterval), Interval>);
static_assert(std::is_same_v<decltype(&SpatialActorNetworkData::sendUpdate), SendUpdate>);
static_assert(std::is_same_v<decltype(SpatialActorNetworkData::mEntity), Actor&>);
static_assert(!std::is_polymorphic_v<SpatialActorNetworkData>); // direct receiver; thisFor is constrained to polymorphic types
static_assert(std::is_same_v<decltype(&Level::tick), Tick>);
static_assert(std::is_same_v<decltype(&Level::$tick), Tick>);
static_assert(std::is_same_v<decltype(&ActorReplication::processReplicationForPlayers), Replicate>);
static_assert(std::is_same_v<decltype(&Level::getSortedPositionsFromClientOffsets), Sorted>);
static_assert(std::is_same_v<decltype(&Level::$getSortedPositionsFromClientOffsets), Sorted>);
static_assert(std::is_same_v<decltype(&ChunkTickOffsetManager::getSortedPositionsFromClientOffsets), Expand>);

// BEGIN temporary movement diagnostics (extended in 0.3.0). Collection uses only
// relaxed atomics; the lever separately mutates tick-confined stable-ID records.
movement::diagnostics::Counters diagnostics;
auto lastDiagnostics = std::chrono::steady_clock::now();
uint64_t diagnosticsGeneration = UINT64_MAX; // matching owner tick only
bool diagnosticsActive() {
    return enabled && tickContext && tickContext->generation == generation;
}
bool diagnosticsActor(Actor const& actor) {
    return !actor.mRemoved && actor.mLevel && &actor.getLevel() == tickContext->level;
}
void reportDiagnostics() {
    auto now = std::chrono::steady_clock::now();
    if (now-lastDiagnostics < std::chrono::seconds(2)) return;
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now-lastDiagnostics).count();
    lastDiagnostics = now;
    auto take = [](auto& count) { return count.exchange(0,std::memory_order_relaxed); };
    auto histogram = [&](auto& values) {
        std::string histograms;
        for (size_t sender = 0; sender < 2; ++sender) {
            histograms += sender == 0 ? " player={" : " other={";
            for (size_t distance = 0; distance < 3; ++distance) {
                if (distance) histograms += ',';
                histograms += std::array{"lt64:[","64to128:[","ge128:["}[distance];
                for (size_t value = 0; value < 11; ++value) {
                    if (value) histograms += ',';
                    histograms += std::to_string(take(values[sender][distance][value]));
                }
                histograms += ']';
            }
            histograms += '}';
        }
        return histograms;
    };
    auto native = histogram(diagnostics.intervals);
    auto final = histogram(diagnostics.finalIntervals);
    UrdeMod::getInstance().getSelf().getLogger().info(
        "[URDE temporary movement diagnostics] window_ms={} ticks={} bins=nullopt,0,1,2,3,4,5-6,7,8-9,10,>10 native={} FINAL={} lever_opened={} lever_capped={} lever_full_rate={} lever_late={} pair_overflow={} sendUpdate_calls={{player:{},other:{}}} skipped={{interval:{},sendUpdate:{}}}",
        elapsed,take(diagnostics.ticks),native,final,take(diagnostics.leverOpened),take(diagnostics.leverCapped),
        take(diagnostics.leverFullRate),take(diagnostics.leverLate),take(diagnostics.pairOverflow),take(diagnostics.sendCalls[0]),take(diagnostics.sendCalls[1]),
        take(diagnostics.skippedInterval),take(diagnostics.skippedSend));
}

LL_TYPE_INSTANCE_HOOK(UpdateInterval, ll::memory::HookPriority::Normal, SpatialActorNetworkData,
    &SpatialActorNetworkData::_getOptimizationUpdateInterval, std::optional<uint64>, Player& player) {
    auto native = origin(player);
    if (!diagnosticsActive()) return native;
    auto& actor = mEntity;
    if (!diagnosticsActor(actor) || !diagnosticsActor(player)) {
        diagnostics.skippedInterval.fetch_add(1,std::memory_order_relaxed);
        return native;
    }
    auto const& sender = actor.getPosition();
    auto const& recipient = player.getPosition();
    auto distance = movement::diagnostics::distanceBucket(
        double(sender.x)-recipient.x,double(sender.y)-recipient.y,double(sender.z)-recipient.z);
    auto senderKind = actor.isPlayer() ? 0 : 1;
    auto final = native;
    auto* send = sendContext;
    auto const& settings = tickContext->snapshot->movement;
    if (send && send->receiver == static_cast<SpatialActorNetworkData const*>(this)
        && send->tick == tickContext && settings.updateInterval
        && actor.hasUniqueID() && player.hasUniqueID()) {
        if (!send->selection) send->selection = settings.select(
            [&]() -> std::string const& { return actor.getTypeName(); },
            [&](auto const& tag) { return actor.hasTag(tag); });
        auto* record = tickContext->pairs->table.touch(actor.getOrCreateUniqueID().rawID,
                                                     player.getOrCreateUniqueID().rawID,send->now);
        if (record) {
            auto decision = movement::decide(native,send->now,record->lastEngaged(),send->selection->interval);
            final = decision.interval;
            if (final) record->sent(send->now);
            if (decision.late) diagnostics.leverLate.fetch_add(1,std::memory_order_relaxed);
            if (decision.outcome == movement::Outcome::opened) diagnostics.leverOpened.fetch_add(1,std::memory_order_relaxed);
            if (decision.outcome == movement::Outcome::capped) diagnostics.leverCapped.fetch_add(1,std::memory_order_relaxed);
            if (decision.outcome != movement::Outcome::unchanged && send->selection->fullRate)
                diagnostics.leverFullRate.fetch_add(1,std::memory_order_relaxed);
        } else diagnostics.pairOverflow.fetch_add(1,std::memory_order_relaxed);
    }
    if (distance) {
        diagnostics.intervals[senderKind][*distance][movement::diagnostics::valueBucket(native)]
            .fetch_add(1,std::memory_order_relaxed);
        diagnostics.finalIntervals[senderKind][*distance][movement::diagnostics::valueBucket(final)]
            .fetch_add(1,std::memory_order_relaxed);
    } else diagnostics.skippedInterval.fetch_add(1,std::memory_order_relaxed);
    return final;
}

LL_TYPE_INSTANCE_HOOK(ObserveSendUpdate, ll::memory::HookPriority::Normal, SpatialActorNetworkData,
    &SpatialActorNetworkData::sendUpdate, void,
    bool forceMove, bool forceMoveLocalEntity, bool forceAbsoluteMovement, bool forceCompletion) {
    bool valid = diagnosticsActive() && diagnosticsActor(mEntity);
    SendContext context{this,tickContext,valid ? tickContext->level->getCurrentTick().tickID : 0,{}};
    Scope scope(sendContext,valid ? &context : nullptr);
    if (diagnosticsActive()) {
        auto& actor = mEntity;
        if (diagnosticsActor(actor)) diagnostics.sendCalls[actor.isPlayer() ? 0 : 1].fetch_add(1,std::memory_order_relaxed);
        else diagnostics.skippedSend.fetch_add(1,std::memory_order_relaxed);
    }
    // Multiple native callers are safe: all flags forwarded, exactly one origin;
    // calls may yield zero/multiple recipient packets and are not deliveries.
    origin(forceMove,forceMoveLocalEntity,forceAbsoluteMovement,forceCompletion);
}
// END temporary movement diagnostics

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
    auto pairs = pairState.load();
    if (!pairs || pairs->generation != epoch) {
        pairs = std::make_shared<PairState>(epoch);
        pairState.store(pairs);
        // Close the publication race with an off-thread invalidate.
        if (epoch != generation) { pairState.store(nullptr); return origin(); }
    }
    if (!snapshot->movement.updateInterval && !pairs->vanilla) pairs->table.clear();
    pairs->vanilla = !snapshot->movement.updateInterval;
    TickContext context{level, epoch, std::move(snapshot),std::move(pairs)};
    Scope scope(tickContext, &context);
    if (diagnosticsGeneration != epoch) {
        diagnosticsGeneration = epoch;
        lastDiagnostics = std::chrono::steady_clock::now();
    }
    radius = context.snapshot->actorInterestChunks;
    observedGeneration = epoch;
    observedPolicy.store(context.snapshot);
    diagnostics.ticks.fetch_add(1,std::memory_order_relaxed);
    origin();
    auto now = level->getCurrentTick().tickID;
    if (now < context.pairs->pruned || now-context.pairs->pruned >= 20) {
        context.pairs->table.prune(now);
        context.pairs->pruned = now;
    }
    if (enabled && epoch == generation) reportDiagnostics();
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
    if (!installHook<UpdateInterval>("SpatialActorNetworkData::_getOptimizationUpdateInterval")) {
        SortedPositions::unhook(); Replication::unhook(); LevelTick::unhook(); return false;
    }
    if (!installHook<ObserveSendUpdate>("SpatialActorNetworkData::sendUpdate")) {
        UpdateInterval::unhook(); SortedPositions::unhook(); Replication::unhook(); LevelTick::unhook(); return false;
    }
    installed = true;
    invalidate();
    enabled = true;
    return true;
}
void invalidate() {
    ++generation; radius = 0; observedPolicy.store(nullptr);
    pairState.store(nullptr);
    diagnostics.reset();
}
bool active() { return enabled && observedGeneration == generation; }
int effectiveRadius() { return observedGeneration == generation ? radius.load() : 0; }
std::string effectiveMovement() {
    auto policy = observedPolicy.load();
    return policy && active() ? policy->movement.describe() + " (scoped movement lever)" : std::string("native/inactive");
}
void disable() {
    enabled = false;
    invalidate();
    if (installed) { ObserveSendUpdate::unhook(); UpdateInterval::unhook(); SortedPositions::unhook(); Replication::unhook(); LevelTick::unhook(); installed = false; }
}
}
