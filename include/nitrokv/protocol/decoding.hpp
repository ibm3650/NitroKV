//
// Created by nikita on 04.04.2026.
//
#pragma once
#include <string_view>

#include "nitrokv/protocol/protocol.hpp"

namespace nitrokv::protocol {
/**
 * @brief Структура, представляющая возвращаемое значение функций парсинга.
 * Внутренние функции используют поле remaining при рекурсивном парсинге.
 */

enum class RespDecodingStatus : uint8_t {
    SUCCESS,
    INVALID_SEPARATOR,
    INVALID_INTEGER,
    INVALID_SYMBOL,
    UNEXPECTED_END,
    EMPTY_BUFFER,
    EXTRA_DATA,
    OUT_OF_MEM,
    UNEXPECTED_ERROR
};
struct RespDecodingResult {               // NOLINT(altera-struct-pack-align)
    std::span<const std::byte> remaining; /**< Остаток буфера */
    RespValue value{};                    /**< Значение переменной, результат парсинга */
    RespDecodingStatus status{};          /**< Значение переменной, результат парсинга */
};

/**
 * @brief Рекурсивный парсер буфера с RESP-данными.
 * @details Обеспечивает полный разбор буфера с контролем корректности данных.
 * Если в буфере присутствует дублирующая терминальная последовательность - она автоматически
 * удаляется. Если хотя бы один из элементов некорректен, или буфер неполный, то процесс
 * останавливается с неудачным исходом.
 * @return RespDecodingResult Если результат парсинга неудачный - в поле #remaining будет находиться
 * входной буфер без изменений, а #value гарантирует значение std::monostate. В случае успеха,
 * остаток буфера будет пустым, а поле значения будет содержать данные, представляющие один из
 * допустимых типов RESP.
 * @param[in] input  Входной буфер байт. Гарантируется, что буфер не будет изменен, в ходе парсинга.
 * @throw нон-throw гарантия: этот метод никогда не выбрасывает исключения.
 */
RespDecodingResult decode(std::span<const std::byte> input) noexcept;
RespDecodingResult decode_first(std::span<const std::byte> input) noexcept;
} // namespace nitrokv::protocol
