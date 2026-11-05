# UtilityRenderDistanceExpansion 0.3.1

Windows x64 mod for Bedrock **1.26.60.05 (client) / BDS 1.26.60.2**, requiring
**LeviLamina >=26.60.2 <26.61.0**. Independent
server actor-interest expansion and client entity/block-entity collection, plus
one **experimental** shared terrain-detail distance. No fog, fancy-leaves,
view-distance, tick-distance, chunk-publishing or chunk-loading changes.

## Commands and configuration

| Side / command root | Key | Default | Allowed |
|---|---|---|---|
| Server `/urde` (operator/console) | `actor_interest_chunks` | `32` chunks | whole `1..32` or `vanilla` |
| Client-local `/urdec` | `entity_range` | `512` blocks | positive, finite `<=512` or `vanilla` |
| Client-local `/urdec` | `block_entity_range` | `512` blocks | positive, finite `<=512` or `vanilla` |
| Client-local `/urdec` | `terrain_detail` | `512` blocks | positive, finite `<=512` or `vanilla` |
| Client-local `/urdec` | `entity_shape` | `circle` | `circle` or `square` |
| Client-local `/urdec` | `replication_margin` | `16` blocks | finite `0..512`, subject to the local 34-chunk disk limit |
| Server `/urde` | `update_interval` | `4` ticks | whole `1..10` or `vanilla` |
| Client hosting `/urdec` | `update_interval` | `1` tick | whole `1..10` or `vanilla` |
| Both (file only, then reload) | `full_rate_types` | `["minecraft:player"]` | list of exact type IDs, e.g. `minecraft:villager` |
| Both (file only, then reload) | `full_rate_tags` | `[]` | list of exact vanilla `/tag` names |

Commands (angle brackets are parameters):

```text
/urde status
/urde reload
/urde set <setting: setting> [value: int]
/urde set <setting: setting> vanilla
/urdec status
/urdec reload
/urdec set <setting: setting> [value: float]
/urdec set <setting: setting> vanilla
/urdec set entity_shape <circle|square>
```

Actions are literal words. The short `setting` enum keeps the key list out of
usage while retaining key completion after `set `. These are **four server rows /
five client rows**. The numeric value is optional in the grammar so a missing
value receives a clear error; a successful `set` still requires it.
`status`/`reload` accept no extra arguments. `replication_margin` accepts numbers
only and rejects `vanilla`; `entity_shape` accepts circle/square in its own row.
Bare `set` and invalid action/shape arguments retain explanatory error messages
after native syntax rejection.
Client `update_interval` uses the float parser but requires a whole value (4.0 is
accepted, 4.5 rejected). The missing `reload` grey hint reported with 0.3.0 remains
under investigation; the exact client hint layout is not established by a build.
Server `/urde` still requires **GameDirectors** permission. Examples:

```text
/urde set actor_interest_chunks 8
/urde set actor_interest_chunks vanilla
/urdec set entity_range 128
/urdec set entity_shape circle
/urdec set replication_margin 16
/urdec set block_entity_range 128
/urdec set terrain_detail vanilla
/urdec status
/urdec reload
```

For local-world/LAN hosting, the client derives an integer chunk disk covering
the horizontal circle of radius `entity_range + replication_margin` around the
player, even when the player and replicated chunk centers are at chunk edges. For
offset `(x,z)`, the closest chunk-center displacement on each axis is
`max(16*|offset|-8, 0)`. Include offsets when the squared combined displacement
is `<= (entity_range+replication_margin)²`; the derived radius is the smallest
integer disk containing those offsets. At the default `512 + 16` this gives
**34 chunks / 3625 offsets**; at `128 + 16` it gives 10 chunks / 317 offsets. The disk is limited to 34 chunks; combinations
exceeding that are rejected. Old files with `entity_range=512` still load with the
16-block default. For `entity_range=vanilla` the native offsets are retained.

If the derived disk is not a superset of native 57 offsets (`dx²+dz² <=17`),
native offsets are retained instead. The margin is a horizontal circle for **both**
entity shapes; it does not cover square-mode corners. The shared replication hooks
stay native on remote servers: on a dedicated server, use an `entity_range` somewhat
below its actor-interest range for a similar edge margin. Dedicated `/urde` remains
independent and still permits radii 1–32 or `vanilla`. Chunk residency, network
policy and other server limits remain in force.

`/urdec status` reports the derived radius/count, the chosen disk/count (57 when
native), the last-server-tick radius and `hosting_replication_active` (a matching
server tick in the current session generation; remains true for a paused host until
lifecycle invalidation). At the defaults it includes
`entity_shape=circle replication_margin=16 ... local_replication=34 ... derived_chunks=34 derived_offsets=3625 ... local_replication_offsets=3625`.

`server.json` and `client.json` live in LL's mod **config directory**, resolved
through `NativeMod::getConfigDir()`. The relevant file is created on first load.
Client JSON requires the original three range keys, with `entity_shape` and
`replication_margin` optional for old files; missing new keys default to `circle`
and `16`. Use the string `"vanilla"`, not zero/null, for the range keys. The margin
is a number and can be zero.
Legacy camelCase keys still load as fallbacks (mixed old/new keys for different
settings are allowed); **both spellings of the same key are rejected** with an
explicit ambiguous-config error. Loading does not rewrite the file. The next
successful `set`/`reload` save writes only snake_case keys.
For example, the default client file is:

```json
{"entity_range":512,"block_entity_range":512,"terrain_detail":512,"entity_shape":"circle","replication_margin":16,"update_interval":1,"full_rate_types":["minecraft:player"],"full_rate_tags":[]}
```

`set` persists the whole validated candidate using temporary-file replacement,
then atomically publishes an immutable requested snapshot. `reload` validates the
whole disk file; malformed/unknown/missing keys, nonfinite/out-of-bounds values,
or persistence failure retain the old snapshot. Changes are limited to one per
second. Status distinguishes requested from the last owner-tick effective
snapshot; these are **policy values, not a guarantee of final visibility**.
Menu changes are saved without dereferencing a world and apply on the next world
tick. Replication application, including the in-process server, occurs in a scoped
server Level tick; client collection application and
active-camera native refresh occur on the client tick. One snapshot is pinned for
each replication/collection cycle. No command-thread native member writes.

## Shapes and known limits

**0.3.1 retains the 0.3.0 movement gate and accounts for late attempted sends.** The
native helper runs first. For each stable sender/recipient ID pair, record the
server tick of every final engaged result. With effective interval N:

| Native result | Condition | Final result |
|---|---|---|
| nullopt | no record | engaged N |
| nullopt | elapsed >= N | engaged clamp(elapsed, 1, 10) |
| nullopt | elapsed < N | nullopt |
| engaged V | V > N | engaged min(V, max(N, elapsed)) |
| engaged V | V <= N | unchanged V |
| any | vanilla / outside matching server tick and sendUpdate | unchanged |

N defaults to dedicated **4**, hosting **1**; exact listed types/tags select **1**.
Without a record, engaged V>N uses N; V<=N always survives unchanged. Every
final engaged result records the current tick. A late send thus gets a payload
matching its actual gap, bounded by native V when engaged or 10 ticks when
opening nullopt. Ten is a conservative half-second payload limit at 20 TPS,
not a recovered hidden native policy. The opening threshold remains N.
This only speeds eligible updates: N at or above the native policy has no steady
cadence effect, apart from the initial no-record opening. Native faster values
survive. It is not a guaranteed heartbeat: change detection, recipient membership,
teleports, prediction and other native routes remain. Windows interpolation uses
`max(3,mTicks)`, so N=1/2 still uses at least three steps. New targets replace the
active target or one pending target; no growing queue was found statically.

0.2.0 changed only the engaged payload, not the elapsed gate, causing a short lerp
followed by waiting in the user's tests. 0.2.1 suspended that cap and measured a
native far-actor gap around seven ticks. Earlier “send interval” claims for 0.2.0
were incorrect. The user found 0.3.0 substantially smoother, with a slight hitch
remaining outside vanilla; 0.3.1 addresses late gaps but needs live acceptance.

Missing movement keys receive side defaults. Missing `full_rate_types` alone
defaults to `["minecraft:player"]`; explicit empty or nonempty lists replace it.
`full_rate_players` is removed: old files load with that key ignored, log once,
and drop it on the next successful save. Player canonical-ID evidence is in the
[safety notes](docs/movement-0.3.0.md); actual player use of this path remains
unproven until a second player is tested. Example server config:

```json
{"actor_interest_chunks":32,"update_interval":4,"full_rate_types":["minecraft:player","minecraft:villager"],"full_rate_tags":["urde_full_rate"]}
```

Apply list edits with `/urde reload` or `/urdec reload`. For example,
`/tag @e[type=minecraft:pig] add urde_full_rate` assigns that vanilla tag; its
movement rule selects N=1. `vanilla` disables both list overrides too.
`status` includes the interval and both list sizes; client status
also reports `hosting_movement(last_server_tick)` or `native/inactive`. These
client keys live in `client.json`; hosting diagnostics run only in the matching
in-process server tick. A remote-joined client cannot observe/control the remote
server's interval hook; use the server build there.

More frequent movement costs more packets: estimate roughly **+70% at N=4** and
**about 6x at N=1** versus the observed native ~7-tick gap for distant moving
actors. These are estimates, not measured bandwidth multipliers; packet size,
compression, bypasses, TPS and recipient count matter. LAN guests incur network
cost even when the host is local.

Temporary counters produce at most one `[URDE temporary movement diagnostics]`
line per two seconds after matching server ticks. For player/other senders and
3D distance `<64`, `[64,128)`, `>=128`, both `native` and `FINAL` vectors count
`nullopt,0,1,2,3,4,5-6,7,8-9,10,>10`. It also records `sendUpdate_calls` by sender
kind, `lever_opened`, `lever_capped`, `lever_full_rate`, `lever_late`, `pair_overflow`, observed
ticks/window milliseconds, and skipped invalid actor/distance
reads. A sendUpdate call is an attempt and may produce zero or multiple packets;
engaged results are gate outcomes, not delivery counts. See
[current rule and interpretation](docs/movement-0.3.1.md) for details. The bounded
65,536-slot pair table retains IDs/ticks only, expires unseen pairs after about
10 seconds of server ticks, and clears on vanilla/exit/generation/disable.

For the next live test, observe one continuously moving animal with one recipient
at a distance where it is visibly ticking for **30 seconds per setting**, in
resident chunks. The user's animal stopping around 160 blocks was attributed to
server tick-distance; render/interest range does not extend simulation.
Collect server logs for each segment (hosting: client LL log), status output and
visual behavior; compare `vanilla`, 4, 2, 1, then add its exact type to
`full_rate_types` and reload with N=4. Vanilla should have identical native/FINAL
histograms and zero lever changes. Faster settings should show `lever_opened`,
fewer FINAL nullopts, and mostly 4/2/1 payloads with longer late-gap payloads;
`lever_late` counts engaged tracked gaps exceeding the selected N. Listed types should increment
`lever_full_rate`. Check smoothness, lag, native prediction interaction and
`pair_overflow=0`. Aggregates cannot prove delivery or exact pair cadence. Later
repeat with a second player and check the player bins; also test lifecycle changes
and root/key hints, invalid commands and persistence.

- Server: horizontal chunk disk `dx*dx + dz*dz <= r*r`, no vertical cutoff.
  Radius 8 contains **197** offsets. Explicit vanilla uses the native **57**
  unchanged; radius 4 would contain only 49. Only the replication-scoped sorted
  position call is replaced; shared offsets and unrelated spawn/loading/ticking
  consumers are untouched. Chunks must already be resident/ready. Long-range
  movement, metadata, equipment and events need live validation. Autonomous
  retention can prevent removal after shrinking.
- Entities: the ordinary collector uses the existing cube half-width and, in
  `circle` mode, filters actor positions to horizontal distance `<= entity_range`
  from the camera target (a cylinder with the existing vertical cube bound).
  `square` retains the existing cube behavior. The filtered span is backed by a
  queue-scope-owned vector that outlives the native synchronous consumer; `square`
  and `vanilla` retain native span storage. Frustum/culler/renderer/resource-pack
  gates remain intact. Supplemental player selection and shadows are not extended
  or guaranteed.
- Block entities: collector cube plus strict **3D point-to-AABB distance < range**
  for collected components using the native base predicate. Special derived
  predicates remain in force (including derived code calling the changed base).
   0.1.2 also scopes the **second base predicate check at world draw time**, using
   the owned player camera's final opaque/alpha component list and render-context
   target position. This fixes a statically proven native 64-block gate missed by
   the queue-only scope; the user confirmed chests visible to 128 in 0.1.2.
   Shadows, UI/items, unrelated cameras and unscoped calls remain native. Temporary
   queue/draw diagnostics and caller sampling were **removed in 0.1.3**. See the
   [caller map and proof limits](docs/block-entity-0.1.2.md).
- Enabled collectors ignore short fog limits; `vanilla` restores the native
  fog-limited collectors. **Visual fog is unchanged.**
- Terrain detail: a **single experimental shared High/Low terrain LOD threshold**
  for eligible vegetation, leaves and raster glass, measured from 3D subchunk
  centers and truncated to an integer before squaring. It is not a per-block or
  three-category control. **VV/RTX glass transparency is NOT guaranteed.** Culler
  refresh timing, backend materials and pack-dependent geometry need live checks.
- Fancy leaves stays native. Distance changes alone do not rebuild meshes or
  create faces omitted at build time; native fancy/layer/backend changes can
  require rebuilding. No universal renderer/long-range-shadow guarantee.
- More interest scans, network/viewer state, render candidates, alpha overdraw and
  VRAM usage are possible. `197/57` is an input-count ratio, not a measured CPU or
  bandwidth multiplier. Bounds are safety limits, not performance recommendations.

Disconnect/world/dimension changes invalidate generation-scoped state; no queued
work captures game pointers. Disabling refreshes surviving cameras natively on
their owner thread before removing hooks. If that thread cannot acknowledge within
five seconds, disabling/unloading fails closed: allow client ticks and retry;
**do not force-unload**. Runtime lifecycle and multi-camera behavior are untested.

## Build

```powershell
xmake f -a x64 -m release -p windows --target_type=server -y
xmake build
xmake f -a x64 -m release -p windows --target_type=client -y
xmake build
```

Optional pure validation/scope checks: `xmake build urde-checks`, then
`xmake run urde-checks` (does not load the game).

Target/DLL: `UtilityRenderDistanceExpansion`. Packaging output, kept separate per
`target_type`: `bin/server/UtilityRenderDistanceExpansion/` and
`bin/client/UtilityRenderDistanceExpansion/`. See [implementation notes](docs/implementation.md)
for hook signatures and the live acceptance checklist. Builds are not runtime
certification; no game launch is needed to build.

The dedicated-server package belongs in `mods/UtilityRenderDistanceExpansion/`.

## License

Licensed under the [GNU Lesser General Public License v3.0](LICENSE) (LGPL-3.0).
The LGPL supplements the [GNU General Public License v3.0](COPYING).
