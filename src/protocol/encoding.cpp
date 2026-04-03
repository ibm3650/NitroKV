//
// Created by nikita on 22.03.2026.
//

#include "nitrokv/protocol/encoding.hpp"

#include <algorithm>
#include <charconv>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>
namespace {

const auto SEP_BYTES = std::as_bytes(std::span(nitrokv::protocol::SEPARATOR));
constexpr size_t MAX_DIGITS_IN_NUMBER = std::numeric_limits<int64_t>::digits10 + 2;
constexpr size_t MAX_BULK_STRING_LENGTH = 512ULL * 1024U * 1024U;


bool reserve_space(const size_t additional_space, std::vector<std::byte>& buffer) noexcept {
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
bool insert_null(const nitrokv::protocol::RespTypePrefix prefix,
                 std::vector<std::byte>& buffer) noexcept {
    constexpr std::array payload = {std::byte{'-'}, std::byte{'1'}, std::byte{'\r'},
                                    std::byte{'\n'}};
    if (!::reserve_space(payload.size() + nitrokv::protocol::PREFIX_LENGTH, buffer)) [[unlikely]] {
        return false;
    }
    buffer.push_back(static_cast<std::byte>(prefix));
    buffer.insert(buffer.end(), payload.begin(), payload.end());
    return true;
}

bool insert_simple_str(nitrokv::protocol::RespTypePrefix prefix,
                       std::string_view val,
                       std::vector<std::byte>& buffer) noexcept {
    const auto val_bytes = std::as_bytes(std::span(val));
    if (std::ranges::find_first_of(val_bytes, SEP_BYTES) != val_bytes.end()) [[unlikely]] {
        return false;
    }

    if (!::reserve_space(
            val_bytes.size_bytes() + nitrokv::protocol::PREFIX_LENGTH + SEP_BYTES.size(), buffer))
        [[unlikely]] {
        return false;
    }

    buffer.push_back(static_cast<std::byte>(prefix));
    buffer.insert(buffer.end(), val_bytes.begin(), val_bytes.end());
    buffer.insert(buffer.end(), SEP_BYTES.begin(), SEP_BYTES.end());
    return true;
}


std::span<char> integer_to_digits(const size_t val, std::span<char> buffer) noexcept {
    const auto [digits_last_ptr, ec] =
        std::to_chars(buffer.data(), buffer.data() + buffer.size_bytes(), val,
                      10); // NOLINT(*-pro-bounds-pointer-arithmetic) Здесь это безовасно
    if (ec != std::errc{}) [[unlikely]] {
        return {};
    }
    return {buffer.data(), digits_last_ptr};
}

} // namespace


namespace nitrokv::protocol {


bool encode(const RespEncInteger val, std::vector<std::byte>& buffer) noexcept {
    std::array<char, MAX_DIGITS_IN_NUMBER> digits; // NOLINT(*-member-init)


    const auto [digits_last_ptr, ec] =
        std::to_chars(digits.data(), digits.data() + digits.size(), val,
                      10); // NOLINT(*-pro-bounds-pointer-arithmetic)
    if (ec != std::errc{}) [[unlikely]] {
        return {};
    }

    const std::span<const std::byte> digits_bytes =
        std::as_bytes(std::span{digits.data(), digits_last_ptr});
    if (digits_bytes.empty()) [[unlikely]] {
        return false;
    }
    const size_t required_size = digits_bytes.size_bytes() + SEP_BYTES.size_bytes() + PREFIX_LENGTH;
    if (!::reserve_space(required_size, buffer)) [[unlikely]] {
        return false;
    }
    buffer.push_back(static_cast<std::byte>(RespTypePrefix::INTEGER));
    buffer.insert(buffer.end(), digits_bytes.begin(), digits_bytes.end());
    buffer.insert(buffer.end(), SEP_BYTES.begin(), SEP_BYTES.end());
    return true;
}

bool encode(const RespEncSimpleString val, std::vector<std::byte>& buffer) noexcept {
    return insert_simple_str(RespTypePrefix::SIMPLE_STRING, val.value, buffer);
}

bool encode(const RespEncError val, std::vector<std::byte>& buffer) noexcept {
    return insert_simple_str(RespTypePrefix::ERROR, val.value, buffer);
}


bool encode(const RespEncArray& payload, std::vector<std::byte>& buffer) noexcept {
    std::array<char, MAX_DIGITS_IN_NUMBER> digits; // NOLINT(*-member-init)
    const std::span<const std::byte> digits_bytes =
        std::as_bytes(::integer_to_digits(payload.size(), digits));
    if (digits_bytes.empty()) [[unlikely]] {
        return false;
    }
    const size_t old_size = buffer.size();
    const size_t required_size = digits_bytes.size_bytes() + SEP_BYTES.size_bytes() + PREFIX_LENGTH;
    if (!::reserve_space(required_size, buffer)) [[unlikely]] {
        return false;
    }

    buffer.push_back(static_cast<std::byte>(RespTypePrefix::ARRAY));
    buffer.insert(buffer.end(), digits_bytes.begin(), digits_bytes.end());
    buffer.insert(buffer.end(), SEP_BYTES.begin(), SEP_BYTES.end());
    for (const RespEncValue& elem : payload) {
        const bool elem_success =
            std::visit([&buffer](const auto& val) { return encode(val, buffer); }, elem.value);
        if (!elem_success) [[unlikely]] {
            buffer.resize(old_size);
            return false;
        }
    }
    return true;
}


bool encode(const RespEncBulkString val, std::vector<std::byte>& buffer) noexcept {
    if (val.value.size() > MAX_BULK_STRING_LENGTH) [[unlikely]] {
        return false;
    }

    std::array<char, MAX_DIGITS_IN_NUMBER> digits; // NOLINT(*-member-init)
    const std::span<const std::byte> digits_bytes =
        std::as_bytes(::integer_to_digits(val.value.size(), digits));
    if (digits_bytes.empty()) [[unlikely]] {
        return false;
    }
    const size_t required_size =
        digits_bytes.size_bytes() + val.value.size() + (SEPARATOR.size() * 2) + PREFIX_LENGTH;
    if (!::reserve_space(required_size, buffer)) [[unlikely]] {
        return false;
    }
    buffer.push_back(static_cast<std::byte>(RespTypePrefix::BULK_STRING));
    buffer.insert(buffer.end(), digits_bytes.begin(), digits_bytes.end());
    buffer.insert(buffer.end(), SEP_BYTES.begin(), SEP_BYTES.end());
    buffer.insert(buffer.end(), val.value.begin(), val.value.end());
    buffer.insert(buffer.end(), SEP_BYTES.begin(), SEP_BYTES.end());
    return true;
}


bool encode(RespEncNullBulkString /*unused*/, std::vector<std::byte>& buffer) noexcept {
    return ::insert_null(RespTypePrefix::BULK_STRING, buffer);
}

bool encode(RespEncNullArray /*unused*/, std::vector<std::byte>& buffer) noexcept {
    return ::insert_null(RespTypePrefix::ARRAY, buffer);
}


} // namespace nitrokv::protocol
