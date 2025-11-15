# Testing Guide

## Test Suite

**~200+ tests** using Google Test:
- **Utils** (25+): File utils (12), ID generator (10+)
- **Services** (85+): Image processor (10), Cache manager (10), Secrets service (10), Album service (15), S3 service (40+)
- **Mappers** (20+): Album mapper DynamoDB conversions
- **Middleware** (15+): Auth middleware, API key validation
- **Controllers** (8): Request/response model serialization
- **Error Handling** (30+): Edge cases, validation, exceptions
- **Integration** (7): End-to-end workflows

## Running Tests

```bash
# Quick run
cd build && ctest --output-on-failure

# Specific test suite
./gara_tests --gtest_filter="FileUtilsTest.*"
./gara_tests --gtest_filter="ImageProcessorTest.*"
./gara_tests --gtest_filter="IntegrationTest.*"
./gara_tests --gtest_filter="IdGeneratorTest.*"
./gara_tests --gtest_filter="AlbumMapperTest.*"
./gara_tests --gtest_filter="S3ServiceTest.*"
./gara_tests --gtest_filter="ErrorHandlingTest.*"

# Specific test case
./gara_tests --gtest_filter="FileUtilsTest.CalculateSHA256FromFile"
```

## Code Coverage

### Enabling Coverage

Build with coverage enabled:

```bash
mkdir build-coverage && cd build-coverage
cmake .. -DENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
make -j
ctest --output-on-failure
```

### Generating Coverage Reports

Using `lcov` and `genhtml`:

```bash
# Install lcov (if not already installed)
# Ubuntu/Debian: sudo apt-get install lcov
# macOS: brew install lcov

# Generate coverage data
lcov --capture --directory . --output-file coverage.info

# Remove system/external libraries from report
lcov --remove coverage.info '/usr/*' '*/build/*' '*/tests/*' '*/googletest/*' --output-file coverage_filtered.info

# Generate HTML report
genhtml coverage_filtered.info --output-directory coverage_report

# View report
open coverage_report/index.html  # macOS
xdg-open coverage_report/index.html  # Linux
```

### Coverage Goals

Target coverage percentages by component:
- **Utils**: 95%+ (high-value, reusable utilities)
- **Services**: 85%+ (business logic)
- **Controllers**: 80%+ (HTTP endpoints)
- **Models/Mappers**: 90%+ (data transformations)
- **Overall**: 85%+

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

## Test Categories

### Unit Tests
Test individual components in isolation:
- **Utils**: ID generation, file operations, hashing
- **Services**: Business logic without external dependencies
- **Mappers**: Data transformation between models and DynamoDB
- **Middleware**: Authentication, request validation

### Integration Tests
Test components working together:
- End-to-end image upload, transformation, and caching
- Multiple transformations of the same image
- Cache hit/miss scenarios
- Deduplication logic

### Error Handling Tests
Test failure modes and edge cases:
- Invalid inputs (empty strings, null values, malformed JSON)
- Non-existent resources (albums, images)
- Duplicate names and conflicts
- Concurrent operations
- Boundary conditions (very large inputs, special characters, Unicode)

## Best Practices

**Do**:
- Use descriptive test names that describe what is being tested
- Test one thing per test (single responsibility)
- Clean up resources in `TearDown()`
- Use fake S3 and DynamoDB for integration tests
- Keep tests fast (< 1s each)
- Test both success and failure paths
- Include edge cases and boundary conditions
- Verify error messages, not just exception types

**Don't**:
- Use real AWS services in tests
- Leave temp files after tests complete
- Depend on test execution order
- Use hardcoded absolute paths
- Skip error handling tests
- Assume inputs are always valid

## New Test Coverage (Recent Additions)

The following tests were added to improve coverage:

### IdGenerator Tests (`tests/utils/id_generator_test.cpp`)
- Format validation (timestamp_UUID pattern)
- Uniqueness across 10,000+ generations
- Chronological ordering
- Thread safety with concurrent generation
- UUID v4 format compliance

### AlbumMapper Tests (`tests/mappers/album_mapper_test.cpp`)
- DynamoDB item to Album model conversion
- Album model to DynamoDB PutItemRequest conversion
- Round-trip mapping verification
- Special characters and Unicode handling
- Large data handling (long strings, many tags)
- Empty and optional field handling

### S3Service Tests (`tests/services/s3_service_test.cpp`)
- Upload/download file operations
- Upload/download data (in-memory)
- Object existence checks
- Presigned URL generation
- Delete operations
- Binary data integrity
- Concurrent uploads
- Special characters in S3 keys

### Error Handling Tests (`tests/error_handling_test.cpp`)
- Album service exceptions (NotFound, Conflict, Validation)
- JSON parsing errors
- Image processor failures
- File utility error handling
- Edge cases (empty inputs, very large inputs, Unicode)
- Concurrent operations safety
