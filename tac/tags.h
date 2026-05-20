enum {
    TAG_EOL             = 0,          // end of list / NULL
    TAG_TAC_CONST       = 0x636e7374, // 'cnst' - for struct Tac_Const
    TAG_TAC_INSTR       = 0x696e7372, // 'insr' - for struct Tac_Instruction
    TAG_TAC_PARAM       = 0x7470726d, // 'tprm' - for struct Tac_Param
    TAG_TAC_STATIC_INIT = 0x73696e69, // 'sini' - for struct Tac_StaticInit
    TAG_TAC_TOPLEVEL    = 0x74706c76, // 'tplv' - for struct Tac_TopLevel
    TAG_TAC_TYPE        = 0x74747970, // 'ttyp' - for struct Tac_Type
    TAG_TAC_VAL         = 0x7476616c, // 'tval' - for struct Tac_Val
};
