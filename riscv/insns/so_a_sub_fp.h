auto streamReg = insn.uve_rd();
auto &destReg = P.SU.registers[streamReg];
auto &src1Reg = P.SU.registers[insn.uve_rs1()];
auto &src2Reg = P.SU.registers[insn.uve_rs2()];
auto &predReg = P.SU.predicates[insn.uve_pred()];

/* The extra argument is passed because we need to tell the lambda the computation type. In C++20 we would
    use a lambda template parameter, however in C++17 we don't have those. As such, we pass an extra value to
    later on infer its type and know the storage we need to use */
auto baseBehaviour = [](auto &dest, auto &src1, auto &src2, auto &pred, auto extra) {
    /* Each stream's elements must have the same width for content to be
     * operated on */
    assert_msg("Given vectors have different widths", src1.getElementWidth() == src2.getElementWidth());
    size_t vLen = src1.getMode() == RegisterMode::Scalar ||  src2.getMode() == RegisterMode::Scalar ? 1 : dest.getVLen();
    bool zeroing = src1.getPredMode() == PredicateMode::Zeroing && src2.getPredMode() == PredicateMode::Zeroing;
    /* We can only operate on the first available values of the stream */
    auto elements1 = src1.getElements();
    auto elements2 = src2.getElements();

    /* Grab used types for storage and operation */
    using StorageType = typename std::remove_reference_t<decltype(dest)>::ElementsType;
    using OperationType = decltype(extra);
    std::vector<StorageType> out = dest.getElements(false); // for merging predication

    auto validElementsIndex = std::min(src1.getValidElements(), src2.getValidElements());

    auto pi = pred.getPredicate();

    for (size_t i = 0; i < vLen; i++) {
        if (i < validElementsIndex){
            if (pi.at((i + 1) * sizeof(OperationType) - 1)) {
                OperationType e1 = readAS<OperationType>(elements1.at(i));
                OperationType e2 = readAS<OperationType>(elements2.at(i));
                out.at(i) = readAS<StorageType>(OperationType(e1 - e2));
                //std::cout << "SUB   " << e1 << " - " << e2 << " = " << readAS<OperationType>(out.at(i)) << "\n";
            }
        } else if (zeroing)
            out.at(i) = 0; // zeroing out the rest of the elements
    }
    //dest.setValidIndex(dest.vLen);
    dest.setMode(vLen == 1 ? RegisterMode::Scalar : RegisterMode::Vector);
    dest.setElements(out);
    //std::cout << "\n\n";
};

/* If the destination register is not configured, we have to build it before the
operation so that its element size matches before any calculations are done */
std::visit([&](auto &dest) {
    if (dest.getStatus() == RegisterStatus::NotConfigured) {
        if (std::holds_alternative<StreamReg64>(src1Reg)) {
            P.SU.makeStreamRegister<std::uint64_t>(streamReg);
            /*operateRegister(P.SU, streamReg, [=](auto& reg) {
              reg.endConfiguration();
            });*/
            dest.endConfiguration();
        } else if (std::holds_alternative<StreamReg32>(src1Reg)) {
            P.SU.makeStreamRegister<std::uint32_t>(streamReg);
            /*operateRegister(P.SU, streamReg, [=](auto& reg) {
              reg.endConfiguration();
            });*/
            dest.endConfiguration();
        } else
            assert_msg("Trying to run so.a.adde.fp with invalid src type", false);
    }
},
           destReg);

std::visit(overloaded{
               [&](StreamReg64 &dest, StreamReg64 &src1, StreamReg64 &src2) { baseBehaviour(dest, src1, src2, predReg, double{}); },
               [&](StreamReg32 &dest, StreamReg32 &src1, StreamReg32 &src2) { baseBehaviour(dest, src1, src2, predReg, float{}); },
               [&](auto &dest, auto &src1, auto &src2) { assert_msg("Invoking so.a.sub.fp with invalid parameter sizes", false); }},
           destReg, src1Reg, src2Reg);