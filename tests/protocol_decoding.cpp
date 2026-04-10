#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <ranges>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

#include "nitrokv/protocol/decoding.hpp"
#include "nitrokv/protocol/protocol.hpp"

namespace {
namespace proto = nitrokv::protocol;

using ByteVec = std::vector<std::byte>;

constexpr std::byte b(const char ch) noexcept {
    return static_cast<std::byte>(static_cast<unsigned char>(ch));
}

ByteVec to_bytes(std::string_view text) {
    ByteVec result;
    result.reserve(text.size());
    for (const char ch : text) {
        result.push_back(b(ch));
    }
    return result;
}

ByteVec bytes(std::initializer_list<std::byte> init) {
    return ByteVec(init);
}

class RespDecodeTest: public ::testing::Test {
protected:
    static std::span<const std::byte> as_input(const ByteVec& buffer) noexcept {
        return std::span<const std::byte>{buffer.data(), buffer.size()};
    }

    template <typename Validator>
    void expect_decode_success(const ByteVec& wire, Validator&& validator) {
        const auto result = proto::decode(as_input(wire));

        EXPECT_EQ(result.status, proto::RespDecodingStatus::SUCCESS);
        EXPECT_TRUE(result.remaining.empty());
        ASSERT_FALSE(std::holds_alternative<std::monostate>(result.value.value));

        validator(result.value);
    }

    void expect_decode_failure(const ByteVec& wire,
                               const proto::RespDecodingStatus expected_status) {
        const auto result = proto::decode(as_input(wire));

        EXPECT_EQ(result.status, expected_status);
        ASSERT_TRUE(std::holds_alternative<std::monostate>(result.value.value));
    }

    template <typename Validator>
    void expect_decode_first_success(const ByteVec& wire,
                                     std::string_view expected_remaining,
                                     Validator&& validator) {
        const auto result = proto::decode_first(as_input(wire));

        EXPECT_EQ(result.status, proto::RespDecodingStatus::SUCCESS);
        EXPECT_TRUE(std::ranges::equal(result.remaining, to_bytes(expected_remaining)));
        ASSERT_FALSE(std::holds_alternative<std::monostate>(result.value.value));

        validator(result.value);
    }

    void expect_decode_first_failure(const ByteVec& wire,
                                     const proto::RespDecodingStatus expected_status) {
        const auto result = proto::decode_first(as_input(wire));

        EXPECT_EQ(result.status, expected_status);
        EXPECT_TRUE(std::ranges::equal(result.remaining, wire));
        ASSERT_TRUE(std::holds_alternative<std::monostate>(result.value.value));
    }
};

// ==========================================
// Empty input / prefix handling
// ==========================================

TEST_F(RespDecodeTest, DecodeRejectsEmptyInput) {
    expect_decode_failure(ByteVec{}, proto::RespDecodingStatus::EMPTY_BUFFER);
}

TEST_F(RespDecodeTest, DecodeFirstRejectsEmptyInput) {
    expect_decode_first_failure(ByteVec{}, proto::RespDecodingStatus::EMPTY_BUFFER);
}

TEST_F(RespDecodeTest, DecodeRejectsUnknownPrefix) {
    expect_decode_failure(to_bytes("!123\r\n"), proto::RespDecodingStatus::INVALID_SYMBOL);
}

TEST_F(RespDecodeTest, DecodeFirstRejectsUnknownPrefix) {
    expect_decode_first_failure(to_bytes("!123\r\n"), proto::RespDecodingStatus::INVALID_SYMBOL);
}

// ==========================================
// Integer decoding
// ==========================================

TEST_F(RespDecodeTest, DecodeIntegerPositive) {
    expect_decode_success(to_bytes(":342\r\n"), [](const proto::RespValue& value) {
        ASSERT_TRUE(std::holds_alternative<proto::RespInteger>(value.value));
        EXPECT_EQ(std::get<proto::RespInteger>(value.value), 342);
    });
}

TEST_F(RespDecodeTest, DecodeIntegerNegative) {
    expect_decode_success(to_bytes(":-342\r\n"), [](const proto::RespValue& value) {
        ASSERT_TRUE(std::holds_alternative<proto::RespInteger>(value.value));
        EXPECT_EQ(std::get<proto::RespInteger>(value.value), -342);
    });
}

TEST_F(RespDecodeTest, DecodeIntegerZero) {
    expect_decode_success(to_bytes(":0\r\n"), [](const proto::RespValue& value) {
        ASSERT_TRUE(std::holds_alternative<proto::RespInteger>(value.value));
        EXPECT_EQ(std::get<proto::RespInteger>(value.value), 0);
    });
}

TEST_F(RespDecodeTest, DecodeIntegerMax) {
    expect_decode_success(to_bytes(":9223372036854775807\r\n"), [](const proto::RespValue& value) {
        ASSERT_TRUE(std::holds_alternative<proto::RespInteger>(value.value));
        EXPECT_EQ(std::get<proto::RespInteger>(value.value),
                  std::numeric_limits<std::int64_t>::max());
    });
}

TEST_F(RespDecodeTest, DecodeIntegerMin) {
    expect_decode_success(to_bytes(":-9223372036854775808\r\n"), [](const proto::RespValue& value) {
        ASSERT_TRUE(std::holds_alternative<proto::RespInteger>(value.value));
        EXPECT_EQ(std::get<proto::RespInteger>(value.value),
                  std::numeric_limits<std::int64_t>::min());
    });
}

TEST_F(RespDecodeTest, DecodeRejectsInvalidIntegerDigits) {
    expect_decode_failure(to_bytes(":12a3\r\n"), proto::RespDecodingStatus::INVALID_SEPARATOR);
}

TEST_F(RespDecodeTest, DecodeRejectsIntegerWithoutSeparator) {
    expect_decode_failure(to_bytes(":123"), proto::RespDecodingStatus::INVALID_SEPARATOR);
}

TEST_F(RespDecodeTest, DecodeRejectsIntegerWithBrokenSeparator) {
    expect_decode_failure(to_bytes(":123\rX"), proto::RespDecodingStatus::INVALID_SEPARATOR);
}

TEST_F(RespDecodeTest, DecodeRejectsExtraDataAfterInteger) {
    expect_decode_failure(to_bytes(":1\r\n+OK\r\n"), proto::RespDecodingStatus::EXTRA_DATA);
}

TEST_F(RespDecodeTest, DecodeFirstReturnsRemainingAfterInteger) {
    expect_decode_first_success(
        to_bytes(":1\r\n+OK\r\n"), "+OK\r\n", [](const proto::RespValue& value) {
            ASSERT_TRUE(std::holds_alternative<proto::RespInteger>(value.value));
            EXPECT_EQ(std::get<proto::RespInteger>(value.value), 1);
        });
}

// ==========================================
// Simple string decoding
// ==========================================

TEST_F(RespDecodeTest, DecodeSimpleStringBasic) {
    expect_decode_success(to_bytes("+OK\r\n"), [](const proto::RespValue& value) {
        ASSERT_TRUE(std::holds_alternative<proto::RespSimpleString>(value.value));
        EXPECT_EQ(std::get<proto::RespSimpleString>(value.value).value, "OK");
    });
}

TEST_F(RespDecodeTest, DecodeSimpleStringEmpty) {
    expect_decode_success(to_bytes("+\r\n"), [](const proto::RespValue& value) {
        ASSERT_TRUE(std::holds_alternative<proto::RespSimpleString>(value.value));
        EXPECT_EQ(std::get<proto::RespSimpleString>(value.value).value, "");
    });
}

TEST_F(RespDecodeTest, DecodeSimpleStringRejectsMissingSeparator) {
    expect_decode_failure(to_bytes("+PONG"), proto::RespDecodingStatus::UNEXPECTED_END);
}

TEST_F(RespDecodeTest, DecodeSimpleStringRejectsBrokenSeparator) {
    expect_decode_failure(to_bytes("+PONG\rX"), proto::RespDecodingStatus::INVALID_SYMBOL);
}

TEST_F(RespDecodeTest, DecodeSimpleStringRejectsExtraData) {
    expect_decode_failure(to_bytes("+OK\r\n:1\r\n"), proto::RespDecodingStatus::EXTRA_DATA);
}

TEST_F(RespDecodeTest, DecodeFirstReturnsRemainingAfterSimpleString) {
    expect_decode_first_success(
        to_bytes("+OK\r\n:1\r\n"), ":1\r\n", [](const proto::RespValue& value) {
            ASSERT_TRUE(std::holds_alternative<proto::RespSimpleString>(value.value));
            EXPECT_EQ(std::get<proto::RespSimpleString>(value.value).value, "OK");
        });
}

// ==========================================
// Error decoding
// ==========================================

TEST_F(RespDecodeTest, DecodeErrorBasic) {
    expect_decode_success(to_bytes("-ERR unknown command\r\n"), [](const proto::RespValue& value) {
        ASSERT_TRUE(std::holds_alternative<proto::RespError>(value.value));
        EXPECT_EQ(std::get<proto::RespError>(value.value).value, "ERR unknown command");
    });
}

TEST_F(RespDecodeTest, DecodeErrorEmpty) {
    expect_decode_success(to_bytes("-\r\n"), [](const proto::RespValue& value) {
        ASSERT_TRUE(std::holds_alternative<proto::RespError>(value.value));
        EXPECT_EQ(std::get<proto::RespError>(value.value).value, "");
    });
}

TEST_F(RespDecodeTest, DecodeErrorRejectsExtraData) {
    expect_decode_failure(to_bytes("-ERR bad\r\n+OK\r\n"), proto::RespDecodingStatus::EXTRA_DATA);
}

TEST_F(RespDecodeTest, DecodeFirstReturnsRemainingAfterError) {
    expect_decode_first_success(
        to_bytes("-ERR bad\r\n:5\r\n"), ":5\r\n", [](const proto::RespValue& value) {
            ASSERT_TRUE(std::holds_alternative<proto::RespError>(value.value));
            EXPECT_EQ(std::get<proto::RespError>(value.value).value, "ERR bad");
        });
}

// ==========================================
// Bulk string decoding
// ==========================================

TEST_F(RespDecodeTest, DecodeBulkStringBasic) {
    expect_decode_success(to_bytes("$5\r\nhello\r\n"), [](const proto::RespValue& value) {
        ASSERT_TRUE(std::holds_alternative<proto::RespBulkString>(value.value));
        const auto& payload = std::get<proto::RespBulkString>(value.value).value;
        EXPECT_TRUE(std::ranges::equal(payload, to_bytes("hello")));
    });
}

TEST_F(RespDecodeTest, DecodeBulkStringEmpty) {
    expect_decode_success(to_bytes("$0\r\n\r\n"), [](const proto::RespValue& value) {
        ASSERT_TRUE(std::holds_alternative<proto::RespBulkString>(value.value));
        EXPECT_TRUE(std::get<proto::RespBulkString>(value.value).value.empty());
    });
}

TEST_F(RespDecodeTest, DecodeBulkStringNull) {
    expect_decode_success(to_bytes("$-1\r\n"), [](const proto::RespValue& value) {
        ASSERT_TRUE(std::holds_alternative<proto::RespNullBulkString>(value.value));
    });
}

TEST_F(RespDecodeTest, DecodeBulkStringBinaryPayload) {
    const ByteVec wire = bytes({b('$'), b('5'), b('\r'), b('\n'), std::byte{0x00}, b('\r'), b('\n'),
                                std::byte{0xFF}, b('A'), b('\r'), b('\n')});

    const ByteVec expected_payload =
        bytes({std::byte{0x00}, b('\r'), b('\n'), std::byte{0xFF}, b('A')});

    expect_decode_success(wire, [&](const proto::RespValue& value) {
        ASSERT_TRUE(std::holds_alternative<proto::RespBulkString>(value.value));
        const auto& payload = std::get<proto::RespBulkString>(value.value).value;
        EXPECT_TRUE(std::ranges::equal(payload, expected_payload));
    });
}

TEST_F(RespDecodeTest, DecodeBulkStringRejectsLengthLessThanMinusOne) {
    expect_decode_failure(to_bytes("$-2\r\n"), proto::RespDecodingStatus::INVALID_INTEGER);
}

TEST_F(RespDecodeTest, DecodeBulkStringRejectsBrokenHeaderSeparator) {
    expect_decode_failure(to_bytes("$5X\nhello\r\n"), proto::RespDecodingStatus::INVALID_SEPARATOR);
}

TEST_F(RespDecodeTest, DecodeBulkStringRejectsIncompletePayload) {
    expect_decode_failure(to_bytes("$5\r\nabc"), proto::RespDecodingStatus::UNEXPECTED_END);
}

TEST_F(RespDecodeTest, DecodeBulkStringRejectsBrokenTrailingSeparator) {
    expect_decode_failure(to_bytes("$5\r\nhello\rX"), proto::RespDecodingStatus::INVALID_SEPARATOR);
}

TEST_F(RespDecodeTest, DecodeBulkStringRejectsExtraData) {
    expect_decode_failure(to_bytes("$3\r\nkey\r\n:1\r\n"), proto::RespDecodingStatus::EXTRA_DATA);
}

TEST_F(RespDecodeTest, DecodeFirstReturnsRemainingAfterBulkString) {
    expect_decode_first_success(
        to_bytes("$3\r\nkey\r\n:1\r\n"), ":1\r\n", [](const proto::RespValue& value) {
            ASSERT_TRUE(std::holds_alternative<proto::RespBulkString>(value.value));
            EXPECT_TRUE(std::ranges::equal(std::get<proto::RespBulkString>(value.value).value,
                                           to_bytes("key")));
        });
}

// ==========================================
// Array decoding
// ==========================================

TEST_F(RespDecodeTest, DecodeArrayEmpty) {
    expect_decode_success(to_bytes("*0\r\n"), [](const proto::RespValue& value) {
        ASSERT_TRUE(std::holds_alternative<proto::RespArray>(value.value));
        EXPECT_TRUE(std::get<proto::RespArray>(value.value).empty());
    });
}

TEST_F(RespDecodeTest, DecodeArrayNull) {
    expect_decode_success(to_bytes("*-1\r\n"), [](const proto::RespValue& value) {
        ASSERT_TRUE(std::holds_alternative<proto::RespNullArray>(value.value));
    });
}

TEST_F(RespDecodeTest, DecodeArrayRejectsLengthLessThanMinusOne) {
    expect_decode_failure(to_bytes("*-2\r\n"), proto::RespDecodingStatus::INVALID_INTEGER);
}

TEST_F(RespDecodeTest, DecodeArrayFlatMixed) {
    expect_decode_success(
        to_bytes("*3\r\n:42\r\n+OK\r\n$3\r\nkey\r\n"), [](const proto::RespValue& value) {
            ASSERT_TRUE(std::holds_alternative<proto::RespArray>(value.value));
            const auto& array = std::get<proto::RespArray>(value.value);

            ASSERT_EQ(array.size(), 3U);

            ASSERT_TRUE(std::holds_alternative<proto::RespInteger>(array[0].value));
            EXPECT_EQ(std::get<proto::RespInteger>(array[0].value), 42);

            ASSERT_TRUE(std::holds_alternative<proto::RespSimpleString>(array[1].value));
            EXPECT_EQ(std::get<proto::RespSimpleString>(array[1].value).value, "OK");

            ASSERT_TRUE(std::holds_alternative<proto::RespBulkString>(array[2].value));
            EXPECT_TRUE(std::ranges::equal(std::get<proto::RespBulkString>(array[2].value).value,
                                           to_bytes("key")));
        });
}

TEST_F(RespDecodeTest, DecodeArrayNested) {
    expect_decode_success(to_bytes("*2\r\n*1\r\n:1\r\n+OK\r\n"), [](const proto::RespValue& value) {
        ASSERT_TRUE(std::holds_alternative<proto::RespArray>(value.value));
        const auto& outer = std::get<proto::RespArray>(value.value);
        ASSERT_EQ(outer.size(), 2U);

        ASSERT_TRUE(std::holds_alternative<proto::RespArray>(outer[0].value));
        const auto& inner = std::get<proto::RespArray>(outer[0].value);
        ASSERT_EQ(inner.size(), 1U);

        ASSERT_TRUE(std::holds_alternative<proto::RespInteger>(inner[0].value));
        EXPECT_EQ(std::get<proto::RespInteger>(inner[0].value), 1);

        ASSERT_TRUE(std::holds_alternative<proto::RespSimpleString>(outer[1].value));
        EXPECT_EQ(std::get<proto::RespSimpleString>(outer[1].value).value, "OK");
    });
}

TEST_F(RespDecodeTest, DecodeArrayWithNullsAndError) {
    expect_decode_success(
        to_bytes("*3\r\n$-1\r\n-ERR foo\r\n*-1\r\n"), [](const proto::RespValue& value) {
            ASSERT_TRUE(std::holds_alternative<proto::RespArray>(value.value));
            const auto& array = std::get<proto::RespArray>(value.value);
            ASSERT_EQ(array.size(), 3U);

            ASSERT_TRUE(std::holds_alternative<proto::RespNullBulkString>(array[0].value));

            ASSERT_TRUE(std::holds_alternative<proto::RespError>(array[1].value));
            EXPECT_EQ(std::get<proto::RespError>(array[1].value).value, "ERR foo");

            ASSERT_TRUE(std::holds_alternative<proto::RespNullArray>(array[2].value));
        });
}

TEST_F(RespDecodeTest, DecodeArrayRejectsTooFewElements) {
    expect_decode_failure(to_bytes("*2\r\n:1\r\n"), proto::RespDecodingStatus::UNEXPECTED_END);
}

TEST_F(RespDecodeTest, DecodeArrayRejectsInvalidElement) {
    expect_decode_failure(to_bytes("*2\r\n:1\r\n!bad\r\n"),
                          proto::RespDecodingStatus::INVALID_SYMBOL);
}

TEST_F(RespDecodeTest, DecodeArrayRejectsExtraData) {
    expect_decode_failure(to_bytes("*1\r\n:1\r\n+OK\r\n"), proto::RespDecodingStatus::EXTRA_DATA);
}

TEST_F(RespDecodeTest, DecodeFirstReturnsRemainingAfterArray) {
    expect_decode_first_success(
        to_bytes("*1\r\n:1\r\n+OK\r\n"), "+OK\r\n", [](const proto::RespValue& value) {
            ASSERT_TRUE(std::holds_alternative<proto::RespArray>(value.value));
            const auto& array = std::get<proto::RespArray>(value.value);
            ASSERT_EQ(array.size(), 1U);
            ASSERT_TRUE(std::holds_alternative<proto::RespInteger>(array[0].value));
            EXPECT_EQ(std::get<proto::RespInteger>(array[0].value), 1);
        });
}
} // namespace
