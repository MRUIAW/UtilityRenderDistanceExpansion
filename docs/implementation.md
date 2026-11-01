# 0.1.0 implementation and acceptance

## Structure

- `src/mod/UrdeMod.*`: LL lifecycle and config directory.
- `src/range/`: JSON persistence, immutable requested/effective stores, validation,
  commands, stack-restoring thread-local scopes, compile-time disk/count checks.
- `src-server/server/ServerRange.cpp`: server-only tick/replication scopes and
  `/urde`; no client server cap or shared-vector mutation.
- `src-client/client/ClientRange.cpp`: client-only queue, two collectors, base
  block predicate, terrain update, client ticks/session listeners and `/urdec`.

## Named hooks and assertions

Each type below is checked with `static_assert(std::is_same_v<decltype(...), Alias>)`
adjacent to the hook. Virtual declarations and their `$` native thunks are both
checked; overloaded actor fetch uses an explicit `static_cast<FetchActors>`.
Hooks use LL's typed instance-hook macro and named native methods, never the
addresses from the investigation. There are no placeholder-by-value parameters.

| Alias | Exact signature | Native hook symbol (C++ spelling) |
|---|---|---|
| `Tick` | `void (Level::*)()` | `Level::$tick` |
| `Replicate` | `void (ActorReplication::*)(std::vector<WeakEntityRef> const&, Level const&, ChunkSource&)` | `ActorReplication::processReplicationForPlayers` |
| `Sorted` | `std::vector<ChunkPos> (Level::*)(std::vector<ChunkPos> const&) const` | `Level::$getSortedPositionsFromClientOffsets` |
| `Queue` | `void (LevelRendererCamera::*)(LevelRenderPreRenderUpdateParameters const&)` | `LevelRendererCamera::$queueRenderEntities` |
| `FetchActors` | `gsl::span<gsl::not_null<Actor*>> (BlockSource::*)(Actor const*, AABB const&, bool, bool)` | `BlockSource::$fetchEntities` (ordinary AABB overload) |
| `FetchBlockActors` | `void (BlockSource::*)(AABB const&, std::vector<gsl::not_null<IVanillaRenderBlockActorComponent*>>&, bool)` | `BlockSource::fetchRenderBlockActorComponents` |
| `Within` | `bool (VanillaBlockActor::*)(Vec3 const&) const` | `VanillaBlockActor::$isWithinRenderDistance` |
| `FarDetail` | `void (LevelRendererCamera::*)()` | `LevelRendererCamera::updateFarChunksDistance` |

The non-hook helper is also asserted:
`Expand = std::vector<ChunkPos> (*)(std::vector<ChunkPos> const&, std::vector<ChunkPos> const&)`,
bound to `ChunkTickOffsetManager::getSortedPositionsFromClientOffsets`.

**Receiver correction to the investigation draft:** LL 26.51.9's
`Hook::thisFor<T>()` takes the **native receiver base**, not the desired complete
type. Level/ILevel and BlockSource/IBlockSource have the first base at the same
address; their hooks use `thisFor<Level>()` / `thisFor<BlockSource>()`.
The block predicate receives the secondary `IVanillaRenderBlockActorComponent`
subobject; it therefore uses `thisFor<IVanillaRenderBlockActorComponent>()`, which
returns the adjusted complete `VanillaBlockActor*`. The draft's literal
`thisFor<VanillaBlockActor>()` would not adjust this receiver and is unsafe. No
hard-coded byte offset is used.

## Live application and safety

Commands never touch cameras/Levels. Candidates are fully validated and persisted
before atomic requested publication; failure retains old snapshots. Bounds:
server integer `1..32`, client finite positive `<=512`, or explicit vanilla. The
server regenerates bounded immutable offsets and checks center+offset overflow.
One-second command throttling limits repeated allocation/churn.

The server's typed Level-tick hook accepts the request only for the active server
Level, then pins it through a synchronous processor scope with matching Level,
ChunkSource and generation. Sorted calls outside that exact scope remain native.
All context pointers refer to stack locals; nested unrelated calls install null
scopes. There is no persistent Level cache or async native-pointer capture.

Client ticks accept the snapshot and reacquire all three shared-owned native
renderer cameras (player, directional shadow, cloud shadow) for an explicit native
distance refresh. The terrain hook always calls native first and writes only
`mFarChunksDistance` afterward while enabled/in session. Vanilla/disable therefore
recomputes native F, preserving native fog arithmetic. Terrain queue/culler timing
and split-screen require live testing; this is not a matching-shadow guarantee.

Queue scopes pin camera, source, session generation, position and config. Actor
fetch requires the exact native ordinary cube, source, null exception actor and
both flags false, only once per queue. Block-component fetch additionally matches
the native temporary output vector and true flag. The base predicate only changes
for components in that queue's collected vector and matching camera position.
Native output/storage and every other gate are preserved.

Session start/exit and dimension identity changes invalidate the client generation
and clear effective state. Scope destruction restores previous contexts on normal
return and exceptions; hooks remain native without a matching active session.
No queued stale work exists: owner ticks consume the latest data rather than
callbacks captured against an old world. Disable turns overrides off, requests
owner-tick native refresh, and waits up to five seconds if called off-owner. An
unresponsive owner makes disable fail with hooks/listeners retained for a safe
retry instead of unloading code or retaining modified live native values silently.

## Build evidence versus runtime limits

Both target-specific headers and required imports were first compiled/linked in a
temporary standalone symbol probe; the mod builds also compile all assertions.
LL prelink supplies named Bedrock runtime imports. This is **not** runtime hook
resolution/dispatch, lifecycle, visuals or performance certification. Hook install
failures abort enable without address substitution. Before installation, enable
also resolves each exact MCAPI/import symbol by name (including the expansion
helper), logging the missing name and returning false if unavailable. These
declaration-generated `$` names are LL's virtual native-symbol aliases, not
addresses. No game was launched.

Local validation: server and client release builds both succeeded on Windows x64
with LL 26.51.9; the final xmake configuration is client. `urde-checks` passed
range/JSON validation (including underflow rejection), persistence, immutable
snapshot pinning, failed-reload retention, disk counts and nested/exception scope
restoration. The only emitted compiler warning was LL's manual-hook macro's
unused `_AutoHookCount`. Client package is in `bin/UtilityRenderDistanceExpansion/`;
the server package was preserved in `build/artifacts/server/UtilityRenderDistanceExpansion/`.
Neither package was deployed. These build outputs are ignored by git.

## Live acceptance checklist — hi3 + BDS-hi3 (not run)

- [ ] Load at menu, enter/exit/rejoin worlds, change dimension, disconnect/reconnect;
  exercise multiple cameras/split-screen. No stale query/member access or crash.
- [ ] Check `status`, valid/invalid values, `vanilla`, one-second throttling, disk
  reload, malformed JSON/persistence failure, restart persistence and independent
  server/client policy. Rejected changes leave both old snapshots intact.
- [ ] Keep test chunks resident independently. Toggle server vanilla→8→vanilla
  while stationary: ordinary mob/player axis/diagonal chunk boundaries; count
  native add/remove transitions and check autonomous retention separately.
- [ ] For >30 seconds outside vanilla interest, verify movement, metadata,
  equipment, damage/events and removal; no frozen entities/ghosts after shrink.
- [ ] Verify native spawning/loading progress, tick distance and chunk publishing
  unchanged. Profile ticks/scans, viewer-actor state and bytes/player/s with
  separated and overlapping viewers; do not assume a 3.456x cost multiplier.
- [ ] With already-received actors, toggle client collection 72→128→72 while
  stationary. Test axis, diagonal and vertical 64/72/96/128 positions and AABB
  overlap at a query face. Check short fog without changing visual fog, frustum,
  occluders, models and shadows separately, normal and extended-player cameras.
- [ ] Test chest/sign and special/derived block entities through both collection
  and strict 3D point-to-box gates; no rejoin/mesh rebuild needed for policy changes.
- [ ] Resident corridor: grass/flowers, ordinary/seasonal leaves, clear/stained/
  tinted glass/panes, water control. Toggle F 64→128→64 without movement; compare
  immediately versus after camera motion to expose cache/order effects.
- [ ] Repeat raster, VV and RTX independently; inspect *through* glass. Compare
  F-only changes with a native fancy-leaves toggle/rebuild. Record frame time,
  draw calls, geometry/overdraw, VRAM and rebuild spikes; glass remains experimental.
- [ ] Set all controls vanilla and disable/unload in-world and from menu; verify
  native F restoration and no stale commands/scopes on re-enable. If owner-thread
  acknowledgement times out, allow ticks and retry rather than force unloading.
