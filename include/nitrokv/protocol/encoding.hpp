//
// Created by nikita on 22.03.2026.
//

#pragma once

#include <vector>

#include "nitrokv/protocol/protocol.hpp"
namespace nitrokv::protocol {


bool encode(RespInteger val, std::vector<std::byte>& buffer) noexcept;
bool encode(RespSimpleString val, std::vector<std::byte>& buffer) noexcept;
bool encode(RespError val, std::vector<std::byte>& buffer) noexcept;
bool encode(const RespArray& payload, std::vector<std::byte>& buffer) noexcept;
bool encode(RespBulkString val, std::vector<std::byte>& buffer) noexcept;
bool encode(RespNullBulkString val, std::vector<std::byte>& buffer) noexcept;
bool encode(RespNullArray val, std::vector<std::byte>& buffer) noexcept;
}; // namespace nitrokv::protocol
