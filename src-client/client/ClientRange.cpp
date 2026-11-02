#include "range/Side.h"
#include "range/Commands.h"
#include "range/Config.h"
#include "range/Limits.h"
#include "range/Scope.h"
#include "range/HookInstall.h"
#include "range/Replication.h"
#include "client/ClientConfig.h"
#include "mod/UrdeMod.h"
#include <condition_variable>
#include <thread>
#include <unordered_set>
#include <vector>
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/command/ClientCommandRegisterEvent.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/client/ClientStartJoinLevelEvent.h"
#include "ll/api/event/world/ClientLevelTickEvent.h"
#include "ll/api/event/server/ServerStoppingEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "ll/api/service/Bedrock.h"
#include "mc/client/multiplayer/ClientLevel.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/client/renderer/game/LevelRenderer.h"
#include "mc/client/renderer/game/LevelRendererPlayer.h"
#include "mc/client/renderer/game/LevelRendererShadowCamera.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/actor/VanillaBlockActor.h"

namespace urde::side {
namespace {
struct Config : client::Settings {
    replication::Range localReplication;
    int derivedReplicationChunks = 0;
    int derivedReplicationOffsets = 0;
    Config() { rebuild(); }
    void rebuild() {
        validate();
        derivedReplicationChunks = requiredLocalChunks(entityRange,replicationMargin);
        derivedReplicationOffsets = entityRange > 0 ? diskCount(derivedReplicationChunks) : 0;
        localReplication.rebuild(localReplicationChunks(entityRange,replicationMargin));
    }
    void set(std::string const& key, std::string const& text) {
        client::Settings::set(key,text);
        rebuild();
    }
    static Config decode(nlohmann::json json) {
        Config result;
        static_cast<client::Settings&>(result) = client::Settings::decode(std::move(json));
        result.rebuild();
        return result;
    }
};
ConfigStore<Config> config;
std::atomic<bool> enabled = false, session = false;
std::atomic<uint64_t> generation = 0;
// Native pointers never escape the owner thread or synchronous queue/draw scopes.
// The persistent session identities are compared only on the client tick thread.
std::atomic<Level const*> ownerLevel = nullptr;
std::atomic<Dimension const*> ownerDimension = nullptr;
std::atomic<float> observedFar = 0, observedFog = 0;
std::mutex ownerMutex;
std::condition_variable restored;
std::thread::id ownerThread;
ll::event::ListenerPtr commandListener, tickListener, exitListener, joinListener, stopListener;
bool installed = false;

struct QueueContext {
    uint64_t generation;
    LevelRendererCamera* camera;
    BlockSource* source;
    Vec3 position;
    AABB nativeBox;
    std::shared_ptr<Config const> snapshot;
    bool actorsFetched = false, blocksFetched = false;
    std::unordered_set<IVanillaRenderBlockActorComponent const*> components;
    std::vector<gsl::not_null<Actor*>> filteredActors;
};
thread_local QueueContext* queueContext = nullptr;
thread_local QueueContext* drawContext = nullptr;
thread_local bool nativeRefresh = false;

bool validCamera(LevelRendererCamera& camera) {
    if (!session.load()) return false;
    auto client = ll::service::getClientInstance();
    if (!client || !client->getLevel()) return false;
    auto* renderer = client->getLevelRenderer();
    if (!renderer) return false;
    // Exclude constructor/destructor and detached cameras BEFORE inspecting
    // their world/view-region members, which may not be initialized there.
    bool owned = static_cast<LevelRendererCamera*>(renderer->mLevelRendererPlayer.get()) == &camera
              || static_cast<LevelRendererCamera*>(renderer->mLevelRendererShadowCamera.get()) == &camera
              || static_cast<LevelRendererCamera*>(renderer->mLevelRendererCloudShadowCamera.get()) == &camera;
    if (!owned) return false;
    auto level = ll::service::getMultiPlayerLevel();
    return level && &*level == &camera.mLevel && &camera.mLevel == ownerLevel
        && camera.mDimension == ownerDimension && camera.mViewRegion->get()
        && camera.mDimension && &camera.mViewRegion->get()->getLevel() == &*level
        && &camera.mViewRegion->get()->getDimension() == camera.mDimension;
}
bool validScope(QueueContext const& context) {
    return enabled && session && context.generation == generation
        && context.source == context.camera->mViewRegion->get()
        && validCamera(*context.camera);
}
AABB cube(Vec3 const& p, float r) {
    // Explicit component arithmetic matches the native ordinary collector.
    return {{p.x-r,p.y-r,p.z-r},{p.x+r,p.y+r,p.z+r}};
}
bool same(Vec3 const& a, Vec3 const& b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
bool same(AABB const& a, AABB const& b) { return same(a.min,b.min) && same(a.max,b.max); }
bool finite(Vec3 const& p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }

using Queue = void (LevelRendererCamera::*)(LevelRenderPreRenderUpdateParameters const&);
using DrawBlocks = void (LevelRendererCamera::*)(BaseActorRenderContext&, bool);
using FetchActors = gsl::span<gsl::not_null<Actor*>> (BlockSource::*)(Actor const*, AABB const&, bool, bool);
using FetchBlockActors = void (BlockSource::*)(AABB const&, std::vector<gsl::not_null<IVanillaRenderBlockActorComponent*>>&, bool);
using Within = bool (VanillaBlockActor::*)(Vec3 const&) const;
using FarDetail = void (LevelRendererCamera::*)();
static_assert(std::is_same_v<decltype(&LevelRendererCamera::queueRenderEntities), Queue>);
static_assert(std::is_same_v<decltype(&LevelRendererCamera::$queueRenderEntities), Queue>);
static_assert(std::is_same_v<decltype(&LevelRendererCamera::renderBlockEntities), DrawBlocks>);
static_assert(std::is_same_v<decltype(&LevelRendererCamera::$renderBlockEntities), DrawBlocks>);
static_assert(std::is_same_v<decltype(static_cast<FetchActors>(&BlockSource::fetchEntities)), FetchActors>);
static_assert(std::is_same_v<decltype(static_cast<FetchActors>(&BlockSource::$fetchEntities)), FetchActors>);
static_assert(std::is_same_v<decltype(&BlockSource::fetchRenderBlockActorComponents), FetchBlockActors>);
static_assert(std::is_same_v<decltype(&VanillaBlockActor::isWithinRenderDistance), Within>);
static_assert(std::is_same_v<decltype(&VanillaBlockActor::$isWithinRenderDistance), Within>);
static_assert(std::is_same_v<decltype(&LevelRendererCamera::updateFarChunksDistance), FarDetail>);

LL_TYPE_INSTANCE_HOOK(FarChunks, ll::memory::HookPriority::Normal, LevelRendererCamera,
    &LevelRendererCamera::updateFarChunksDistance, void) {
    origin(); // native fog start/end arithmetic is preserved
    if (!enabled || nativeRefresh || !validCamera(*this)) return;
    auto snapshot = queueContext && queueContext->camera == this && validScope(*queueContext)
                  ? queueContext->snapshot : config.active();
    if (snapshot && snapshot->terrainDetail) mFarChunksDistance = snapshot->terrainDetail;
    observedFar = mFarChunksDistance;
    observedFog = mCurrentDistanceFog->mEnd;
}
LL_TYPE_INSTANCE_HOOK(QueueEntities, ll::memory::HookPriority::Normal, LevelRendererCamera,
    &LevelRendererCamera::$queueRenderEntities, void, LevelRenderPreRenderUpdateParameters const& parameters) {
    Scope clearDraw(drawContext, static_cast<QueueContext*>(nullptr));
    auto snapshot = config.active();
    if (!enabled || !snapshot || !validCamera(*this)) {
        Scope clear(queueContext, static_cast<QueueContext*>(nullptr));
        return origin(parameters);
    }
    Vec3 position = mCameraTargetPos;
    float fog = mCurrentDistanceFog->mEnd;
    if (!finite(position) || !std::isfinite(fog) || fog < 0) {
        Scope clear(queueContext, static_cast<QueueContext*>(nullptr));
        return origin(parameters);
    }
    QueueContext context{generation.load(), this, mViewRegion->get(), position,
                         cube(position,std::min(72.0f,fog)), std::move(snapshot)};
    Scope scope(queueContext,&context);
    origin(parameters);
}
LL_TYPE_INSTANCE_HOOK(DrawBlockEntities, ll::memory::HookPriority::Normal, LevelRendererCamera,
    &LevelRendererCamera::$renderBlockEntities, void, BaseActorRenderContext& renderContext, bool alpha) {
    Scope clearQueue(queueContext, static_cast<QueueContext*>(nullptr));
    Scope clearDraw(drawContext, static_cast<QueueContext*>(nullptr));
    auto snapshot = config.active();
    if (!enabled || !snapshot || !validCamera(*this)) return origin(renderContext,alpha);
    auto client = ll::service::getClientInstance();
    auto* renderer = client->getLevelRenderer();
    // Only the active player camera's ordinary/alpha world draw is proven here.
    // Shadow overrides, detached/offscreen cameras, UI/items and server stay native.
    if (static_cast<LevelRendererCamera*>(renderer->mLevelRendererPlayer.get()) != this
        || &renderContext.mClientInstance != &*client || !renderContext.mImpl.get())
        return origin(renderContext,alpha);
    Vec3 position = renderContext.mImpl->mCameraTargetPosition;
    if (!finite(position)) return origin(renderContext,alpha);
    QueueContext context{generation.load(),this,mViewRegion->get(),position,{},std::move(snapshot)};
    context.blocksFetched = true; // membership comes from the native final draw list
    auto const& list = alpha ? mRenderComponentRenderAlphaQueue.get() : mRenderComponentRenderQueue.get();
    context.components.reserve(list.size());
    for (auto component : list) context.components.insert(component.get());
    Scope scope(drawContext,&context);
    origin(renderContext,alpha);
}
LL_TYPE_INSTANCE_HOOK(FetchEntities, ll::memory::HookPriority::Normal, BlockSource,
    static_cast<FetchActors>(&BlockSource::$fetchEntities), gsl::span<gsl::not_null<Actor*>>,
    Actor const* except, AABB const& box, bool hitbox, bool display) {
    auto* source = thisFor<BlockSource>();
    auto* context = queueContext;
    if (context && validScope(*context) && context->source == source && !context->actorsFetched
        && !except && !hitbox && !display && same(box,context->nativeBox)) {
        context->actorsFetched = true;
        if (context->snapshot->entityRange) {
            auto actors = origin(except,cube(context->position,context->snapshot->entityRange),hitbox,display);
            if (context->snapshot->entityShape == UrdeEntityShape::square) return actors;
            double const r = context->snapshot->entityRange;
            context->filteredActors.reserve(actors.size());
            for (auto actor : actors) {
                auto const& p = actor->getPosition();
                double dx = double(p.x) - context->position.x;
                double dy = double(p.y) - context->position.y;
                double dz = double(p.z) - context->position.z;
                if (dx*dx + dz*dz <= r*r && std::abs(dy) <= r) context->filteredActors.push_back(actor);
            }
            // The enclosing queue owns this vector until native span consumption ends.
            return {context->filteredActors.data(), context->filteredActors.size()};
        }
    }
    return origin(except,box,hitbox,display); // native span and storage lifetime retained
}
LL_TYPE_INSTANCE_HOOK(FetchBlockEntities, ll::memory::HookPriority::Normal, BlockSource,
    &BlockSource::fetchRenderBlockActorComponents, void, AABB const& box,
    std::vector<gsl::not_null<IVanillaRenderBlockActorComponent*>>& result, bool flag) {
    auto* context = queueContext;
    if (!context || !validScope(*context)) return origin(box,result,flag);
    bool sourceMatch = context->source == thisFor<BlockSource>();
    bool listMatch = &result == &context->camera->mTempBlockActorList.get();
    bool boxMatch = same(box,context->nativeBox);
    bool match = sourceMatch && listMatch && boxMatch && flag && !context->blocksFetched;
    if (match) {
        // Mark consumed before native invocation to keep nested fetches native.
        context->blocksFetched = true;
        if (context->snapshot->blockEntityRange)
            origin(cube(context->position,context->snapshot->blockEntityRange),result,flag);
        else origin(box,result,flag);
        context->components.reserve(result.size());
        for (auto component : result) context->components.insert(component.get());
    } else {
        origin(box,result,flag);
    }
}
LL_TYPE_INSTANCE_HOOK(BlockWithin, ll::memory::HookPriority::Normal, VanillaBlockActor,
    &VanillaBlockActor::$isWithinRenderDistance, bool, Vec3 const& position) {
    // Native symbol receives the secondary render-component subobject, not the
    // BlockActor/complete-object address. Hook::thisFor<T> expects that RECEIVER
    // base T (see LL Hook.h), and performs the base-to-complete adjustment.
    auto* actor = thisFor<IVanillaRenderBlockActorComponent>();
    auto* context = drawContext ? drawContext : queueContext;
    if (context && validScope(*context) && context->blocksFetched && context->snapshot->blockEntityRange
        && same(position,context->position)
        && context->components.contains(static_cast<IVanillaRenderBlockActorComponent*>(actor))) {
        auto const& box = actor->getAABB(); // preserve native virtual AABB selection
        double dx = double(position.x)-std::clamp(position.x,box.min.x,box.max.x);
        double dy = double(position.y)-std::clamp(position.y,box.min.y,box.max.y);
        double dz = double(position.z)-std::clamp(position.z,box.min.z,box.max.z);
        double r = context->snapshot->blockEntityRange;
        double distanceSquared = dx*dx + dy*dy + dz*dz;
        return distanceSquared < r*r;
    }
    return origin(position); // derived overrides are not detoured
}

// Reacquire live, shared-owned cameras each tick; never cache a raw camera or
// capture one in a queued command. All three native active renderer cameras are
// refreshed, including restoration when terrainDetail becomes vanilla.
void refreshCameras(bool nativeOnly = false) {
    struct RestoreFlag {
        bool previous;
        explicit RestoreFlag(bool value) : previous(std::exchange(nativeRefresh,value)) {}
        ~RestoreFlag() { nativeRefresh = previous; }
    } flag(nativeOnly);
    auto client = ll::service::getClientInstance();
    if (!client || !client->getLevel()) return;
    auto* renderer = client->getLevelRenderer();
    if (!renderer) return;
    auto player = renderer->mLevelRendererPlayer;
    auto shadow = renderer->mLevelRendererShadowCamera;
    auto cloud = renderer->mLevelRendererCloudShadowCamera;
    for (LevelRendererCamera* camera : {static_cast<LevelRendererCamera*>(player.get()),
                                       static_cast<LevelRendererCamera*>(shadow.get()),
                                       static_cast<LevelRendererCamera*>(cloud.get())}) {
        if (camera && validCamera(*camera)) camera->updateFarChunksDistance();
    }
}
void clearSession() {
    // Close the cross-thread hosting provider before invalidating its scopes.
    { std::lock_guard lock(ownerMutex); session = false; }
    ++generation;
    replication::invalidate();
    config.clearEffective();
    ownerLevel = nullptr;
    ownerDimension = nullptr;
    restored.notify_all();
}
void tick(Level& level) {
    { std::lock_guard lock(ownerMutex); ownerThread = std::this_thread::get_id(); }
    if (!enabled) {
        if (session) { refreshCameras(); clearSession(); }
        return;
    }
    auto client = ll::service::getClientInstance();
    if (!client || client->getLevel() != &level || !client->getLocalPlayer()) return;
    auto* player = client->getLocalPlayer();
    auto* dimension = &player->getDimension();
    if (ownerLevel != &level || ownerDimension != dimension) {
        // Restore any still-live previous-dimension cameras before forgetting
        // their identity. Destroyed cameras are never retained or dereferenced.
        refreshCameras(true);
        clearSession();
        ownerLevel = &level;
        ownerDimension = dimension;
    }
    session = true;
    config.accept();
    // Explicit refresh, not waiting for an unrelated native distance event.
    // Reapplying each tick also covers native camera recreation without a pointer cache.
    refreshCameras();
}
std::string describe(Config const& value) {
    return "entity_range=" + rangeText(value.entityRange) + " block_entity_range=" + rangeText(value.blockEntityRange)
         + " terrain_detail=" + rangeText(value.terrainDetail)
         + " entity_shape=" + std::string(value.entityShape == UrdeEntityShape::circle ? "circle" : "square")
         + " replication_margin=" + commandNumberText(value.replicationMargin);
}
std::string status() {
    auto active = config.active();
    auto desired = config.desired();
    int derived = desired->localReplication.actorInterestChunks;
    return "client requested: " + describe(*desired) + "; effective(last owner tick): "
         + (active && enabled && session ? describe(*active) : "native/inactive")
          + "; last_refreshed_camera_f=" + std::to_string(observedFar.load())
           + " fog_end=" + std::to_string(observedFog.load())
           + "; local_replication=" + rangeText(float(derived))
           + (desired->entityRange && !derived ? " (derived disk below native floor)" : "")
           + " derived_chunks=" + std::to_string(desired->derivedReplicationChunks)
           + " derived_offsets=" + std::to_string(desired->derivedReplicationOffsets)
           + " hosting_replication_active=" + (enabled && session && replication::active() ? "true" : "false")
             + " local_replication_offsets=" + std::to_string(derived ? desired->localReplication.offsets.size() : 57)
            + " local_replication_chunks(last_server_tick)=" + rangeText(float(replication::effectiveRadius()))
           + "; terrain_detail experimental=true (VV/RTX glass NOT guaranteed); enabled collection ignores fog (vanilla keeps native fog limit), visual fog unchanged; "
            "entity shape applies to ordinary collection / strict 3D block-AABB predicate / integer subchunk-center terrain LOD; "
            "residency, culler and renderer gates still apply; remote server /urde still caps available actors";
}
void commands() {
    auto& command = ll::command::CommandRegistrar::getClientInstance().getOrCreateCommand(
        "urdec", "URDE client-local ranges (terrain detail experimental)");
    registerControls<UrdeClientKey,float>(command,status,
        [](auto const& key, auto const& value) { return config.change(key,value) + status(); },
        [] { return config.reload() + status(); });
}
}
bool load(std::filesystem::path const& dir) {
    try { config.load(dir / "client.json"); return true; }
    catch (std::exception const& error) { UrdeMod::getInstance().getSelf().getLogger().error("{}",error.what()); return false; }
}
bool enable() {
    if (installed) { enabled = true; return true; }
    for (auto symbol : {
        "?$queueRenderEntities@LevelRendererCamera@@QEAAXAEBULevelRenderPreRenderUpdateParameters@@@Z",
        "?$renderBlockEntities@LevelRendererCamera@@QEAAXAEAVBaseActorRenderContext@@_N@Z",
        "?$fetchEntities@BlockSource@@QEAA?AV?$span@V?$not_null@PEAVActor@@@gsl@@$0?0@gsl@@PEBVActor@@AEBVAABB@@_N2@Z",
        "?fetchRenderBlockActorComponents@BlockSource@@QEAAXAEBVAABB@@AEAV?$vector@V?$not_null@PEAVIVanillaRenderBlockActorComponent@@@gsl@@V?$allocator@V?$not_null@PEAVIVanillaRenderBlockActorComponent@@@gsl@@@std@@@std@@_N@Z",
        "?$isWithinRenderDistance@VanillaBlockActor@@QEBA_NAEBVVec3@@@Z",
        "?updateFarChunksDistance@LevelRendererCamera@@QEAAXXZ"
    }) if (!requireSymbol(symbol)) return false;
    // Manual registration permits fail-closed rollback, unlike an auto registrar.
    if (!installHook<FarChunks>("LevelRendererCamera::updateFarChunksDistance")) return false;
    if (!installHook<QueueEntities>("LevelRendererCamera::queueRenderEntities")) { FarChunks::unhook(); return false; }
    if (!installHook<FetchEntities>("BlockSource::fetchEntities")) { QueueEntities::unhook(); FarChunks::unhook(); return false; }
    if (!installHook<FetchBlockEntities>("BlockSource::fetchRenderBlockActorComponents")) { FetchEntities::unhook(); QueueEntities::unhook(); FarChunks::unhook(); return false; }
    if (!installHook<BlockWithin>("VanillaBlockActor::isWithinRenderDistance")) {
        FetchBlockEntities::unhook(); FetchEntities::unhook(); QueueEntities::unhook(); FarChunks::unhook(); return false;
    }
    if (!installHook<DrawBlockEntities>("LevelRendererCamera::renderBlockEntities")) {
        BlockWithin::unhook(); FetchBlockEntities::unhook(); FetchEntities::unhook(); QueueEntities::unhook(); FarChunks::unhook(); return false;
    }
    if (!replication::enable([]() -> std::shared_ptr<replication::Range const> {
        // Pin requested directly on the IN-PROCESS SERVER tick; client effective
        // publication remains owned exclusively by the client tick.
        if (!enabled || !session) return nullptr;
        auto snapshot = config.desired();
        auto* range = &snapshot->localReplication;
        return std::shared_ptr<replication::Range const>(std::move(snapshot), range);
    })) {
        DrawBlockEntities::unhook(); BlockWithin::unhook(); FetchBlockEntities::unhook(); FetchEntities::unhook(); QueueEntities::unhook(); FarChunks::unhook();
        return false;
    }
    installed = true;
    ++generation;
    enabled = true;
    auto& bus = ll::event::EventBus::getInstance();
    commandListener = bus.emplaceListener<ll::event::ClientCommandRegisterEvent>([](auto&) { commands(); });
    tickListener = bus.emplaceListener<ll::event::ClientLevelTickEvent>([](auto& event) { tick(event.level()); });
    // Restore while cameras still exist; if the exit event is post-teardown there
    // are no surviving native values to restore. All scopes self-clear on return.
    exitListener = bus.emplaceListener<ll::event::ClientExitLevelEvent>([](auto&) {
        refreshCameras(true);
        clearSession();
    });
    joinListener = bus.emplaceListener<ll::event::ClientStartJoinLevelEvent>([](auto&) {
        refreshCameras(true);
        clearSession();
    });
    stopListener = bus.emplaceListener<ll::event::ServerStoppingEvent>([](auto&) { replication::invalidate(); });
    if (ll::service::getCommandRegistry(true)) commands();
    return true;
}
bool disable() {
    enabled = false;
    ++generation;
    replication::invalidate();
    if (!installed) return true;
    bool onOwner;
    { std::lock_guard lock(ownerMutex); onOwner = ownerThread == std::this_thread::get_id(); }
    if (session && !onOwner) {
        // No queued callback/captured game pointer can survive DLL unload. The
        // existing owner tick or exit listener acknowledges native restoration.
        std::unique_lock lock(ownerMutex);
        if (!restored.wait_for(lock,std::chrono::seconds(5),[] { return !session.load(); })) {
            UrdeMod::getInstance().getSelf().getLogger().error("Disable deferred: client owner thread did not acknowledge native terrain restoration; retry, do not force unload");
            return false;
        }
    } else if (session) { refreshCameras(); clearSession(); }
    auto& bus = ll::event::EventBus::getInstance();
    replication::disable();
    for (auto const& listener : {commandListener,tickListener,exitListener,joinListener,stopListener})
        if (listener) bus.removeListener(listener);
    commandListener = tickListener = exitListener = joinListener = stopListener = nullptr;
    DrawBlockEntities::unhook(); BlockWithin::unhook(); FetchBlockEntities::unhook(); FetchEntities::unhook(); QueueEntities::unhook(); FarChunks::unhook();
    installed = false;
    config.clearEffective();
    return true;
}
}
