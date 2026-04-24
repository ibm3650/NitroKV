//
// Created by nikita on 17.04.2026.
//

#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <ranges>
#include <span>
#include <string_view>
#include <vector>

#include "../include/nitrokv/command/commands.hpp"
#include "nitrokv/protocol/protocol.hpp"

namespace {

namespace proto = nitrokv::command;

using ByteSpan = std::span<const std::byte>;

ByteSpan bytes_view(std::string_view text) {
    return std::as_bytes(std::span{text.data(), text.size()});
}

bool bytes_equal(ByteSpan actual, std::string_view expected) {
    return std::ranges::equal(actual, bytes_view(expected));
}

nitrokv::protocol::RespValue make_bulk(std::string_view text) {
    return nitrokv::protocol::RespValue{.value =
                                            nitrokv::protocol::RespBulkString{bytes_view(text)}};
}

nitrokv::protocol::RespValue make_simple(std::string_view text) {
    return nitrokv::protocol::RespValue{.value = nitrokv::protocol::RespSimpleString{text}};
}

nitrokv::protocol::RespValue make_integer(nitrokv::protocol::RespInteger value) {
    return nitrokv::protocol::RespValue{.value = value};
}

nitrokv::protocol::RespValue
make_array(std::initializer_list<nitrokv::protocol::RespValue> values) {
    return nitrokv::protocol::RespValue{.value = nitrokv::protocol::RespArray{values}};
}

class CommandParserTest: public ::testing::Test {
protected:
    static void expect_error(const proto::CmdParsingResult& result, proto::ParsingError error) {
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), error);
    }
    static void check_has_value(const proto::CmdParsingResult& result) {
        ASSERT_TRUE(result.has_value());
    }
    template <typename T> static const T* expect_command(const proto::CmdParsingResult& result) {
        check_has_value(result);
        const auto* command = std::get_if<T>(&result.value());
        EXPECT_NE(command, nullptr);
        return command;
    }
};

} // namespace

// ==========================================
// Root-level format validation
// ==========================================

TEST_F(CommandParserTest, RejectsNonArrayRoot) {
    const auto result = nitrokv::command::parser(make_bulk("GET"));
    expect_error(result, proto::ParsingError::COMMAND_ARGS_IS_NOT_ARRAY);
}

TEST_F(CommandParserTest, RejectsEmptyCommandArray) {
    const auto result = nitrokv::command::parser(make_array({}));
    expect_error(result, proto::ParsingError::EMPTY_COMMAND);
}

TEST_F(CommandParserTest, RejectsNonBulkCommandName) {
    const auto result =
        nitrokv::command::parser(make_array({make_simple("GET"), make_bulk("key")}));
    expect_error(result, proto::ParsingError::INVALID_COMMAND_TYPE);
}

TEST_F(CommandParserTest, RejectsUnknownCommand) {
    const auto result = nitrokv::command::parser(make_array({make_bulk("MGET"), make_bulk("key")}));
    expect_error(result, proto::ParsingError::UNKNOWN_COMMAND);
}

// ==========================================
// PING
// ==========================================

TEST_F(CommandParserTest, ParsesPing) {
    const auto result = nitrokv::command::parser(make_array({make_bulk("PING")}));

    const auto* command = expect_command<proto::PingCommand>(result);
    ASSERT_NE(command, nullptr);
}

TEST_F(CommandParserTest, RejectsPingWithArguments) {
    const auto result =
        nitrokv::command::parser(make_array({make_bulk("PING"), make_bulk("payload")}));

    expect_error(result, proto::ParsingError::INVALID_ARGUMENTS_NUM);
}

// ==========================================
// GET
// ==========================================

TEST_F(CommandParserTest, ParsesGet) {
    const auto result =
        nitrokv::command::parser(make_array({make_bulk("GET"), make_bulk("user:42")}));

    const auto* command = expect_command<proto::GetCommand>(result);
    ASSERT_NE(command, nullptr);
    EXPECT_TRUE(bytes_equal(command->key, "user:42"));
}

TEST_F(CommandParserTest, RejectsGetWithWrongArity) {
    const auto result = nitrokv::command::parser(
        make_array({make_bulk("GET"), make_bulk("key"), make_bulk("extra")}));

    expect_error(result, proto::ParsingError::INVALID_ARGUMENTS_NUM);
}

TEST_F(CommandParserTest, RejectsGetWithNonBulkArgument) {
    const auto result = nitrokv::command::parser(make_array({make_bulk("GET"), make_integer(123)}));

    expect_error(result, proto::ParsingError::INVALID_ARGUMENT_TYPE);
}

// ==========================================
// DEL
// ==========================================

TEST_F(CommandParserTest, ParsesDel) {
    const auto result =
        nitrokv::command::parser(make_array({make_bulk("DEL"), make_bulk("cache-key")}));

    const auto* command = expect_command<proto::DelCommand>(result);
    ASSERT_NE(command, nullptr);
    EXPECT_TRUE(bytes_equal(command->key, "cache-key"));
}

// ==========================================
// TTL
// ==========================================

TEST_F(CommandParserTest, ParsesTtl) {
    const auto result =
        nitrokv::command::parser(make_array({make_bulk("TTL"), make_bulk("session")}));

    const auto* command = expect_command<proto::TtlCommand>(result);
    ASSERT_NE(command, nullptr);
    EXPECT_TRUE(bytes_equal(command->key, "session"));
}

// ==========================================
// EXISTS
// ==========================================

TEST_F(CommandParserTest, ParsesExists) {
    const auto result =
        nitrokv::command::parser(make_array({make_bulk("EXISTS"), make_bulk("feature-flag")}));

    const auto* command = expect_command<proto::ExistsCommand>(result);
    ASSERT_NE(command, nullptr);
    EXPECT_TRUE(bytes_equal(command->key, "feature-flag"));
}

// ==========================================
// SET
// ==========================================

TEST_F(CommandParserTest, ParsesSet) {
    const auto result = nitrokv::command::parser(
        make_array({make_bulk("SET"), make_bulk("token"), make_bulk("abc123")}));

    const auto* command = expect_command<proto::SetCommand>(result);
    ASSERT_NE(command, nullptr);
    EXPECT_TRUE(bytes_equal(command->key, "token"));
    EXPECT_TRUE(bytes_equal(command->value, "abc123"));
}

TEST_F(CommandParserTest, RejectsSetWithWrongArity) {
    const auto result =
        nitrokv::command::parser(make_array({make_bulk("SET"), make_bulk("token")}));

    expect_error(result, proto::ParsingError::INVALID_ARGUMENTS_NUM);
}

TEST_F(CommandParserTest, RejectsSetWithNonBulkKey) {
    const auto result = nitrokv::command::parser(
        make_array({make_bulk("SET"), make_integer(123), make_bulk("value")}));

    expect_error(result, proto::ParsingError::INVALID_ARGUMENT_TYPE);
}

TEST_F(CommandParserTest, RejectsSetWithNonBulkValue) {
    const auto result = nitrokv::command::parser(
        make_array({make_bulk("SET"), make_bulk("key"), make_integer(777)}));

    expect_error(result, proto::ParsingError::INVALID_ARGUMENT_TYPE);
}

// ==========================================
// EXPIRE
// ==========================================

TEST_F(CommandParserTest, ParsesExpireWithoutOption) {
    const auto result = nitrokv::command::parser(
        make_array({make_bulk("EXPIRE"), make_bulk("temp"), make_bulk("60")}));

    const auto* command = expect_command<proto::ExpireCommand>(result);
    ASSERT_NE(command, nullptr);
    EXPECT_TRUE(bytes_equal(command->key, "temp"));
    EXPECT_EQ(command->ttl_seconds, std::chrono::seconds{60});
    EXPECT_FALSE(command->comparison.has_value());
}

TEST_F(CommandParserTest, ParsesExpireWithNxOption) {
    const auto result = nitrokv::command::parser(
        make_array({make_bulk("EXPIRE"), make_bulk("temp"), make_bulk("60"), make_bulk("NX")}));

    const auto* command = expect_command<proto::ExpireCommand>(result);
    ASSERT_NE(command, nullptr);
    EXPECT_EQ(command->ttl_seconds, std::chrono::seconds{60});
    ASSERT_TRUE(command->comparison.has_value());
    EXPECT_EQ(*command->comparison, proto::ExpireCommand::ComparisonType::NX);
}

TEST_F(CommandParserTest, ParsesExpireWithXxOption) {
    const auto result = nitrokv::command::parser(
        make_array({make_bulk("EXPIRE"), make_bulk("temp"), make_bulk("60"), make_bulk("XX")}));

    const auto* command = expect_command<proto::ExpireCommand>(result);
    ASSERT_NE(command, nullptr);
    ASSERT_TRUE(command->comparison.has_value());
    EXPECT_EQ(*command->comparison, proto::ExpireCommand::ComparisonType::XX);
}

TEST_F(CommandParserTest, ParsesExpireWithGtOption) {
    const auto result = nitrokv::command::parser(
        make_array({make_bulk("EXPIRE"), make_bulk("temp"), make_bulk("60"), make_bulk("GT")}));

    const auto* command = expect_command<proto::ExpireCommand>(result);
    ASSERT_NE(command, nullptr);
    ASSERT_TRUE(command->comparison.has_value());
    EXPECT_EQ(*command->comparison, proto::ExpireCommand::ComparisonType::GT);
}

TEST_F(CommandParserTest, ParsesExpireWithLtOption) {
    const auto result = nitrokv::command::parser(
        make_array({make_bulk("EXPIRE"), make_bulk("temp"), make_bulk("60"), make_bulk("LT")}));

    const auto* command = expect_command<proto::ExpireCommand>(result);
    ASSERT_NE(command, nullptr);
    ASSERT_TRUE(command->comparison.has_value());
    EXPECT_EQ(*command->comparison, proto::ExpireCommand::ComparisonType::LT);
}

TEST_F(CommandParserTest, RejectsExpireWithWrongArity) {
    const auto result =
        nitrokv::command::parser(make_array({make_bulk("EXPIRE"), make_bulk("temp")}));

    expect_error(result, proto::ParsingError::INVALID_ARGUMENTS_NUM);
}

TEST_F(CommandParserTest, RejectsExpireWithNonBulkKey) {
    const auto result = nitrokv::command::parser(
        make_array({make_bulk("EXPIRE"), make_integer(1), make_bulk("60")}));

    expect_error(result, proto::ParsingError::INVALID_ARGUMENT_TYPE);
}

TEST_F(CommandParserTest, RejectsExpireWithNonBulkTtlArgument) {
    const auto result = nitrokv::command::parser(
        make_array({make_bulk("EXPIRE"), make_bulk("temp"), make_integer(60)}));

    expect_error(result, proto::ParsingError::INVALID_ARGUMENT_TYPE);
}

TEST_F(CommandParserTest, RejectsExpireWithInvalidIntegerFormat) {
    const auto result = nitrokv::command::parser(
        make_array({make_bulk("EXPIRE"), make_bulk("temp"), make_bulk("12x")}));

    expect_error(result, proto::ParsingError::INVALID_INTEGER_FORMAT);
}

TEST_F(CommandParserTest, RejectsExpireWithNegativeTtl) {
    const auto result = nitrokv::command::parser(
        make_array({make_bulk("EXPIRE"), make_bulk("temp"), make_bulk("-5")}));

    expect_error(result, proto::ParsingError::NEGATIVE_TTL_VALUE);
}

TEST_F(CommandParserTest, RejectsExpireWithUnknownOption) {
    const auto result = nitrokv::command::parser(
        make_array({make_bulk("EXPIRE"), make_bulk("temp"), make_bulk("60"), make_bulk("ZZ")}));

    expect_error(result, proto::ParsingError::UNKNOWN_PARAMETER_OPTION);
}

TEST_F(CommandParserTest, RejectsExpireWhenOptionHasWrongType) {
    const auto result = nitrokv::command::parser(
        make_array({make_bulk("EXPIRE"), make_bulk("temp"), make_bulk("60"), make_integer(1)}));

    expect_error(result, proto::ParsingError::INVALID_ARGUMENT_TYPE);
}

// ==========================================
// Command name normalization
// ==========================================

TEST_F(CommandParserTest, AcceptsLowercaseCommandName) {
    const auto result =
        nitrokv::command::parser(make_array({make_bulk("get"), make_bulk("alpha")}));

    const auto* command = expect_command<proto::GetCommand>(result);
    ASSERT_NE(command, nullptr);
    EXPECT_TRUE(bytes_equal(command->key, "alpha"));
}

TEST_F(CommandParserTest, AcceptsMixedCaseCommandName) {
    const auto result =
        nitrokv::command::parser(make_array({make_bulk("eXiStS"), make_bulk("alpha")}));

    const auto* command = expect_command<proto::ExistsCommand>(result);
    ASSERT_NE(command, nullptr);
    EXPECT_TRUE(bytes_equal(command->key, "alpha"));
}
