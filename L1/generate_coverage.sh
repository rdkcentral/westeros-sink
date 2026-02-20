#!/bin/bash
################################################################################
# Generate Coverage Report for L1 Tests
################################################################################

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

cd "${BUILD_DIR}"

echo "Generating L1 Coverage Report..."

# Run all tests to generate coverage data
ctest --output-on-failure 2>&1 | tee ctest_output.log

# Run test executables directly to ensure .gcda generation
for test_dir in brcmtests drmtests emutests icegdltests rawtests rpitests v4l2tests; do
    if [ -d "${test_dir}" ] && [ -f "${test_dir}/L1_test_${test_dir%tests}" ]; then
        (cd "${test_dir}" && ./L1_test_${test_dir%tests} > /dev/null 2>&1) || true
    fi
done

# Flush coverage data
sync
sleep 1

echo "Collecting coverage data..."

# Remove any zero-size .gcda files that might cause issues
find . -name "*.gcda" -size 0 -delete 2>/dev/null || true

# Use geninfo with permissive settings for C++ code
geninfo \
    . \
    --output-filename coverage_all.info \
    --gcov-tool gcov \
    --ignore-errors source,gcov,mismatch,format \
    --compat-libtool \
    --memory 0 \
    2>&1 | tail -50 || true

if [ ! -f "coverage_all.info" ]; then
    lcov --directory . \
         --capture \
         --output-file coverage_all.info \
         --ignore-errors all \
         2>&1 | tail -20 || true
fi



echo "Removing system and external library coverage..."
if grep -q "^SF:" coverage_all.info 2>/dev/null; then
    lcov --remove coverage_all.info \
        '/usr/*' \
        '*/gtest/*' \
        --output-file coverage_all.info \
        --ignore-errors source,mismatch \
        2>&1 | tail -5 || true
else
    echo "  Skipped - no source files in coverage data"
fi

if grep -q "^SF:" coverage_all.info 2>/dev/null; then
    lcov --remove coverage_all.info \
        '*/L1/mocks/*' \
        '*/L1/brcmtests/*' \
        '*/L1/drmtests/*' \
        '*/L1/emutests/*' \
        '*/L1/icegdltests/*' \
        '*/L1/rawtests/*' \
        '*/L1/rpitests/*' \
        '*/L1/v4l2tests/*' \
        '*/utilities/*' \
        '*/googletest/*' \
        --output-file coverage_all.info \
        --ignore-errors source,mismatch \
        2>&1 | tail -5 || true
else
    echo "  Skipped - no source files in coverage data"
fi

echo "Generating HTML coverage report..."
if grep -q "^SF:" coverage_all.info 2>/dev/null; then
    genhtml coverage_all.info \
        --output-directory coverage_html \
        --ignore-errors source,mismatch \
        2>&1 | tail -10 || true
else
    echo "  Skipped - no source files in coverage data"
fi

echo ""
echo "=========================================="
echo "Coverage Summary"
echo "=========================================="
if [ -f "coverage_all.info" ]; then
    # Extract coverage statistics
    LF_TOTAL=$(grep "^LF:" coverage_all.info 2>/dev/null | awk -F: '{sum+=$2} END {if (sum>0) print sum; else print 0}' || echo "0")
    LH_TOTAL=$(grep "^LH:" coverage_all.info 2>/dev/null | awk -F: '{sum+=$2} END {if (sum>0) print sum; else print 0}' || echo "0")
    
    LF_TOTAL=$((LF_TOTAL + 0))
    LH_TOTAL=$((LH_TOTAL + 0))
    
    echo "Lines Instrumented: ${LF_TOTAL}"
    echo "Lines Hit: ${LH_TOTAL}"
    
    # Calculate coverage percentage
    if [ "$LF_TOTAL" -gt 0 ]; then
        COVERAGE_PCT=$(awk "BEGIN {printf \"%.2f\", ($LH_TOTAL / $LF_TOTAL) * 100}")
        echo ""
        echo "Coverage Percentage: ${COVERAGE_PCT}%"
    fi
fi
echo ""
echo "========================================="
echo "✓ Coverage report generated!"
echo "========================================="

exit 0