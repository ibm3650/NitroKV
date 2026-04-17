//
// Created by nikita on 17.04.2026.
//

#pragma once
#include <type_traits>
#include <utility>


template <typename T>
concept aggregate = std::is_aggregate_v<T>;

template <typename T, typename... Args>
concept aggregate_initializable = aggregate<T> && requires { T{std::declval<Args>()...}; };


struct any {
    template <typename T> constexpr operator T() const noexcept;
};


template <std::size_t I> using indexed_any = any;

template <aggregate T, typename Indices> struct aggregate_initializable_from_indices;

template <aggregate T, std::size_t... Indices>
struct aggregate_initializable_from_indices<T, std::index_sequence<Indices...>>
    : std::bool_constant<aggregate_initializable<T, indexed_any<Indices>...>> {};


template <typename T, std::size_t N>
concept aggregate_initializable_with_n_args =
    aggregate<T> &&
    aggregate_initializable_from_indices<T, std::make_index_sequence<N>>::value;


template <aggregate T, std::size_t N, bool CanInitialize>
struct aggregate_field_count
    : aggregate_field_count<T, N + 1, aggregate_initializable_with_n_args<T, N + 1>> {};

template <aggregate T, std::size_t N>
struct aggregate_field_count<T, N, false>: std::integral_constant<std::size_t, N - 1> {};


template <aggregate T> struct num_aggregate_fields: aggregate_field_count<T, 0, true> {};


template <typename T, std::size_t N>
concept is_same_args_count = num_aggregate_fields<T>::value == N;

template <aggregate T>
constexpr std::size_t num_aggregate_fields_v = num_aggregate_fields<T>::value;



