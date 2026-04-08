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
#include <variant>

#include "nitrokv/protocol/detail.hpp"

namespace {

/**
 *
 * @param[in] input Буфер, содержащий целочисленное значение в строковом представлении
 * @return Кортеж с остатком буфера и полученное число. Третий элемент отображает статус парсинга
 */
std::tuple<std::span<const std::byte>, int64_t, bool>
parse_resp_integer(const std::span<const std::byte> input) noexcept {
    if (input.empty()) {
        return {{}, {}, false};
    }
    int64_t value{};
    const char* payload_as_char = reinterpret_cast<const char*>(input.data());
    const size_t payload_length = input.size();
    const auto [ptr, ec] =
        std::from_chars(payload_as_char, payload_as_char + payload_length, value);
    if (ec != std::errc{}) {
        return {{}, {}, false};
    }
    return {{reinterpret_cast<const std::byte*>(ptr), input.data() + payload_length}, value, true};
}


bool is_correct_separator(const std::span<const std::byte> input) noexcept {
    // if (input.size() < nitrokv::protocol::RESP_SEPARATOR.size()) {
    //     return false;
    // }
    return input.size() >= nitrokv::protocol::detail::SEP_BYTES.size() &&
           input[0] == nitrokv::protocol::detail::SEP_BYTES[0] &&
           input[1] == nitrokv::protocol::detail::SEP_BYTES[1];
    // return std::ranges::equal(nitrokv::protocol::detail::SEP_BYTES,
    //                           input.subspan(0, nitrokv::protocol::detail::SEP_BYTES.size()));
}


nitrokv::protocol::RespDecodingResult parse_integer(std::span<const std::byte> input) {
    // int64_t value{};
    const auto [remaining, value, status] = parse_resp_integer(input);
    if (!status) {
        return {};
    }
    if (!is_correct_separator(remaining)) {
        return {};
    }
    return {{remaining.subspan(nitrokv::protocol::detail::SEP_BYTES.size())}, {value}};
}

nitrokv::protocol::RespDecodingResult parse_array(const std::span<const std::byte> input) noexcept {
    auto [remaining, elements_count, status] = parse_resp_integer(input);
    if (!status) {
        return {};
    }
    if (!is_correct_separator(remaining)) {
        return {};
    }
    // std::span payload{reinterpret_cast<const std::byte*>(remain_ptr), input.data() +
    // input.size()}; bool const is_correct_separator =
    //     std::ranges::equal(nitrokv::protocol::detail::SEP_BYTES,
    //                        payload.subspan(0, nitrokv::protocol::detail::SEP_BYTES.size()));

    // payload.subspan(std::max(0LL, elements_count),nitrokv::protocol::RESP_SEPARATOR.size())
    // if (!is_correct_separator) {
    //     return {};
    // }
    remaining = remaining.subspan(nitrokv::protocol::detail::SEP_BYTES.size());
    if (elements_count == -1) {
        return {remaining, {nitrokv::protocol::RespNullArray{}}};
    }

    // is_correct_separator =
    // std::ranges::equal(std::as_bytes(std::span(nitrokv::protocol::RESP_SEPARATOR)),
    //     payload.subspan(std::max(0LL, elements_count),nitrokv::protocol::RESP_SEPARATOR.size()));
    // payload = payload.subspan(nitrokv::protocol::RESP_SEPARATOR.size());


    // nitrokv::protocol::RespEncValue value{};
    nitrokv::protocol::RespArray output;
    if (!nitrokv::protocol::detail::reserve_space(elements_count, output)) {
        return {};
    }
    for (size_t i = 0; i < elements_count; ++i) {
        if (remaining.empty()) {
            return {};
        }
        auto [remaining_pld, value] = nitrokv::protocol::decode(remaining);
        if (std::holds_alternative<std::monostate>(value.value)) {
            return {};
        }
        // if (output.size() != elements_count && remaining_pld.empty()) {
        //     return {};
        // }
        output.emplace_back(std::move(value));
        remaining = remaining_pld;
    }
    return {remaining, {std::move(output)}};
}


nitrokv::protocol::RespDecodingResult
parse_bulk_string(const std::span<const std::byte> input) noexcept {
    // size_t elements_count{};
    // const auto [ptr, ec] =
    //     std::from_chars(input.data(), input.data() + input.size(), elements_count);
    // if (ec != std::errc{}) {
    //     // return std::nullopt();
    //     return {};
    // }
    // if (input.size() < elements_count + nitrokv::protocol::RESP_SEPARATOR.size()) {
    //     return {};
    // }
    // if (*ptr != nitrokv::protocol::RESP_SEPARATOR[0] &&
    //     *(ptr + 1) != nitrokv::protocol::RESP_SEPARATOR[1]) {
    //     return {};
    // }
    auto [remaining, elements_count, status] = parse_resp_integer(input);
    if (!status) {
        return {};
    }
    if (!is_correct_separator(remaining)) {
        return {};
    }
    remaining = remaining.subspan(nitrokv::protocol::detail::SEP_BYTES.size());
    if (elements_count == -1) {
        return {remaining, {nitrokv::protocol::RespNullBulkString{}}};
    }

    if (remaining.size() < elements_count + nitrokv::protocol::detail::SEP_BYTES.size()) {
        return {};
    }
    if (!is_correct_separator(remaining.subspan(elements_count))) {
        return {};
    }


    const std::byte* remaining_data = remaining.data();
    const std::byte* str_end = remaining_data + elements_count;

    return {{str_end + 2, remaining_data + remaining.size()},
            {nitrokv::protocol::RespBulkString{{remaining_data, str_end}}}};
}


nitrokv::protocol::RespDecodingResult
parse_simple_string(const std::span<const std::byte> input,
                    const nitrokv::protocol::RespTypePrefix prefix) {
    // nitrokv::protocol::DecodingRsult parse_simple_string(std::string_view input) {
    // size_t elements_count{};
    // const auto [ptr, ec] =
    //     std::from_chars<size_t>(input.data(), input.data() + input.size(), elements_count);
    // if (ec != std::errc{}) {
    //     // return std::nullopt();
    //     return {input, {}};
    // }
    // if (elements_count < elements_count + nitrokv::protocol::SEPARATOR.size()) {
    //     return {input, {}};
    // }
    const auto first_sym = std::ranges::find_first_of(input, nitrokv::protocol::detail::SEP_BYTES);
    if (first_sym == input.end()) {
        return {};
    }
    if (!is_correct_separator({first_sym, input.end()})) {
        return {};
    }

    std::string_view const value{reinterpret_cast<const char*>(input.data()),
                                 reinterpret_cast<const char*>(&*first_sym)};

    const std::span<const std::byte> remaining{first_sym + 2, input.end()};
    if (prefix == nitrokv::protocol::RespTypePrefix::SIMPLE_STRING) {
        return {remaining, {nitrokv::protocol::RespSimpleString{value}}};
    }

    return {remaining, {nitrokv::protocol::RespError{value}}};
}
} // namespace


namespace nitrokv::protocol {
RespDecodingResult decode(const std::span<const std::byte> input) noexcept {
    if (input.empty()) {
        return {.remaining = input};
    }
    const auto resp_prefix = static_cast<RespTypePrefix>(input[0]);
    RespDecodingResult parsed_val;
    switch (const std::span resp_payload = input.subspan(1); resp_prefix) {
        case RespTypePrefix::ARRAY:
            parsed_val = ::parse_array(resp_payload);
            break;
        case RespTypePrefix::SIMPLE_STRING:
        case RespTypePrefix::ERROR:
            parsed_val = ::parse_simple_string(resp_payload, resp_prefix);
            break;
        case RespTypePrefix::BULK_STRING:
            parsed_val = ::parse_bulk_string(resp_payload);
            break;
        case RespTypePrefix::INTEGER:
            parsed_val = ::parse_integer(resp_payload);
            break;
        default:
            return {.remaining = input};
    }
    if (std::ranges::equal(parsed_val.remaining, std::as_bytes(std::span(RESP_SEPARATOR)))) {
        return {.value = std::move(parsed_val.value)};
    }
    if (std::holds_alternative<std::monostate>(parsed_val.value.value)) {
        return {.remaining = input};
    }
    return parsed_val;
}
} // namespace nitrokv::protocol
