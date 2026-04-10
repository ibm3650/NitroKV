//
// Created by nikita on 04.04.2026.
//
#include "nitrokv/protocol/decoding.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <ranges>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>

#include "nitrokv/protocol/detail.hpp"

namespace {
nitrokv::protocol::RespDecodingResult decode_one(std::span<const std::byte> input) noexcept;

/**
 *
 * @param[in] input Буфер, содержащий целочисленное значение в строковом представлении
 * @return Кортеж с остатком буфера и полученное число. Третий элемент отображает статус парсинга
 */
[[nodiscard]] std::tuple<std::span<const std::byte>, int64_t, nitrokv::protocol::RespDecodingStatus>
parse_resp_integer(const std::span<const std::byte> input) noexcept {
    using nitrokv::protocol::RespDecodingStatus;
    if (input.empty()) {
        return {{}, {}, RespDecodingStatus::EMPTY_BUFFER};
    }
    int64_t value{};
    const char* input_as_char = reinterpret_cast<const char*>(input.data());
    const size_t input_length = input.size();
    const auto [ptr, ec] = std::from_chars(input_as_char, input_as_char + input_length, value);
    if (ec != std::errc{}) {
        return {{}, {}, RespDecodingStatus::INVALID_INTEGER};
    }
    return {{reinterpret_cast<const std::byte*>(ptr), input.data() + input_length},
            value,
            RespDecodingStatus::SUCCESS};
}


[[nodiscard]] bool is_correct_separator(const std::span<const std::byte> input) noexcept {

    return input.size() >= nitrokv::protocol::RESP_SEPARATOR_LENGTH &&
           input[0] == nitrokv::protocol::detail::SEP_BYTES[0] &&
           input[1] == nitrokv::protocol::detail::SEP_BYTES[1];
}


[[nodiscard]] nitrokv::protocol::RespDecodingResult
parse_integer(std::span<const std::byte> input) {

    const auto [remaining, value, status] = parse_resp_integer(input);
    if (status != nitrokv::protocol::RespDecodingStatus::SUCCESS) {
        return {.status = status};
    }
    if (!is_correct_separator(remaining)) {
        return {.status = nitrokv::protocol::RespDecodingStatus::INVALID_SEPARATOR};
    }
    return {{remaining.subspan(nitrokv::protocol::RESP_SEPARATOR_LENGTH)},
            {value},
            nitrokv::protocol::RespDecodingStatus::SUCCESS};
}

[[nodiscard]] nitrokv::protocol::RespDecodingResult
parse_array(const std::span<const std::byte> input) noexcept {
    auto [remaining, elements_count, status] = parse_resp_integer(input);
    if (status != nitrokv::protocol::RespDecodingStatus::SUCCESS) {
        return {.status = status};
    }
    if (elements_count < -1) {
        return {.status = nitrokv::protocol::RespDecodingStatus::INVALID_INTEGER};
    }
    if (!is_correct_separator(remaining)) {
        return {.status = nitrokv::protocol::RespDecodingStatus::INVALID_SEPARATOR};
    }

    remaining = remaining.subspan(nitrokv::protocol::RESP_SEPARATOR_LENGTH);
    if (elements_count == -1) {
        return {remaining,
                {nitrokv::protocol::RespNullArray{}},
                nitrokv::protocol::RespDecodingStatus::SUCCESS};
    }


    nitrokv::protocol::RespArray output;
    if (!nitrokv::protocol::detail::reserve_space(static_cast<size_t>(elements_count), output)) {
        return {.status = nitrokv::protocol::RespDecodingStatus::OUT_OF_MEM};
    }
    for (size_t i = 0; std::cmp_less(i, elements_count); ++i) {
        if (remaining.empty()) {
            return {.status = nitrokv::protocol::RespDecodingStatus::UNEXPECTED_END};
        }
        auto [remaining_pld, value, decoding_status] = decode_one(remaining);
        if (decoding_status != nitrokv::protocol::RespDecodingStatus::SUCCESS) {
            return {.status = decoding_status};
        }
        try {
            output.emplace_back(std::move(value));
        } catch (...) {
            return {.status = nitrokv::protocol::RespDecodingStatus::OUT_OF_MEM};
        }
        remaining = remaining_pld;
    }
    return {remaining, {std::move(output)}, nitrokv::protocol::RespDecodingStatus::SUCCESS};
}


[[nodiscard]] nitrokv::protocol::RespDecodingResult
parse_bulk_string(const std::span<const std::byte> input) noexcept {
    auto [remaining, elements_count, status] = parse_resp_integer(input);
    if (status != nitrokv::protocol::RespDecodingStatus::SUCCESS) {
        return {.status = status};
    }
    if (elements_count < -1) {
        return {.status = nitrokv::protocol::RespDecodingStatus::INVALID_INTEGER};
    }
    if (!is_correct_separator(remaining)) {
        return {.status = nitrokv::protocol::RespDecodingStatus::INVALID_SEPARATOR};
    }


    remaining = remaining.subspan(nitrokv::protocol::RESP_SEPARATOR_LENGTH);
    if (elements_count == -1) {
        return {remaining,
                {nitrokv::protocol::RespNullBulkString{}},
                nitrokv::protocol::RespDecodingStatus::SUCCESS};
    }

    if (remaining.size() <
        static_cast<size_t>(elements_count) + nitrokv::protocol::RESP_SEPARATOR_LENGTH) {
        return {.status = nitrokv::protocol::RespDecodingStatus::UNEXPECTED_END};
    }
    if (!is_correct_separator(remaining.subspan(static_cast<size_t>(elements_count)))) {
        return {.status = nitrokv::protocol::RespDecodingStatus::INVALID_SEPARATOR};
    }


    const std::byte* remaining_data = remaining.data();
    const std::byte* str_end = remaining_data + elements_count;

    return {{str_end + nitrokv::protocol::RESP_SEPARATOR_LENGTH, remaining_data + remaining.size()},
            {nitrokv::protocol::RespBulkString{{remaining_data, str_end}}},
            nitrokv::protocol::RespDecodingStatus::SUCCESS};
}


[[nodiscard]] nitrokv::protocol::RespDecodingResult
parse_simple_string(const std::span<const std::byte> input,
                    const nitrokv::protocol::RespTypePrefix prefix) {
    const auto first_sym = std::ranges::find_first_of(input, nitrokv::protocol::detail::SEP_BYTES);
    if (first_sym == input.end()) {
        return {.status = nitrokv::protocol::RespDecodingStatus::UNEXPECTED_END};
    }
    if (!is_correct_separator({first_sym, input.end()})) {
        return {.status = nitrokv::protocol::RespDecodingStatus::INVALID_SYMBOL};
    }

    std::string_view const value{reinterpret_cast<const char*>(input.data()),
                                 reinterpret_cast<const char*>(&*first_sym)};

    const std::span<const std::byte> remaining{first_sym + 2, input.end()};
    if (prefix == nitrokv::protocol::RespTypePrefix::SIMPLE_STRING) {
        return {remaining,
                {nitrokv::protocol::RespSimpleString{value}},
                nitrokv::protocol::RespDecodingStatus::SUCCESS};
    }

    return {remaining,
            {nitrokv::protocol::RespError{value}},
            nitrokv::protocol::RespDecodingStatus::SUCCESS};
}

[[nodiscard]] nitrokv::protocol::RespDecodingResult
decode_one(const std::span<const std::byte> input) noexcept {
    if (input.empty()) {
        return {.status = nitrokv::protocol::RespDecodingStatus::EMPTY_BUFFER};
    }
    const auto resp_prefix = static_cast<nitrokv::protocol::RespTypePrefix>(input[0]);
    switch (const std::span resp_payload = input.subspan(1); resp_prefix) {
        case nitrokv::protocol::RespTypePrefix::ARRAY:
            return ::parse_array(resp_payload);
        case nitrokv::protocol::RespTypePrefix::SIMPLE_STRING:
        case nitrokv::protocol::RespTypePrefix::ERROR:
            return ::parse_simple_string(resp_payload, resp_prefix);
        case nitrokv::protocol::RespTypePrefix::BULK_STRING:
            return ::parse_bulk_string(resp_payload);
        case nitrokv::protocol::RespTypePrefix::INTEGER:
            return ::parse_integer(resp_payload);
        default:
            return {.status = nitrokv::protocol::RespDecodingStatus::INVALID_SYMBOL};
    }
}


} // namespace


namespace nitrokv::protocol {
RespDecodingResult decode_first(const std::span<const std::byte> input) noexcept {
    if (input.empty()) {
        return {.remaining = input, .status = RespDecodingStatus::EMPTY_BUFFER};
    }
    RespDecodingResult parsed_val = ::decode_one(input);
    if (parsed_val.status != RespDecodingStatus::SUCCESS) {
        return {.remaining = input, .status = parsed_val.status};
    }
    return parsed_val;
}
RespDecodingResult decode(const std::span<const std::byte> input) noexcept {

    RespDecodingResult parsed_val = ::decode_one(input);
    if (parsed_val.status != RespDecodingStatus::SUCCESS) {
        return {.remaining = input, .status = parsed_val.status};
    }
    if (!parsed_val.remaining.empty()) {
        return {.remaining = input, .status = RespDecodingStatus::EXTRA_DATA};
    }
    return {.value = std::move(parsed_val.value), .status = parsed_val.status};
}
} // namespace nitrokv::protocol
