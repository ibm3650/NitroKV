//
// Created by nikita on 03.04.2026.
//
#pragma once
#include <charconv>
#include <concepts>
#include <limits>
#include <span>

#include "nitrokv/protocol/protocol.hpp"

namespace nitrokv::protocol::detail {
template <typename T>
concept PureInteger =
    std::integral<T> && !std::same_as<T, bool> && !std::same_as<T, char> &&
    !std::same_as<T, unsigned char> && !std::same_as<T, char8_t> && !std::same_as<T, char16_t> &&
    !std::same_as<T, char32_t> && !std::same_as<T, wchar_t> && !std::same_as<T, signed char>;


template <typename T>
concept FitsInInt64 =
    PureInteger<T> && (std::numeric_limits<T>::max() <= std::numeric_limits<int64_t>::max()) &&
    (std::numeric_limits<T>::min() >= std::numeric_limits<int64_t>::min());


std::span<char> integer_to_digits(const PureInteger auto val, std::span<char> buffer) noexcept {
    const auto [digits_last_ptr, ec] =
        std::to_chars(buffer.data(), buffer.data() + buffer.size_bytes(), val,
                      10); // NOLINT(*-pro-bounds-pointer-arithmetic) Здесь это безовасно
    if (ec != std::errc{}) [[unlikely]] {
        return {};
    }
    return {buffer.data(), digits_last_ptr};
}
} // namespace nitrokv::protocol::detail
