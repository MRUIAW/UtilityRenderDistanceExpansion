# UtilityRenderDistanceExpansion 0.1.6

Windows x64 mod for Bedrock **1.26.51.01 / LeviLamina 26.51.9**. Independent
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

Exact overload syntax (angle brackets describe required parameters, not text to type):

```text
/urdec status
/urdec reload
/urdec set <entity_range|block_entity_range|terrain_detail> <value: float>
/urdec set <entity_range|block_entity_range|terrain_detail> <vanilla>
/urdec set replication_margin <value: float>
/urdec set entity_shape <circle|square>
/urde status
/urde reload
/urde set <actor_interest_chunks> <value: int>
/urde set <actor_interest_chunks> <vanilla>
```

The native grey syntax hints/autocomplete expose `status|set|reload` at the root,
then **all** side-specific key choices before you type the key. After the key,
the range overloads offer the numeric `value: float` (client) / `value: int` (server)
parameter and the hinted/completable **`vanilla`** choice. `entity_shape` offers
hinted `circle` and `square` values; `replication_margin` takes a number only. These are hard enums,
not arbitrary strings or suggestions added only after a prefix. Bedrock may show
the alternatives as separate syntax rows rather than a combined `number|vanilla`
label; exact spacing/localized type labels depend on the chat UI. Server `/urde`
still requires **GameDirectors** permission. Examples:

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
JSON requires the original three range keys, with `entity_shape` and
`replication_margin` optional for old files; missing new keys default to `circle`
and `16`. Use the string `"vanilla"`, not zero/null, for the range keys. The margin
is a number and can be zero.
Legacy camelCase keys still load as fallbacks (mixed old/new keys for different
settings are allowed); **both spellings of the same key are rejected** with an
explicit ambiguous-config error. Loading does not rewrite the file. The next
successful `set`/`reload` save writes only snake_case keys.
For example, the default client file is:

```json
{"entity_range":512,"block_entity_range":512,"terrain_detail":512,"entity_shape":"circle","replication_margin":16}
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

Target/DLL: `UtilityRenderDistanceExpansion`. Packaging output:
`bin/UtilityRenderDistanceExpansion/`. See [implementation notes](docs/implementation.md)
for hook signatures and the live acceptance checklist. Builds are not runtime
certification; no game launch is needed to build.

## License

Licensed under the [GNU Lesser General Public License v3.0](LICENSE) (LGPL-3.0).
The LGPL supplements the [GNU General Public License v3.0](COPYING).
