
/**
* @file commands.hpp
 * @date 17.04.2026
 * @author nikita
 */
#pragma once
#include <chrono>
#include <expected>
#include <optional>
#include <variant>

#include "nitrokv/protocol/protocol.hpp"

namespace nitrokv::command {
struct SetCommand {
    std::span<const std::byte> key;
    std::span<const std::byte> value;
};


struct GetCommand {
    std::span<const std::byte> key;
};

struct DelCommand {
    std::span<const std::byte> key;
};
struct TtlCommand {
    std::span<const std::byte> key;
};
struct ExistsCommand {
    std::span<const std::byte> key;
};


struct PingCommand {};


struct ExpireCommand {
    enum class ComparisonType : uint8_t { NX, XX, GT, LT };
    std::span<const std::byte> key;
    std::chrono::seconds ttl_seconds;
    std::optional<ComparisonType> comparison;
};

using Command = std::variant<SetCommand,
                             GetCommand,
                             DelCommand,
                             PingCommand,
                             ExpireCommand,
                             TtlCommand,
                             ExistsCommand>;


enum class ParsingError : uint8_t {
    INVALID_ARGUMENT_TYPE,
    INVALID_INTEGER_FORMAT,
    NEGATIVE_TTL_VALUE,
    UNKNOWN_PARAMETER_OPTION,
    INVALID_COMMAND_TYPE,
    COMMAND_ARGS_IS_NOT_ARRAY,
    EMPTY_COMMAND,
    UNKNOWN_COMMAND,
    INVALID_ARGUMENTS_NUM
};
using CmdParsingResult = std::expected<Command, ParsingError>;

CmdParsingResult parser(const protocol::RespValue& args_raw);

} // namespace nitrokv::protocol
