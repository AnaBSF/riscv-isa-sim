#ifndef STREAMING_UNIT_HPP
#define STREAMING_UNIT_HPP

#include "descriptors.h"
#include "helpers.h"

/* Necessary for using MMU */
class processor_t;
class streamingUnit_t;

// extern processor_t *globalProcessor;
#define gMMU(p) (*(p->get_mmu()))

enum class RegisterConfig { NoStream,
                            Load,
                            IndSource,
                            Store };
enum class RegisterStatus { NotConfigured,
                            Running,
                            Finished };
enum class RegisterMode { Vector,
                          Scalar };

enum class PredicateMode { Zeroing,
                           Merging };   

/* --- Streaming Registers --- */

/* T is one of std::uint8_t, std::uint16_t, std::uint32_t or std::uint64_t and
  represents the type of the elements of a stream at a given moment. It is the
  type configured to store a value at a given time */
template <typename T>
struct streamRegister_t {
    using ElementsType = T;
    /* The gem5 implementation was made with 64 bytes, so this value mirrors it.
    It can be changed to another value that is a power of 2, with atleast 8 bytes
    to support at the 64-bit operations */
    static constexpr size_t registerLength = 64; // in Bytes
    //static constexpr size_t registerLength = 16; // in Bytes
    /* During computations, we test if two streams have the same element width
    using this property */
    static constexpr size_t elementWidth = sizeof(ElementsType);
    /* This property limits how many elements can be manipulated during a
    computation and also how many can be loaded/stored at a time */
    static constexpr size_t vLen = registerLength / elementWidth;

    /* In this implementation, the concept of a stream and register are heavily
    intertwined. As such, stream attributes, such as dimensions, modifiers, EOD flags,
    status and configuration are associated with the registers holding each stream.
    In real hardware, they would be independent, with streams being merely associated
    with a certain register and values being first loaded to a buffer in the
    Streaming Engine and then loaded to the correspondent register.
    Because here the stream load and store operations are synchronous to the
    rest of the execution, such is not necessary and a stream is loaded directly
    to the register that holds all its information.*/
    /* Last dimension cannot have a modifier */
    // static constexpr size_t maxDimensions = 8;
    // static constexpr size_t maxModifiers = su->maxDimensions - 1;

    /* FOR DEBUGGING */
    size_t registerN;

    streamRegister_t(streamingUnit_t *su = nullptr, PredicateMode pm = PredicateMode::Zeroing, RegisterConfig t = RegisterConfig::NoStream, size_t regN = -1) :
     registerN(regN), su(su), predMode(pm), type(t) {
        status = RegisterStatus::NotConfigured;
        mode = RegisterMode::Vector;
        validElements = 0;
        vecCfgDim = 0;
        baseAddress = 0;
    }

    void addStaticModifier(staticModifier_t mod);
    void addDynamicModifier(dynamicModifier_t mod);
    void addScatterGModifier(scatterGModifier_t mod);
    void addDimension(dimension_t dim);
    void configureVecDim(const int = -1);
    void startConfiguration(size_t base_address);
    void endConfiguration();
    void finishStream();
    std::vector<ElementsType> getElements(bool causesUpdate = true);
    bool getDynModElement(int &value);
    void setElements(std::vector<ElementsType> e, bool causesUpdate = true);
    void setValidIndex(const size_t i);
    void setMode(const RegisterMode m);
    void setPredMode(const PredicateMode pm);
    bool hasStreamFinished() const;
    //void clearEndOfDimensionOfDim(size_t i);
    bool isEndOfDimensionOfDim(size_t i) const;

    size_t getElementWidth() const;
    size_t getVLen() const;
    size_t getValidElements() const;
    size_t getRegisterLength() const;
    RegisterConfig getType() const;
    RegisterStatus getStatus() const;
    RegisterMode getMode() const;
    PredicateMode getPredMode() const;

    /* FOR DEBUGGING*/
    void printRegN(char *str = "");

    friend class streamingUnit_t;

private:
    streamingUnit_t *su;
    std::vector<ElementsType> elements = std::vector<ElementsType>(vLen);
    size_t validElements;
    /* Same ordeal as above. Although the amount of dimensions is capped, we can avoid
    indexing by just calling the size method */
    std::deque<dimension_t> dimensions;
    /* Modifiers are different in that they don't have to scale linearly in a stream
    configuration. As such, it is better to have a container that maps a dimension's
    index to its modifier. When updating stream the iterators, we can test if a dimension
    for the given index exists before the calculations */
    std::unordered_multimap<int, staticModifier_t> staticModifiers;
    std::unordered_multimap<int, dynamicModifier_t> dynamicModifiers;
    std::unordered_multimap<int, scatterGModifier_t> scatterGModifiers;
    PredicateMode predMode;
    RegisterConfig type;
    RegisterStatus status;
    RegisterMode mode;

    int vecCfgDim;

    size_t baseAddress;

    size_t generateAddress();
    bool isDimensionFullyDone(const std::deque<dimension_t>::const_iterator start, const std::deque<dimension_t>::const_iterator end) const;
    bool isStreamDone() const;
    bool tryGenerateAddress(size_t &address);
    void applySGMods(size_t dimN);
    void setDynamicModsNotApplied(size_t dimN);
    void setSGModsNotApplied(size_t dimN);
    void updateIteration();
    void updateAsLoad();
    void updateAsStore();
};

/* --- Predicate Registers --- */

struct predRegister_t {
    static constexpr size_t registerLength = 64; // in Bytes
    static constexpr size_t elementWidth = sizeof(uint8_t);
    static constexpr size_t vLen = registerLength / elementWidth;

    predRegister_t(std::vector<uint8_t> e = std::vector<uint8_t>(vLen), PredicateMode pm = PredicateMode::Merging) {
        elements = e;
        predMode = pm;
    }

    std::vector<uint8_t> getPredicate() const;

    PredicateMode getPredMode() const;
    void setPredMode(const PredicateMode pm);

private:
    std::vector<uint8_t> elements = std::vector<uint8_t>(vLen);

    PredicateMode predMode;

    friend class streamingUnit_t;
};

/* --- Streaming Unit --- */

using StreamReg8 = streamRegister_t<std::uint8_t>;
using StreamReg16 = streamRegister_t<std::uint16_t>;
using StreamReg32 = streamRegister_t<std::uint32_t>;
using StreamReg64 = streamRegister_t<std::uint64_t>;

struct streamingUnit_t {
    processor_t *p = nullptr; // updated by the processor in processor.cc
    /* UVE specification is to have 32 streaming/vectorial registers */
    static constexpr size_t registerCount = 32;
    static constexpr size_t predRegCount = 16;
    static constexpr size_t maxDimensions = 8;
    /* There are 2 types at play when implementing the UVE specification. A storage
    type, which is how values get stored, and a computation type, the type a value
    should have when doing computations. Using a variant allows us to have almost
    full type-safety when storing/retriving values, including how many elements can
    be contained in a register at a given moment. During computations, we might need
    a raw cast to a signed or floating-point value.  */
    using RegisterType = std::variant<StreamReg8, StreamReg16, StreamReg32, StreamReg64>;

    std::array<std::array<bool, maxDimensions>, registerCount> EODTable;

    std::array<RegisterType, registerCount> registers;
    std::array<predRegister_t, predRegCount> predicates;

    streamingUnit_t() {
        predicates.at(0).elements = std::vector<uint8_t>(predicates.at(0).vLen, 1);
    }

    template <typename T>
    void makeStreamRegister(size_t streamRegister, RegisterConfig type = RegisterConfig::NoStream, PredicateMode pm = PredicateMode::Zeroing);

    void makePredRegister(std::vector<uint8_t> elements, size_t predRegister, PredicateMode pm = PredicateMode::Merging);

    void updateEODTable(const size_t stream);

    template <typename Operation>
    auto operateRegister(size_t streamRegister, Operation &&op) {
        assert_msg("Tried to use a register index higher than the available registers.", streamRegister < registerCount);
        return std::visit([op = std::move(op)](auto &reg) { return op(reg); }, registers.at(streamRegister));
    }

    template <typename Operation>
    auto operatePredReg(size_t predRegister, Operation &&op) {
        assert_msg("Tried to use a predicate register index higher than the available predicate registers.", predRegister < predRegCount);
        return std::visit([op = std::move(op)](auto &reg) { return op(reg); }, predicates.at(predRegister));
    }
};

#endif // STREAMING_UNIT_HPP
