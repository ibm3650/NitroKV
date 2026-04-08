#include <algorithm>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <random>
#include <span>
#include <string_view>
#include <vector>

#include "nitrokv/protocol/encoding.hpp"
#include "nitrokv/protocol/protocol.hpp"

namespace {

namespace proto = nitrokv::protocol;

using ByteVec = std::vector<std::byte>;

ByteVec to_bytes(std::string_view text) {
    ByteVec result;
    result.reserve(text.size());
    for (const char ch : text) {
        result.push_back(static_cast<std::byte>(ch));
    }
    return result;
}

ByteVec make_resp_scalar(char prefix, std::string_view body) {
    ByteVec result;
    result.reserve(1 + body.size() + 2);
    result.push_back(static_cast<std::byte>(prefix));
    for (const char ch : body) {
        result.push_back(static_cast<std::byte>(ch));
    }
    result.push_back(static_cast<std::byte>('\r'));
    result.push_back(static_cast<std::byte>('\n'));
    return result;
}

ByteVec generate_random_bytes(std::size_t size) {
    ByteVec result(size);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 255);

    std::ranges::generate(result, [&]() { return static_cast<std::byte>(dist(gen)); });

    return result;
}

class RespEncodeTestBase: public ::testing::Test {
protected:
    ByteVec buffer_;

    template <typename T> void expect_encoded(const T& value, std::string_view expected_wire) {
        const bool success = proto::encode(value, buffer_);
        EXPECT_TRUE(success);
        EXPECT_EQ(buffer_, to_bytes(expected_wire));
    }

    template <typename T> void expect_encode_fail(const T& value) {
        const ByteVec before = buffer_;
        const bool success = proto::encode(value, buffer_);
        EXPECT_FALSE(success);
        EXPECT_EQ(buffer_, before);
    }

    void set_buffer(std::string_view initial_wire) {
        buffer_ = to_bytes(initial_wire);
    }
};

class RespIntegerEncodeTest: public RespEncodeTestBase {};
class RespBulkStringEncodeTest: public RespEncodeTestBase {};
class RespSimpleStringEncodeTest: public RespEncodeTestBase {};
class RespArrayEncodeTest: public RespEncodeTestBase {};
class RespErrorEncodeTest: public RespEncodeTestBase {};
class RespNullEncodeTest: public RespEncodeTestBase {};
class RespEncodeIntegrationTest: public RespEncodeTestBase {};

} // namespace

// ==========================================
// Integer encoding
// ==========================================

TEST_F(RespIntegerEncodeTest, EncodesPositiveValue) {
    expect_encoded(proto::RespInteger{342}, ":342\r\n");
}

TEST_F(RespIntegerEncodeTest, EncodesNegativeValue) {
    expect_encoded(proto::RespInteger{-342}, ":-342\r\n");
}

TEST_F(RespIntegerEncodeTest, EncodesZero) {
    expect_encoded(proto::RespInteger{0}, ":0\r\n");
}

TEST_F(RespIntegerEncodeTest, EncodesMaxValue) {
    expect_encoded(std::numeric_limits<proto::RespInteger>::max(), ":9223372036854775807\r\n");
}

TEST_F(RespIntegerEncodeTest, EncodesMinValue) {
    expect_encoded(std::numeric_limits<proto::RespInteger>::min(), ":-9223372036854775808\r\n");
}

// ==========================================
// Bulk string encoding
// ==========================================

TEST_F(RespBulkStringEncodeTest, EncodesSmallTextPayload) {
    constexpr std::string_view text = "NitroKV";
    expect_encoded(proto::RespBulkString{std::as_bytes(std::span{text})}, "$7\r\nNitroKV\r\n");
}

TEST_F(RespBulkStringEncodeTest, EncodesEmptyPayload) {
    constexpr std::string_view text = "";
    expect_encoded(proto::RespBulkString{std::as_bytes(std::span{text})}, "$0\r\n\r\n");
}

TEST_F(RespBulkStringEncodeTest, AppendsToExistingBuffer) {
    set_buffer("*2\r\n");

    constexpr std::string_view text = "key";
    const bool success =
        proto::encode(proto::RespBulkString{std::as_bytes(std::span{text})}, buffer_);

    EXPECT_TRUE(success);
    EXPECT_EQ(buffer_, to_bytes("*2\r\n$3\r\nkey\r\n"));
}

TEST_F(RespBulkStringEncodeTest, EncodesBinaryRandomPayload) {
    const ByteVec payload = generate_random_bytes(1000);

    const bool success = proto::encode(proto::RespBulkString{payload}, buffer_);

    EXPECT_TRUE(success);
    EXPECT_EQ(buffer_.size(), 1009U);

    const ByteVec expected_header = to_bytes("$1000\r\n");
    const ByteVec expected_tail = to_bytes("\r\n");

    EXPECT_TRUE(std::equal(expected_header.begin(), expected_header.end(), buffer_.begin()));
    EXPECT_TRUE(std::equal(expected_tail.begin(), expected_tail.end(), buffer_.end() - 2));
}

TEST_F(RespBulkStringEncodeTest, RejectsPayloadLargerThanLimit) {
    set_buffer("initial_state");

    constexpr std::size_t overflow_size = (512ULL * 1024 * 1024) + 1;
    const std::span<const std::byte> fake_huge_payload(static_cast<const std::byte*>(nullptr),
                                                       overflow_size);

    expect_encode_fail(proto::RespBulkString{fake_huge_payload});
}

// ==========================================
// Simple string encoding
// ==========================================

TEST_F(RespSimpleStringEncodeTest, EncodesBasicText) {
    expect_encoded(proto::RespSimpleString{"OK"}, "+OK\r\n");
}

TEST_F(RespSimpleStringEncodeTest, EncodesPongResponse) {
    expect_encoded(proto::RespSimpleString{"PONG"}, "+PONG\r\n");
}

TEST_F(RespSimpleStringEncodeTest, EncodesEmptyString) {
    expect_encoded(proto::RespSimpleString{""}, "+\r\n");
}

TEST_F(RespSimpleStringEncodeTest, AppendsToExistingBuffer) {
    set_buffer("*2\r\n");

    const bool success = proto::encode(proto::RespSimpleString{"QUEUED"}, buffer_);

    EXPECT_TRUE(success);
    EXPECT_EQ(buffer_, to_bytes("*2\r\n+QUEUED\r\n"));
}

TEST_F(RespSimpleStringEncodeTest, RejectsCarriageReturnAndLineFeedInjection) {
    expect_encode_fail(proto::RespSimpleString{"Hacked\r\n+OK"});
    expect_encode_fail(proto::RespSimpleString{"Line1\nLine2"});
    expect_encode_fail(proto::RespSimpleString{"Carriage\rReturn"});
}

// ==========================================
// Array encoding
// ==========================================

TEST_F(RespArrayEncodeTest, EncodesEmptyArray) {
    proto::RespArray array;
    expect_encoded(array, "*0\r\n");
}

TEST_F(RespArrayEncodeTest, EncodesFlatMixedArray) {
    proto::RespArray array;
    array.emplace_back(
        proto::RespValue{proto::RespBulkString{std::as_bytes(std::span{"Nitro\r\n", 7})}});
    array.emplace_back(proto::RespValue{proto::RespInteger{42}});

    const bool success = proto::encode(array, buffer_);

    EXPECT_TRUE(success);
    EXPECT_EQ(buffer_, to_bytes("*2\r\n$7\r\nNitro\r\n\r\n:42\r\n"));
}

TEST_F(RespArrayEncodeTest, EncodesNestedArrays) {
    proto::RespArray inner_array;
    inner_array.emplace_back(proto::RespValue{proto::RespInteger{1}});

    proto::RespArray outer_array;
    outer_array.emplace_back(proto::RespValue{std::move(inner_array)});
    outer_array.emplace_back(proto::RespValue{proto::RespSimpleString{"OK"}});

    expect_encoded(outer_array, "*2\r\n*1\r\n:1\r\n+OK\r\n");
}

TEST_F(RespArrayEncodeTest, AppendsToExistingBuffer) {
    set_buffer("+QUEUED\r\n");

    proto::RespArray array;
    array.emplace_back(proto::RespValue{proto::RespInteger{100}});

    const bool success = proto::encode(array, buffer_);

    EXPECT_TRUE(success);
    EXPECT_EQ(buffer_, to_bytes("+QUEUED\r\n*1\r\n:100\r\n"));
}

// ==========================================
// Error encoding
// ==========================================

TEST_F(RespErrorEncodeTest, EncodesStandardError) {
    expect_encoded(proto::RespError{"ERR unknown command 'foobar'"},
                   "-ERR unknown command 'foobar'\r\n");
}

TEST_F(RespErrorEncodeTest, EncodesEmptyError) {
    expect_encoded(proto::RespError{""}, "-\r\n");
}

TEST_F(RespErrorEncodeTest, RejectsCarriageReturnAndLineFeedInjection) {
    expect_encode_fail(proto::RespError{"ERR message\rhacked"});
    expect_encode_fail(proto::RespError{"ERR message\nhacked"});
    expect_encode_fail(proto::RespError{"ERR\r\n+OK\r\n"});
}

TEST_F(RespErrorEncodeTest, AppendsToExistingBuffer) {
    set_buffer("*2\r\n");

    const bool success = proto::encode(proto::RespError{"WRONGTYPE"}, buffer_);

    EXPECT_TRUE(success);
    EXPECT_EQ(buffer_, to_bytes("*2\r\n-WRONGTYPE\r\n"));
}

// ==========================================
// Null encoding
// ==========================================

TEST_F(RespNullEncodeTest, EncodesNullBulkString) {
    expect_encoded(proto::RespNullBulkString{}, "$-1\r\n");
}

TEST_F(RespNullEncodeTest, EncodesNullArray) {
    expect_encoded(proto::RespNullArray{}, "*-1\r\n");
}

TEST_F(RespNullEncodeTest, AppendsNullValuesToExistingBuffer) {
    set_buffer("*3\r\n");

    const bool success_bulk = proto::encode(proto::RespNullBulkString{}, buffer_);
    const bool success_array = proto::encode(proto::RespNullArray{}, buffer_);

    EXPECT_TRUE(success_bulk);
    EXPECT_TRUE(success_array);
    EXPECT_EQ(buffer_, to_bytes("*3\r\n$-1\r\n*-1\r\n"));
}

// ==========================================
// Integration / mixed scenarios
// ==========================================

TEST_F(RespEncodeIntegrationTest, EncodesArrayWithMixedNullsAndErrors) {
    proto::RespArray array;
    array.reserve(3);
    array.push_back(proto::RespValue{proto::RespNullBulkString{}});
    array.push_back(proto::RespValue{proto::RespError{"ERR foo"}});
    array.push_back(proto::RespValue{proto::RespNullArray{}});

    expect_encoded(array, "*3\r\n$-1\r\n-ERR foo\r\n*-1\r\n");
}

TEST_F(RespEncodeIntegrationTest, RollsBackArrayOnElementFailure) {
    set_buffer("PREVIOUS_DATA");
    const ByteVec expected = buffer_;

    proto::RespArray array;
    array.push_back(proto::RespValue{proto::RespNullBulkString{}});
    array.push_back(proto::RespValue{proto::RespError{"HACK\r\nED"}});
    array.push_back(proto::RespValue{proto::RespNullArray{}});

    const bool success = proto::encode(array, buffer_);

    EXPECT_FALSE(success);
    EXPECT_EQ(buffer_, expected);
}
