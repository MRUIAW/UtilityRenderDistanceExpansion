# UtilityRenderDistanceExpansion 0.1.0

Windows x64 mod for Bedrock **1.26.51.01 / LeviLamina 26.51.9**. Independent
server actor-interest expansion and client entity/block-entity collection, plus
one **experimental** shared terrain-detail distance. No fog, fancy-leaves,
view-distance, tick-distance, chunk-publishing or chunk-loading changes.

## Commands and configuration

| Side / command root | Key | Default | Allowed |
|---|---|---|---|
| Server `/urde` (operator/console) | `actorInterestChunks` | `8` chunks | whole `1..32` or `vanilla` |
| Client-local `/urdec` | `entityRange` | `128` blocks | positive, finite `<=512` or `vanilla` |
| Client-local `/urdec` | `blockEntityRange` | `128` blocks | positive, finite `<=512` or `vanilla` |
| Client-local `/urdec` | `terrainDetail` | `128` blocks | positive, finite `<=512` or `vanilla` |

Each root supports `status`, `set <key> <value|vanilla>`, and `reload`:

```text
/urde set actorInterestChunks 8
/urde set actorInterestChunks vanilla
/urdec set entityRange 128
/urdec set blockEntityRange 128
/urdec set terrainDetail vanilla
/urdec status
/urdec reload
```

The builds are separate: the client variant does not install the server controls
for an integrated server. A remote server does not cap the client's configuration.
However, a client cannot render entities/chunks it has not received.

`server.json` and `client.json` live in LL's mod **config directory**, resolved
through `NativeMod::getConfigDir()`. The relevant file is created on first load.
JSON contains exactly the keys above; use the string `"vanilla"`, not zero/null.
For example, the default client file is:

```json
{"entityRange":128,"blockEntityRange":128,"terrainDetail":128}
```

`set` persists the whole validated candidate using temporary-file replacement,
then atomically publishes an immutable requested snapshot. `reload` validates the
whole disk file; malformed/unknown/missing keys, nonfinite/out-of-bounds values,
or persistence failure retain the old snapshot. Changes are limited to one per
second. Status distinguishes requested from the last owner-tick effective
snapshot; these are **policy values, not a guarantee of final visibility**.
Menu changes are saved without dereferencing a world and apply on the next world
tick. Server application occurs in a scoped Level tick; client application and
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
- Entities: ordinary collector **cube half-width**, not a sphere. Native span
  storage, frustum/culler/renderer/resource-pack gates remain intact. Supplemental
  player selection and shadows are not extended or guaranteed.
- Block entities: collector cube plus strict **3D point-to-AABB distance < range**
  for collected components using the native base predicate. Special derived
  predicates remain in force (including derived code calling the changed base).
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

The template's [LICENSE](LICENSE) (CC0-1.0) is **unchanged**. The project owner
still needs to confirm the intended project license; no new license was selected.
