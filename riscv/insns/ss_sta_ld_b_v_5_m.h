auto streamReg = insn.uve_rd();
auto &destReg = P.SU.registers[streamReg];
auto baseReg = insn.uve_conf_base();

uint64_t base = READ_REG(baseReg);

P.SU.makeStreamRegister<std::uint8_t>(streamReg, RegisterConfig::Load, PredicateMode::Merging);
/*operateRegister(P.SU, streamReg, [=](auto& reg) {
    reg.startConfiguration(base);
});*/
std::visit([&](auto& reg){
    reg.startConfiguration(base);
    reg.configureVecDim(4);
}, destReg);