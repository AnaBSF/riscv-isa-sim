auto regN = insn.uve_branch_rs();
auto& streamReg = P.SU.registers[regN];
auto branchIMM = insn.uve_branch_imm();

if (!P.SU.EODTable.at(regN).at(2))
    set_pc(pc + branchIMM);