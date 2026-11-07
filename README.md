# UtilityRenderDistanceExpansion 1.0.0

A [LeviLamina](https://github.com/LiteLDev/LeviLamina) mod for Minecraft Bedrock that lets players see and shoot
actors, see block entities and keep terrain detail **farther than vanilla**, and makes
distant movement smooth. It has a **server build** (dedicated server) and a
**client build** (also covers single-player and LAN hosting).

- Game: Bedrock **1.26.60** (client 1.26.60.05, BDS 1.26.60.2), Windows x64.
- Requires **LeviLamina `>=26.60.2 <26.61.0`**.
- License: LGPL-3.0.

## What it does

| Feature | Side | Default |
|---|---|---|
| Send actors to players from farther away (replication range) | server, and the client when it hosts a world | 32 chunks (512 blocks) |
| Draw actors farther away | client | 512 blocks |
| Draw block entities (chests, signs, ...) farther away | client | 512 blocks |
| Keep vegetation, leaves and glass at high detail farther away (**experimental**) | client | 512 blocks |
| Update distant moving actors more often, with matching interpolation | server, and the client when it hosts a world | 4 ticks (server), 1 tick (hosting) |

It does not change fog, fancy leaves, view distance, tick distance, chunk loading or
shadows. Actors only move inside the server's simulation range (`tick-distance`).

## Install

Use `lip` with `tooth.json`, or copy the folder by hand.

| Build | Copy `UtilityRenderDistanceExpansion/` to |
|---|---|
| Dedicated server | `<server>/mods/UtilityRenderDistanceExpansion/` |
| Client | `<game instance>/mods/UtilityRenderDistanceExpansion/` |

Run the server build on the server and the client build on every player who wants
the client features. For single-player or LAN hosting you only need the **client**
build: its in-process server uses the same settings as the client.

Each build creates its own config file in the mod's `config/` folder on first start:
`server.json` or `client.json`.

## Commands

Server: `/urde` (needs operator / GameDirectors permission). Client: `/urdec`
(local to your game, no permission needed).

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

`set` takes effect on the next tick and is saved to the config file. `vanilla`
restores the native behaviour for that setting. `reload` re-reads the config file; if
the file is invalid the old settings stay. Changes are limited to one per second.
The in-game grey syntax hint does not list `reload`, but Tab completes it and it works.

### Settings

| Command | Setting | Default | Allowed | What it controls |
|---|---|---|---|---|
| `/urde` | `actor_interest_chunks` | 32 | whole 1..32, or `vanilla` | How far (in chunks) the server sends actors to each player. Vanilla is a fixed 57-chunk pattern, about 70 blocks. |
| `/urde` | `update_interval` | 4 | whole 1..10, or `vanilla` | Minimum ticks between movement updates for distant actors (see below). |
| `/urdec` | `entity_range` | 512 | 1..512, or `vanilla` | How far actors are drawn. |
| `/urdec` | `block_entity_range` | 512 | 1..512, or `vanilla` | How far block entities are drawn. |
| `/urdec` | `terrain_detail` | 512 | 1..512, or `vanilla` | Distance up to which vegetation, leaves and raster glass keep high detail. **Experimental.** |
| `/urdec` | `entity_shape` | circle | `circle`, `square` | `circle` draws actors within a horizontal circle of `entity_range`; `square` uses a cube. |
| `/urdec` | `replication_margin` | 16 | 0..512 | Extra blocks of replication beyond `entity_range` when you host a world. |
| `/urdec` | `update_interval` | 1 | whole 1..10, or `vanilla` | Same as the server setting, for the world you host. |

Both sides also read two lists from their config file (edit the file, then `reload`):

- `full_rate_types`: entity type ids that always use the fastest update rate. Default `["minecraft:player"]` (only used when the key is missing).
- `full_rate_tags`: actors with one of these `/tag` names always use the fastest update rate. Default `[]`.

Example `server.json`:

```json
{"actor_interest_chunks":32,"update_interval":4,"full_rate_types":["minecraft:player","minecraft:villager"],"full_rate_tags":["fullrate"]}
```

Example `client.json`:

```json
{"entity_range":512,"block_entity_range":512,"terrain_detail":512,"entity_shape":"circle","replication_margin":16,"update_interval":1,"full_rate_types":["minecraft:player"],"full_rate_tags":[]}
```

## How the ranges work together

**Actors** need two things: the server must send them (`actor_interest_chunks`), and
the client must draw them (`entity_range`). The result is the smaller of the two.
When you play on a dedicated server, set `entity_range` a little below the server's
range so actors near the edge do not freeze before they disappear. When you host a
world yourself, the client derives the server range from `entity_range +
replication_margin` automatically, and never uses less than vanilla.

**Block entities** (`block_entity_range`) and **terrain detail** (`terrain_detail`)
are client-only; the server does not need to do anything, but the chunks must be
loaded (server `view-distance`).

**Smooth distant movement.** Vanilla sends far-away actors about every 7 ticks and
tells the client to glide over that time. `update_interval` makes the server send
sooner (never later), and the packet tells the client the real time since the
previous update, so the motion stays smooth. Lower values mean smoother and more
accurate positions but more network traffic. Estimate for distant moving actors:
about +70% packets at 4, about 6x at 1, compared with vanilla. Values at or above the
native interval (about 7) change nothing. Players use the fastest rate if
`minecraft:player` is in `full_rate_types`.

## Known limits

- Terrain detail is a single experimental threshold for vegetation, leaves and glass. It does not guarantee glass transparency under Vibrant Visuals or ray tracing, and it does not change fancy leaves.
- Shadows (Vibrant Visuals) are not extended.
- Actors beyond the server's `tick-distance` stand still.
- A remote server's movement and range settings cannot be changed from the client; use the server build there.
- The movement lever has been tested with animals. Other players on this path have not been tested yet.
- Longer ranges mean more actors to process and send. Network and CPU costs were estimated, not measured.

## Build

```powershell
xmake f -a x64 -m release -p windows --target_type=server -y
xmake build
xmake f -a x64 -m release -p windows --target_type=client -y
xmake build
```

Output goes to `bin/server/UtilityRenderDistanceExpansion/` and
`bin/client/UtilityRenderDistanceExpansion/`. `xmake build urde-checks` followed by
`xmake run urde-checks` runs the game-independent checks.

Developer notes (hook signatures, reverse-engineering evidence, live acceptance
notes) are in [docs/](docs/).

## License

Licensed under the [GNU Lesser General Public License v3.0](LICENSE) (LGPL-3.0).
The LGPL supplements the [GNU General Public License v3.0](COPYING).
