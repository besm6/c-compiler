# Bemsh C Runtime Library for BESM-6

Bemsh (Cyrillic autocode) port of the hand-written BESM-6 C runtime helpers, assembled by
`besmc` (Assembler БЕМШ) into `libbem.bin` for the Dubna simulator. This is the third
assembler dialect the backend targets, alongside Madlen ([../madlen/](../madlen)) and the
Unix `b6as` ([../unix/](../unix)).

**Scope (task B4): helpers only.** Each `.bemsh` file is a hand-translation of the
same-stem `../madlen/b_NAME.madlen`, covering the full `LIBC_MADLEN` set — the calling
convention (`_save`/`_save0`/`_ret`), the arithmetic / comparison / shift / pointer / byte
helpers, `b_tout`, `exit`, `frexp`, `ldexp` (41 files). The C-level libc
(`LIBC_C_PORTABLE`, and the three Dubna C leaves `putbyte`/`flush`/`getch`) is **not** built
here yet: the C→Bemsh compile path needs the Bemsh code emitter
([../../../backend/besm6/emit_bemsh.c](../../../backend/besm6/emit_bemsh.c), task B1) wired
through `genbesm` plus the B2 name mangler. So `libbem.bin` is currently the helper set only.

Build: `make` (the `besm-libc-bemsh` ALL target) → `build/libc/besm6/bemsh/libbem.bin`,
staged next to `besm-tests` as `build/backend/besm6/libbem.bin` for the future Bemsh run
harness (task B3).

## Exported helper-symbol map (this pins B2's target and B3's link symbols)

Every Madlen `b/NAME` symbol becomes `_NAME` (drop the `b`, `/`→`_`, ≤6 chars). Non-`b/`
entry points (`exit`, `frexp`, `ldexp`) keep their names. This matches
[../../../backend/besm6/tmp/bemsh.dub](../../../backend/besm6/tmp/bemsh.dub) (`_ret`,
`_save`). In Dubna output the leading `_` renders as the Cyrillic `Ю`.

| Group | Madlen → Bemsh entry symbols |
|-------|------------------------------|
| Calling convention | `b/save`→`_save`, `b/save0`→`_save0`, `b/ret`→`_ret` |
| Signed arithmetic | `b/mul`→`_mul`, `b/div`→`_div`, `b/mod`→`_mod` |
| Unsigned arithmetic | `b/uadd`→`_uadd`, `b/usub`→`_usub`, `b/umul`→`_umul`, `b/udiv`→`_udiv`, `b/umod`→`_umod`, `b/uneg`→`_uneg` |
| Shifts | `b/lsh`→`_lsh`, `b/rsh`→`_rsh` |
| Signed compares | `b/eq`→`_eq`, `b/ne`→`_ne`, `b/lt`→`_lt`, `b/le`→`_le`, `b/gt`→`_gt`, `b/ge`→`_ge`, `b/not`→`_not` |
| Unsigned compares | `b/ult`→`_ult`, `b/ule`→`_ule`, `b/ugt`→`_ugt`, `b/uge`→`_uge` |
| FP compares | `b/flt`→`_flt`, `b/fle`→`_fle`, `b/fgt`→`_fgt`, `b/fge`→`_fge` |
| Conversions | `b/dtoi`→`_dtoi`, `b/dtou`→`_dtou`, `b/utod`→`_utod` |
| Pointer / byte | `b/padd`→`_padd`, `b/pinc`→`_pinc`, `b/pdec`→`_pdec`, `b/pdiff`→`_pdiff`, `b/stb`→`_stb` |
| I/O · exit · math | `b/tout`→`_tout`, `exit`, `frexp`, `ldexp` |

**Shared constant block** (defined in [b_ret.bemsh](b_ret.bemsh)): `_true` is exported
(`входн`); the two words that follow it are `_true+1` = bit 48 and `_true+2` = bits 48…1.
Other modules import `_true` (`внешн ._true`) and reference `_true+1` / `_true+2` **directly
in instruction operands** — Bemsh forbids `ЭКВ` on an external symbol, but an
external±offset in an operand is fine.

## Madlen → Bemsh translation reference

Driven by the two mnemonic tables in the backend:
[besm_mnem.c](../../../backend/besm6/besm_mnem.c) (Latin) and
[emit_bemsh.c](../../../backend/besm6/emit_bemsh.c) `besm_cyr_mnem[]` (Cyrillic). All
mnemonics and directives are lowercase (Bemsh is case-insensitive).

**Line form.** Label in column 1 (≤6 chars, blank = no label); the index/base register is
parenthesized **after** the operand: Madlen `15 ,mtj, 14` → `уии 14(15)`; `13 ,uj,` → `пб (13)`.

**Directives / data.**

| Madlen | Bemsh | Notes |
|--------|-------|-------|
| `,name,` | `старт 1` | needs a start-address operand (empty errors) |
| `,end,` | `финиш` | |
| `,subp,` | `NAME  внешн  .NAME` | external, `.name` search-all form |
| `,entry,` | `входн NAME` | + the label on the following word |
| `,base,*` | `употр *(14)` | use register as base (does not load it) |
| `,bss,` (empty) | `NAME экв *` | reserves 0 words = a bare label |
| `,bss, N` | `NAME пам N` | reserve N words |
| `,log, X` | `конд в'X'` | whole-word octal, right-justified (as-is) |
| `,oct, X` | `конд в'<X left-filled to 16 octal digits>'` | left-fill: pad zeros on the right |
| `,equ, E` | `NAME экв E` | local labels only — not on externals |
| `,*71,` | `э71` | extracode (opcodes 050–077 are `э<octal>`) |
| `,*74,` | `э74` | |

**Literal operands** (Madlen address-field bare numbers are decimal; octal needs a `B`
suffix — Bemsh matches: bare = decimal, octal = `в'…'`):

| Madlen literal | Bemsh | |
|----------------|-------|---|
| `= <octal>` | `=в'<octal>'` | LOG, right-fill (value as-is) |
| `=: <octal>` | `=в'<octal left-filled to 16 digits>'` | OCT, left-fill (e.g. `=:64` → `=в'6400000000000000'`) |

**Comments.** Full-line: `*` in column 1 (safe inside the `ввд$$$` block). Inline: `;` after
the operand — but **never on an instruction with a truly empty operand** (the assembler
reads the `;` as the operand); those keep only a preceding `*` line or no comment.

**Deck framing.** `besmc` feeds the `.bemsh` source verbatim after its `*bemsh` control card
and adds no Macro-Bemsh markers, so each file wraps its module in the required
`ввд$$$` … module … `квч$$$ / трн$$$ / 0-0 / блмак / бтмалф / кнц$$$` deck.

**Special renderings.**

- `b_tout`: the extracode-71 control word `info: 12 ,040,` + blank half becomes
  `info уи (12)` (opcode 040 = `уи`, index register 12) + `конк в'0'` (the zero half-word),
  reproducing the same 48-bit word.
- `b_udiv` / `b_umod` route through sub-helpers via `,call, b/X` → `пв _X(13)` (VJM through
  r13); every callee is declared `внешн ._X` (Bemsh needs an explicit external declaration,
  unlike Madlen's auto-declaring `,call,` macro).
- Local labels are truncated to ≤6 chars and `*` is dropped: `havebyte`→`haveby`,
  `b*large`→`blarge`, `u*fst`→`ufst`, etc. (consistently at definition and every use).
