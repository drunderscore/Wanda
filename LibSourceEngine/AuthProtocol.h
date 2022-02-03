/*
 * Copyright (c) 2022, James Puleo <james@jame.xyz>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

namespace SourceEngine
{
enum class AuthProtocol : int
{
    AuthCertificate = 1,
    HashedCDKey = 2,
    Steam = 3
};
}