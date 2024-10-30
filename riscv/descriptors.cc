#include "descriptors.h"
#include "streaming_unit.h"
#include <iostream>
#include <limits.h>

/* Start of dimension_t function definitions */

void dimension_t::resetIterValues() {
    iter_offset = offset;
    iter_size = size;
    iter_stride = stride;
    endOfDimension = iter_size == 0;

    // std::cout << "RESET >>> iter_size: " << iter_size << std::endl;
}

bool dimension_t::isEmpty() const {
    return iter_size == 0;
}

bool dimension_t::advance() {
    if (iter_size == 0) {
        //std::cout << "CANNOT ADVANCE >>> size: " << iter_size << ", idx: " << iter_index << std::endl;
        return false;
    }
    ++iter_index;
    if (iter_index >= iter_size){
        //std::cout << "EOD MARKED\n";
        setEndOfDimension(true);
    }
    //std::cout << "ADVANCE >>> size: " << iter_size << ", idx: " << iter_index << std::endl;
    return true;
}

bool dimension_t::isLastIteration() const {
    return iter_index + 1 >= iter_size;
}

bool dimension_t::isEndOfDimension() const {
    return endOfDimension;
}

void dimension_t::setEndOfDimension(bool b) {
    // std::cout << "Setting end of dimension to: " << b << std::endl;
    endOfDimension = b;
    if (!b)
        iter_index = 0;
}

size_t dimension_t::calcAddress(size_t width) const {
    //std::cout << "...Calculating address: siz: " << iter_size  << ", idx: " << iter_index << std::endl;
    return iter_offset + iter_stride * iter_index * width;
}

size_t dimension_t::getSize() const {
    return iter_size;
}

/* Start of modifier_t function definitions */

void staticModifier_t::modDimension(std::deque<dimension_t> &dims, const size_t elementWidth) {
    int valueChange = behaviour == staticBehaviour::Increment ? displacement : -1 * displacement;
    dimension_t &dim = dims.at(targetDim);

    if (target == Target::Offset) {
        dim.iter_offset += valueChange * elementWidth;
        if (dim.iter_offset < 0)
            dim.iter_offset = 0;
        // std::cout << "iter_offset: " << dim.iter_offset << std::endl;
    } else if (target == Target::Size) {
        dim.iter_size += valueChange;
        if (dim.iter_size <= 0) {
            dim.iter_size = 0;
            //std::cout << "MOD >>> iter_size EOD MARKED: " << dim.iter_size << std::endl;
            dim.setEndOfDimension(true);
        }
        //std::cout << "MOD >>> iter_size: " << dim.iter_size << std::endl;
        if (dim.iter_size) {
            dim.setEndOfDimension(false); 
            // std::cout << "REMOVING EOD\n";
        }
    } else if (target == Target::Stride) {
        dim.iter_stride += valueChange;
        // std::cout << "iter_stride: " << dim.iter_stride << std::endl;
    } else {
        assert_msg("Unexpected target for a static modifier", false);
    }
}

void dynamicModifier_t::calculateValueChange(auto &target, auto baseValue, dynamicBehaviour behaviour, int valueChange) {
    switch (behaviour) {
    case dynamicBehaviour::Add:
        target = baseValue + valueChange;
        break;
    case dynamicBehaviour::Subtract:
        target = baseValue - valueChange;
        break;
    case dynamicBehaviour::Set:
        target = valueChange;
        break;
    case dynamicBehaviour::Increment:
        target += valueChange;
        break;
    case dynamicBehaviour::Decrement:
        target -= valueChange;
        break;
    default:
        assert_msg("Unexpected behaviour for a dynamic modifier", false);
    }
}

void dynamicModifier_t::getIndirectRegisterValues() {
    auto &src = (su->registers).at(sourceStream);
    std::visit(overloaded{
                   [&](auto &reg) {
                       sourceEnd = !reg.getDynModElement(indirectRegisterValue);
                       // std::cout << "u" << reg.registerN << "   Getting dyn el\n";
                   }},
               src);
    /*if (sourceEnd)
        std::cout << "sourceEnd\n";*/
    // print value
    // std::cout << "indirectRegisterValue: " << indirectRegisterValue << std::endl;
}

void dynamicModifier_t::modDimension(std::deque<dimension_t> &dims, const size_t elementWidth) {
    // size_t valueChange = behaviour == Behaviour::Increment ? displacement : -1*displacement;
    dimension_t &dim = dims.at(targetDim);
    if (!sourceEnd) {
        getIndirectRegisterValues();

        if (target == Target::Offset) {
            calculateValueChange(dim.iter_offset, dim.offset, behaviour, indirectRegisterValue * elementWidth);
            // dim.setEndOfDimension(false);
            // std::cout << "dyn offset: " << indirectRegisterValue << std::endl;
        } else if (target == Target::Size) {
            calculateValueChange(dim.iter_size, dim.size, behaviour, indirectRegisterValue);
            if (dim.iter_size) {
                dim.setEndOfDimension(false);
                // std::cout << "REMOVING EOD\n";
            }
            // std::cout << "iter_size: " << dim.iter_size << std::endl;
        } else if (target == Target::Stride) {
            calculateValueChange(dim.iter_stride, dim.stride, behaviour, indirectRegisterValue);
            // std::cout << "iter_stride: " << dim.iter_stride << std::endl;
        } else {
            assert_msg("Unexpected target for a dynamic modifier", false);
        }

        modApplied = true;

    } else {
        dim.setEndOfDimension(true);
        sourceEnd = false;
        // std::cout << "ACABOU\n";
    }
}

void scatterGModifier_t::calculateValueChange(auto &target, auto baseValue, dynamicBehaviour behaviour, int valueChange) {
    switch (behaviour) {
    case dynamicBehaviour::Add:
        target = baseValue + valueChange;
        break;
    case dynamicBehaviour::Subtract:
        target = baseValue - valueChange;
        break;
    case dynamicBehaviour::Set:
        target = valueChange;
        break;
    case dynamicBehaviour::Increment:
        target += valueChange;
        break;
    case dynamicBehaviour::Decrement:
        target -= valueChange;
        break;
    default:
        assert_msg("Unexpected behaviour for a scatter-gather modifier", false);
    }
}

void scatterGModifier_t::getIndirectRegisterValues() {
    auto &src = (su->registers).at(sourceStream);
    std::visit(overloaded{
                   [&](auto &reg) {
                       sourceEnd = !reg.getDynModElement(indirectRegisterValue);
                       // std::cout << "u" << reg.registerN << "   Getting dyn el\n";
                   }},
               src);

    // print value
    // std::cout << "sgRegisterValue: " << indirectRegisterValue << std::endl;
}

void scatterGModifier_t::modDimension(dimension_t &dim, const size_t elementWidth) {
    // size_t valueChange = behaviour == Behaviour::Increment ? displacement : -1*displacement;
    if (!sourceEnd) {
        getIndirectRegisterValues();

        calculateValueChange(dim.iter_offset, dim.offset, behaviour, indirectRegisterValue * elementWidth);
        // dim.setEndOfDimension(false);

        /*if (behaviour != dynamicBehaviour::Increment && behaviour != dynamicBehaviour::Decrement)
            dim.iter_size = UINT_MAX;*/
        /* print dimension
        std::cout << "SG dimension_t: ";
        std::cout << "offset: " << dim.iter_offset << std::endl;
        std::cout << "size: " << dim.iter_size << ", ";
        std::cout << "stride: " << dim.iter_stride << ", ";
        std::cout << "EOD: " << (int)dim.endOfDimension << std::endl;
        std::cout << "iter_offset: " << dim.iter_offset << std::endl;*/

        modApplied = true;

    } else {
        dim.setEndOfDimension(true);
        // sourceEnd = false;
        // std::cout << "ACABOU sg\n";
    }
}

/*void modifier_t::printModifier() const {
    // print modifier
    std::cout << "modifier_t: ";
    switch (type) {
    case Type::Static:
        std::cout << "Static";
        break;
    case Type::Indirect:
        std::cout << "Indirect";
        break;
    default:
        assert_msg("Unhandled Type case in modifiers's printModifier",
                   false);
    }
    std::cout << ", ";
    switch (target) {
    case Target::None:
        std::cout << "None";
        break;
    case Target::Offset:
        std::cout << "Offset";
        break;
    case Target::Size:
        std::cout << "Size";
        break;
    case Target::Stride:
        std::cout << "Stride";
        break;
    default:
        assert_msg("Unhandled Target case in modifiers's printModifier",
                   false);
    }
    std::cout << ", ";
    switch (behaviour) {
    case Behaviour::None:
        std::cout << "None";
        break;
    case Behaviour::Increment:
        std::cout << "Increment";
        break;
    case Behaviour::Decrement:
        std::cout << "Decrement";
        break;
    default:
        assert_msg("Unhandled Behaviour case in modifiers's printModifier",
                   false);
    }
    std::cout << ", displacement: " << displacement << ", size: " << size;
}*/