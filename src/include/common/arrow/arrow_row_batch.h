#pragma once

#include <array>
#include <vector>

#include "common/arrow/arrow.h"
#include "common/arrow/arrow_buffer.h"
#include "common/types/types.h"
#include "main/query_result.h"

struct ArrowSchema;

namespace kuzu {
namespace common {

// An Arrow Vector(i.e., Array) is defined by a few pieces of metadata and data:
//  1) a logical data type;
//  2) a sequence of buffers: validity bitmaps, data buffer, overflow(optional), children(optional).
//  3) a length as a 64-bit signed integer;
//  4) a null count as a 64-bit signed integer;
//  5) an optional dictionary for dictionary-encoded arrays.
// See https://arrow.apache.org/docs/format/Columnar.html for more details.

static inline std::int64_t getNumBytesForBits(std::int64_t numBits) {
    return (numBits + 7) / 8;
}

struct ArrowVector {
    ArrowBuffer data;
    ArrowBuffer validity;
    ArrowBuffer overflow;

    std::int64_t numValues = 0;
    std::int64_t numNulls = 0;

    std::vector<std::unique_ptr<ArrowVector>> childData;

    // The arrow array C API data, only set after Finalize
    std::unique_ptr<ArrowArray> array;
    std::array<const void*, 3> buffers = {{nullptr, nullptr, nullptr}};
    std::vector<ArrowArray*> childPointers;
};

// An arrow data chunk consisting of N rows in columnar format.
class ArrowRowBatch {
public:
    ArrowRowBatch(std::vector<DataType> types, std::int64_t capacity);

    //! Append a data chunk to the underlying arrow array
    ArrowArray append(main::QueryResult& queryResult, std::int64_t chunkSize);

private:
    static std::unique_ptr<ArrowVector> createVector(const DataType& type, std::int64_t capacity);
    static void appendValue(ArrowVector* vector, Value* value);

    static ArrowArray* convertVectorToArray(const DataType& type, ArrowVector& vector);
    static inline void initializeNullBits(ArrowBuffer& validity, std::int64_t capacity) {
        auto numBytesForValidity = getNumBytesForBits(capacity);
        validity.resize(numBytesForValidity, 0xFF);
    }
    static void copyNonNullValue(ArrowVector* vector, Value* value, std::int64_t pos);
    static void copyNullValue(ArrowVector* vector, Value* value, std::int64_t pos);

    template<typename T>
    static void templateInitializeVector(ArrowVector* vector, std::int64_t capacity);
    template<typename T>
    static void templateCopyNonNullValue(ArrowVector* vector, Value* value, std::int64_t pos);
    template<typename T>
    static void templateCopyNullValue(ArrowVector* vector, Value* value, std::int64_t pos);
    template<typename T>
    static ArrowArray* templateCreateArray(ArrowVector& vector);

    ArrowArray toArray();

private:
    std::vector<DataType> types;
    std::vector<std::unique_ptr<ArrowVector>> vectors;
    std::int64_t numTuples;
};

} // namespace common
} // namespace kuzu
