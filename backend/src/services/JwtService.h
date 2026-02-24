#pragma once
#include <string>
#include <optional>
#include <ctime>

/// Minimal HS256 JWT implementation using OpenSSL.
class JwtService {
public:
    struct Claims {
        long long userId;
        std::string tokenType;  // "access" | "refresh"
        long long   exp;
        long long   iat;
        bool isAdmin = false;
    };

    static JwtService& instance();

    /// Create a signed access token
    std::string createAccessToken(long long userId, bool isAdmin = false) const;

    /// Create a signed refresh token
    std::string createRefreshToken(long long userId, bool isAdmin = false) const;

    /// Verify and decode a token. Returns nullopt if invalid or expired.
    std::optional<Claims> verify(const std::string& token) const;

private:
    JwtService() = default;
};
