auto streamReg = insn.uve_rd();
auto &destReg = P.SU.registers[streamReg];
auto &srcReg = P.SU.registers[insn.uve_rs1()];

auto baseBehaviour = [](auto &dest, auto &src, auto extra) {
    using StorageType = uint8_t;
    using OperationType = decltype(extra);
    size_t destVLen = dest.getVLen();
    size_t srcVLen = src.getVLen();

    auto elements = src.getElements(!src.getValidElements());

    auto srcValidElements = src.getValidElements();

    size_t finalElementCount = std::min(destVLen, srcValidElements);

    std::vector<StorageType> out(destVLen, 0);

    for (size_t i = 0; i < finalElementCount; ++i)
        out.at(i) = readAS<StorageType>(static_cast<signed char>(readAS<OperationType>(elements.at(i))));

    if (finalElementCount < srcValidElements) {
        src.setValidIndex(srcValidElements - finalElementCount);
        // set src elements to the remaining elements (from finalElementCount to srcValidElements)
       decltype(elements) newElements(srcVLen, 0);
        for (size_t i = 0; i < srcValidElements - finalElementCount; ++i)
            newElements.at(i) = elements.at(i + finalElementCount);
        src.setElements(newElements);
    } else
        src.setValidIndex(0);

    dest.setValidIndex(finalElementCount);
    dest.setElements(out);
};

/* If the destination register is not configured, we have to build it before the
operation so that its element size matches before any calculations are done */
std::visit([&](auto &dest) {
    if (dest.getStatus() == RegisterStatus::NotConfigured) {
        P.SU.makeStreamRegister<std::uint8_t>(streamReg);
        dest.endConfiguration();
    }
},
           destReg);

std::visit(overloaded{
               [&](StreamReg8 &dest, StreamReg8 &src) { baseBehaviour(dest, src, (signed char){}); },
               [&](StreamReg8 &dest, StreamReg16 &src) { baseBehaviour(dest, src, (short int){}); },
               [&](StreamReg8 &dest, StreamReg32 &src) { baseBehaviour(dest, src, (int){}); },
               [&](StreamReg8 &dest, StreamReg64 &src) { baseBehaviour(dest, src, (long int){}); },
               [&](auto &dest, auto &src) { assert_msg("Invoking so.v.cv.sg.b with invalid parameter sizes", false); }},
           destReg, srcReg);