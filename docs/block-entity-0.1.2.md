# 0.1.2 block-entity draw gate

## Proven cause and caller map

The user's 0.1.1 logs establish matched fetches, no in-queue fallback and accepted
distances of 68.5, yet chests disappear at ~64. Static inspection now establishes
an independent **second invocation of the same native predicate after queueing**:

| Path | Education (ida-edu4) | Windows client on disk |
|---|---|---|
| Queue / insertion | `104147020` → `104148e60`, slot `+30` call `104148f28` | `144692670` → `1446959a0`, call site `144695a10` |
| World opaque/alpha draw | `10412f150` → `10375f020` at `10412f344` | `144675960` → `14626c490` at `144675b0e` |
| Dispatcher range recheck | `10375f06d`, return `10375f070` | slot load `14626c4cd`, CFG call `14626c4d4`, return `14626c4da` |
| Base component predicate | `10a90c290` | `14962aa60`, strict squared distance `<4096` |
| Complete-object copy | `10a90c1f0`, primary vtable `+108` | no separate copy established |
| Beacon component override | `10a87bbb0`; complete copy `10a87bb60` | matching leaf body `142db2650` |

Addresses are evidence only, never runtime hook selectors. Windows preferred
module base is `140000000`; expected dispatcher return RVA is **`626c4da`**.
Both dispatcher bodies return without drawing when that second predicate fails.
The queue TLS is absent at this point, so 0.1.1 necessarily calls native `<4096`.
This proves a missed gate and explains the observed boundary on the traced path;
it does not prove that every backend will now draw the chest.

The dispatcher reads context `mImpl` at `+a8` on both platforms. Its predicate
argument is **`Impl::mCameraTargetPosition`**, Windows `+28`, Education `+30`
(different offscreen-capture layout). The next camera-position field is not used
for this check. Code uses typed members, not these offsets.

All direct callers found for Education's distance-checking dispatcher wrapper:

| Caller | Dispatcher call site(s) | Treatment |
|---|---|---|
| `101f1ef80` | `101f20b15`, `101f20bce` | separate client rendering path, exact semantic name unproven; native |
| StructureVolumeRenderer callback `101ffd210` (RTTI identified) | `101ffd2de` | editor/structure path; native |
| camera `renderBlockEntities`, `10412f150` | `10412f344` | proven ordinary/alpha world path; scoped |
| `10415bb90` | `10415cb5d` | separate recursive render path, exact name unproven; native |
| player camera render-entity body `10415fa50` | `104160407` | supplemental path, not added to scope; native |

Player override `10415e1b0` and another override `104175240` tail-jump to the
base camera draw body. Player vtable entry `10f2d3a68` points to that wrapper.
The base draw body iterates the final opaque/alpha queues directly; no intervening
list-copy distance filter was found. Dispatcher inner explicit-render-position
overload `1037495b0` has no separate range compare in its inspected body. Its
other direct callers are `103748fb0` (three calls) and `103774a50`; these bypass
the outer recheck and are not newly hooked. The previously inspected ChestRenderer
body `1037668d0` has no identified independent 64-block test.

Base and Beacon predicate xrefs are vtable data references, not direct calls.
Education render-component vtables surveyed through the common component entries
yield base and Beacon predicate targets only. Installed LL block-actor headers
also declare only Beacon as a special override. Beacon returns true for its native
flag, otherwise tests horizontal x/z squared distance `<4096`; it must **not** be
replaced with the ordinary strict 3D AABB test. Both copies remain native.

## Constant/caller search limits

Read-only executable surveys found 50,660 Education / 38,899 Windows functions
with candidate slot-30 loads/calls. Most are unrelated vtables or member loads;
boolean-consumer filtering and rendering-path inspection identified the queue
and dispatcher calls above. Examples rejected: Education `1037a2fb0` dispatches
function objects over linked subchunk data; `104159370`'s slot-30 predicate takes
only `this`, not a Vec3, so is not this interface.

RIP-relative 64/4096 surveys found 51 Education / 26 Windows candidate functions.
The base/Beacon copies above are real 4096 tests. Education `1041074f0` and Windows
`144812a30` use subchunk centers (`16*x+8`) and a projected camera corridor
(`256`, `4096`), not an inlined block-component point-to-AABB predicate. They are
not changed. Windows `149738aa0` is another constant candidate, **not attributed
as a block-entity gate**. No additional inlined 4096 test was found in the
inspected ordinary camera/dispatcher/chest bodies. A primary-slot-108 boolean
candidate scan found no candidates in Education's `1037...`/`1041...` rendering
regions. These opcode/constant scans are **not an exhaustive all-indirect-call or
all-inlining proof** (encodings, compiler transformations and other callees can
escape them). Unidentified paths stay native; this report does not claim a complete
whole-program caller enumeration.

Evidence files remain under the authorized `ida-edu4` directory: `urde-012-a`–`e`,
`urde-012-*-slot30.txt`, `*-boolslots.txt`, `*-floats.txt`, `urde-012-primary108.txt`,
`urde-012-vtable-predicates.txt`, and `urde-012-windows-bodies.txt`. Only
`minecraft-edu.i64` there was opened, sequentially; annotations were saved and every
batch explicitly closed it. Windows evidence is disk-only, without process access.

## Implementation and safety

One new hook: public MCAPI `LevelRendererCamera::$renderBlockEntities`, exact
`void (LevelRendererCamera::*)(BaseActorRenderContext&, bool)`. Both virtual and
native-thunk signatures have `static_assert`s. Enable resolves the exact named
symbol first, installs fail-closed and rolls back; disable removes the hook.
No dispatcher hook: its metadata optional contains LL placeholder types by value.

The new stack-local RAII scope pins an immutable effective config, generation,
owned player camera, its client Level/dimension/view source and the typed render
context target. It also requires the same client instance. Membership is copied
from the **selected native final opaque/alpha list**, not the earlier temporary
fetch list. Predicate takeover additionally requires matching position and list
membership; it reuses the existing strict 3D virtual-AABB test and corrected
`thisFor<IVanillaRenderBlockActorComponent>()` secondary-base adjustment.

Queue entry clears draw TLS; draw entry clears queue and inherited draw TLS,
including native/ineligible nested invocations. Scopes restore on return/exception;
no component/camera pointer persists afterward. Existing lifecycle generation
invalidation covers disconnect, join, dimension/world end and disable. Menus and
in-process server predicates have no matching render scope and stay native.
Shadow/cloud cameras and unrelated/offscreen/UI/item draw invocations are not
newly expanded. Existing 0.1.1 queue/terrain behavior is unchanged; this is not a
shadow-range guarantee. Shared replication files are unchanged.

## Live diagnostics and acceptance (not run)

Existing `[URDE temporary block-entity diagnostics]` fields are preserved.
At the same global two-second cadence, queue or draw completion also emits:

```text
[URDE temporary block-entity draw diagnostics] draws=... takeover=... fallback=... reasons(...) maxAcceptedDistance=... outsideQueue=... unscopedNative=... otherCameraScope=...
[URDE temporary block-entity caller] outsideQueue Minecraft+0x626c4da (once per session/generation; evidence only)
```

`outsideQueue` includes scoped draws and fully unscoped calls; `unscopedNative`
is the subset without either scope. The first eligible Minecraft caller sample
uses `_ReturnAddress()` in the predicate hook, subtracts the main module base and
checks module allocation ownership. It is numeric evidence only, not an address
hook. Another outside caller may win the first sample. The sample is emitted
only once per lifecycle generation, not once per interval. Counts are temporary,
session-gated and reset with the generation; summary state retains no game pointers.

`otherCameraScope` counts rejected position/membership calls that match another
currently owned camera's target AND native component list. Shared components can
make attribution ambiguous; it is a diagnostic match, not an assertion of caller
identity. Such calls always fall back. The old predicate counts, fallback reasons,
position sample and maximum remain queue-only; draw counts, reasons and maximum
are reported separately. Neither maximum certifies rendering.

Test 128 → vanilla → 128 around 64/72/96/128, opaque/alpha, normal/third-person,
dimension/rejoin, remote/local hosting, UI/item previews and shadows. Expect
`draws>0`, draw `takeover>0`, accepted distances beyond64, and ordinarily low draw
fallback. If draws stay zero, runtime dispatch/symbol resolution needs checking;
if draw takeover succeeds but visibility still stops at64, another gate remains.
Builds and game-independent RAII checks cannot certify visuals or live dispatch.

## Offline validation

Both 0.1.2 Windows x64 release builds passed with LL 26.51.9; final xmake config
is `target_type=client`. Packages: client `bin/UtilityRenderDistanceExpansion/`,
server `build/artifacts/server/UtilityRenderDistanceExpansion/`; both manifests
report 0.1.2 and the correct side. `urde-checks` passed, including new nested
queue/draw null-scope and exception-restoration checks. The only compiler warning
was LL's existing manual-hook `_AutoHookCount` warning.

Offline prelink of the compiled client object against installed
`bedrock_runtime_data` v26.51.1-client.12 generated the exact
`?$renderBlockEntities@LevelRendererCamera@@QEAAXAEAVBaseActorRenderContext@@_N@Z`
import, verified with `llvm-nm`. This proves named symbol-data availability, not
live dispatch. Replication/server sources and LICENSE/COPYING match 0.1.1 exactly.
Final exclusive-access check confirmed the annotated ida-edu4 database was closed
and released. No game launch, deployment, push or interaction with the running
hi3/BDS-hi3 processes occurred.
