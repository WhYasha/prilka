#include <gtest/gtest.h>
#include "services/JwtService.h"
#include "config/Config.h"
#include <cstdlib>

// Set minimal env before tests
class JwtEnv : public ::testing::Environment {
public:
    void SetUp() override {
        setenv("JWT_SECRET", "test-secret-at-least-16-chars-long", 1);
        setenv("JWT_ACCESS_TTL",  "3600",   1);
        setenv("JWT_REFRESH_TTL", "604800", 1);
    }
};

testing::Environment* const jwtEnv =
    testing::AddGlobalTestEnvironment(new JwtEnv);

TEST(JwtService, AccessTokenRoundTrip) {
    auto& jwt = JwtService::instance();
    long long uid = 42;
    std::string token = jwt.createAccessToken(uid);
    ASSERT_FALSE(token.empty());

    auto claims = jwt.verify(token);
    ASSERT_TRUE(claims.has_value());
    EXPECT_EQ(claims->userId, uid);
    EXPECT_EQ(claims->tokenType, "access");
}

TEST(JwtService, RefreshTokenRoundTrip) {
    auto& jwt = JwtService::instance();
    long long uid = 99;
    std::string token = jwt.createRefreshToken(uid);
    ASSERT_FALSE(token.empty());

    auto claims = jwt.verify(token);
    ASSERT_TRUE(claims.has_value());
    EXPECT_EQ(claims->userId, uid);
    EXPECT_EQ(claims->tokenType, "refresh");
}

TEST(JwtService, TamperedSignatureRejected) {
    auto& jwt = JwtService::instance();
    std::string token = jwt.createAccessToken(1);
    // Flip a character in the signature part
    auto dotPos = token.rfind('.');
    ASSERT_NE(dotPos, std::string::npos);
    token[dotPos + 1] ^= 0x01;

    auto claims = jwt.verify(token);
    EXPECT_FALSE(claims.has_value());
}

TEST(JwtService, WrongTypeRejectedAsAccess) {
    auto& jwt = JwtService::instance();
    std::string refresh = jwt.createRefreshToken(5);
    auto claims = jwt.verify(refresh);
    ASSERT_TRUE(claims.has_value());
    // Filter layer checks tokenType == "access"; this test verifies type is recorded correctly
    EXPECT_EQ(claims->tokenType, "refresh");
    EXPECT_NE(claims->tokenType, "access");
}

TEST(JwtService, InvalidTokenReturnsNullopt) {
    auto& jwt = JwtService::instance();
    EXPECT_FALSE(jwt.verify("").has_value());
    EXPECT_FALSE(jwt.verify("not.a.token").has_value());
    EXPECT_FALSE(jwt.verify("eyJ.eyJ.bad_sig").has_value());
}
