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

#include "nitrokv/protocol/commands.hpp" // Подставь свой реальный header
#include "nitrokv/protocol/protocol.hpp"

namespace {

namespace proto = nitrokv::protocol;

using ByteSpan = std::span<const std::byte>;

ByteSpan bytes_view(std::string_view text) {
    return std::as_bytes(std::span{text.data(), text.size()});
}

bool bytes_equal(ByteSpan actual, std::string_view expected) {
    return std::ranges::equal(actual, bytes_view(expected));
}

proto::RespValue make_bulk(std::string_view text) {
    return proto::RespValue{.value = proto::RespBulkString{bytes_view(text)}};
}

proto::RespValue make_simple(std::string_view text) {
    return proto::RespValue{.value = proto::RespSimpleString{text}};
}

proto::RespValue make_integer(proto::RespInteger value) {
    return proto::RespValue{.value = value};
}

proto::RespValue make_array(std::initializer_list<proto::RespValue> values) {
    return proto::RespValue{.value = proto::RespArray{values}};
}

class CommandParserTest: public ::testing::Test {
protected:
    static void expect_error(const CmdParsingResult& result, ParsingError error) {
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), error);
    }
    static void check_has_value(const CmdParsingResult& result) {
        ASSERT_TRUE(result.has_value());
    }
    template <typename T> static const T* expect_command(const CmdParsingResult& result) {
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
    const auto result = parser(make_bulk("GET"));
    expect_error(result, ParsingError::COMMAND_ARGS_IS_NOT_ARRAY);
}

TEST_F(CommandParserTest, RejectsEmptyCommandArray) {
    const auto result = parser(make_array({}));
    expect_error(result, ParsingError::EMPTY_COMMAND);
}

TEST_F(CommandParserTest, RejectsNonBulkCommandName) {
    const auto result = parser(make_array({make_simple("GET"), make_bulk("key")}));
    expect_error(result, ParsingError::INVALID_COMMAND_TYPE);
}

TEST_F(CommandParserTest, RejectsUnknownCommand) {
    const auto result = parser(make_array({make_bulk("MGET"), make_bulk("key")}));
    expect_error(result, ParsingError::UNKNOWN_COMMAND);
}

// ==========================================
// PING
// ==========================================

TEST_F(CommandParserTest, ParsesPing) {
    const auto result = parser(make_array({make_bulk("PING")}));

    const auto* command = expect_command<PingCommand>(result);
    ASSERT_NE(command, nullptr);
}

TEST_F(CommandParserTest, RejectsPingWithArguments) {
    const auto result = parser(make_array({make_bulk("PING"), make_bulk("payload")}));

    expect_error(result, ParsingError::INVALID_ARGUMENTS_NUM);
}

// ==========================================
// GET
// ==========================================

TEST_F(CommandParserTest, ParsesGet) {
    const auto result = parser(make_array({make_bulk("GET"), make_bulk("user:42")}));

    const auto* command = expect_command<GetCommand>(result);
    ASSERT_NE(command, nullptr);
    EXPECT_TRUE(bytes_equal(command->key, "user:42"));
}

TEST_F(CommandParserTest, RejectsGetWithWrongArity) {
    const auto result =
        parser(make_array({make_bulk("GET"), make_bulk("key"), make_bulk("extra")}));

    expect_error(result, ParsingError::INVALID_ARGUMENTS_NUM);
}

TEST_F(CommandParserTest, RejectsGetWithNonBulkArgument) {
    const auto result = parser(make_array({make_bulk("GET"), make_integer(123)}));

    expect_error(result, ParsingError::INVALID_ARGUMENT_TYPE);
}

// ==========================================
// DEL
// ==========================================

TEST_F(CommandParserTest, ParsesDel) {
    const auto result = parser(make_array({make_bulk("DEL"), make_bulk("cache-key")}));

    const auto* command = expect_command<DelCommand>(result);
    ASSERT_NE(command, nullptr);
    EXPECT_TRUE(bytes_equal(command->key, "cache-key"));
}

// ==========================================
// TTL
// ==========================================

TEST_F(CommandParserTest, ParsesTtl) {
    const auto result = parser(make_array({make_bulk("TTL"), make_bulk("session")}));

    const auto* command = expect_command<TtlCommand>(result);
    ASSERT_NE(command, nullptr);
    EXPECT_TRUE(bytes_equal(command->key, "session"));
}

// ==========================================
// EXISTS
// ==========================================

TEST_F(CommandParserTest, ParsesExists) {
    const auto result = parser(make_array({make_bulk("EXISTS"), make_bulk("feature-flag")}));

    const auto* command = expect_command<ExistsCommand>(result);
    ASSERT_NE(command, nullptr);
    EXPECT_TRUE(bytes_equal(command->key, "feature-flag"));
}

// ==========================================
// SET
// ==========================================

TEST_F(CommandParserTest, ParsesSet) {
    const auto result =
        parser(make_array({make_bulk("SET"), make_bulk("token"), make_bulk("abc123")}));

    const auto* command = expect_command<SetCommand>(result);
    ASSERT_NE(command, nullptr);
    EXPECT_TRUE(bytes_equal(command->key, "token"));
    EXPECT_TRUE(bytes_equal(command->value, "abc123"));
}

TEST_F(CommandParserTest, RejectsSetWithWrongArity) {
    const auto result = parser(make_array({make_bulk("SET"), make_bulk("token")}));

    expect_error(result, ParsingError::INVALID_ARGUMENTS_NUM);
}

TEST_F(CommandParserTest, RejectsSetWithNonBulkKey) {
    const auto result =
        parser(make_array({make_bulk("SET"), make_integer(123), make_bulk("value")}));

    expect_error(result, ParsingError::INVALID_ARGUMENT_TYPE);
}

TEST_F(CommandParserTest, RejectsSetWithNonBulkValue) {
    const auto result = parser(make_array({make_bulk("SET"), make_bulk("key"), make_integer(777)}));

    expect_error(result, ParsingError::INVALID_ARGUMENT_TYPE);
}

// ==========================================
// EXPIRE
// ==========================================

TEST_F(CommandParserTest, ParsesExpireWithoutOption) {
    const auto result =
        parser(make_array({make_bulk("EXPIRE"), make_bulk("temp"), make_bulk("60")}));

    const auto* command = expect_command<ExpireCommand>(result);
    ASSERT_NE(command, nullptr);
    EXPECT_TRUE(bytes_equal(command->key, "temp"));
    EXPECT_EQ(command->ttl_seconds, std::chrono::seconds{60});
    EXPECT_FALSE(command->comparison.has_value());
}

TEST_F(CommandParserTest, ParsesExpireWithNxOption) {
    const auto result = parser(
        make_array({make_bulk("EXPIRE"), make_bulk("temp"), make_bulk("60"), make_bulk("NX")}));

    const auto* command = expect_command<ExpireCommand>(result);
    ASSERT_NE(command, nullptr);
    EXPECT_EQ(command->ttl_seconds, std::chrono::seconds{60});
    ASSERT_TRUE(command->comparison.has_value());
    EXPECT_EQ(*command->comparison, ExpireCommand::ComparisonType::NX);
}

TEST_F(CommandParserTest, ParsesExpireWithXxOption) {
    const auto result = parser(
        make_array({make_bulk("EXPIRE"), make_bulk("temp"), make_bulk("60"), make_bulk("XX")}));

    const auto* command = expect_command<ExpireCommand>(result);
    ASSERT_NE(command, nullptr);
    ASSERT_TRUE(command->comparison.has_value());
    EXPECT_EQ(*command->comparison, ExpireCommand::ComparisonType::XX);
}

TEST_F(CommandParserTest, ParsesExpireWithGtOption) {
    const auto result = parser(
        make_array({make_bulk("EXPIRE"), make_bulk("temp"), make_bulk("60"), make_bulk("GT")}));

    const auto* command = expect_command<ExpireCommand>(result);
    ASSERT_NE(command, nullptr);
    ASSERT_TRUE(command->comparison.has_value());
    EXPECT_EQ(*command->comparison, ExpireCommand::ComparisonType::GT);
}

TEST_F(CommandParserTest, ParsesExpireWithLtOption) {
    const auto result = parser(
        make_array({make_bulk("EXPIRE"), make_bulk("temp"), make_bulk("60"), make_bulk("LT")}));

    const auto* command = expect_command<ExpireCommand>(result);
    ASSERT_NE(command, nullptr);
    ASSERT_TRUE(command->comparison.has_value());
    EXPECT_EQ(*command->comparison, ExpireCommand::ComparisonType::LT);
}

TEST_F(CommandParserTest, RejectsExpireWithWrongArity) {
    const auto result = parser(make_array({make_bulk("EXPIRE"), make_bulk("temp")}));

    expect_error(result, ParsingError::INVALID_ARGUMENTS_NUM);
}

TEST_F(CommandParserTest, RejectsExpireWithNonBulkKey) {
    const auto result = parser(make_array({make_bulk("EXPIRE"), make_integer(1), make_bulk("60")}));

    expect_error(result, ParsingError::INVALID_ARGUMENT_TYPE);
}

TEST_F(CommandParserTest, RejectsExpireWithNonBulkTtlArgument) {
    const auto result =
        parser(make_array({make_bulk("EXPIRE"), make_bulk("temp"), make_integer(60)}));

    expect_error(result, ParsingError::INVALID_ARGUMENT_TYPE);
}

TEST_F(CommandParserTest, RejectsExpireWithInvalidIntegerFormat) {
    const auto result =
        parser(make_array({make_bulk("EXPIRE"), make_bulk("temp"), make_bulk("12x")}));

    expect_error(result, ParsingError::INVALID_INTEGER_FORMAT);
}

TEST_F(CommandParserTest, RejectsExpireWithNegativeTtl) {
    const auto result =
        parser(make_array({make_bulk("EXPIRE"), make_bulk("temp"), make_bulk("-5")}));

    expect_error(result, ParsingError::NEGATIVE_TTL_VALUE);
}

TEST_F(CommandParserTest, RejectsExpireWithUnknownOption) {
    const auto result = parser(
        make_array({make_bulk("EXPIRE"), make_bulk("temp"), make_bulk("60"), make_bulk("ZZ")}));

    expect_error(result, ParsingError::UNKNOWN_PARAMETER_OPTION);
}

TEST_F(CommandParserTest, RejectsExpireWhenOptionHasWrongType) {
    const auto result = parser(
        make_array({make_bulk("EXPIRE"), make_bulk("temp"), make_bulk("60"), make_integer(1)}));

    expect_error(result, ParsingError::INVALID_ARGUMENT_TYPE);
}

// ==========================================
// Command name normalization
// ==========================================

TEST_F(CommandParserTest, AcceptsLowercaseCommandName) {
    const auto result = parser(make_array({make_bulk("get"), make_bulk("alpha")}));

    const auto* command = expect_command<GetCommand>(result);
    ASSERT_NE(command, nullptr);
    EXPECT_TRUE(bytes_equal(command->key, "alpha"));
}

TEST_F(CommandParserTest, AcceptsMixedCaseCommandName) {
    const auto result = parser(make_array({make_bulk("eXiStS"), make_bulk("alpha")}));

    const auto* command = expect_command<ExistsCommand>(result);
    ASSERT_NE(command, nullptr);
    EXPECT_TRUE(bytes_equal(command->key, "alpha"));
}
