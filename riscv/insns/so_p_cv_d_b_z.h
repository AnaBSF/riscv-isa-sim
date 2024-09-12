auto &srcPReg = P.SU.predicates[insn.uve_pred_rs1()]; // double
auto dest = insn.uve_pred_rd();
auto &destPReg = P.SU.predicates[dest]; // byte 

auto srcPred = srcPReg.getPredicate();
size_t size = srcPReg.vLen;
std::vector<uint8_t> destPred(size, 0);

size_t srcSkip = sizeof(double);
size_t destSkip = sizeof(char);

for (size_t i = 0, j = 0; i < size && j < size; i += destSkip, j += srcSkip) {
    destPred.at(i) = srcPred.at(j);
}

P.SU.makePredRegister(destPred, dest, PredicateMode::Zeroing);