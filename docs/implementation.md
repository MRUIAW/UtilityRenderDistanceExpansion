# 0.1.4 implementation and acceptance

## 0.1.4 naming and hosting floor

Live 0.1.3 server help confirmed LL snake_cases enum values. Command keys,
callbacks, status and persisted JSON now consistently use snake_case; enum
identifiers themselves are snake_case too. `migrateKeys` normalizes each legacy
JSON alias before strict schema validation; both spellings of one setting are
rejected with an explicit error. Read is nonmutating on disk; the next successful
save through the existing config store encodes only canonical names. Unknown,
missing, invalid and failed-reload behavior is unchanged.

Client-only `localReplicationChunks` returns0 (native passthrough) unless
`ceil(entity_range/16) >=5`. The native57 offsets obey squared radius<=17;
radius4 misses (4,1), while radius5 contains every native offset. Thus values
<=64 blocks no longer reduce hosting updates below native; values just above64
use radius5,128 uses8. No cube-corner margin is added. Dedicated server policy
still accepts1–32/vanilla and does not use this floor. Status reports
`local_replication=vanilla (entity_range below vanilla)` for small nonzero ranges.
Pure checks cover the boundary, native57 set containment, alias migration and
duplicate rejection. Draw scopes, typed hooks and movement update logic are unchanged.
Both 0.1.4 release targets and pure checks passed; final build configuration is
client. No deployment/game launch was performed. Movement research follows the
implementation commit and is recorded outside the repository in `range-findings.md`.

The [0.1.2 caller map and proof limits](block-entity-0.1.2.md) supersede the
historical 0.1.1 chest-cause conclusion below. Local hosting is unchanged.

The user accepted 0.1.2: chests render to 128. 0.1.3 removes the temporary
diagnostics without changing its functional render scopes or replication.

## 0.1.3 command grammar and hints

The old shared `SetParams { std::string key; std::string value; }` correctly
registered both required fields after literal `set`; the error is **the value's
grammar**, not a missing field/overload. Installed LL26.51.9 `Overload.h`:
`required()` selects the reflected member by name, and `addParam()` uses its
`ParamTraits` parser/type/parse-rule. `ParamTraits.h` lines77–79 map `std::string`
to `HardNonTerminal::Id`; float maps to `Val` and int to `Int` (lines68–75).
Thus the registry rejects bare `96` as a numeric token where it expects an
identifier, before our callback/validation. This explains the reported error and
affects both callers of the shared registration. No IDA is necessary.

`registerControls<UrdeClientKey,float>` / `<UrdeServerKey,int>` now register
numeric and vanilla overloads. External-linkage parameter structs contain a hard
key enum plus float/int, or the same key enum plus one-value `UrdeVanilla` enum.
LL `ParamTraits<enum>` sets `Enum` and `EnumAutocompleteExpansion`, registering
enumerator spellings through `CommandRegistrar::tryRegisterEnum` / magic_enum.
Grammar, enum counts/spellings, two-member reflection and side/type pairings are
compile-time asserted. See README for the exact per-side syntax and hints.

Literal `status`, `set`, `reload` remain natural LL text overloads: LL26.51.9's
[`OverloadData::addTextImpl`](https://github.com/LiteLDev/LeviLamina/blob/v26.51.9/src/ll/api/command/OverloadData.cpp)
already registers them as enum parameters with `EnumAutocompleteExpansion`.
No generic action enum is needed (which could allow `status <key> <value>`).
All fixed choices are therefore present in command metadata before prefix input;
the grey rows' exact layout/localization still requires in-game verification.

Numeric callbacks use locale-independent round-trip `to_chars` text and call the
unchanged `config.change`/`parseRange` pipeline. This avoids `to_string` rounding
small valid floats to0; there is no six-decimal precision loss or bypass of
finite/positive/max/integer checks. Only enum vanilla submits the sentinel text.
One-second throttling, disk persistence before immutable publication, failed
update retention, effective owner-thread acceptance and GameDirectors are unchanged.

0.1.3 validation: server and client release builds passed with LL 26.51.9; both
package manifests report 0.1.3 with the correct side, and final configuration is
client. `urde-checks` passed, including numeric-command round trips, small positive
float preservation and zero/negative/nonfinite/over-limit rejection. Compiler
warnings were only the existing LL manual-hook `_AutoHookCount` warning. No game
was launched/deployed, no running game process was touched, and no IDA was opened.
In-game command execution and exact grey-row layout remain to be live-tested;
the accepted 0.1.2 rendering/local replication behavior is intentionally retained.

## Structure

- `src/mod/UrdeMod.*`: LL lifecycle and config directory.
- `src/range/`: JSON persistence, immutable requested/effective stores, validation,
  commands, stack-restoring thread-local scopes, compile-time disk/count checks.
- `src/range/Replication.*`: common server Level-tick/replication/sorted scopes,
  immutable disk policy and named-symbol checks, compiled into both builds.
- `src-server/server/ServerRange.cpp`: dedicated config and `/urde` adapter.
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
| `DrawBlocks` | `void (LevelRendererCamera::*)(BaseActorRenderContext&, bool)` | `LevelRendererCamera::$renderBlockEntities` |
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

### 0.1.1 local hosting (with the 0.1.4 floor above)

The client configuration embeds a prebuilt, game-pointer-free replication policy.
The shared consumer pins an aliasing shared pointer to the **requested** immutable
client snapshot on the **server Level tick**, not the client tick. This does not
publish the client's effective render snapshot from a second owner. Policy offsets
are prebuilt during config validation; no native offsets/members are written from
the render/command thread. The dedicated
adapter retains its existing `config.accept()` behavior and command schema.

LL 26.51.9 `src-client/ll/api/service/TargetedBedrock.cpp` confirms that `getLevel()`
uses `getMinecraft(false)` from the tracked `ServerInstance`, while
`getMultiPlayerLevel()` uses client data. Requiring that exact live non-client
Level therefore excludes remote joins; client session gating additionally excludes
menus/transitions. Generation invalidation covers join/exit, client dimension
identity changes, server stopping and disable. Native pointers remain stack-only.
Status activity means a matching server owner tick in the current lifecycle
generation, not proof that actors were sent; paused hosts retain it until lifecycle
invalidation. Vanilla still reports hosting
activity but leaves sorted positions native. Runtime dispatch must be live-tested.

### 0.1.1 block-entity static investigation

1. **Position suspect rejected for the traced path.** Education insertion
   `0x104148e60` passes camera+`0x628` directly to virtual predicate slot `0x30`;
   its ordinary fetch reads the same position. Historical Windows insertion
   `0x1446959a0`, at `0x144695a10`, passes camera+`0x660` directly: header
   `mCameraTargetPos`, not eye/render-origin position or an adjusted copy.
2. **Collector mismatch not established.** Education queue `0x104147020` and
   Windows queue `0x144692670` build min/max-normalized `p +/- min(72,fogEnd)`
   with component float add/subtract, use true (Windows `mov r9b,1`), and pass the
   exact native temporary block-component vector (Windows camera+`0xa10`). For
   finite position/nonnegative finite fog accepted by our queue guard, this
   matches `cube()`; there is no evidence justifying relaxed matching. The native
   collector enumerates resident horizontal chunks; it does not itself add a
   second 64/72 distance predicate.
3. **Downstream remains native.** Both insertion paths next test AABB subchunk
   visibility/culler and translated frustum, then queue the component. The traced
   visibility helper has no independent literal distance cutoff. Education
   `ChestRenderer` vtable render body `0x1037668d0` was inspected: chest state,
   pairing, transforms/material/model work, but no identified second 64/72 camera
   range test. This is not an exhaustive backend/callee proof or a live attribution.
   No necessary symbol-less gate was proven; no new gate hook was added.

**The ~70-block chest cause remains unproven; this release does not claim it is
fixed.** The actionable change is diagnostics plus cheaper membership. The set is
built once from the matched native fetch result and destroyed with the queue;
predicate lookup is average O(1), instead of O(n) per call. Existing secondary-base
adjustment, strict point-to-virtual-AABB test and special derived behavior remain.
All IDA opens used only `ida-edu4/minecraft-edu.i64`, sequentially, saved annotations
and called `idapro.close_database(True)` in `finally`. Evidence batches are
`urde-011-a/b/c`, `urde-011-chest-renderer.txt` and Windows historical dumps.

The installed client `bedrock_runtime_data` v26.51.1-client.12 was checked offline
with prelink v0.8.7 against the compiled shared replication object: all four exact
tick/processor/sorted/helper imports were generated. A negative-control object
referencing a deliberately nonexistent name produced no such import, confirming
this is symbol-data availability, not unconditional import fabrication. Both mod
builds linked successfully. Runtime enable still checks all four exact symbols
and fails closed if the actual game's symbol table differs. No game was launched
or attached for this check.

### Historical temporary live log marker (removed in 0.1.3)

The following describes 0.1.1/0.1.2 only; **none of these logs/counters or caller
samples are emitted in 0.1.3**. Their aggregate lock, includes used only for
sampling, per-scope diagnostic state and `inOtherCamera` were removed. Functional
draw/queue scopes, sets and short-circuit predicate guards are unchanged.

Historical prefix in LL `logs/latest.log`:
`[URDE temporary block-entity diagnostics]`.
One summary across all cameras/threads is emitted at most once per two seconds,
only on active queue completion. Counters reset after each summary and on session
generation change; game pointers never enter aggregate state. Fields:

- `fetch matched`, `notMatched`, `reasons(box,flag,resultList,source,duplicate)`;
  reasons may overlap; `absent` counts queue scopes with no eligible fetch call.
- `listSize(last,max)` after native fetch (including native unmatched fetches).
- `predicate takeover`, `fallback`, reasons `position,notInList,noFetch,vanilla,invalidScope`.
- `maxAcceptedDistance` is the maximum strict point-to-AABB distance accepted by
  our predicate in this interval, **not proof of final rendering**.
- `positionMismatchSample=passed=(x,y,z) target=(x,y,z)` is one sample per summary,
   or `none`. In 0.1.2 outside-queue calls are counted; only the proven owned-player
   world draw scope can take over the base predicate there. See the new caller map.

Test local hosting at 128 (derived 8 / 197), vanilla, dimension/world changes and
remote join (inactive) separately. Compare chest tests with `matched>0`,
`takeover>0`, `notInList=0`, and accepted distances beyond 72. If those hold but
chests vanish, investigate native downstream gates without assuming this is a
predicate failure. If `absent>0` or a fallback reason dominates, inspect that path
first. The logger is temporary and intentionally independent of client console.

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
   for components in that queue's collected vector and matching camera position;
   0.1.2 additionally permits the equivalent guarded world draw scope described above.
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
