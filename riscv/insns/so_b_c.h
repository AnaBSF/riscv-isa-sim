auto& streamReg = P.SU.registers[insn.uve_branch_rs()];
auto branchIMM = insn.uve_branch_imm();

const bool complete = std::visit([](const auto &reg){
    return reg.hasStreamFinished();
}, streamReg);

if (complete)
    set_pc(pc + branchIMM);