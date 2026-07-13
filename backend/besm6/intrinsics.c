#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "besm.h"
#include "frame.h"
#include "internal.h"
#include "tac.h"

// Lowering of the <besm6.h> compiler intrinsics (docs/Besm6_Intrinsics.md).
//
// An intrinsic *is* a call in the IR: it is declared as an ordinary prototype, so the
// front end checks its arity and coerces its arguments like any other call, and it reaches
// the back end as a TAC_INSTRUCTION_FUN_CALL whose fun_name begins with `__besm6_`.  Only
// instruction selection knows better: codegen_intrinsic intercepts the call and emits the
// machine instruction inline instead of a `,call,`.
//
// Every `__besm6_` name must be intercepted here.  All of them collide under Madlen's
// 8-character truncation (`__besm6_apx` and `__besm6_arx` both sanitize to `**BESM6*`), so
// one left to fall through would not fail to link — it would silently alias whichever
// intrinsic the assembler saw first.  Hence the fatal_error at the bottom rather than a
// `return false`.

// The Tier-2 bit-manipulation intrinsics: gather, scatter, population count, highest set
// bit, and the machine's own end-around-carry add.  Each is a single A-op-X instruction —
// the operand comes from memory, the accumulator is both the other input and the result —
// so each lowers to exactly the inline shape of a C binary operator.
static const struct {
    const char *name;
    Besm_InstrKind kind;
    bool mult_omega; // the op leaves multiplicative ω, so it needs the correcting AOX
} bit_intrinsics[] = {
    { "__besm6_apx", BESM_LOG_APX, false }, // 020 сбр — gather the bits selected by a mask
    { "__besm6_aux", BESM_LOG_AUX, false }, // 021 рзб — scatter into a mask's positions
    { "__besm6_acx", BESM_LOG_ACX, false }, // 022 чед — popcount(a) ⊞ x
    { "__besm6_anx", BESM_LOG_ANX, false }, // 023 нед — highest-set-bit position ⊞ x
    { "__besm6_arx", BESM_LOG_ARX, true },  // 013 слц — a ⊞ x (end-around carry)
};

//
// Lower one intrinsic call, or return false if `instr` is an ordinary call.
//
// Called from the top of instr.c's FUN_CALL case, before any argument setup — and so also
// above the _Noreturn branch, which a `_Noreturn` intrinsic would otherwise take.
//
bool codegen_intrinsic(const Tac_Instruction *instr, const Frame *f, Besm_Block *block,
                       Besm_Instr **tail)
{
    const char *name = instr->u.fun_call.fun_name;
    if (strncmp(name, "__besm6_", 8) != 0)
        return false;

    for (size_t i = 0; i < sizeof(bit_intrinsics) / sizeof(bit_intrinsics[0]); i++) {
        if (strcmp(name, bit_intrinsics[i].name) != 0)
            continue;

        const Tac_Val *a = instr->u.fun_call.args;
        const Tac_Val *x = a ? a->next : NULL;
        if (!x || x->next)
            fatal_error("intrinsic %s takes exactly two arguments", name);

        // The inline binop shape: A = a; A op= x; dst = A.  A zero constant operand needs
        // no literal at all — the instruction is left with an empty address field and reads
        // memory word 0, which always reads as zero (b6_popcount(a) is exactly that).
        emit_xta_val(block, tail, f, a);
        emit_arith_val(block, tail, bit_intrinsics[i].kind, f, x);

        if (bit_intrinsics[i].mult_omega) {
            // ARX leaves *multiplicative* ω, under which a following uza/u1a would test
            // abs(A) < 0.5 instead of A ≠ 0 — and the peephole's compare→branch fusion puts
            // a branch right here whenever the result feeds an `if`.  OR in memory word 0:
            // A is unchanged and ω becomes logical again.  The same no-op `,aox,` the
            // unsigned runtime helpers use; see docs/Besm6_Runtime_Library.md § ω mode.
            emit(block, tail, BESM_LOG_AOX);
        }

        const Tac_Val *dst = instr->u.fun_call.dst;
        if (dst && dst->kind == TAC_VAL_VAR)
            emit_store_a(block, tail, f, dst->u.var_name);
        return true;
    }

    fatal_error("intrinsic %s is not lowered yet (tasks I3-I5 in backend/besm6/TODO.md)", name);
}
