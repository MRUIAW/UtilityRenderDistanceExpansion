# 0.3.1 command hints: evidence and temporary comparison

## Accepted live result; experiment removed in 0.3.2

The user tested all six styles: each grey hint showed only `set` and `status`,
while Tab still completed `reload`. The packet audit retained `reload`
(`urdehint5 row=3 column=0 name=reload`). **Conclusion: the client UI hides
`reload` from the grey hint although it is serialized to the client; changing
registration style did not fix it.** The user accepts the real usage below.

0.3.2 removes every probe root, `HintExperiments.h`, the registry/packet audit,
and the xmake option/define. The following investigation and test instructions
are a historical record of 0.3.1, not commands or options available in 0.3.2.

## Original investigation

The user reports that 0.3.0 still omits `reload` from the grey hint after `/urde `.
The exact Windows client cause remains **unproven**. Neither a successful build
nor an enum containing three strings proves that all three are displayed.

## Evidence

Inspected LL26.60.2 `Overload.h`, `OverloadData`, `CommandHandle`,
`CommandRegistrar`, `ParamTraits.h`, `EnumName.h`, and the native
`CommandRegistry` / `CommandParameterData` / `AvailableCommandsPacketPayload`
declarations. The earlier inspected official registrar source creates/reuses
`ll_text_enum_name_<word>` for literals. Thus a literal is a singleton enum
parameter, but identical literal words need not create a fresh enum per overload.
Reflected enum parameters register values from magic_enum under one type name.
SoftEnum uses string parsing with a separate completion table, accepting values
outside its suggested set. No inspected LL path explicitly deletes `reload`.

The native registry retains signatures/overloads, `mEnums`, `mEnumValues`,
`mEnumLookup`, `mEnumValueLookup`, and soft-enum tables. Each hard-enum entry maps
global text indices to parsed numeric values. AvailableCommands contains enum
tables and per-command overloads with parameter name, parseSymbol, optional flag
and paramOptions. These declarations expose no explicit two-action display limit.
They do not establish how Windows chat merges, orders or clips first-word hints.

Only authorized `ida-edu4` was used. Education syntax traversal `0x108e437e0`
was identified through getCommandSyntaxInformation closure RTTI/vtable references
(`0x10f5116a0`, `0x10f5117a0`); usage builder `0x108e42370` calls parameter builder
`0x108e419a0`. That builder tests hard-enum bit `0x200000`, reads options at
parameter+0x61, renders a required singleton as a literal, and expands short
multi-value enums only with option bit 1. The vector-span check is <=0x5f bytes
(16 bytes per value here, hence at most five entries). Otherwise it prints
parameter/type labels. This is evidence for compact usage, **not** a two-action
limit and not proof of the current Windows hint implementation.
Autocomplete traversal `0x108e3f4d0` was located through RTTI/vtable `0x10f511510`;
incomplete decompilation did not establish the missing-reload cause. Annotations
were saved and the database closed.

## Probe roots and expected observations

All commands only echo parsed input. Server roots require GameDirectors; client
roots have the real client command's permission. Replace `urdehint` below with
`urdechint` for the identical client experiment. After each root type a space.
The following are metadata-based expectations to compare with the UI, not claimed
live results. Ordering and deduplication remain test questions.

| Roots (server / client) | Registration | Expected first-word candidates and usage |
|---|---|---|
| `urdehint1` / `urdechint1` | Separate literal status, set numeric, set vanilla, reload | status, set, reload; separate status/reload rows, expanded alpha/beta set rows |
| `urdehint2` / `urdechint2` | Shared Action{status,set,reload} in numeric/vanilla rows | all three in an expanded action parameter on both rows; reproduces the 0.3.0 style |
| `urdehint3` / `urdechint3` | Idle{status,reload} row plus literal set rows | status/reload together, set separately; all three should be available if rows are merged |
| `urdehint4` / `urdechint4` | Soft action and key enums, optional numeric value | completion suggestions status,set,reload and alpha,beta; usage may show named string/soft-enum placeholders instead of a value list |
| `urdehint5` / `urdechint5` | Same literal rows as 1, key expansion disabled | same action candidates; set usage has named setting/Key rather than alpha\|beta; key completion should remain |
| `urdehint6` / `urdechint6` | Reordered{reload,set,status}, one numeric row | reload,set,status in reflected order; contrasts with 2, but both order and competing-row count differ |

Hard-key rows accept alpha/beta and `[value: float]`; literal set styles also
accept `set alpha vanilla`. Shared action/soft probes intentionally allow unusual
combinations and only echo them. Examples: `/urdehint1 reload`,
`/urdehint2 set alpha 4`, `/urdehint3 reload`, `/urdehint4 set beta 2.5`,
`/urdehint5 set alpha vanilla`, `/urdehint6 reload`.

One xmake option, `urde_hint_experiment` (default true), defines
`URDE_HINT_EXPERIMENT`. `xmake f --urde_hint_experiment=n` removes all probe
registration and audits on the next build. No persistent experiment setting exists.

At registration, `[URDE temporary command hint]` logs real/probe registry rows
and columns, enum names/options/values, serialized named hard/soft enum tables
with indices, and packet parameter names/raw symbols/optional/options. Raw packet
symbols are not decoded using assumed internal registry flags. This serializes
an in-memory snapshot; it does not intercept a network send or inspect the
client's received packet. Registry values missing reload implicate registration;
registry containing it but serialized tables lacking it implicates serialization.
Presence in both narrows the remaining possibilities to packet consumption,
syntax/semantic filtering or UI display; it does not distinguish those alone.
Audit failures log a warning without preventing real command registration.

## Real commands

No style fixed the grey hint omission. The literal registration centralized in
`registerControls` is accepted and unchanged in 0.3.2. Real rows:

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

`setting` is both the enum name and parameter name. Clearing only
EnumAutocompleteExpansion avoids expanding the key list; LL26.60.2 deoption uses
logical `!`, so the code clears the bit explicitly. The client numeric enum
excludes entity_shape. Margin remains numeric-only; vanilla receives the existing
clear rejection. Shape values retain their own choice row. Numeric parsing,
validation, throttling, immutable snapshots, persistence and permissions survive.

The missing numeric-value callback keeps its prior message. Bare set,
status/reload extras and incompatible shape arguments are now native syntax
failures. A scoped ExecutedCommandEvent listener adds their prior explanatory
message after failure, without adding usage rows or executing anything. It checks
the exact side's registry, suppressed-output flag and server permission first.
The native syntax error may also be visible. This output path needs live testing.

## Live comparison

1. Use an authorized server account; type every server root plus space, then
   every client root plus space. Record grey text and completion list separately,
   all shown/missing words, ordering, and whether scrolling exposes more rows.
2. Type `r` after each root and execute its full `reload`. Record whether reload
   parses even when absent from the initial grey hint. Probes never reload config.
3. Compare 1/5 after `set ` and compare 2/6. Include startup command-audit logs.
4. Check real rows, setting completion, shape choices, missing/invalid values,
   `status`/`reload` extras, margin vanilla, persistence/reload and nonoperator
   rejection. Space real setting changes at least one second apart.
