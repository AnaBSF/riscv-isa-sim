auto &srcPReg = P.SU.predicates[insn.uve_pred_rs1()]; // half
auto dest = insn.uve_pred_rd();
auto &destPReg = P.SU.predicates[dest]; // double 

auto srcPred = srcPReg.getPredicate();
size_t size = srcPReg.vLen;
std::vector<uint8_t> destPred(size, 0);

size_t srcSkip = sizeof(short);
size_t destSkip = sizeof(double);

for (size_t i = 0, j = 0; i < size && j < size; i += destSkip, j += srcSkip) {
    destPred.at(i) = srcPred.at(j);
}

P.SU.makePredRegister(destPred, dest, PredicateMode::Zeroing);