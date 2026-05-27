
/**
 * @file core.cpp
 * @date 22.04.2026
 * @author nikita
 */
#include <expected>
#include <list>
#include <mutex>
#include <unordered_map>
#include <utility>

#include "nitrokv/core/cache.hpp"
#include "nitrokv/core/results.hpp"


namespace {

std::list<nitrokv::core::Entry*> lru;
struct FNVHasher {
    std::size_t operator()(const nitrokv::core::BytesT& bytes) const noexcept {
        size_t hval = 1469598103934665603ULL;

        for (const std::byte b : bytes) {
            hval ^= static_cast<size_t>(std::to_integer<unsigned char>(b));
            hval *= 1099511628211ULL;
        }

        return hval;
    }
};

std::unordered_map<nitrokv::core::BytesT, nitrokv::core::Entry, FNVHasher> index_map{};

nitrokv::core::TimingWheel<nitrokv::core::Entry*> timing_wheel;

void cancel_ttl(nitrokv::core::Entry& entry) {
    if (entry.tw_position) {
        timing_wheel.cancel(*entry.tw_position);
        entry.tw_position = std::nullopt;
    }

    entry.expire_at = std::nullopt;
}
void erase_entry(nitrokv::core::Entry& entry) {
    cancel_ttl(entry);

    lru.erase(entry.lru_it);

    index_map.erase(*entry.key);
}
[[nodiscard]] bool is_expired(const nitrokv::core::Entry& entry,
                              const std::chrono::steady_clock::time_point now) noexcept {
    return entry.expire_at && *entry.expire_at <= now;
}

} // namespace

namespace nitrokv::core {


CoreStatusCode cache_push(const BytesViewT key, const BytesViewT val) {
    // Поиск уже существующего вхождения, без добавления нового
    auto it = index_map.find({key.begin(), key.end()});
    if (it != index_map.end()) {
        // По правилу LRU переместить в начало данную запись
        lru.splice(lru.begin(), lru, it->second.lru_it);
        auto& entry = it->second;
        entry.lru_it = lru.begin();
        entry.value.assign_range(val);
        // Cброс TTL
        cancel_ttl(entry);
        return CoreStatusCode::NO_ERRORS;
    }

    // Вытеснение последнего, найменее значимого элемента по правилу LRU, если достигнут лимит
    // занятых слотов
    if (index_map.size() >= LRU_SLOTS_MAX) {
        erase_entry(*lru.back());
    }

    try {
        // Вставка нового значения в хеш-таблицу
        auto [new_it, state] =
            index_map.emplace(BytesT{key.begin(), key.end()}, Entry{
                                                                  .value{val.begin(), val.end()},
                                                              });
        // Сохранение указателя на ключ
        new_it->second.key = &new_it->first;
        // Вставка нового значения в начало списка в соответствии с LRU
        lru.emplace_front(&new_it->second);
        new_it->second.lru_it = lru.begin();
        return CoreStatusCode::NO_ERRORS;
    } catch (const std::bad_alloc&) {
        return CoreStatusCode::OUT_OF_MEMORY;
    }
}

bool cache_pop(const BytesViewT key) {
    const auto it = index_map.find({key.begin(), key.end()});

    if (it == index_map.end()) {
        return false;
    }
    erase_entry(it->second);
    return true;
}


[[nodiscard]] bool cache_contains(const BytesViewT key) {
    const auto it = index_map.find(BytesT{key.begin(), key.end()});

    if (it == index_map.end()) {
        return false;
    }

    Entry& entry = it->second;
    const auto now = std::chrono::steady_clock::now();

    if (is_expired(entry, now)) {
        erase_entry(entry);
        return false;
    }

    return true;
}

bool cache_set_ttl(const BytesViewT key, const std::chrono::seconds ttl) {
    const auto it = index_map.find(BytesT{key.begin(), key.end()});

    if (it == index_map.end()) {
        return false;
    }

    Entry& entry = it->second;

    if (ttl <= std::chrono::seconds{0}) {
        erase_entry(entry);
        return true;
    }

    const auto now = std::chrono::steady_clock::now();

    if (is_expired(entry, now)) {
        erase_entry(entry);
        return false;
    }

    if (entry.tw_position) {
        timing_wheel.cancel(*entry.tw_position);
        entry.tw_position = std::nullopt;
    }

    entry.expire_at = now + ttl;
    entry.tw_position = timing_wheel.schedule(&entry, ttl);

    return true;
}


[[nodiscard]] std::expected<std::chrono::seconds, CoreStatusCode>
cache_get_ttl(const BytesViewT key) {
    const auto it = index_map.find(BytesT{key.begin(), key.end()});

    if (it == index_map.end()) {
        return std::unexpected(CoreStatusCode::KEY_NOT_FOUND);
    }

    Entry& entry = it->second;
    const auto now = std::chrono::steady_clock::now();

    if (is_expired(entry, now)) {
        erase_entry(entry);
        return std::unexpected(CoreStatusCode::KEY_NOT_FOUND);
    }

    if (!entry.expire_at) {
        return std::unexpected(CoreStatusCode::TTL_NOT_SET);
    }

    return std::chrono::duration_cast<std::chrono::seconds>(*entry.expire_at - now);
}

[[nodiscard]] std::optional<BytesViewT> cache_get(const BytesViewT key) {
    const auto it = index_map.find(BytesT{key.begin(), key.end()});

    if (it == index_map.end()) {
        return std::nullopt;
    }

    Entry& entry = it->second;
    const auto now = std::chrono::steady_clock::now();

    if (is_expired(entry, now)) {
        erase_entry(entry);
        return std::nullopt;
    }

    lru.splice(lru.begin(), lru, entry.lru_it);
    entry.lru_it = lru.begin();

    return BytesViewT{entry.value};
}


void cache_tick_expiration() {
    const auto now = std::chrono::steady_clock::now();

    timing_wheel.tick([&](Entry* entry) {
        if (entry == nullptr) {
            return;
        }

        // Timer-node уже удалён самим wheel.tick().
        entry->tw_position = std::nullopt;

        if (!entry->expire_at || *entry->expire_at > now) {
            return;
        }

        erase_entry(*entry);
    });
}

} // namespace nitrokv::core
