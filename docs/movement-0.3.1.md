# 0.3.1 elapsed-gap movement payloads

The user found 0.3.0 much smoother, with a slight hitch remaining for nonvanilla
settings. Its gate opens on attempted ticks, so actual gaps can exceed configured
N. Writing fixed N then finishes interpolation before the next attempted send.
This is a plausible remaining hitch mechanism, not a proven diagnosis of the
live visual symptom. No tick-distance or actor simulation change is included.

The scoped hooks, native call order, opening threshold, bounded stable-ID pair
table and native timestamp behavior remain as documented in
[0.3.0](movement-0.3.0.md). `last` means our previous FINAL engaged result, not
the last attempt or the opaque native timestamp. `touch` never advances last;
`sent(now)` runs for every final engaged result, including unchanged native V.

For enabled N=1..10, let elapsed=now-last when a valid record exists:

| Native result | Final result |
|---|---|
| nullopt, no last | engaged N |
| nullopt, elapsed<N | nullopt |
| nullopt, elapsed>=N | engaged clamp(elapsed,1,10) |
| engaged V>N | min(V,max(N,elapsed)); missing/backward last uses elapsed=0 |
| engaged V<=N | unchanged V, including zero |
| vanilla / outside scopes / pair overflow | unchanged native result |

The nullopt payload cap of 10 is an explicit conservative half-second limit at
20 TPS, not a recovered hidden native policy. For an engaged value native V is
the upper bound: no native engaged result is withheld and no payload exceeds V.
A first V<=N remains unchanged to preserve faster native behavior. Backward ticks
cannot underflow and do not open an existing nullopt record.

Examples with N=4: native7 at elapsed4 returns4; at elapsed6 returns6; at
elapsed10 retains7. Nullopt at elapsed6 opens6, and at elapsed29 opens10.
Native1 stays1 at any gap. N=1 with an attempted gap3 can return3, accounting for
the gap while retaining the same gate threshold. Windows' recovered
max(3,mTicks) interpolation floor and active/one-pending target replacement
remain relevant; no growing queue was found. A payload cap cannot guarantee
continuous movement across every long gap or native bypass.

`lever_opened` still counts nullopt-to-engaged changes; `lever_capped` counts only
actual native payload reductions; `lever_full_rate` counts changed decisions
selected by a listed type/tag. New `lever_late` counts tracked FINAL engaged
results with elapsed>N, including unchanged faster native values. First results,
vanilla, untracked overflow and nullopt results do not count as late. Counter
collection/reset uses the existing relaxed atomics and two-second report limit:

```text
... native=... FINAL=... lever_opened=... lever_capped=... lever_full_rate=... lever_late=... pair_overflow=... sendUpdate_calls=... skipped=...
```

Pure checks cover first/on-time/late/capped openings, native V<=N, vanilla,
N=1, repeated/backward ticks, sparse attempts and actual pair record updates.
An exhaustive n=1..10, V=0..15, gap=0..20 sweep verifies native payloads are
never enlarged or withheld, capped means reduced, and late means gap>N.
Existing configuration, list selection, lifecycle/scope, bounded-table and
persistence checks remain included. These verify arithmetic/state, not delivery
or visual acceptance.

For live acceptance, keep one continuously moving animal inside resident,
actively ticking chunks and one recipient; compare vanilla,4,2,1 for 30 seconds
each, then exact type/tag selection with global N=4. Collect status, logs and
visual observations. Expect unchanged native/FINAL under vanilla, opened/capped
counts under enabled policies, possible longer FINAL values when lever_late>0,
and pair_overflow=0. Aggregates mix actors/pairs and cannot measure single-pair
packet timing. Repeat with a second player and local hosting; remote joins alone
must remain inactive. The user's approximately160-block stop was attributed to
server tick-distance and is outside this fix.
