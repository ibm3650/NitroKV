/**
 * @file results.hpp
 * @date 20.04.2026
 * @author nikita
 */
#pragma once
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace nitrokv::core {

using BytesViewT = std::span<const std::byte>;


enum class CoreStatusCode : uint8_t { NO_ERRORS,OUT_OF_MEMORY, KEY_NOT_FOUND, TTL_NOT_SET };


struct SetResult {};


struct GetResult {
    std::optional<BytesViewT> value;
};

struct DelResult {
    size_t deleted_cnt;
};

struct PingResult {};

// struct ErrorResult {
//     std::string error;
// };

struct ExpireResult {
    bool is_set;
};

struct TtlResult {
    enum class TTLState : uint8_t {
        KEY_NOT_EXISTS,
        TTL_NOT_SET,
        SUCCESS,
        UNKNOWN_ERROR
    };
    TTLState ttl_state;
    std::chrono::seconds ttl;
};

struct ExistsResult {
    bool exists;
};


} // namespace nitrokv::core
