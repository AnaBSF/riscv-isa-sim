auto streamReg = insn.uve_rd();
auto &destReg = P.SU.registers[streamReg];
auto baseReg = insn.uve_conf_base();

uint64_t base = READ_REG(baseReg);

P.SU.makeStreamRegister<std::uint32_t>(streamReg, RegisterConfig::Load);

std::visit([&](auto& reg){
    reg.startConfiguration(base);
    reg.configureVecDim();
}, destReg);