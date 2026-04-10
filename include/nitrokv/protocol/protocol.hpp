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
inline constexpr std::array RESP_SEPARATOR{'\r', '\n'};

inline constexpr size_t RESP_PREFIX_LENGTH = 1U;
inline constexpr size_t RESP_SEPARATOR_LENGTH = 2U;

enum class RespTypePrefix : char {
    INTEGER = ':',
    SIMPLE_STRING = '+',
    BULK_STRING = '$',
    ERROR = '-',
    ARRAY = '*',
};

static_assert(sizeof(RespTypePrefix) == RESP_PREFIX_LENGTH, "RespTypePrefix size mismatch!");

struct RespValue;

struct RespNullArray {};

struct RespNullBulkString {};

using RespInteger = int64_t;

using RespArray = std::vector<RespValue>;

struct RespSimpleString { // NOLINT(altera-struct-pack-align)
    std::string_view value;
};

struct RespError { // NOLINT(altera-struct-pack-align)
    std::string_view value;
};

struct RespBulkString { // NOLINT(altera-struct-pack-align)
    std::span<const std::byte> value;
};

struct RespValue { // NOLINT(altera-struct-pack-align)
    std::variant<std::monostate,
                 RespInteger,
                 RespSimpleString,
                 RespBulkString,
                 RespNullArray,
                 RespNullBulkString,
                 RespArray,
                 RespError>
        value;
};


}; // namespace nitrokv::protocol
