**What makes this schema distinctive for AVR:**

**Register hierarchy** — four distinct register types capture the ISA's range constraints precisely: `greg` (R0–R31, all instructions), `ureg` (R16–R31, immediate operands and MULS), `mreg` (R16–R23, MULSU/FMUL\*), `ereg` (even-numbered only, MOVW), plus `word_reg` (R24/R26/R28/R30, ADIW/SBIW) and `ptr_reg` (X/Y/Z pointer pairs).

**Indirect addressing** — `indir_addr` unifies all indirect load/store modes in one type: plain `@Ptr`, `@Ptr+` (post-increment), `@-Ptr` (pre-decrement) for all three pointer pairs, plus `IndYq`/`IndZq` for the Y+q and Z+q displacement forms (LDD/STD, q = 0..63). X has no displacement mode.

**Branch coverage** — all 20 named branch mnemonics are included (BRBS/BRBC generic forms plus all 18 named aliases: BREQ/BRNE, BRCS/BRCC, BRLO/BRSH, BRMI/BRPL, BRGE/BRLT, BRVS/BRVC, BRHS/BRHC, BRTS/BRTC, BRIE/BRID). BRLO and BRSH are modelled as separate constructors from BRCS/BRCC even though they encode identically — compilers choose between them based on semantic context.

**Skip instructions** (CPSE/SBRC/SBRS/SBIC/SBIS) form their own group — AVR's unique "skip the next instruction" mechanism has no direct equivalent in any other ISA in this series.

**Program memory access** — `LPM_R0`/`LPM`/`ELPM_R0`/`ELPM` for reading flash, `SPM`/`SPM_INC` for self-programming, all modelled with their distinct no-operand and register forms.

**XMEGA extensions** — `XCH`/`LAC`/`LAS`/`LAT` (atomic load-and-modify via Z) and `DES` (hardware DES/AES round instruction) are included with notes on availability.
