//
// Created by nikita on 22.03.2026.
//

#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

namespace nitrokv::protocol {
inline constexpr std::array SEPARATOR{'\r', '\n'};

inline constexpr size_t PREFIX_LENGTH = 1U;

enum class RespTypePrefix : char {
    INTEGER = ':',
    SIMPLE_STRING = '+',
    BULK_STRING = '$',
    ERROR = '-',
    ARRAY = '*',
};

static_assert(sizeof(RespTypePrefix) == PREFIX_LENGTH, "RespTypePrefix size mismatch!");

struct RespEncNullArray {};

struct RespEncNullBulkString {};

using RespEncInteger = int64_t;

struct RespEncSimpleString { // NOLINT(altera-struct-pack-align)
    std::string_view value;
};

struct RespEncError { // NOLINT(altera-struct-pack-align)
    std::string_view value;
};

struct RespEncBulkString { // NOLINT(altera-struct-pack-align)
    std::span<const std::byte> value;
};

struct RespEncValue;

using RespEncArray = std::vector<RespEncValue>;

struct RespEncValue { // NOLINT(altera-struct-pack-align)
    std::variant<RespEncInteger,
                 RespEncSimpleString,
                 RespEncBulkString,
                 RespEncNullArray,
                 RespEncNullBulkString,
                 RespEncArray,
                 RespEncError>
        value;
};


}; // namespace nitrokv::protocol
