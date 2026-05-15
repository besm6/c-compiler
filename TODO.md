# TODO

Work plan, ordered by recommended implementation sequence.

## BESM-6 backend

| # | Task | Description | Effort |
|---|------|-------------|--------|
| 12 | BESM-6 machine model | Define the register file, instruction encoding, and calling convention in `backend/besm6.h`. | L |
| 13 | Instruction selection | Walk each `Tac_Instruction` and emit the corresponding BESM-6 instruction(s). Start with arithmetic, load/store, and control flow. | XL |
| 14 | Register allocation | Implement a register allocator (linear scan or graph colouring) over TAC temporaries, targeting BESM-6's accumulator-based architecture. | XL |
| 15 | Assembly / object output | Write output in a format consumable by the Dubna monitor or a BESM-6 assembler. | L |
