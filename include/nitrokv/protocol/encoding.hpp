//
// Created by nikita on 22.03.2026.
//

#pragma once

#include <cstdint>
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
// template<typename T>
// requires std::is_same_v<std::remove_cv_t<std::remove_reference_t<T>>, RespEncNullBulkString> ||
//          std::is_same_v<std::remove_cv_t<std::remove_reference_t<T>>, RespEncNullArray>
// bool encode(T /*unused*/, std::vector<std::byte>& buffer) {
//     constexpr std::array payload = {std::byte{'-'}, std::byte{'1'}, std::byte{'\r'},
//                                     std::byte{'\n'}};
//     // if (!::reserve_space(payload.size() + PREFIX_LENGTH, buffer)) [[unlikely]] {
//     //     return false;
//     // }
//     if constexpr (std::same_as<std::remove_cvref_t<T>, RespEncNullBulkString>) {
//         buffer.push_back(static_cast<std::byte>(RespTypePrefix::BULK_STRING));
//     }else {
//         buffer.push_back(static_cast<std::byte>(RespTypePrefix::ARRAY));
//     }
//     buffer.insert(buffer.end(), payload.begin(), payload.end());
//     return true;
// }
}; // namespace nitrokv::protocol
