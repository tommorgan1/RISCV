#pragma once

#include <cstdint>


template <typename Narrow>
constexpr int64_t sign_extend(Narrow v) noexcept
{
    return static_cast<int64_t>(static_cast<std::make_signed_t<Narrow>>(v));
}