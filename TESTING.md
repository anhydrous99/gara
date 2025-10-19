# Testing Guide

## Test Suite

**~47 tests** using Google Test:
- File utils (12): SHA256, I/O, validation
- Image processor (10): Transformation, resizing, format conversion (uses real libvips)
- Cache manager (10): Cache operations
- Controllers (8): Request/response models
- Integration (7): End-to-end workflows

## Running Tests

```bash
# Quick run
cd build && ctest --output-on-failure

# Specific test suite
./gara_tests --gtest_filter="FileUtilsTest.*"
./gara_tests --gtest_filter="ImageProcessorTest.*"
./gara_tests --gtest_filter="IntegrationTest.*"

# Specific test case
./gara_tests --gtest_filter="FileUtilsTest.CalculateSHA256FromFile"
```

## CI/CD

Tests run automatically on GitHub Actions for:
- Push to `main` or `develop`
- All pull requests
- Platforms: Ubuntu Latest, macOS Latest

Workflow: `.github/workflows/test.yml`

## Writing Tests

### Basic Structure

```cpp
#include <gtest/gtest.h>

class MyComponentTest : public ::testing::Test {
protected:
    void SetUp() override { /* Setup */ }
    void TearDown() override { /* Cleanup */ }
};

TEST_F(MyComponentTest, TestName) {
    EXPECT_TRUE(condition);
    EXPECT_EQ(expected, actual);
}
```

### Using Fake S3 (In-Memory)

```cpp
#include "mocks/mock_s3_service.h"

auto fake_s3 = std::make_shared<FakeS3Service>();
fake_s3->uploadData(data, "key");
EXPECT_TRUE(fake_s3->objectExists("key"));
```

### Common Assertions

```cpp
EXPECT_TRUE(condition);
EXPECT_FALSE(condition);
EXPECT_EQ(expected, actual);
EXPECT_NE(val1, val2);
EXPECT_LT(val1, val2);  // Less than
EXPECT_STREQ("expected", actual_cstr);
EXPECT_THROW(statement, exception_type);
```

## Adding Tests

1. Create test file: `tests/services/my_test.cpp`
2. Add to `tests/CMakeLists.txt`:
   ```cmake
   add_executable(gara_tests
       # existing...
       services/my_test.cpp
   )
   ```
3. Run: `cd build && cmake .. && make -j && ctest`

## Best Practices

**Do**:
- Use descriptive test names
- Test one thing per test
- Clean up resources in `TearDown()`
- Use fake S3 for integration tests
- Keep tests fast (< 1s each)

**Don't**:
- Use real AWS S3
- Leave temp files
- Depend on test execution order
- Use hardcoded absolute paths
