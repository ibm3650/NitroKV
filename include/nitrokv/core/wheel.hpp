/**
 * @file wheel.hpp
 * @date 07.05.2026
 * @author nikita
 */

#pragma once
#include <chrono>
#include <concepts>
#include <list>
#include <utility>
#include <array>
#include <cstddef>
namespace nitrokv::core {
template <typename T, size_t SLOTS_COUNT = 60> class TimingWheel {
public:
    static_assert(SLOTS_COUNT > 0);
    // TODO: Упорядочивать элементы в списке по TTL[rounds]
    struct SlotEntry {
        size_t rounds_left;
        T value;
    };
    using SlotItemsT = std::list<SlotEntry>;
    using SlotIterator = SlotItemsT::iterator;
    struct TimingWheelPosition {
        // TODO: Разрешить создание данного класса только текущему
        // TODO: Добавить флаг инвалидации
        size_t slot;
        SlotIterator iterator;
    };

    void cancel(const TimingWheelPosition& pos) noexcept {
        wheel_slots_[pos.slot].erase(pos.iterator);
    }

    template <typename U>
        requires std::constructible_from<T, U&&>
    [[nodiscard]] TimingWheelPosition schedule(U&& value, const std::chrono::seconds& ttl) {
        // const std::chrono::seconds& ttl) noexcept(std::is_nothrow_constructible_v<T>) {
        const auto ttl_raw = ttl.count();
        const size_t offset = ttl_raw <= 0 ? 0UL : static_cast<size_t>(ttl_raw - 1);

        const size_t slot = (current_slot_ + offset) % SLOTS_COUNT;
        const size_t rounds = offset / SLOTS_COUNT;
        auto& bucket = wheel_slots_[slot];
        const auto it = bucket.emplace(bucket.end(), rounds, std::forward<U>(value));

        return {slot, it};
    }
    template <typename Fn> void tick(Fn&& on_expire) {
        const size_t processing_slot = current_slot_;
        current_slot_ = (current_slot_ + 1UL) % SLOTS_COUNT;

        auto& bucket = wheel_slots_[processing_slot];

        for (auto it = bucket.begin(); it != bucket.end();) {
            if (it->rounds_left > 0UL) {
                --it->rounds_left;
                ++it;
                continue;
            }
            // T value;
            // if constexpr (std::is_trivially_copyable_v<T>) {
            //     value = it->value;
            // } else {
            //     value = std::move(it->value);
            // }
            // it = bucket.erase(it);
            //
            // std::forward<Fn>(on_expire)(value);
            T value = std::move(it->value);
            it = bucket.erase(it);
            std::forward<Fn>(on_expire)(std::move(value));
        }

    }

private:
    std::array<SlotItemsT, SLOTS_COUNT> wheel_slots_{};
    size_t current_slot_{0};
};
}; // namespace nitrokv::core
