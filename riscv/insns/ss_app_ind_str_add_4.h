auto &destReg = P.SU.registers[insn.uve_rd()];
auto origin = insn.uve_mod_origin();

std::visit([&](auto &reg) {
    reg.addDynamicModifier(dynamicModifier_t(Target::Stride, dynamicBehaviour::Add, origin, &(P.SU), 3));
}, destReg);