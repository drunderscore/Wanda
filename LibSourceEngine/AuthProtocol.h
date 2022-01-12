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