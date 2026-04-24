//
// Created by nikita on 22.03.2026.
//


#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <expected>
#include <functional>
#include <ranges>
#include <span>
#include <string_view>

#include "../../include/nitrokv/command/commands.hpp"
#include "../../include/nitrokv/command/meta.hpp"
#include "nitrokv/protocol/protocol.hpp"

namespace {


/**
 *
 * @param resp_value Ссылка на std::variant Resp-типов
 * @return Опциональное значение, если тип совпадает - то bulk string как сырые байты, иначе -
 * пусто.
 */
[[nodiscard]] std::optional<std::span<const std::byte>>
try_get_bulk_string_bytes(const nitrokv::protocol::RespValue& resp_value) noexcept {
    const auto* bulk_str = std::get_if<nitrokv::protocol::RespBulkString>(&resp_value.value);
    if (bulk_str == nullptr) {
        return std::nullopt;
    }
    return bulk_str->value;
}


template <typename T>
    requires is_same_args_count<T, 1>
[[nodiscard]] std::expected<T, nitrokv::command::ParsingError>
parse_unary_command(const std::span<const nitrokv::protocol::RespValue>& args) noexcept {
    auto bulk_or_none = try_get_bulk_string_bytes(args[0]);
    if (!bulk_or_none) {
        return std::unexpected(nitrokv::command::ParsingError::INVALID_ARGUMENT_TYPE);
    }
    return T{*bulk_or_none};
}


nitrokv::command::CmdParsingResult
parse_ping(const std::span<const nitrokv::protocol::RespValue> /*args*/) {
    return nitrokv::command::PingCommand{};
}


nitrokv::command::CmdParsingResult
parse_get(const std::span<const nitrokv::protocol::RespValue> args) {
    return parse_unary_command<nitrokv::command::GetCommand>(args);
}

nitrokv::command::CmdParsingResult
parse_del(const std::span<const nitrokv::protocol::RespValue> args) {
    return parse_unary_command<nitrokv::command::DelCommand>(args);
}


nitrokv::command::CmdParsingResult
parse_ttl(const std::span<const nitrokv::protocol::RespValue> args) {
    return parse_unary_command<nitrokv::command::TtlCommand>(args);
}

nitrokv::command::CmdParsingResult
parse_exists(const std::span<const nitrokv::protocol::RespValue> args) {
    return parse_unary_command<nitrokv::command::ExistsCommand>(args);
}


nitrokv::command::CmdParsingResult
parse_set(const std::span<const nitrokv::protocol::RespValue> args) {
    const auto key = try_get_bulk_string_bytes(args[0]);
    const auto value = try_get_bulk_string_bytes(args[1]);
    if (!key) {
        return std::unexpected(nitrokv::command::ParsingError::INVALID_ARGUMENT_TYPE);
    }
    if (!value) {
        return std::unexpected(nitrokv::command::ParsingError::INVALID_ARGUMENT_TYPE);
    }
    return nitrokv::command::SetCommand{*key, *value};
}


nitrokv::command::CmdParsingResult
parse_expire(const std::span<const nitrokv::protocol::RespValue> args) {
    // EXPIRE key seconds [NX | XX | GT | LT]
    const auto key = try_get_bulk_string_bytes(args[0]);
    if (!key) {
        return std::unexpected(nitrokv::command::ParsingError::INVALID_ARGUMENT_TYPE);
    }

    const auto ttl_raw = try_get_bulk_string_bytes(args[1]);
    if (!ttl_raw) {
        return std::unexpected(nitrokv::command::ParsingError::INVALID_ARGUMENT_TYPE);
    }


    int64_t ttl{};
    const char* input_as_char = reinterpret_cast<const char*>(ttl_raw->data());
    const size_t input_length = ttl_raw->size();
    const auto [ptr, ec] = std::from_chars(input_as_char, input_as_char + input_length, ttl);
    if (ec != std::errc{} || ptr != input_as_char + input_length) {
        return std::unexpected(nitrokv::command::ParsingError::INVALID_INTEGER_FORMAT);
    }
    if (ttl < 0) {
        return std::unexpected(nitrokv::command::ParsingError::NEGATIVE_TTL_VALUE);
    }
    if (args.size() == 2U) {
        return nitrokv::command::ExpireCommand{*key, std::chrono::seconds{ttl}};
    }

    const auto comparison_arg = try_get_bulk_string_bytes(args[2]);
    if (!comparison_arg) {
        return std::unexpected(nitrokv::command::ParsingError::INVALID_ARGUMENT_TYPE);
    }

    nitrokv::command::ExpireCommand::ComparisonType ct;
    if (std::ranges::equal(std::as_bytes(std::span("NX", 2)), *comparison_arg)) {
        ct = nitrokv::command::ExpireCommand::ComparisonType ::NX;
    } else if (std::ranges::equal(std::as_bytes(std::span("XX", 2)), *comparison_arg)) {
        ct = nitrokv::command::ExpireCommand::ComparisonType ::XX;
    } else if (std::ranges::equal(std::as_bytes(std::span("GT", 2)), *comparison_arg)) {
        ct = nitrokv::command::ExpireCommand::ComparisonType ::GT;
    } else if (std::ranges::equal(std::as_bytes(std::span("LT", 2)), *comparison_arg)) {
        ct = nitrokv::command::ExpireCommand::ComparisonType ::LT;
    } else {
        return std::unexpected(nitrokv::command::ParsingError::UNKNOWN_PARAMETER_OPTION);
    }
    return nitrokv::command::ExpireCommand{*key, std::chrono::seconds{ttl}, ct};
}

struct CommandDescription {
    nitrokv::command::CmdParsingResult (*cmd_parser)(std::span<const nitrokv::protocol::RespValue>);
    struct {
        size_t min;
        size_t max;
    } arity;
};


constexpr auto COMMAND_TABLE = std::to_array<std::pair<std::string_view, CommandDescription>>({
    {"SET", {.cmd_parser = parse_set, .arity = {.min = 2, .max = 2}}},
    {"GET", {.cmd_parser = parse_get, .arity = {.min = 1, .max = 1}}},
    {"DEL", {.cmd_parser = parse_del, .arity = {.min = 1, .max = 1}}},
    {"PING", {.cmd_parser = parse_ping, .arity = {.min = 0, .max = 0}}},
    {"EXPIRE", {.cmd_parser = parse_expire, .arity = {.min = 2, .max = 3}}},
    {"TTL", {.cmd_parser = parse_ttl, .arity = {.min = 1, .max = 1}}},
    {"EXISTS", {.cmd_parser = parse_exists, .arity = {.min = 1, .max = 1}}},
});

} // namespace

namespace nitrokv::command {
CmdParsingResult parser(const protocol::RespValue& args_raw) {
    const auto* args = std::get_if<protocol::RespArray>(&args_raw.value);
    if (args == nullptr) {
        return std::unexpected(ParsingError::COMMAND_ARGS_IS_NOT_ARRAY);
    }
    if (args->empty()) {
        return std::unexpected(ParsingError::EMPTY_COMMAND);
    }

    const auto command = try_get_bulk_string_bytes((*args)[0]);
    if (!command) {
        return std::unexpected(ParsingError::INVALID_COMMAND_TYPE);
    }

    const char* cmd_as_char = reinterpret_cast<const char*>(command->data());
    const size_t cmd_length = command->size();
    std::string cmd_upper{cmd_as_char, cmd_length};
    std::ranges::transform(cmd_upper, cmd_upper.begin(), ::toupper);
    const auto* it = std::ranges::find(COMMAND_TABLE, cmd_upper,
                                       &std::pair<std::string_view, CommandDescription>::first);

    if (it == COMMAND_TABLE.end()) {
        return std::unexpected(ParsingError::UNKNOWN_COMMAND);
    }
    const auto& descriptor = it->second;
    if (size_t const count = args->size() - 1U;
        count < descriptor.arity.min || count > descriptor.arity.max) {
        return std::unexpected(ParsingError::INVALID_ARGUMENTS_NUM);
    }
    return descriptor.cmd_parser(std::span(*args).subspan(1));
}
} // namespace nitrokv::command
