/*
 * Copyright (c) 2022, James Puleo <james@jame.xyz>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <LibCore/Stream.h>

inline ErrorOr<Bytes> operator>>(Core::Stream::Stream& stream, Bytes bytes) { return stream.read(bytes); }
inline ErrorOr<size_t> operator<<(OutputStream& stream, ReadonlyBytes bytes) { return stream.write(bytes); }

template<typename Integral>
ErrorOr<Bytes> operator>>(Core::Stream::Stream& stream, Integral& value) requires IsIntegral<Integral>
{
    return stream.read({&value, sizeof(value)});
}
template<typename Integral>
ErrorOr<size_t> operator<<(Core::Stream::Stream& stream, Integral value) requires IsIntegral<Integral>
{
    return stream.write({&value, sizeof(value)});
}

template<typename FloatingPoint>
ErrorOr<Bytes> operator>>(Core::Stream::Stream& stream, FloatingPoint& value) requires IsFloatingPoint<FloatingPoint>
{
    return stream.read({&value, sizeof(value)});
}
template<typename FloatingPoint>
ErrorOr<size_t> operator<<(Core::Stream::Stream& stream, FloatingPoint value) requires IsFloatingPoint<FloatingPoint>
{
    return stream.write({&value, sizeof(value)});
}