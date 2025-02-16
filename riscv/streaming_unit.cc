#include "streaming_unit.h"
#include "mmu.h"
#include "processor.h"

#define gMMU(p) (*(p->get_mmu()))

template <typename T>
void streamRegister_t<T>::addStaticModifier(staticModifier_t mod) {
    assert_msg("Cannot append more modifiers as max dimensions were reached", dimensions.size() + 1 < su->maxDimensions);
    // staticModifiers.insert({0, mod});
    //  add a static modifier to the current dimension
    staticModifiers.insert({dimensions.size() - 1, mod});
    /* print modifiers
    std::cout << "\nModifiers: ";
    for (auto &m : modifiers)
        std::cout << m.first << "  ";
    std::cout << std::endl;*/
}

template <typename T>
void streamRegister_t<T>::addDynamicModifier(dynamicModifier_t mod) {
    assert_msg("Cannot append more modifiers as max dimensions were reached", dimensions.size() + 1 < su->maxDimensions);
    // dynamicModifiers.insert({0, mod});
    //  add a dynamic modifier to the current dimension
    dynamicModifiers.insert({dimensions.size() - 1, mod});
    /* print modifiers
    std::cout << "\nModifiers: ";
    for (auto &m : modifiers)
        std::cout << m.first << "  ";
    std::cout << std::endl;*/
}

template <typename T>
void streamRegister_t<T>::addScatterGModifier(scatterGModifier_t mod) {
    assert_msg("Cannot append more modifiers as max dimensions were reached", dimensions.size() + 1 < su->maxDimensions);
    scatterGModifiers.insert({0, mod});
    /* print modifiers
    std::cout << "\nModifiers: ";
    for (auto &m : modifiers)
        std::cout << m.first << "  ";
    std::cout << std::endl;*/
}

template <typename T>
void streamRegister_t<T>::addDimension(dimension_t dim) {
    assert_msg("Cannot append more dimensions as the max value was reached", dimensions.size() < su->maxDimensions);

    dimensions.push_back(dim);

    std::unordered_multimap<int, staticModifier_t> updatedModifiers;
}

template <typename T>
void streamRegister_t<T>::configureVecDim(const int cfgIndex) {
    mode = RegisterMode::Vector;
    if (type != RegisterConfig::Load)
        validElements = vLen;
    vecCfgDim = cfgIndex;
}

template <typename T>
void streamRegister_t<T>::startConfiguration(size_t base_address) {
    status = RegisterStatus::NotConfigured;
    mode = RegisterMode::Scalar;
    baseAddress = base_address;
    dimensions.clear();
}

template <typename T>
void streamRegister_t<T>::endConfiguration() {
    status = RegisterStatus::Running;

    // If vecCfgDim is -1, then the innermost dimension is the vector coupled dimension
    if (vecCfgDim == -1)
        vecCfgDim = dimensions.size() - 1;

    // Apply all dynamic modifiers for the first iteration
    for (size_t i = 0; i < dimensions.size(); i++) {
        // std::cout << "u" << registerN << "    Applying dynamic modifiers to dim " << i << std::endl;
        auto dynamicModifierIters = dynamicModifiers.equal_range(i);
        for (auto it = dynamicModifierIters.first; it != dynamicModifierIters.second; ++it) {
            // std::cout << "u" << registerN << "    Applying dynamic modifier to dim " << i << std::endl;
            it->second.modDimension(dimensions, elementWidth);
        }
    }

    su->updateEODTable(registerN);
}

template <typename T>
void streamRegister_t<T>::finishStream() {
    status = RegisterStatus::Finished;
    type = RegisterConfig::NoStream;

    // Mark all dimensions as finished in EODTable to avoid wrong branching
    for (dimension_t &dim : dimensions)
        dim.setEndOfDimension(true);
    su->updateEODTable(registerN);
}

template <typename T>
std::vector<T> streamRegister_t<T>::getElements(bool causesUpdate) {
    // std::cout << "u" << registerN << "    Getting elements" << std::endl;
    if (causesUpdate && this->type == RegisterConfig::Load)
        updateAsLoad();

    std::vector<T> e(elements.begin(), elements.end());

    return e;
}

template <typename T>
bool streamRegister_t<T>::getDynModElement(int &value) {
    assert_msg("Dynamic modifier source is not correctly configured", type == RegisterConfig::IndSource);

    // std::cout << "u" << registerN << "    Getting modifier element" << std::endl;

    updateAsLoad();

    value = readAS<int>(elements.at(0));

    return !hasStreamFinished();
}

template <typename T>
void streamRegister_t<T>::setElements(std::vector<T> e, bool causesUpdate) {
    // assert_msg("Trying to set values to a load stream", type != RegisterConfig::Load && type != RegisterConfig::IndSource);

    elements = e;

    if (causesUpdate && this->type == RegisterConfig::Store)
        updateAsStore();
}

template <typename T>
void streamRegister_t<T>::setValidIndex(const size_t i) {
    assert_msg("Trying to set valid index to invalid value", i <= vLen);

    validElements = i;
}

template <typename T>
void streamRegister_t<T>::setMode(const RegisterMode m) {
    mode = m;
    if (type != RegisterConfig::Load) {
        if (m == RegisterMode::Scalar)
            validElements = 1;
        else
            validElements = vLen;
    }
}

template <typename T>
void streamRegister_t<T>::setPredMode(const PredicateMode pm) {
    predMode = pm;
}

template <typename T>
bool streamRegister_t<T>::hasStreamFinished() const {
    return status == RegisterStatus::Finished;
}

template <typename T>
bool streamRegister_t<T>::isEndOfDimensionOfDim(size_t i) const {
    assert_msg("Trying to check EOD of invalid dimension", i < dimensions.size());
    return dimensions.at(i).isEndOfDimension();
}

template <typename T>
size_t streamRegister_t<T>::getElementWidth() const {
    return elementWidth;
}

template <typename T>
size_t streamRegister_t<T>::getVLen() const {
    return vLen;
}

template <typename T>
size_t streamRegister_t<T>::getValidElements() const {
    return validElements;
}

template <typename T>
RegisterStatus streamRegister_t<T>::getStatus() const {
    return status;
}

template <typename T>
RegisterConfig streamRegister_t<T>::getType() const {
    return type;
}

template <typename T>
RegisterMode streamRegister_t<T>::getMode() const {
    return mode;
}

template <typename T>
PredicateMode streamRegister_t<T>::getPredMode() const {
    return predMode;
}

/* FOR DEBUGGING*/
template <typename T>
void streamRegister_t<T>::printRegN(char *str) {
    if (registerN >= 0) {
        fprintf(stderr, ">>> UVE Register u%ld %s <<<\n", registerN, str);
    } else
        fprintf(stderr, ">>> Register number not set for debugging. %s<<<", str);
}

template <typename T>
size_t streamRegister_t<T>::generateAddress() {
    /* Result will be the final accumulation of all offsets calculated per dimension */
    size_t init = baseAddress; // base address of the stream
    int dimN = 0;

    // std::cout << "u" << registerN << ": Generating address." << std::endl;

    return std::accumulate(dimensions.rbegin(), dimensions.rend(), init, [&](size_t acc, dimension_t &dim) {
        if (dim.isLastIteration() && isDimensionFullyDone(dimensions.end() - dimN, dimensions.end()))
            dim.setEndOfDimension(true);
        ++dimN;
        // std::cout << "u" << registerN << "    Calculating address for dim " << dimensions.size() - dimN + 1 << std::endl;
        return acc + dim.calcAddress(elementWidth);
    });
}

template <typename T>
bool streamRegister_t<T>::isDimensionFullyDone(const std::deque<dimension_t>::const_iterator start, const std::deque<dimension_t>::const_iterator end) const {
    return std::accumulate(start, end, true, [](bool acc, const dimension_t &dim) {
        return acc && dim.isEndOfDimension();
    });
    ;
}

template <typename T>
bool streamRegister_t<T>::isStreamDone() const {
    // return isDimensionFullyDone(dimensions.begin(), dimensions.end());
    return dimensions.at(0).isEndOfDimension();
}

template <typename T>
bool streamRegister_t<T>::tryGenerateAddress(size_t &address) {
    /* There are two situations that prevent us from generating offsets/iterating a stream:
    1) We are at the last iteration of the outermost dimension
    2) We just finished the last iteration of a dimension and there is a configure
    stream vector modifier at that same dimension. In these cases, the generation can
    only resume after an exterior call to setEndOfDimension(false) */

    /* The outermost dimension is the last one in the container */

    if (isStreamDone()) {
        /*if (registerN == 3)
            std::cout << "u" << registerN << " Stream is done HERE GENERATE" << std::endl;*/
        finishStream();
        return false;
    }

    bool canGenerateAddress = true;

    for (int i = int(dimensions.size() - 1); i >= 0; i--) {
        applySGMods(i);
        if (dimensions.at(i).isEmpty()) {
            for (int j = i; j < int(dimensions.size()); j++)
                dimensions.at(j).setEndOfDimension(true);
            canGenerateAddress = false;
        }
        if (i == vecCfgDim && isDimensionFullyDone(dimensions.begin() + i, dimensions.end())){
            canGenerateAddress =  false;
            break;
        }
    }

    address = generateAddress();
    return canGenerateAddress;
}

template <typename T>
void streamRegister_t<T>::applySGMods(size_t dimN) {
    auto currentModifierIters1 = scatterGModifiers.equal_range(dimN);
    for (auto it = currentModifierIters1.first; it != currentModifierIters1.second; ++it) {
        if (!it->second.isApplied()) {
            // std::cout << "u" << registerN << "    Applying scatter-gather modifier to dim " << dimN << std::endl;
            it->second.modDimension(dimensions.at(dimN), elementWidth);
        }
    }
}

template <typename T>
void streamRegister_t<T>::setDynamicModsNotApplied(size_t dimN) {
    auto currentModifierIters = dynamicModifiers.equal_range(dimN);
    for (auto it = currentModifierIters.first; it != currentModifierIters.second; ++it)
        it->second.setApplied(false);
}

template <typename T>
void streamRegister_t<T>::setSGModsNotApplied(size_t dimN) {
    auto currentModifierIters = scatterGModifiers.equal_range(dimN);
    for (auto it = currentModifierIters.first; it != currentModifierIters.second; ++it)
        it->second.setApplied(false);
}

template <typename T>
void streamRegister_t<T>::updateIteration() {
    // std::cout << "u" << registerN << ": Updating iteration STARTED." << std::endl;

    if (isStreamDone()) {
        finishStream();
        /*if (registerN == 3)
            std::cout << "u" << registerN << " Stream is done HERE ITER" << std::endl;*/
        return;
    }

    /* Iteration starts from the innermost dimension and updates the next if the current reaches an overflow */
    // std::cout << "Updating iteration. Dimension " << dimensions.size() << std::endl;
    bool validIter;
    /*if (registerN == 3)
        std::cout << "Updating iteration. Dimension " << dimensions.size() << std::endl;*/
    validIter = dimensions.back().advance();
    /*if (registerN == 3)
        std::cout << "u" << registerN << " ValidIter: " << validIter << std::endl;*/

    for (int i = dimensions.size() - 1; i > 0; --i) {
        auto &currDim = dimensions.at(i);
        /* The following calculations are only necessary if we ARE in the
        last iteration of a dimension */

        if (currDim.isEndOfDimension()) {

            // std::cout << "u" << registerN << " EOD: dim " << i + 1 << " ended" << std::endl;

            // If modifiers exist, the values at targetted dimensions might have been modified.
            // As such, we need to reset them before next iteration.
            if (validIter) {
                auto currentModifierIters = staticModifiers.equal_range(i);
                for (auto it = currentModifierIters.first; it != currentModifierIters.second; ++it) {
                    int target = it->second.getTargetDim();
                    // std::cout << "dim " << i + 1 << ": Resetting dim " << target + 1 << std::endl;
                    dimensions.at(target).resetIterValues();
                }
            }
            // If dynamic modifiers exist, we also need to apply them before the first iteration of the target dimension.
            /*auto currentDynaminModifierIters = dynamicModifiers.equal_range(i);
            for (auto it = currentDynaminModifierIters.first; it != currentDynaminModifierIters.second; ++it) {
                int target = it->second.getTargetDim();
                dimensions.at(target).resetIterValues();
                if (registerN == 3) {
                    std::cout << "u" << registerN << "   Dim "<< i+1 << " ended: Applying dynamic modifier to dim " << target << std::endl;
                }
                it->second.modDimension(dimensions, elementWidth);
            }*/

            // Reset EOD flag of current dimension if iteration was successful
            currDim.setEndOfDimension(false);

            auto &nextDim = dimensions.at(i - 1);

            // Unflag dynamic modifiers of current dimension
            // setDynamicModsNotApplied(i);

            // Iterate upper dimension
            validIter = nextDim.advance();
            /*if (registerN == 3)
                std::cout << "u" << registerN << " ValidIter: " << validIter << std::endl;*/

            // Unflag scatter dynamic modifiers of upper dimension
            // setSGModsNotApplied(i+1);

            if (validIter) {
                // Apply static modifiers associated with upper dimension to target dimensions
                auto upperModifierIters = staticModifiers.equal_range(i - 1);
                for (auto it = upperModifierIters.first; it != upperModifierIters.second; ++it) {
                    // std::cout << "dim " << i << ": Applying static modifier to dim " << it->second.getTargetDim() + 1 << std::endl;
                    it->second.modDimension(dimensions, elementWidth);
                }

                // Apply dynamic modifiers associated with upper dimension to target dimensions
                auto upperDynamicModifierIters = dynamicModifiers.equal_range(i - 1);
                for (auto it = upperDynamicModifierIters.first; it != upperDynamicModifierIters.second; ++it) {
                    int target = it->second.getTargetDim();
                    /*if (registerN == 3) {
                        std::cout << "u" << registerN << ": Applying dynamic modifier to dim " << target << std::endl;
                    }*/
                    if (nextDim.isLastIteration())
                        nextDim.resetIterValues();
                    it->second.modDimension(dimensions, elementWidth);
                }
            }
        }
    }

    // std::cout << "u" << registerN << ": Updating iteration ENDED." << std::endl;
}

template <typename T>
void streamRegister_t<T>::updateAsLoad() {
    assert_msg("Trying to update as load a non-load stream", type == RegisterConfig::Load || type == RegisterConfig::IndSource);
    if (isStreamDone()) { // doesn't try to load if stream has finished
        finishStream();
        /*if (registerN == 3)
            std::cout << "u" << registerN << " Stream is done HERE LOAD" << std::endl;*/
        return;
    }
    // elements.clear();
    // elements.reserve(vLen);

    size_t eCount = 0;
    validElements = 0; // reset valid index

    size_t offset;

    size_t max = mode == RegisterMode::Vector ? vLen : 1;

    // std::cout << "u" << registerN << ": Loading values." << std::endl;
    int g = 0;
    //do {
        //std::cout << "u" << registerN << ": Loading values. > " << g++ << std::endl;
        while (eCount < max && tryGenerateAddress(offset)) {
            auto value = [this](auto address) -> ElementsType {
                if constexpr (std::is_same_v<ElementsType, std::uint8_t>)
                    return readAS<ElementsType>(gMMU(su->p).template load<std::uint8_t>(address));
                else if constexpr (std::is_same_v<ElementsType, std::uint16_t>)
                    return readAS<ElementsType>(gMMU(su->p).template load<std::uint16_t>(address));
                else if constexpr (std::is_same_v<ElementsType, std::uint32_t>)
                    return readAS<ElementsType>(gMMU(su->p).template load<std::uint32_t>(address));
                else
                    return readAS<ElementsType>(gMMU(su->p).template load<std::uint64_t>(address));
            }(offset);

            // elements.push_back(value);
            /*if (registerN == 3 || registerN == 4)
                std::cout << "u" << registerN << "   Loaded Value: " << readAS<double>(value) << std::endl;*/
            elements.at(eCount) = value;
            /*if (registerN==3)
                std::cout << "u" << registerN << "    Loaded Value: " << readAS<double>(value) << std::endl;*/
            ++validElements;
            for (size_t i = 0; i < dimensions.size(); i++)
                setSGModsNotApplied(i);
            if (tryGenerateAddress(offset) && ++eCount < max) {
                updateIteration(); // reset EOD flags and iterate stream
            } else {
                break;
            }
        }
        su->updateEODTable(registerN); // save current state of the stream so that branches can catch EOD flags
        // std::cout << "eCount: " << eCount << std::endl;
        // std::cout << "vLen: " << vLen << std::endl;
        // if (eCount < max) {    // iteration is already updated when register is full
        updateIteration(); // reset EOD flags and iterate stream
        /*for (size_t i = 0; i < dimensions.size() - 1; i++)
            setDynamicModsNotApplied(i, true);*/
        /*for (size_t i = 0; i < dimensions.size(); i++)
            setSGModsNotApplied(i);*/
        //}
    //} while (validElements == 0 /*&& tryGenerateAddress(offset)*/);
}

template <typename T>
void streamRegister_t<T>::updateAsStore() {
    assert_msg("Trying to update as store a non-store stream", type == RegisterConfig::Store);
    // std::cout << "Updating as store" << std::endl;
    if (isStreamDone()) {
        finishStream();
        return;
    }
    // std::cout << "Storing " << elements.size() << " elements. eCount=" << vLen << std::endl;
    size_t offset;
    size_t eCount = 0;

    /*
    std::cout << "Storing " << validElements << " elements." << std::endl;
    // print vecCfg
    printRegN("\nvecCfg: ");
    for (auto &v : vecCfg)
        std::cout << v << " ";
    std::cout << std::endl;
    */
    //do {
        while (eCount < validElements && tryGenerateAddress(offset)) {
            // auto value = elements.front();
            // elements.erase(elements.begin());
            // elements.pop_front(); //-- std::array
            auto value = elements.at(eCount);
            // std::cout << "\nStored Values: " << readAS<double>(value) << " ";
            if constexpr (std::is_same_v<ElementsType, std::uint8_t>)
                gMMU(su->p).template store<std::uint8_t>(offset, readAS<ElementsType>(value));
            else if constexpr (std::is_same_v<ElementsType, std::uint16_t>)
                gMMU(su->p).template store<std::uint16_t>(offset, readAS<ElementsType>(value));
            else if constexpr (std::is_same_v<ElementsType, std::uint32_t>)
                gMMU(su->p).template store<std::uint32_t>(offset, readAS<ElementsType>(value));
            else
                gMMU(su->p).template store<std::uint64_t>(offset, readAS<ElementsType>(value));
            for (size_t i = 0; i < dimensions.size(); i++)
                setSGModsNotApplied(i);
            ++eCount;
            if (tryGenerateAddress(offset) && eCount < validElements) {
                updateIteration(); // reset EOD flags and iterate stream
                //++eCount;
            } else
                break;
        }
        // std::cout << std::endl;
        // std::cout << "UPDATING EODTABLE" <<std::endl;
        su->updateEODTable(registerN); // save current state of the stream so that branches can catch EOD flags
                                       // if (eCount < validElements)       // iteration is already updated when register is full
        updateIteration();             // reset EOD flags and iterate stream
        // elements.clear();
    //} while (eCount == 0 /*&& tryGenerateAddress(offset)*/);
}

std::vector<uint8_t> predRegister_t::getPredicate() const {
    return elements;
}

PredicateMode predRegister_t::getPredMode() const {
    return predMode;
}

void predRegister_t::setPredMode(const PredicateMode pm) {
    predMode = pm;
}

void streamingUnit_t::updateEODTable(const size_t stream) {
    int r = 0, d = 0;
    std::visit([&](const auto reg) {
        int d = 0;
        for (const auto dim : reg.dimensions) {
            EODTable.at(stream).at(d) = dim.isEndOfDimension();
            ++d;
        }
    },
               registers.at(stream));
}

template <typename T>
void streamingUnit_t::makeStreamRegister(size_t streamRegister, RegisterConfig type, PredicateMode pm) {
    assert_msg("Tried to use a register index higher than the available registers", streamRegister < registerCount);
    if constexpr (std::is_same_v<T, std::uint8_t>) {
        registers.at(streamRegister) = StreamReg8{this, pm, type, streamRegister};
    } else if constexpr (std::is_same_v<T, std::uint16_t>) {
        registers.at(streamRegister) = StreamReg16{this, pm, type, streamRegister};
    } else if constexpr (std::is_same_v<T, std::uint32_t>) {
        registers.at(streamRegister) = StreamReg32{this, pm, type, streamRegister};
    } else if constexpr (std::is_same_v<T, std::uint64_t>) {
        registers.at(streamRegister) = StreamReg64{this, pm, type, streamRegister};
    } else {
        static_assert(always_false_v<T>, "Cannot create register with this element width");
    }
}

void streamingUnit_t::makePredRegister(std::vector<uint8_t> elements, size_t predRegister, PredicateMode pm) {
    assert_msg("Tried to alter p0 register, which is hardwired to 1", predRegister);
    assert_msg("Tried to use a predicate register index higher than the available predicate registers", predRegister < predRegCount);
    assert_msg("Tried to create predicate with invalid size", elements.size() == predicates.at(predRegister).vLen);
    for (auto &p : elements)
        assert_msg("Invalid values for predicate (must be 0 or 1)", !p || p == 1);
    predicates.at(predRegister) = predRegister_t{elements, pm};
}

template class streamRegister_t<uint8_t>;
template class streamRegister_t<uint16_t>;
template class streamRegister_t<uint32_t>;
template class streamRegister_t<uint64_t>;
template void streamingUnit_t::makeStreamRegister<uint8_t>(size_t streamRegister, RegisterConfig type, PredicateMode pm);
template void streamingUnit_t::makeStreamRegister<uint16_t>(size_t streamRegister, RegisterConfig type, PredicateMode pm);
template void streamingUnit_t::makeStreamRegister<uint32_t>(size_t streamRegister, RegisterConfig type, PredicateMode pm);
template void streamingUnit_t::makeStreamRegister<uint64_t>(size_t streamRegister, RegisterConfig type, PredicateMode pm);