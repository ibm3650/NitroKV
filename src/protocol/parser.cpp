//
// Created by nikita on 22.03.2026.
//


#include <algorithm>
#include <charconv>
#include <chrono>
#include <expected>
#include <functional>
#include <ranges>
#include <span>
#include <string_view>

#include "nitrokv/protocol/commands.hpp"
#include "nitrokv/protocol/meta.hpp"
#include "nitrokv/protocol/protocol.hpp"


struct CommandDescription {
    CmdParsingResult (*cmd_parser)(std::span<const nitrokv::protocol::RespValue>);
    // std::function<Command(std::span<const nitrokv::protocol::RespValue>)> cmd_parser;
    // std::function<void(const nitrokv::protocol::RespArray*)> handler;
    struct {
        size_t min;
        size_t max;
    } arity;
    // uint32_t flags;
    // std::string_view name;
};

namespace {
/**
 *
 * @param resp_value Ссылка на std::variant Resp-типов
 * @return Опциональное значение, если тип совпадает - то bulk string как сырые байты, иначе -
 * пусто.
 */
[[nodiscard]] std::optional<std::span<const std::byte>>
try_get_bulk_string_bytes(const nitrokv::protocol::RespValue& resp_value) noexcept {
    const auto* bulk_str = std::get_if<nitrokv::protocol::RespBulkString>(&resp_value.value);
    if (bulk_str == nullptr) {
        return std::nullopt;
    }
    return bulk_str->value;
}


template <typename T>
    requires is_same_args_count<T, 1>
[[nodiscard]] std::expected<T, ParsingError>
parse_unary_command(const std::span<const nitrokv::protocol::RespValue>& args) noexcept {
    auto bulk_or_none = try_get_bulk_string_bytes(args[0]);
    if (!bulk_or_none) {
        return std::unexpected(ParsingError::INVALID_ARGUMENT_TYPE);
    }
    return T{*bulk_or_none};
}
} // namespace


CmdParsingResult parse_ping(const std::span<const nitrokv::protocol::RespValue> /*args*/) {
    return PingCommand{};
}


CmdParsingResult parse_get(const std::span<const nitrokv::protocol::RespValue> args) {
    return parse_unary_command<GetCommand>(args);
}

CmdParsingResult parse_del(const std::span<const nitrokv::protocol::RespValue> args) {
    return parse_unary_command<DelCommand>(args);
}


CmdParsingResult parse_ttl(const std::span<const nitrokv::protocol::RespValue> args) {
    return parse_unary_command<TtlCommand>(args);
}

CmdParsingResult parse_exists(const std::span<const nitrokv::protocol::RespValue> args) {
    return parse_unary_command<ExistsCommand>(args);
}


CmdParsingResult parse_set(const std::span<const nitrokv::protocol::RespValue> args) {
    const auto key = try_get_bulk_string_bytes(args[0]);
    const auto value = try_get_bulk_string_bytes(args[1]);
    if (!key) {
        return std::unexpected(ParsingError::INVALID_ARGUMENT_TYPE);
    }
    if (!value) {
        return std::unexpected(ParsingError::INVALID_ARGUMENT_TYPE);
    }
    return SetCommand{*key, *value};
}


CmdParsingResult parse_expire(const std::span<const nitrokv::protocol::RespValue> args) {
    // EXPIRE key seconds [NX | XX | GT | LT]
    const auto key = try_get_bulk_string_bytes(args[0]);
    if (!key) {
        return std::unexpected(ParsingError::INVALID_ARGUMENT_TYPE);
    }

    const auto ttl_raw = try_get_bulk_string_bytes(args[1]);
    if (!ttl_raw) {
        return std::unexpected(ParsingError::INVALID_ARGUMENT_TYPE);
    }


    int64_t ttl{};
    const char* input_as_char = reinterpret_cast<const char*>(ttl_raw->data());
    const size_t input_length = ttl_raw->size();
    const auto [ptr, ec] = std::from_chars(input_as_char, input_as_char + input_length, ttl);
    if (ec != std::errc{} || ptr != input_as_char + input_length) {
        return std::unexpected(ParsingError::INVALID_INTEGER_FORMAT);
    }
    if (ttl < 0) {
        return std::unexpected(ParsingError::NEGATIVE_TTL_VALUE);
    }
    if (args.size() == 2U) {
        return ExpireCommand{*key, std::chrono::seconds{ttl}};
    }
    // const std::array available_comp_opt = {
    //     std::as_bytes(std::span("NX", 2)), std::as_bytes(std::span("GT", 2)),
    //     std::as_bytes(std::span("XX", 2)), std::as_bytes(std::span("LT", 2))};

    const auto comparison_arg = try_get_bulk_string_bytes(args[2]);
    if (!comparison_arg) {
        return std::unexpected(ParsingError::INVALID_ARGUMENT_TYPE);
    }

    ExpireCommand::ComparisonType ct;
    if (std::ranges::equal(std::as_bytes(std::span("NX", 2)), *comparison_arg)) {
        ct = ExpireCommand::ComparisonType ::NX;
    } else if (std::ranges::equal(std::as_bytes(std::span("XX", 2)), *comparison_arg)) {
        ct = ExpireCommand::ComparisonType ::XX;
    } else if (std::ranges::equal(std::as_bytes(std::span("GT", 2)), *comparison_arg)) {
        ct = ExpireCommand::ComparisonType ::GT;
    } else if (std::ranges::equal(std::as_bytes(std::span("LT", 2)), *comparison_arg)) {
        ct = ExpireCommand::ComparisonType ::LT;
    } else {
        return std::unexpected(ParsingError::UNKNOWN_PARAMETER_OPTION);
    }
    // bool exists = std::ranges::any_of(available_comp_opt, [&](auto opt) {
    //     return std::ranges::equal(opt, comarsion_arg->value);
    // });
    // if (!exists) {
    //     return {};
    // }

    return ExpireCommand{*key, std::chrono::seconds{ttl}, ct};


    // auto spans_equal_insensitive = [](auto left, auto right) {
    //     return std::ranges::equal(left, right, [](std::byte a, std::byte b) {
    //         return std::tolower(static_cast<unsigned char>(a)) ==
    //                std::tolower(static_cast<unsigned char>(b));
    //     });
    // };
    //
    //
    // if (!std::ranges::any_of(available_comp_opt, [&](auto opt) {
    //     return spans_equal_insensitive(opt, comarsion_arg->value);
    // })) {
    //
    //     return {};
    // }
}

#include <array>
static constexpr auto COMMAND_TABLE =
    std::to_array<std::pair<std::string_view, CommandDescription>>({
        // static const std::unordered_map<std::string_view, CommandDescription> COMMAND_TABLE = {
        {"SET", {.cmd_parser = parse_set, .arity = {.min = 2, .max = 2}}},
        {"GET", {.cmd_parser = parse_get, .arity = {.min = 1, .max = 1}}},
        {"DEL", {.cmd_parser = parse_del, .arity = {.min = 1, .max = 1}}},
        {"PING", {.cmd_parser = parse_ping, .arity = {.min = 0, .max = 0}}},
        {"EXPIRE", {.cmd_parser = parse_expire, .arity = {.min = 2, .max = 3}}},
        {"TTL", {.cmd_parser = parse_ttl, .arity = {.min = 1, .max = 1}}},
        {"EXISTS", {.cmd_parser = parse_exists, .arity = {.min = 1, .max = 1}}},
    });


CmdParsingResult parser(const nitrokv::protocol::RespValue& args_raw) {
    const auto* args = std::get_if<nitrokv::protocol::RespArray>(&args_raw.value);
    if (args == nullptr) {
        return std::unexpected(ParsingError::COMMAND_ARGS_IS_NOT_ARRAY);
    }
    if (args->empty()) {
        return std::unexpected(ParsingError::EMPTY_COMMAND);
    }

    const auto command = try_get_bulk_string_bytes((*args)[0]);
    if (!command) {
        return std::unexpected(ParsingError::INVALID_COMMAND_TYPE);
    }

    const char* cmd_as_char = reinterpret_cast<const char*>(command->data());
    const size_t cmd_length = command->size();
    std::string cmd_upper{cmd_as_char, cmd_length};
    std::ranges::transform(cmd_upper, cmd_upper.begin(), ::toupper);
    const auto* it = std::ranges::find(COMMAND_TABLE, cmd_upper,
                                       &std::pair<std::string_view, CommandDescription>::first);

    // const auto it = COMMAND_TABLE.find(cmd_upper);
    if (it == COMMAND_TABLE.end()) {
        return std::unexpected(ParsingError::UNKNOWN_COMMAND);
    }
    const auto& descriptor = it->second;
    if (size_t const count = args->size() - 1U;
        count < descriptor.arity.min || count > descriptor.arity.max) {
        return std::unexpected(ParsingError::INVALID_ARGUMENTS_NUM);
    }
    return descriptor.cmd_parser(std::span(*args).subspan(1));
}


// template <class... Types> struct TypeArray {
//     using AsTuple = std::tuple<Types...>;
//
//     template <std::size_t I> using get = std::tuple_element_t<I, AsTuple>;
//
//     static constexpr std::size_t SIZE = sizeof...(Types);
// };
//
// using types = TypeArray<int, float>;
// types::get<0>;
//
// template <size_t N> struct FixedString {
//     char data[N]{};
//
//     // constexpr конструктор позволяет принимать строковые литералы
//     constexpr FixedString(const char (&str)[N]) {
//         std::copy_n(str, N, data);
//     }
// };
//
//
// template <FixedString... Strs> struct CompTimeStr {
//     // Храним строки как массив указателей или копируем их
//     static constexpr size_t count = sizeof...(Strs);
//     const char* data[count] = {Strs.data...};
//
//     void print() {
//         // for (const char* s : data) std::cout << s << " ";
//     }
// };
//
//
// // template<FixedString... Strs, auto functor=[]{}>
// // struct CompTimeStr {
// //     // Храним строки как массив указателей или копируем их
// //     static constexpr size_t count = sizeof...(Strs);
// //     const char* data[count] = { Strs.data... };
// //
// //     void print() {
// //         // for (const char* s : data) std::cout << s << " ";
// //     }
// // };
//
//
// template <CompTimeStr options = CompTimeStr<>{}> struct CommandRestriction {};
//
//
// using PING = CommandRestriction<CompTimeStr<"dddd", "rrrr">{}>;
//
// using s33tr = CompTimeStr<"dddd", "rrrr">;


// TODO: std::expected<Command, CommandParseError>auto out = func();
//  check for possible failure
// A& valRef{*out};
// … // do stuff with valRef

// template<typename T>
// constexpr std::remove_reference_t<T>&& move(T&& t) noexcept {
//     return static_cast<std::remove_reference_t<T>&&>(t);
// }


// auto expected_func() -> std::expected<Verbose, std::string>
// {
//     if (5 > 10) {
//         return std::unexpected("math is broken");
//     }
//     return std::expected<Verbose, std::string> { std::in_place, 7 };
// }


// c++23 auto expected_func_2() -> std::expected<Verbose, std::string>
//  {
//      std::expected<Verbose, std::string> ret;
//      if (5 > 10) {
//          // Это необходимо для NRVO:
//          ret = std::unexpected("math is broken");
//          return ret;
//      }
//      ret.emplace(7);
//      return ret;
//  }


// #include <optional>
// #include <variant>
// #include <vector>
// #include <string>
//
// // 1. Прямое конструирование в std::optional (без временного вектора)
// std::optional<std::vector<int>> opt{std::in_place, {1, 2, 3, 4, 5}};
//
// // 2. Устранение неоднозначности в std::variant
// // Если в variant есть и int, и float, передача 10.5 вызовет ошибку. Тег решает это:
// std::variant<int, float> v{std::in_place_type<int>, 10.5}; // Будет создан int(10)
//
// // 3. Создание объекта с несколькими аргументами
// std::variant<std::string, int> v2{std::in_place_index<0>, 10, 'A'}; // Создаст строку
// "AAAAAAAAAA"


// // TODO:
// auto [key, val, ex_opt] = ArgParser(args)
//     .required<nitrokv::protocol::RespBulkString>()
//     .required<nitrokv::protocol::RespBulkString>()
//     .optional<int64_t>("EX")
//     .flag("NX")
//     .parse();
