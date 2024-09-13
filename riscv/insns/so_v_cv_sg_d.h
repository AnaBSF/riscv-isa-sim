auto streamReg = insn.uve_rd();
auto &destReg = P.SU.registers[streamReg];
auto &srcReg = P.SU.registers[insn.uve_rs1()];


auto baseBehaviour = [](auto &dest, auto &src, auto extra) {
    using StorageType = uint64_t;
    using OperationType = decltype(extra);
    size_t destVLen = dest.getVLen();
    size_t srcVLen = src.getVLen();
    size_t finalElementCount = std::min(destVLen, src.getValidElements());
    
    auto elements = src.getElements();

    std::vector<StorageType> out(destVLen, 0);

    for (size_t i = 0; i < finalElementCount; ++i)
        out.at(i) = static_cast<StorageType>(readAS<OperationType>(elements.at(i)));

    dest.setValidIndex(finalElementCount);
    dest.setElements(out);
};

/* If the destination register is not configured, we have to build it before the
operation so that its element size matches before any calculations are done */
std::visit([&](auto &dest) {
    if (dest.getStatus() == RegisterStatus::NotConfigured) {
        P.SU.makeStreamRegister<std::uint64_t>(streamReg);
        dest.endConfiguration();
    }
}, destReg);

std::visit(overloaded{
    [&](StreamReg64 &dest, StreamReg8 &src) { baseBehaviour(dest, src, (signed char){}); },
    [&](StreamReg64 &dest, StreamReg16 &src) { baseBehaviour(dest, src, (short int){}); },
    [&](StreamReg64 &dest, StreamReg32 &src) { baseBehaviour(dest, src, (int){}); },
    [&](StreamReg64 &dest, StreamReg64 &src) { baseBehaviour(dest, src, (long int){}); },
    [&](auto &dest, auto &src) { assert_msg("Invoking so.v.cv.sg.d with invalid parameter sizes", false); }
}, destReg, srcReg);