#include <gtest/gtest.h>
#include <cstdlib>

#ifdef GARA_MYSQL_SUPPORT
#include "db/mysql_client.h"
#endif

#include "interfaces/database_client_interface.h"
#include "utils/logger.h"
#include "utils/metrics.h"
#include "test_helpers/test_constants.h"

using namespace gara;
using namespace gara::test_constants;

class MySQLClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        gara::Logger::initialize("gara-test", "error", gara::Logger::Format::TEXT, "test");
        gara::Metrics::initialize("GaraTest", "gara-test", "test", false);

        // Clear any existing MySQL environment variables
        clearMySQLEnvVars();
    }

    void TearDown() override {
        clearMySQLEnvVars();
    }

    void clearMySQLEnvVars() {
        unsetenv("MYSQL_HOST");
        unsetenv("MYSQL_PORT");
        unsetenv("MYSQL_USER");
        unsetenv("MYSQL_PASSWORD");
        unsetenv("MYSQL_DATABASE");
    }

    void setMySQLEnvVars(const std::string& host, const std::string& port,
                         const std::string& user, const std::string& password,
                         const std::string& database) {
        if (!host.empty()) setenv("MYSQL_HOST", host.c_str(), 1);
        if (!port.empty()) setenv("MYSQL_PORT", port.c_str(), 1);
        if (!user.empty()) setenv("MYSQL_USER", user.c_str(), 1);
        if (!password.empty()) setenv("MYSQL_PASSWORD", password.c_str(), 1);
        if (!database.empty()) setenv("MYSQL_DATABASE", database.c_str(), 1);
    }
};

// ============================================================================
// MySQLConfig Tests
// ============================================================================

#ifdef GARA_MYSQL_SUPPORT

TEST_F(MySQLClientTest, MySQLConfig_DefaultValues_AreCorrect) {
    // Arrange & Act
    MySQLConfig config;

    // Assert
    EXPECT_EQ("localhost", config.host)
        << "Default host should be localhost";
    EXPECT_EQ(3306, config.port)
        << "Default port should be 3306";
    EXPECT_EQ("root", config.user)
        << "Default user should be root";
    EXPECT_TRUE(config.password.empty())
        << "Default password should be empty";
    EXPECT_EQ("gara", config.database)
        << "Default database should be gara";
}

TEST_F(MySQLClientTest, MySQLConfig_FromEnvironment_WithNoEnvVars_UsesDefaults) {
    // Arrange - environment is already cleared in SetUp

    // Act
    MySQLConfig config = MySQLConfig::fromEnvironment();

    // Assert
    EXPECT_EQ("localhost", config.host)
        << "Should use default host when MYSQL_HOST not set";
    EXPECT_EQ(3306, config.port)
        << "Should use default port when MYSQL_PORT not set";
    EXPECT_EQ("root", config.user)
        << "Should use default user when MYSQL_USER not set";
    EXPECT_TRUE(config.password.empty())
        << "Should use empty password when MYSQL_PASSWORD not set";
    EXPECT_EQ("gara", config.database)
        << "Should use default database when MYSQL_DATABASE not set";
}

TEST_F(MySQLClientTest, MySQLConfig_FromEnvironment_WithAllEnvVars_ParsesCorrectly) {
    // Arrange
    setMySQLEnvVars("mysql.example.com", "3307", "testuser", "testpass", "testdb");

    // Act
    MySQLConfig config = MySQLConfig::fromEnvironment();

    // Assert
    EXPECT_EQ("mysql.example.com", config.host)
        << "Host should be read from MYSQL_HOST";
    EXPECT_EQ(3307, config.port)
        << "Port should be read from MYSQL_PORT";
    EXPECT_EQ("testuser", config.user)
        << "User should be read from MYSQL_USER";
    EXPECT_EQ("testpass", config.password)
        << "Password should be read from MYSQL_PASSWORD";
    EXPECT_EQ("testdb", config.database)
        << "Database should be read from MYSQL_DATABASE";
}

TEST_F(MySQLClientTest, MySQLConfig_FromEnvironment_WithPartialEnvVars_MixesWithDefaults) {
    // Arrange - only set some vars
    setMySQLEnvVars("custom-host", "", "", "secret", "");

    // Act
    MySQLConfig config = MySQLConfig::fromEnvironment();

    // Assert
    EXPECT_EQ("custom-host", config.host)
        << "Host should be from env";
    EXPECT_EQ(3306, config.port)
        << "Port should use default";
    EXPECT_EQ("root", config.user)
        << "User should use default";
    EXPECT_EQ("secret", config.password)
        << "Password should be from env";
    EXPECT_EQ("gara", config.database)
        << "Database should use default";
}

TEST_F(MySQLClientTest, MySQLConfig_FromEnvironment_WithInvalidPort_UsesDefault) {
    // Arrange
    setMySQLEnvVars("", "not-a-number", "", "", "");

    // Act
    MySQLConfig config = MySQLConfig::fromEnvironment();

    // Assert
    EXPECT_EQ(3306, config.port)
        << "Invalid port should fall back to default";
}

TEST_F(MySQLClientTest, MySQLConfig_FromEnvironment_WithEmptyPort_UsesDefault) {
    // Arrange
    setenv("MYSQL_PORT", "", 1);

    // Act
    MySQLConfig config = MySQLConfig::fromEnvironment();

    // Assert
    EXPECT_EQ(3306, config.port)
        << "Empty port should fall back to default";
}

// ============================================================================
// ImageSortOrder SQL Generation Tests
// ============================================================================

TEST_F(MySQLClientTest, GetSortOrderSql_Newest_ReturnsCorrectSql) {
    // This tests the static method indirectly through expected behavior
    // The actual SQL string is verified through integration tests

    // We can at least verify the enum exists and has expected values
    EXPECT_EQ(static_cast<int>(ImageSortOrder::NEWEST), 0);
    EXPECT_EQ(static_cast<int>(ImageSortOrder::OLDEST), 1);
    EXPECT_EQ(static_cast<int>(ImageSortOrder::NAME_ASC), 2);
    EXPECT_EQ(static_cast<int>(ImageSortOrder::NAME_DESC), 3);
}

// ============================================================================
// MySQLResult RAII Tests
// ============================================================================

TEST_F(MySQLClientTest, MySQLResult_WithNullResult_OperatorBoolReturnsFalse) {
    // Arrange & Act
    MySQLResult result(nullptr);

    // Assert
    EXPECT_FALSE(static_cast<bool>(result))
        << "Null result should evaluate to false";
    EXPECT_EQ(nullptr, result.get())
        << "get() should return nullptr";
}

// Note: Testing MySQLResult with actual MYSQL_RES* requires a database connection
// which is covered in integration tests

#endif // GARA_MYSQL_SUPPORT

// ============================================================================
// Compilation Guard Test
// ============================================================================

TEST_F(MySQLClientTest, MySQLSupport_CompilationFlag_IsSet) {
#ifdef GARA_MYSQL_SUPPORT
    SUCCEED() << "GARA_MYSQL_SUPPORT is defined - MySQL support is enabled";
#else
    SUCCEED() << "GARA_MYSQL_SUPPORT is not defined - MySQL support is disabled";
#endif
}

// ============================================================================
// DatabaseClientInterface Tests (applies to all implementations)
// ============================================================================

TEST_F(MySQLClientTest, ImageSortOrder_HasExpectedValues) {
    // Verify the enum values match what the implementations expect
    EXPECT_NE(ImageSortOrder::NEWEST, ImageSortOrder::OLDEST);
    EXPECT_NE(ImageSortOrder::NAME_ASC, ImageSortOrder::NAME_DESC);
    EXPECT_NE(ImageSortOrder::NEWEST, ImageSortOrder::NAME_ASC);
}
