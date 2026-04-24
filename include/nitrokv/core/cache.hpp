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


namespace nitrokv::core {


[[nodiscard]] std::optional<BytesViewT> cache_get(BytesViewT key) noexcept;

CoreStatusCode cache_push(BytesViewT key, BytesViewT value) ;

bool cache_pop(BytesViewT key) ;

bool cache_set_ttl(BytesViewT key, std::chrono::seconds ttl) noexcept;

[[nodiscard]] bool cache_contains(BytesViewT /*key*/) ;

[[nodiscard]] std::expected<std::chrono::seconds, CoreStatusCode>
    cache_get_ttl(BytesViewT /*key*/) ;

} // namespace nitrokv::core
