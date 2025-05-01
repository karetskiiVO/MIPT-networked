#pragma once
#include "mathUtils.h"
#include <inttypes.h>
#include <limits>

template <typename PackedType>
PackedType packFloat (float val, float min, float max, uint8_t bits) {
    PackedType range = (1 << bits) - 1;
    return range * ((clamp(val, min, max) - min) / (max - min));
}

template <typename PackedType>
float unpackFloat (PackedType packed, float min, float max, uint8_t bits) {
    PackedType range = (1 << bits) - 1;
    return float(packed) / range * (max - min) + min;
}

template <typename PackedType, uint8_t bits>
struct PackedFloat {
    PackedType packedVal;

    PackedFloat (float val, float min, float max) {
        pack(val, min, max);
    }

    PackedFloat(PackedType compressed_val) : packedVal(compressed_val) {}

    void pack (float val, float min, float max) { 
        packedVal = packFloat<PackedType>(val, min, max, bits);
    }

    float unpack (float min, float max) {
        return unpackFloat<PackedType>(packedVal, min, max, bits);
    }
};

using float4bitsQuantized = PackedFloat<uint8_t, 4>;
