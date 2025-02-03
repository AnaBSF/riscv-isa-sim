auto streamReg = insn.uve_rd();
auto &destReg = P.SU.registers[streamReg];
auto baseReg = insn.uve_conf_base();

uint64_t base = READ_REG(baseReg);

P.SU.makeStreamRegister<std::uint8_t>(streamReg, RegisterConfig::IndSource);

std::visit([&](auto& reg){
    reg.startConfiguration(base);
}, destReg);