# Changelog

## [0.1.4]

- Standardize command keys, status fields and JSON on snake_case, matching LL's in-game enum syntax. Legacy camelCase JSON keys load as fallbacks; reject both spellings of any one setting. The next successful save writes only snake_case.
- Floor client-host replication at native: use the derived `ceil(entity_range/16)` disk only for radius >=5, a superset of the native57 offsets (`dx²+dz² <=17`); otherwise leave native offsets unchanged. Status explains the below-vanilla fallback. Dedicated admin radii1–4 remain allowed; no cube-corner margin is added.
- Preserve accepted block-entity drawing, diagnostics removal and immutable owner-thread application. Movement update-rate investigation is research only, not a code change.

## [0.1.3]

- Fix numeric `set` values on both sides: LL 26.51.9 maps the old reflected strings to identifier (`Id`) grammar, which rejects bare numeric tokens. Register native client float/server integer overloads plus a separate hard-enum `vanilla` overload.
- Use side-specific hard enums for key hints/autocomplete; keep LL's enum-expanded literal `status`, `set`, `reload` hints. Preserve validation, persistence, immutable snapshots, owner-thread application and server GameDirectors permission.
- Remove all temporary client block-entity counters/logging/caller sampling and diagnostic-only camera scans. Preserve the accepted 0.1.2 draw scope, membership sets, guards and replication; retain its historical caller-map document.

## [0.1.2]

- Fix the statically proven second base block-entity range check in the world draw dispatcher: add a named, signature-asserted `LevelRendererCamera::renderBlockEntities` scope for the owned player camera's opaque/alpha final component lists and typed render-context target position.
- Keep queue matching, special derived predicates (Beacon), native shadow/UI/item/unscoped calls and all 0.1.1 local-host replication unchanged. No dispatcher placeholder-by-value hook, address hook or patch.
- Preserve temporary queue diagnostics; add draw takeover/fallback, outside-queue/unscoped/other-camera-match counts and a once-per-session/generation Minecraft-relative caller sample, with the same two-second summary limit.
- Document cross-platform caller/4096 evidence and scan limits. Static gate attribution is proven; final chest visibility and runtime dispatch still require live testing.

## [0.1.1]

- Share the scoped server-Level-tick/actor-replication/sorted-position lever in `src/range/Replication.*`; client local-world/LAN hosting derives its immutable disk from `ceil(entity_range/16)`, with native vanilla behavior and no new setting. Dedicated `/urde` remains independent. (0.1.4 adds the native floor.)
- Show derived local replication radius and current-generation hosting activity in `/urdec status`.
- Statically verify block-entity predicate position and collector arguments; retain matching guards rather than invent an unproven chest fix. Preserve downstream culler, frustum and renderer behavior.
- Add temporary rate-limited LL block-entity diagnostics (fetch matching/reasons/list size, predicate takeover/fallback reasons/position sample, maximum accepted distance), and replace linear membership scans with a per-queue hash set.
- Preserve LGPL-3.0 licensing; no deployment or game launch performed.

## [0.1.0]

- Add replication-scoped server actor-interest disk (default 8 chunks / 197 offsets), preserving explicit native vanilla behavior.
- Add client-local entity collection and two-gate block-entity range (default 128 blocks), ignoring collection fog limits without changing fog.
- Add experimental shared terrain-detail distance (default 128); leave fancy leaves native and do not guarantee VV/RTX glass.
- Add independent persistent server/client configs, validated immutable snapshots, live commands and owner-thread camera refresh/restoration.
- Require named typed hooks with signature assertions; no address hooks, patches, shared-offset mutation or placeholder-container reconstruction.

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).
