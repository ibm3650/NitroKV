#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cstddef>
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

        EXPECT_TRUE(result.remaining.empty());
        ASSERT_FALSE(std::holds_alternative<std::monostate>(result.value.value));

        validator(result.value);
    }

    void expect_decode_failure(const ByteVec& wire) {
        const auto result = proto::decode(as_input(wire));

        EXPECT_TRUE(std::ranges::equal(result.remaining, wire));
        ASSERT_TRUE(std::holds_alternative<std::monostate>(result.value.value));
    }
};

// ==========================================
// Integer decoding
// ==========================================

TEST_F(RespDecodeTest, DecodesPositiveInteger) {
    expect_decode_success(to_bytes(":342\r\n"), [](const proto::RespValue& value) {
        ASSERT_TRUE(std::holds_alternative<proto::RespInteger>(value.value));
        EXPECT_EQ(std::get<proto::RespInteger>(value.value), 342);
    });
}

TEST_F(RespDecodeTest, DecodesNegativeInteger) {
    expect_decode_success(to_bytes(":-342\r\n"), [](const proto::RespValue& value) {
        ASSERT_TRUE(std::holds_alternative<proto::RespInteger>(value.value));
        EXPECT_EQ(std::get<proto::RespInteger>(value.value), -342);
    });
}

TEST_F(RespDecodeTest, DecodesZeroInteger) {
    expect_decode_success(to_bytes(":0\r\n"), [](const proto::RespValue& value) {
        ASSERT_TRUE(std::holds_alternative<proto::RespInteger>(value.value));
        EXPECT_EQ(std::get<proto::RespInteger>(value.value), 0);
    });
}

TEST_F(RespDecodeTest, DecodesIntegerMaxValue) {
    expect_decode_success(to_bytes(":9223372036854775807\r\n"), [](const proto::RespValue& value) {
        ASSERT_TRUE(std::holds_alternative<proto::RespInteger>(value.value));
        EXPECT_EQ(std::get<proto::RespInteger>(value.value),
                  std::numeric_limits<std::int64_t>::max());
    });
}

TEST_F(RespDecodeTest, DecodesIntegerMinValue) {
    expect_decode_success(to_bytes(":-9223372036854775808\r\n"), [](const proto::RespValue& value) {
        ASSERT_TRUE(std::holds_alternative<proto::RespInteger>(value.value));
        EXPECT_EQ(std::get<proto::RespInteger>(value.value),
                  std::numeric_limits<std::int64_t>::min());
    });
}

TEST_F(RespDecodeTest, RejectsIntegerWithoutSeparator) {
    expect_decode_failure(to_bytes(":123"));
}

TEST_F(RespDecodeTest, RejectsIntegerWithBrokenSeparator) {
    expect_decode_failure(to_bytes(":123\rX"));
}

TEST_F(RespDecodeTest, RejectsIntegerWithInvalidDigits) {
    expect_decode_failure(to_bytes(":12a3\r\n"));
}

TEST_F(RespDecodeTest, RejectsTrailingDataAfterInteger) {
    expect_decode_failure(to_bytes(":1\r\n+OK\r\n"));
}

// ==========================================
// Simple string decoding
// ==========================================

TEST_F(RespDecodeTest, DecodesSimpleString) {
    expect_decode_success(to_bytes("+OK\r\n"), [](const proto::RespValue& value) {
        ASSERT_TRUE(std::holds_alternative<proto::RespSimpleString>(value.value));
        EXPECT_EQ(std::get<proto::RespSimpleString>(value.value).value, "OK");
    });
}

TEST_F(RespDecodeTest, DecodesEmptySimpleString) {
    expect_decode_success(to_bytes("+\r\n"), [](const proto::RespValue& value) {
        ASSERT_TRUE(std::holds_alternative<proto::RespSimpleString>(value.value));
        EXPECT_EQ(std::get<proto::RespSimpleString>(value.value).value, "");
    });
}

TEST_F(RespDecodeTest, RejectsSimpleStringWithoutSeparator) {
    expect_decode_failure(to_bytes("+PONG"));
}

TEST_F(RespDecodeTest, RejectsTrailingDataAfterSimpleString) {
    expect_decode_failure(to_bytes("+OK\r\n:1\r\n"));
}

// ==========================================
// Error decoding
// ==========================================

TEST_F(RespDecodeTest, DecodesErrorString) {
    expect_decode_success(to_bytes("-ERR unknown command\r\n"), [](const proto::RespValue& value) {
        ASSERT_TRUE(std::holds_alternative<proto::RespError>(value.value));
        EXPECT_EQ(std::get<proto::RespError>(value.value).value, "ERR unknown command");
    });
}

TEST_F(RespDecodeTest, DecodesEmptyErrorString) {
    expect_decode_success(to_bytes("-\r\n"), [](const proto::RespValue& value) {
        ASSERT_TRUE(std::holds_alternative<proto::RespError>(value.value));
        EXPECT_EQ(std::get<proto::RespError>(value.value).value, "");
    });
}

TEST_F(RespDecodeTest, RejectsTrailingDataAfterErrorString) {
    expect_decode_failure(to_bytes("-ERR bad\r\n+OK\r\n"));
}

// ==========================================
// Bulk string decoding
// ==========================================

TEST_F(RespDecodeTest, DecodesBulkString) {
    expect_decode_success(to_bytes("$5\r\nhello\r\n"), [](const proto::RespValue& value) {
        ASSERT_TRUE(std::holds_alternative<proto::RespBulkString>(value.value));
        const auto& bulk = std::get<proto::RespBulkString>(value.value).value;
        EXPECT_TRUE(std::ranges::equal(bulk, to_bytes("hello")));
    });
}

TEST_F(RespDecodeTest, DecodesEmptyBulkString) {
    expect_decode_success(to_bytes("$0\r\n\r\n"), [](const proto::RespValue& value) {
        ASSERT_TRUE(std::holds_alternative<proto::RespBulkString>(value.value));
        const auto& bulk = std::get<proto::RespBulkString>(value.value).value;
        EXPECT_TRUE(bulk.empty());
    });
}

TEST_F(RespDecodeTest, DecodesBinaryBulkString) {
    const ByteVec wire = bytes({b('$'), b('5'), b('\r'), b('\n'), std::byte{0x00}, b('\r'), b('\n'),
                                std::byte{0xFF}, b('A'), b('\r'), b('\n')});

    const ByteVec expected_payload =
        bytes({std::byte{0x00}, b('\r'), b('\n'), std::byte{0xFF}, b('A')});

    expect_decode_success(wire, [&](const proto::RespValue& value) {
        ASSERT_TRUE(std::holds_alternative<proto::RespBulkString>(value.value));
        const auto& bulk = std::get<proto::RespBulkString>(value.value).value;
        EXPECT_TRUE(std::ranges::equal(bulk, expected_payload));
    });
}

TEST_F(RespDecodeTest, DecodesNullBulkString) {
    expect_decode_success(to_bytes("$-1\r\n"), [](const proto::RespValue& value) {
        ASSERT_TRUE(std::holds_alternative<proto::RespNullBulkString>(value.value));
    });
}

TEST_F(RespDecodeTest, RejectsBulkStringWithLengthSmallerThanMinusOne) {
    expect_decode_failure(to_bytes("$-2\r\n"));
}

TEST_F(RespDecodeTest, RejectsBulkStringWithIncompletePayload) {
    expect_decode_failure(to_bytes("$5\r\nabc"));
}

TEST_F(RespDecodeTest, RejectsBulkStringWithoutTrailingSeparator) {
    expect_decode_failure(to_bytes("$5\r\nhelloX"));
}

TEST_F(RespDecodeTest, RejectsBulkStringWithBrokenHeaderSeparator) {
    expect_decode_failure(to_bytes("$5X\nhello\r\n"));
}

TEST_F(RespDecodeTest, RejectsTrailingDataAfterBulkString) {
    expect_decode_failure(to_bytes("$3\r\nkey\r\n:1\r\n"));
}

// ==========================================
// Array decoding
// ==========================================

TEST_F(RespDecodeTest, DecodesEmptyArray) {
    expect_decode_success(to_bytes("*0\r\n"), [](const proto::RespValue& value) {
        ASSERT_TRUE(std::holds_alternative<proto::RespArray>(value.value));
        const auto& array = std::get<proto::RespArray>(value.value);
        EXPECT_TRUE(array.empty());
    });
}

TEST_F(RespDecodeTest, DecodesNullArray) {
    expect_decode_success(to_bytes("*-1\r\n"), [](const proto::RespValue& value) {
        ASSERT_TRUE(std::holds_alternative<proto::RespNullArray>(value.value));
    });
}

TEST_F(RespDecodeTest, RejectsArrayLengthSmallerThanMinusOne) {
    expect_decode_failure(to_bytes("*-2\r\n"));
}

TEST_F(RespDecodeTest, DecodesFlatMixedArray) {
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

TEST_F(RespDecodeTest, DecodesNestedArrays) {
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

TEST_F(RespDecodeTest, DecodesArrayWithNullsAndError) {
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

TEST_F(RespDecodeTest, RejectsArrayWithTooFewElements) {
    expect_decode_failure(to_bytes("*2\r\n:1\r\n"));
}

TEST_F(RespDecodeTest, RejectsArrayWhenOneElementIsInvalid) {
    expect_decode_failure(to_bytes("*2\r\n:1\r\n+BAD\rX"));
}

TEST_F(RespDecodeTest, RejectsTrailingDataAfterArray) {
    expect_decode_failure(to_bytes("*1\r\n:1\r\n+OK\r\n"));
}

// ==========================================
// Prefix / malformed input
// ==========================================

TEST_F(RespDecodeTest, RejectsUnknownPrefix) {
    expect_decode_failure(to_bytes("!123\r\n"));
}

TEST_F(RespDecodeTest, RejectsEmptyInput) {
    expect_decode_failure(ByteVec{});
}

TEST_F(RespDecodeTest, RejectsOnlyPrefixWithoutPayload) {
    expect_decode_failure(to_bytes(":"));
    expect_decode_failure(to_bytes("+"));
    expect_decode_failure(to_bytes("$"));
    expect_decode_failure(to_bytes("*"));
    expect_decode_failure(to_bytes("-"));
}
} // namespace
