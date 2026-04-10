//
// Created by nikita on 03.04.2026.
//
#pragma once
#include <charconv>
#include <concepts>
#include <limits>
#include <span>
#include <stdexcept>

#include "nitrokv/protocol/protocol.hpp"

namespace nitrokv::protocol::detail {
inline const auto SEP_BYTES = std::as_bytes(std::span(nitrokv::protocol::RESP_SEPARATOR));
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

template <typename T>
bool reserve_space(const size_t additional_space, std::vector<T>& buffer) noexcept {
    const size_t current_size = buffer.size();
    if (current_size > std::numeric_limits<size_t>::max() - additional_space) {
        return false;
    }
    try {
        buffer.reserve(current_size + additional_space);
    } catch (const std::length_error&) {
        return false;
    } catch (const std::bad_alloc&) {
        return false;
    }
    return true;
}


} // namespace nitrokv::protocol::detail
