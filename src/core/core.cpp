
/**
 * @file core.cpp
 * @date 22.04.2026
 * @author nikita
 */
#include <expected>
#include <list>
#include <mutex>
#include <unordered_map>

#include "nitrokv/core/cache.hpp"
#include "nitrokv/core/results.hpp"

namespace nitrokv::core {
struct Entry;
using BytesT = std::vector<std::byte>;

constexpr size_t TIMING_WHEEL_SLOTS{60};
constexpr size_t LRU_SLOTS_MAX{64};
static std::array<std::list<Entry*>, TIMING_WHEEL_SLOTS> timing_wheel_slots{};

struct Entry {
    std::optional<std::chrono::seconds> ttl; // Опционально, время жизни ключа
    // относительно его вставки ts

    std::chrono::time_point<std::chrono::steady_clock> ts; // Временная метка вставки в таблицу


    BytesT value; // Владеющее хранение значения

    std::list<Entry*>::iterator
        lru_it; // Если запрос на удаление по ключу, нужно знать, кого удалить
    // из списка

    const BytesT*
        key; // Невладеющее хранение ключа, нужен, при запросе на удаление от таймера, или при
    // удалении хвоста при вставке.

    size_t tw_rounds; // Количество оборотов колесика, если значение tll больше полного оборота
    size_t timing_wheel_slot; // Ячейка в колесике, для получения списка ключей на удаление

    std::list<Entry*>::iterator
        tw_it; // Итератор на узел списка, полученного по индексу timing_wheel_slot
    // Необходим если значение удаляется вытеснением или вручную
};

std::list<Entry*> lru;


struct BytesHasher {
    std::size_t operator()(const BytesT& bytes) const noexcept {
        std::size_t hash = 1469598103934665603ULL; // FNV-1a 64-bit basis

        for (std::byte b : bytes) {
            hash ^= static_cast<std::size_t>(std::to_integer<unsigned char>(b));
            hash *= 1099511628211ULL;
        }

        return hash;
    }
};

std::unordered_map<BytesT, Entry, BytesHasher> index_map{};


// struct Shard {
//     using ListIt = std::list<Entry>::iterator;
//
//     std::mutex mutex;
//     std::list<Entry> lru;
//     std::unordered_map<Key, ListIt> index;
// };

CoreStatusCode cache_push(const BytesViewT key, const BytesViewT val) {
    auto it = index_map.find({key.begin(), key.end()});
    if (it != index_map.end()) {
        lru.splice(lru.begin(), lru, it->second.lru_it);
        it->second.lru_it = lru.begin();
        it->second.value.assign_range(val);
        // TODO:Сбрасывать TTL
        return CoreStatusCode::NO_ERRORS;
    }

    if (index_map.size() >= LRU_SLOTS_MAX) {
        auto last = lru.back();
        if (last->ttl) {
            timing_wheel_slots[last->timing_wheel_slot].erase(last->tw_it);
        }
        lru.pop_back();
        index_map.erase(*last->key);
    }

    auto [new_it, state] =
        index_map.emplace(BytesT{key.begin(), key.end()}, Entry{
                                                              .ttl{std::nullopt},
                                                              .ts{std::chrono::steady_clock::now()},
                                                              .value{val.begin(), val.end()},
                                                          });

    new_it->second.key = &new_it->first;
    lru.emplace_front(&new_it->second);
    new_it->second.lru_it = lru.begin();
    return CoreStatusCode::NO_ERRORS;
}

bool cache_pop(const BytesViewT key) {
    const auto it = index_map.find({key.begin(), key.end()});

    if (it == index_map.end()) {
        return false;
    }

    if (it->second.ttl) {
        timing_wheel_slots[it->second.timing_wheel_slot].erase(it->second.tw_it);
    }
    lru.erase(it->second.lru_it);
    index_map.erase(it);
    return true;
}


[[nodiscard]] bool cache_contains(const BytesViewT key) {
    return index_map.contains({key.begin(), key.end()});
}


[[nodiscard]] std::expected<std::chrono::seconds, CoreStatusCode>
cache_get_ttl(const BytesViewT key)  {
    const auto it = index_map.find({key.begin(), key.end()});

    if (it == index_map.end()) {
        return std::unexpected(CoreStatusCode::KEY_NOT_FOUND);
    }
    if (!it->second.ttl) {
        return std::unexpected{CoreStatusCode::TTL_NOT_SET};
    }

    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() -
                                                            it->second.ts + *it->second.ttl);
}

size_t round_start_ts() {
    return {};
}


size_t current_ts() {
    return {};
}

auto set_into_wheel(Entry* ptr, std::chrono::seconds ttl) {
    const int  free_ticks_curr = TIMING_WHEEL_SLOTS - current_ts() - round_start_ts();
    size_t ticks = std::max(0, static_cast<int>(ttl.count()) - free_ticks_curr) % TIMING_WHEEL_SLOTS;
    size_t rounds = (ticks ? 1 : 0) + free_ticks_curr / TIMING_WHEEL_SLOTS;
    ptr->ttl = ttl;
    ptr->timing_wheel_slot = ticks;
    ptr->tw_rounds = rounds;
    // timing_wheel_slots[ticks].push_back(ptr);
    return timing_wheel_slots[ticks].insert(timing_wheel_slots[ticks].begin(),ptr);
}

bool cache_set_ttl(const BytesViewT key, std::chrono::seconds ttl) noexcept {
    const auto it = index_map.find({key.begin(), key.end()});

    if (it == index_map.end()) {
        return false;
    }

    if (it->second.ttl) {
        timing_wheel_slots[it->second.timing_wheel_slot].erase(it->second.tw_it);
    }
    it->second.tw_it = set_into_wheel(&it->second, ttl);
    it->second.ts = std::chrono::steady_clock::now();
    return true;
}

[[nodiscard]] std::optional<BytesViewT> cache_get(const BytesViewT key) noexcept {
    const auto it = index_map.find({key.begin(), key.end()});

    if (it == index_map.end()) {
        return std::nullopt;
    }
    return std::span<const std::byte>{it->second.value};
}





} // namespace nitrokv::core
