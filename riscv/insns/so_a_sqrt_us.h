auto streamReg = insn.uve_rd();
auto &destReg = P.SU.registers[streamReg];
auto &srcReg = P.SU.registers[insn.uve_rs1()];
auto &predReg = P.SU.predicates[insn.uve_pred()];

// The extra argument is passed because we need to tell the lambda the computation type. In C++20 we would use a lambda template parameter, however in C++17 we don't have those. As such, we pass an extra value to later on infer its type and know the storage we need to use
auto baseBehaviour = [](auto &dest, auto &src, auto &pred, auto extra) {
    auto vLen = src.getMode() == RegisterMode::Scalar ? 1 : dest.getVLen();
    bool zeroing = src.getPredMode() == PredicateMode::Zeroing;

    auto elements = src.getElements();
    auto destElements = dest.getElements(false);
    auto validElementsIndex = src.getValidElements();
    auto pi = pred.getPredicate();

    using StorageType = typename std::remove_reference_t<decltype(dest)>::ElementsType;
    using OperationType = decltype(extra);

    std::vector<StorageType> out = destElements;

    for (size_t i = 0; i < vLen; i++) {
        if (i < validElementsIndex){
            if (pi.at((i+1)*sizeof(OperationType)-1)){
                OperationType e = readAS<OperationType>(elements.at(i));
                if (e >= 0)
                    out.at(i) = readAS<StorageType>(sqrt(e));
                else
                    out.at(i) = readAS<StorageType>(0);
            }
        } else
            out.at(i) = 0; // zeroing out the rest of the elements
    }
    //dest.setValidIndex(dest.vLen);
    dest.setMode(vLen == 1 ? RegisterMode::Scalar : RegisterMode::Vector);
    dest.setElements(out);
};

// If the destination register is not configured, we have to build it before the operation so that its element size matches before any calculations are done
std::visit([&](auto &dest) {
    if (dest.getStatus() == RegisterStatus::NotConfigured) {
        if (std::holds_alternative<StreamReg8>(srcReg)) {
            P.SU.makeStreamRegister<std::uint8_t>(streamReg);
            dest.endConfiguration();
        } else if (std::holds_alternative<StreamReg16>(srcReg)) {
            P.SU.makeStreamRegister<std::uint16_t>(streamReg);
            dest.endConfiguration();
        } else if (std::holds_alternative<StreamReg32>(srcReg)) {
            P.SU.makeStreamRegister<std::uint32_t>(streamReg);
            dest.endConfiguration();
        } else if (std::holds_alternative<StreamReg64>(srcReg)) {
            P.SU.makeStreamRegister<std::uint64_t>(streamReg);
            dest.endConfiguration();
        } else
            assert_msg("Trying to run so.a.sqrt.us with invalid src type", false);
    }
}, destReg);

std::visit(overloaded{
    [&](StreamReg8 &dest, StreamReg8 &src) { baseBehaviour(dest, src, predReg, (unsigned char){}); },
    [&](StreamReg16 &dest, StreamReg16 &src) { baseBehaviour(dest, src, predReg, (unsigned short int){}); },
    [&](StreamReg32 &dest, StreamReg32 &src) { baseBehaviour(dest, src, predReg, (unsigned int){}); },
    [&](StreamReg64 &dest, StreamReg64 &src) { baseBehaviour(dest, src, predReg, (unsigned long int){}); },
    [&](auto &dest, auto &src) { assert_msg("Invoking so.a.sqrt.us with invalid parameter sizes", false); }
}, destReg, srcReg);