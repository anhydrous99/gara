# GitHub Actions Workflow Improvements

## Current State Analysis

The current workflow (`.github/workflows/test.yml`) has a solid foundation but is missing several optimizations and best practices.

**Strengths:**
- ✅ Multi-platform testing (Ubuntu + macOS)
- ✅ Uploads test results as artifacts
- ✅ Verbose output for debugging
- ✅ Uses appropriate action versions

**Areas for Improvement:**
- ❌ No dependency caching (slow builds)
- ❌ No code coverage reporting
- ❌ No build artifact caching
- ❌ No test result visualization
- ❌ Limited to single compiler version
- ❌ No static analysis or linting
- ❌ No performance benchmarking
- ❌ No security scanning

---

## Priority 1: Performance Optimizations (Quick Wins)

### 1.1 Add Dependency Caching

**Impact:** 50-70% faster CI runs
**Effort:** Low

```yaml
# Ubuntu dependency caching
- name: Cache apt packages
  uses: actions/cache@v4
  with:
    path: /var/cache/apt/archives
    key: ${{ runner.os }}-apt-${{ hashFiles('.github/workflows/test.yml') }}
    restore-keys: |
      ${{ runner.os }}-apt-

# macOS dependency caching
- name: Cache Homebrew packages
  uses: actions/cache@v4
  with:
    path: ~/Library/Caches/Homebrew
    key: ${{ runner.os }}-brew-${{ hashFiles('.github/workflows/test.yml') }}
    restore-keys: |
      ${{ runner.os }}-brew-
```

### 1.2 Add CMake Build Caching

**Impact:** 60-80% faster rebuilds on incremental changes
**Effort:** Low

```yaml
- name: Cache CMake build
  uses: actions/cache@v4
  with:
    path: |
      build
      ~/.ccache
    key: ${{ runner.os }}-build-${{ hashFiles('**/CMakeLists.txt', '**/*.cpp', '**/*.h') }}
    restore-keys: |
      ${{ runner.os }}-build-

- name: Install ccache
  run: |
    sudo apt-get install -y ccache
    echo "/usr/lib/ccache" >> $GITHUB_PATH

- name: Configure CMake with ccache
  run: cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
```

### 1.3 Parallelize Jobs with Matrix Strategy

**Impact:** Test multiple configurations simultaneously
**Effort:** Medium

```yaml
jobs:
  test:
    strategy:
      fail-fast: false
      matrix:
        os: [ubuntu-latest, macos-latest]
        build_type: [Debug, Release]
        compiler:
          - { cc: gcc-11, cxx: g++-11 }
          - { cc: clang-14, cxx: clang++-14 }
        exclude:
          # macOS doesn't use gcc
          - os: macos-latest
            compiler: { cc: gcc-11, cxx: g++-11 }

    runs-on: ${{ matrix.os }}

    env:
      CC: ${{ matrix.compiler.cc }}
      CXX: ${{ matrix.compiler.cxx }}
```

---

## Priority 2: Code Coverage Reporting

### 2.1 Add Coverage Collection

**Impact:** Track test coverage, improve code quality
**Effort:** Medium

```yaml
- name: Configure CMake with Coverage
  run: |
    mkdir -p build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON

- name: Generate Coverage Report
  run: |
    cd build
    lcov --capture --directory . --output-file coverage.info
    lcov --remove coverage.info '/usr/*' '*/tests/*' '*/googletest/*' --output-file coverage_filtered.info
    lcov --list coverage_filtered.info

- name: Upload Coverage to Codecov
  uses: codecov/codecov-action@v4
  with:
    files: ./build/coverage_filtered.info
    flags: unittests
    name: codecov-gara-image
    fail_ci_if_error: true
```

### 2.2 Add Coverage Badge

Add to README.md:
```markdown
[![codecov](https://codecov.io/gh/anhydrous99/gara-image/branch/main/graph/badge.svg)](https://codecov.io/gh/anhydrous99/gara-image)
```

---

## Priority 3: Test Result Visualization

### 3.1 Add Test Report Publishing

**Impact:** Better visibility into test failures
**Effort:** Low

```yaml
- name: Publish Test Results
  uses: EnricoMi/publish-unit-test-result-action@v2
  if: always()
  with:
    files: |
      build/Testing/**/*.xml
      build/test_results.xml
    check_name: Test Results (${{ matrix.os }})

- name: Generate Test Summary
  if: always()
  run: |
    cd build
    ctest --output-junit test_results.xml
```

---

## Priority 4: Code Quality & Static Analysis

### 4.1 Add clang-tidy Static Analysis

**Impact:** Catch bugs, enforce coding standards
**Effort:** Medium

```yaml
- name: Run clang-tidy
  run: |
    cd build
    run-clang-tidy -p . -header-filter='.*' \
      -checks='*,-fuchsia-*,-google-*,-llvm-*,-modernize-use-trailing-return-type' \
      ../src ../include 2>&1 | tee clang-tidy-report.txt

- name: Upload clang-tidy Report
  uses: actions/upload-artifact@v4
  with:
    name: clang-tidy-report
    path: build/clang-tidy-report.txt
```

### 4.2 Add cppcheck Analysis

**Impact:** Additional static analysis
**Effort:** Low

```yaml
- name: Install cppcheck
  run: sudo apt-get install -y cppcheck

- name: Run cppcheck
  run: |
    cppcheck --enable=all --inconclusive --xml --xml-version=2 \
      --suppress=missingIncludeSystem \
      src/ include/ 2> build/cppcheck-report.xml

- name: Upload cppcheck Report
  uses: actions/upload-artifact@v4
  with:
    name: cppcheck-report
    path: build/cppcheck-report.xml
```

---

## Priority 5: Security & Dependency Scanning

### 5.1 Add Dependency Scanning

**Impact:** Identify vulnerable dependencies
**Effort:** Low

```yaml
- name: Run Trivy vulnerability scanner
  uses: aquasecurity/trivy-action@master
  with:
    scan-type: 'fs'
    scan-ref: '.'
    format: 'sarif'
    output: 'trivy-results.sarif'

- name: Upload Trivy results to GitHub Security
  uses: github/codeql-action/upload-sarif@v3
  with:
    sarif_file: 'trivy-results.sarif'
```

---

## Priority 6: Performance Benchmarking

### 6.1 Add Performance Tests

**Impact:** Track performance regressions
**Effort:** Medium-High

```yaml
- name: Run Performance Benchmarks
  run: |
    cd build
    ./gara_benchmarks --benchmark_format=json --benchmark_out=benchmark_results.json

- name: Store Benchmark Results
  uses: benchmark-action/github-action-benchmark@v1
  with:
    tool: 'googletest'
    output-file-path: build/benchmark_results.json
    github-token: ${{ secrets.GITHUB_TOKEN }}
    auto-push: true
```

---

## Priority 7: Additional Improvements

### 7.1 Add Build Time Tracking

```yaml
- name: Build with Timing
  run: |
    cd build
    time make -j$(nproc) VERBOSE=1 2>&1 | tee build.log

- name: Parse Build Times
  run: |
    cd build
    grep "Built target" build.log | awk '{print $NF, $3}' > build_times.txt
```

### 7.2 Add Test Execution Time Analysis

```yaml
- name: Run tests with timing
  run: |
    cd build
    ctest --output-on-failure --verbose --output-junit test_results.xml --schedule-random

- name: Identify Slow Tests
  run: |
    cd build
    ctest --rerun-failed --output-on-failure
    grep "Test #" Testing/Temporary/LastTest.log | sort -k6 -n | tail -10
```

### 7.3 Add Conditional Coverage Requirements

```yaml
- name: Check Coverage Threshold
  run: |
    cd build
    COVERAGE=$(lcov --summary coverage_filtered.info | grep lines | awk '{print $2}' | sed 's/%//')
    if (( $(echo "$COVERAGE < 85.0" | bc -l) )); then
      echo "Coverage $COVERAGE% is below 85% threshold"
      exit 1
    fi
```

### 7.4 Add Documentation Generation

```yaml
- name: Generate Doxygen Documentation
  if: github.ref == 'refs/heads/main'
  run: |
    sudo apt-get install -y doxygen graphviz
    doxygen Doxyfile

- name: Deploy Documentation
  if: github.ref == 'refs/heads/main'
  uses: peaceiris/actions-gh-pages@v3
  with:
    github_token: ${{ secrets.GITHUB_TOKEN }}
    publish_dir: ./docs/html
```

---

## Recommended Implementation Order

### Phase 1: Quick Wins (Week 1)
1. ✅ Add dependency caching (Ubuntu + macOS)
2. ✅ Add CMake build caching with ccache
3. ✅ Add test result artifacts

**Expected Impact:** 50-70% faster CI, ~5-10 min savings per run

### Phase 2: Coverage & Quality (Week 2)
1. ✅ Add code coverage collection
2. ✅ Integrate Codecov
3. ✅ Add coverage badge to README
4. ✅ Set coverage threshold (85%)

**Expected Impact:** Visibility into untested code, quality gate

### Phase 3: Static Analysis (Week 3)
1. ✅ Add clang-tidy
2. ✅ Add cppcheck
3. ✅ Configure analysis rules
4. ✅ Fix reported issues

**Expected Impact:** Catch ~10-20 bugs, improve code quality

### Phase 4: Advanced Features (Week 4)
1. ✅ Add matrix testing (multiple compilers/OS)
2. ✅ Add security scanning (Trivy)
3. ✅ Add test result visualization
4. ✅ Add performance benchmarking

**Expected Impact:** Comprehensive CI/CD pipeline

---

## Estimated Time Investment vs. ROI

| Improvement | Effort | Time Savings | Quality Impact |
|-------------|--------|--------------|----------------|
| Dependency caching | 1h | 5 min/run | - |
| Build caching | 1h | 8 min/run | - |
| Code coverage | 2h | - | High |
| Matrix testing | 2h | - | High |
| Static analysis | 3h | - | Very High |
| Test visualization | 1h | 2 min/run | Medium |
| Security scanning | 1h | - | High |
| Performance benchmarks | 4h | - | Medium |

**Total Investment:** ~15 hours
**Daily Savings:** ~15 minutes per CI run × 10 runs/day = 2.5 hours/day
**Payback Period:** < 1 week

---

## Example Complete Workflow

See `IMPROVED_WORKFLOW.yml` for a complete example implementing all Priority 1-3 improvements.

---

## Monitoring & Metrics

Track these metrics after implementation:

- **CI Runtime:** Target < 5 minutes (currently ~15 min)
- **Cache Hit Rate:** Target > 80%
- **Code Coverage:** Target > 85%
- **Static Analysis Issues:** Target 0 new issues per PR
- **Flaky Tests:** Target 0 (track with test retries)

---

## Next Steps

1. Review this document with the team
2. Prioritize improvements based on team needs
3. Implement Phase 1 (quick wins) first
4. Measure impact and iterate
5. Continue with subsequent phases

**Recommendation:** Start with Phase 1 this week for immediate 50%+ speedup in CI times.
