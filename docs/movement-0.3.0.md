# 0.3.0 movement gate and commands

## Current Windows caller and safety evidence

Analysis used only the authorized Education `ida-edu4` database plus read-only
disassembly of the current Windows executables on disk. No Windows IDB, game
launch, process attachment or live game interaction was used. Runtime hooks use
LL26.60.2 named, signature-asserted methods, never these addresses.

The addresses in the earlier request, BDS `0x1413e7730` and client `0x141040390`,
belong to the historical 26.51 executables. Current Windows gates were matched
uniquely using the component constants `0xc29078a0`, `0x62a584c8`, `0x02fce669`
and inspected control flow. These function names are inferred from that match.

| Path | BDS 1.26.60.2 | Windows client 1.26.60.05 |
|---|---|---|
| sendUpdate | `0x140bb86b0` | `0x1422c8100` |
| recipient callback | `0x140bce450` | `0x1422d7bf0` |
| only scanned direct helper call | `0x140bce5e9` | `0x1422d7d89` |
| interval helper | `0x140bb9190` | `0x1422c9360` |
| callback reference in sendUpdate | `0x140bb8a93` | `0x1422c84e3` |

Each executable has exactly one scanned direct helper call. Each callback has
one RIP-relative reference from its sendUpdate body. Education independently has
one direct caller of gate `0x105757d60`: call `0x105784c34` in recipient callback
`0x1057849e0`, referenced once from sendUpdate `0x105756940`; xref lists were
untruncated. Scans do not prove the absence of computed/indirect callers, so the
lever also requires a thread-local RAII sendUpdate scope matching both the helper
receiver and enclosing server tick. Out-of-send calls preserve native results and
cannot consume a pair slot.

Both current Windows callbacks check the recipient network-target component and
replication membership **before** invoking the helper. Teleport/absolute routes
bypass this ordinary gate. On engaged optional, the callback appends that
recipient once, retains the interval payload, and writes the native last-send
tick (BDS `0x140bce648`). BDS sendUpdate then emits per-recipient data in
`0x140bb8ae0`–`0x140bb8b32`. The inspected callback invokes the helper once per
eligible recipient; opening it does not create a second append/send. Repeated
nullopt calls in the same tick cannot reopen a recorded slot. Native engaged
results, including 0/1, always retain their faster behavior.

The inspected helper's only disengaged return follows the failed elapsed-tick
comparison (BDS compare `0x140bb9659`, disengagement `0x140bb9787/9789`). Missing
optimization policy/component, ground/force changes and disabled optimization
select interval 0/bypass rather than another indistinguishable nullopt. Recipient
ineligibility returns before the helper. Native no-change/packet-construction
gates outside the helper remain. Thus an opened result reaches the ordinary send
path statically, but is not proof of successful network delivery. Prediction-hint
work can already have occurred on the closed native branch before URDE opens it;
its interaction with extra ordinary movement needs live checking. No duplicate
ordinary append or growing client queue was found; transport loss/order, other
mods, player route coverage and actual visual latency remain untested.

## Client interpolation and Player ID

Current Windows handler `0x1422c8d20` reads packet mTicks and calls interpolation
helper `0x142c74350` with `max(3,mTicks)`. A new update replaces the current
interpolation target unless the active interpolation must complete; that case
stores one replaceable pending target. There is no unbounded queue in the
inspected routine. N=1/2 still has a three-step floor and needs visual testing.
Education's analogous handler uses fixed three steps and cannot establish this
Windows payload behavior.

LL v26.60.2 `src/mc/world/actor/Actor.cpp` implements `Actor::getTypeName()` as
`getActorIdentifier().mCanonicalName->getString()`. Education constructor
`0x109a0fd40` installs Player vtable `0x10f5a4630` (RTTI `6Player`) and copies the
hashed `minecraft:player` identifier at `0x109a119c4`–`0x109a11a5b` into actor
state. Its global `0x10fa02978` is initialized with that canonical text at
`0x108f18be7`. This supports exact `minecraft:player` classification; the pure
checks assert the default and case-sensitive match. It does not prove a live
Player reaches this send path: the solo user's player-sender histogram was zero.
A second-player test is still required; prediction/correction routes may differ.

## Lever and bounded state

Origin runs once before the decision. If enabled, choose general N (dedicated 4,
hosting 1, whole 1..10), or 1 for an exact type/tag match. Vanilla disables all
selection and overrides. With the last final engaged tick L:

| Native | Pair condition | Final |
|---|---|---|
| nullopt | no L, or now-L >= N | engaged N; record now |
| nullopt | now-L < N | nullopt |
| engaged V>N | any | engaged N; record now |
| engaged V<=N | any | unchanged; record now |

No matching server tick/send receiver, removed/cross-Level actor, missing stable
ID or saturated bucket means native passthrough. Generation and live non-client
Level identity are checked. Remote-joined clients have no matching server Level.
N never delays native sends: settings at/above native cadence cannot slow it,
though a pair with no record can open immediately. Native change detection and
bypass routes prevent any universal N-tick heartbeat guarantee.

The table has 16,384 four-way buckets, at most 65,536 records (about 2.5 MiB at
40 bytes/record). Ordered actor/player unique IDs and server ticks are stored,
never game pointers. One pair hash and at most four comparisons per hook; no
nodes, rehash or per-pair allocation. Classification is cached once per sendUpdate
scope; native tag lookup internals are not an allocation guarantee. A full bucket
preserves native without evicting live slots and increments pair_overflow, even
if other buckets are empty. Touch expires records at 200 unseen ticks; matching
tick maintenance prunes every 20 ticks, so untouched entries can remain up to
roughly 11 seconds at 20 TPS. Pauses/low TPS extend wall-clock retention.

The tick pins shared ownership of its table. Exit/dimension/generation/disable
immediately unpublishes it; in-flight scopes retain their old table until unwind
and fail subsequent generation checks. No off-thread table writes or per-call
locks are added. Vanilla clears records once when accepted on the server tick.
The first active tick allocates the fixed table; commands publish immutable
game-pointer-free policy only.

Missing full_rate_types defaults to ["minecraft:player"]. Explicit lists, even
empty, replace it. The legacy full_rate_players key is ignored regardless of
value; one notice per config store explains migration. Loading does not rewrite
disk; the next successful set/reload save drops it. Whole-float client command
values are accepted for update_interval, but JSON stores an integer or vanilla.

Compared with the measured native ~7-tick far-moving-actor gap, estimate roughly
+70% packets at N=4 and about 6x at N=1. These rounded estimates are not measured
bytes/CPU multipliers. N<3 keeps the native three-step interpolation floor.

## Temporary diagnostics

Counters remain relaxed lock-free atomics; pair state is separate and confined to
the server tick. Native and FINAL histograms each split player/other senders and
3D distance <64 / [64,128) / >=128, with bins
`nullopt,0,1,2,3,4,5-6,7,8-9,10,>10`. Invalid actor/distance reads are skipped;
out-of-tick calls are ignored. No formatting or clock reads occur in these two
movement hooks. The matching tick formats/drains at most once per two seconds;
window time starts on the first tick of each generation.

```text
[URDE temporary movement diagnostics] window_ms=W ticks=T bins=nullopt,0,1,2,3,4,5-6,7,8-9,10,>10 native= player={lt64:[V],64to128:[V],ge128:[V]} other={lt64:[V],64to128:[V],ge128:[V]} FINAL= player={lt64:[V],64to128:[V],ge128:[V]} other={lt64:[V],64to128:[V],ge128:[V]} lever_opened=O lever_capped=C lever_full_rate=F pair_overflow=B sendUpdate_calls={player:P,other:Q} skipped={interval:I,sendUpdate:S}
```

Each V is eleven counts. lever_opened means nullopt became engaged; lever_capped
means V>N was lowered; lever_full_rate is the subset of those changes selected
through type/tag. sendUpdate_calls are attempts, not deliveries, and may yield
zero/many recipient packets. Histograms mix actors/recipients; they cannot prove
one pair's exact cadence or transport success. Lifecycle invalidation resets all
counters; native and FINAL should agree with zero lever changes under vanilla.

## Command registration evidence and UI limit

Official LL v26.60.2 sources inspected:
`src/ll/api/command/{CommandHandle,OverloadData,CommandRegistrar}.cpp` plus installed
`ParamTraits.h`, `Optional.h` and overload headers. addTextImpl registers expanded
enum Placeholder data; addText creates/reuses `ll_text_enum_name_<word>`. It does
**not** allocate a fresh text enum per overload. ParamTraits registers reflected
enum identities/values; int is native Int, float Val, string Id (which rejects bare
numeric values). registerOverload forwards parameter vectors to Bedrock.

The verified 0.2.1 metadata problem is two root enum identities containing `set`:
UrdeAction and UrdeSet, plus several key enum identities with repeated spellings.
LL has no inspected branch deleting reload. The precise native UI suppression
rule could not be established: current headers expose IntellisenseHandler and
CommandRegistry autocomplete/syntax methods, but RTTI string inspection did not
resolve their implementation. Duplicate identity is an evidenced registration
cause for competing alternatives; its exact link to missing reload is unproven.

0.3.0 uses the same UrdeAction {status,set,reload} and same side-key enum in every
overload. Expected root hints are those three once each; set-key hints are each
side's keys once each. Fixed vanilla/circle/square values remain hard enums.
Two server overloads / three client overloads register:

```text
<action> [key] [value: int or float]
<action> <key> <vanilla>
<action> <key> <circle|square>    (client only)
```

The numeric row also accepts bare status/reload and catches incomplete set.
Runtime validation rejects status/reload with key/value, nonintegral intervals,
numbers for entity_shape, vanilla for shape/margin, and shapes for other keys.
Sharing keys trades contextual suggestion filtering for consistent identities:
shape values may be offered on another key and then rejected. This is not a
claim that the client shows separate literal status/reload or shape-only rows.
The player must verify grey hints and white usage; all grammar is localized in
Commands.h/CommandValues.h for adjustment. GameDirectors, one-second throttling,
validation and persist-before-publish behavior remain.

## Validation and live acceptance

Pure checks cover nullopt/no-record/boundaries/vanilla/native faster values,
N=1..10, simulated native seven-tick cadence, exact list defaults/migration,
notice-once/save removal, bounded pair saturation/pruning/ordering, diagnostics
reset, immutable publication, existing ranges and nested/exception RAII scopes.
Both Windows x64 release targets must link; server first, client last. Linker
success is the build evidence; symbol_missing reports are excluded.

0.3.0 validation on 2026-11-06: `urde-checks` passed; both configured Windows x64
release builds compiled, linked and packaged successfully with LL26.60.2. Server
was built first, client last and remains selected. Both manifests report 0.3.0 and
their correct platform under `bin/server/UtilityRenderDistanceExpansion/` and
`bin/client/UtilityRenderDistanceExpansion/`. Existing LL `_AutoHookCount` unused
warnings remain. Only tooth.json version changed; both LL ranges remain
`>=26.60.2 <26.61.0`. Authorized ida-edu4 annotations were saved and its session
closed successfully. No push, deployment, game launch or live process interaction.

Live test (not run by the agent): keep one animal continuously moving about 200
blocks away in resident/ticking chunks with one recipient. Compare vanilla, 4,
2, 1 for ~30 seconds each, allowing one second between command changes; collect
status, visual smoothness/lag and segmented diagnostics. Set N=4, list its exact
type and reload: expect the N=1 behavior and lever_full_rate>0. General intervals
should show opened gates/fewer FINAL nullopts; vanilla should show no changes.
Check pair_overflow=0, no accumulating lag, ghosts or duplicate movement, and
prediction interactions. If FINAL opens but movement stays sparse/stutters,
packet capture is needed to distinguish downstream loss from interpolation.
Repeat later with a second player, checking player bins and separate prediction
routes. Also verify root/key hints, misuse errors, reload/restart persistence,
world/dimension changes, disable/re-enable and inactive remote joins.
