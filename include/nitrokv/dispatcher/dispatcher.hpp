
/**
 * @file dispatcher.hpp
 * @date 21.04.2026
 * @author nikita
 */
#pragma once
#include <variant>

#include "nitrokv/command/commands.hpp"
#include "nitrokv/core/results.hpp"

namespace nitrokv::dispatcher {

// using CmdResponse = std::variant<core::SetResult,
//                                  core::GetResult,
//                                  core::PingResult,
//                                  core::ExpireResult,
//                                  core::DelResult,
//                                  core::TtlResult,
//                                  core::ExistsResult>;


[[nodiscard]] protocol::RespValue dispatch(const command::Command& cmd_variant);


} // namespace nitrokv::dispatcher
