/**
 * @file dispatcher.cpp
 * @date 21.04.2026
 * @author nikita
 */
#include "nitrokv/dispatcher/dispatcher.hpp"
#include "nitrokv/command/commands.hpp"
#include "nitrokv/core/cache.hpp"
#include "nitrokv/core/results.hpp"

namespace {
using CommandResult = std::expected<nitrokv::protocol::RespValue, nitrokv::core::CoreStatusCode>;


CommandResult get_handle(const nitrokv::core::BytesViewT key) noexcept {
    auto val = nitrokv::core::cache_get(key);
    if (!val) {
        return nitrokv::protocol::RespValue{nitrokv::protocol::RespNullBulkString{}};
    }
    return nitrokv::protocol::RespValue{nitrokv::protocol::RespBulkString{*val}};
}

CommandResult set_handle(const nitrokv::core::BytesViewT key,
                         const nitrokv::core::BytesViewT value) noexcept {
    if (const auto result = nitrokv::core::cache_push(key, value);
        result != nitrokv::core::CoreStatusCode::NO_ERRORS) {
        return std::unexpected(result);
    }
    return nitrokv::protocol::RespValue{nitrokv::protocol::RespSimpleString{"OK"}};
}

CommandResult del_handle(const nitrokv::core::BytesViewT key) noexcept {
    return nitrokv::protocol::RespValue{
        nitrokv::protocol::RespInteger{nitrokv::core::cache_pop(key)}};
}

CommandResult expire_handle(const nitrokv::core::BytesViewT key,
                            const std::chrono::seconds ttl) noexcept {
    return nitrokv::protocol::RespValue{
        nitrokv::protocol::RespInteger{nitrokv::core::cache_set_ttl(key, ttl)}};
}

CommandResult ping_handle() noexcept {
    return nitrokv::protocol::RespValue{nitrokv::protocol::RespSimpleString{"PONG"}};
}

CommandResult exists_handle(const nitrokv::core::BytesViewT key) noexcept {
    return nitrokv::protocol::RespValue{
        nitrokv::protocol::RespInteger{nitrokv::core::cache_contains(key)}};
}


CommandResult ttl_handle(const nitrokv::core::BytesViewT key){
    const auto result = nitrokv::core::cache_get_ttl(key);
    if (!result) {
        switch (result.error()) {
            case nitrokv::core::CoreStatusCode::KEY_NOT_FOUND:
                return nitrokv::protocol::RespValue{nitrokv::protocol::RespInteger{-2}};
            case nitrokv::core::CoreStatusCode::TTL_NOT_SET:
                return nitrokv::protocol::RespValue{nitrokv::protocol::RespInteger{-1}};
            default:
                return std::unexpected(result.error());
        }
    }
    return nitrokv::protocol::RespValue{nitrokv::protocol::RespInteger{result.value().count()}};
}
template <typename T>
inline constexpr bool dependent_false_v = false;
}; // namespace

namespace nitrokv::dispatcher {


[[nodiscard]] nitrokv::protocol::RespValue dispatch(const command::Command& cmd_variant) {
    auto result = std::visit(
        [](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, command::GetCommand>) {
                return get_handle(arg.key);
            } else if constexpr (std::is_same_v<T, command::SetCommand>) {
                return set_handle(arg.key, arg.value);
            } else if constexpr (std::is_same_v<T, command::DelCommand>) {
                return del_handle(arg.key);
            } else if constexpr (std::is_same_v<T, command::TtlCommand>) {
                return ttl_handle(arg.key);
            } else if constexpr (std::is_same_v<T, command::ExistsCommand>) {
                return exists_handle(arg.key);
            } else if constexpr (std::is_same_v<T, command::ExpireCommand>) {
                return expire_handle(arg.key, arg.ttl_seconds);
            } else if constexpr (std::is_same_v<T, command::PingCommand>) {
                return ping_handle();
            } else {
                static_assert(dependent_false_v<T>, "Unhandled command type in dispatcher");
            }
        },
        cmd_variant);
    if (result) {
        return result.value();
    }
    switch (result.error()) {
        case nitrokv::core::CoreStatusCode::OUT_OF_MEMORY:
            return nitrokv::protocol::RespValue{protocol::RespError{"Out of memory"}};
        default:
            return nitrokv::protocol::RespValue{protocol::RespError{"Unhandled error"}};
    }
}


} // namespace nitrokv::dispatcher
