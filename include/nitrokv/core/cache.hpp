/**
* @file cache.hpp
 * @date 21.04.2026
 * @author nikita
 */
#pragma once
#include <cstddef>
#include <optional>
#include <span>

#include "results.hpp"
#include "nitrokv/core/wheel.hpp"

namespace nitrokv::core {

inline constexpr size_t LRU_SLOTS_MAX{64};


using BytesT = std::vector<std::byte>;


struct Entry {
    BytesT value;
    std::list<Entry*>::iterator lru_it;
    const BytesT* key;
    std::optional<TimingWheel<Entry*>::TimingWheelPosition> tw_position;
    std::optional<std::chrono::steady_clock::time_point> expire_at;
};

void cache_tick_expiration();

CoreStatusCode cache_push(BytesViewT key, BytesViewT value) ;

bool cache_pop(BytesViewT key) ;

bool cache_set_ttl(BytesViewT key, std::chrono::seconds ttl) noexcept;

[[nodiscard]] bool cache_contains(BytesViewT key) ;

[[nodiscard]] std::optional<BytesViewT> cache_get(BytesViewT key) noexcept;

[[nodiscard]] std::expected<std::chrono::seconds, CoreStatusCode>
    cache_get_ttl(BytesViewT key) ;

} // namespace nitrokv::core
