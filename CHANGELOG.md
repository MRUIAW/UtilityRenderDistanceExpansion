# Changelog

## [0.1.0]

- Add replication-scoped server actor-interest disk (default 8 chunks / 197 offsets), preserving explicit native vanilla behavior.
- Add client-local entity collection and two-gate block-entity range (default 128 blocks), ignoring collection fog limits without changing fog.
- Add experimental shared terrain-detail distance (default 128); leave fancy leaves native and do not guarantee VV/RTX glass.
- Add independent persistent server/client configs, validated immutable snapshots, live commands and owner-thread camera refresh/restoration.
- Require named typed hooks with signature assertions; no address hooks, patches, shared-offset mutation or placeholder-container reconstruction.

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).
