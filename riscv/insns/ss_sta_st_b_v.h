#define readRegAS(T, reg) static_cast<T>( READ_REG(reg) )

auto streamReg = insn.uve_rd();
auto &destReg = P.SU.registers[streamReg];
auto baseReg = insn.uve_conf_base();

uint64_t base = READ_REG(baseReg);

P.SU.makeStreamRegister<std::uint8_t>(streamReg, RegisterConfig::Store);
/*operateRegister(P.SU, streamReg, [=](auto& reg) {
    reg.startConfiguration(base);
});*/
std::visit([&](auto& reg){
    reg.startConfiguration(base);
    reg.configureVecDim();
}, destReg);