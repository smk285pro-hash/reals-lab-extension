#pragma once

#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace reals::test {

// Terminal ANSI color helper
class Color {
public:
    static void initConsole() {
#ifdef _WIN32
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            DWORD dwMode = 0;
            if (GetConsoleMode(hOut, &dwMode)) {
                dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                SetConsoleMode(hOut, dwMode);
            }
        }
#endif
    }

    static constexpr std::string_view reset() { return "\033[0m"; }
    static constexpr std::string_view bold() { return "\033[1m"; }
    static constexpr std::string_view red() { return "\033[31m"; }
    static constexpr std::string_view green() { return "\033[32m"; }
    static constexpr std::string_view yellow() { return "\033[33m"; }
    static constexpr std::string_view blue() { return "\033[34m"; }
    static constexpr std::string_view magenta() { return "\033[35m"; }
    static constexpr std::string_view cyan() { return "\033[36m"; }
    static constexpr std::string_view gray() { return "\033[90m"; }
};

class TestFailureException : public std::exception {
public:
    explicit TestFailureException(std::string message) : m_msg(std::move(message)) {}
    [[nodiscard]] const char* what() const noexcept override { return m_msg.c_str(); }
private:
    std::string m_msg;
};

// Base Test Fixture
class TestFixture {
public:
    virtual ~TestFixture() = default;
    virtual void SetUp() {}
    virtual void TearDown() {}
};

struct TestCaseInfo {
    std::string suiteName;
    std::string testName;
    std::function<void()> runFunc;
    std::string file;
    int line = 0;
};

class TestRegistry {
public:
    static TestRegistry& instance() {
        static TestRegistry reg;
        return reg;
    }

    void registerTest(std::string suiteName, std::string testName,
                      std::function<void()> func, std::string file, int line) {
        m_tests.push_back(TestCaseInfo{
            std::move(suiteName),
            std::move(testName),
            std::move(func),
            std::move(file),
            line
        });
    }

    [[nodiscard]] const std::vector<TestCaseInfo>& tests() const {
        return m_tests;
    }

private:
    TestRegistry() = default;
    std::vector<TestCaseInfo> m_tests;
};

struct TestAutoRegister {
    TestAutoRegister(std::string suiteName, std::string testName,
                     std::function<void()> func, std::string file, int line) {
        TestRegistry::instance().registerTest(
            std::move(suiteName), std::move(testName), std::move(func), std::move(file), line);
    }
};

// Runner execution and reporting
class TestRunner {
public:
    static int run(int argc, char** argv) {
        Color::initConsole();
        std::string suiteFilter;
        std::string nameFilter;
        bool listOnly = false;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg.rfind("--suite=", 0) == 0) {
                suiteFilter = arg.substr(8);
            } else if (arg.rfind("--filter=", 0) == 0) {
                nameFilter = arg.substr(9);
            } else if (arg == "--list" || arg == "-l") {
                listOnly = true;
            } else if (arg == "--help" || arg == "-h") {
                std::cout << "Reals Lab Test Runner Options:\n"
                          << "  --suite=<Name>     Run only tests in specified suite\n"
                          << "  --filter=<Pattern> Run only tests matching substring\n"
                          << "  --list             List all registered test cases\n"
                          << "  --help             Show this help message\n";
                return 0;
            }
        }

        const auto& allTests = TestRegistry::instance().tests();

        if (listOnly) {
            std::cout << Color::bold() << "Registered Tests (" << allTests.size() << " total):\n" << Color::reset();
            for (const auto& t : allTests) {
                std::cout << "  - " << t.suiteName << "." << t.testName << " (" << t.file << ":" << t.line << ")\n";
            }
            return 0;
        }

        std::cout << Color::bold() << Color::cyan()
                  << "======================================================================\n"
                  << "        Reals Lab — Comprehensive End-to-End Test Suite\n"
                  << "======================================================================\n"
                  << Color::reset();

        size_t totalRun = 0;
        size_t passed = 0;
        size_t failed = 0;

        auto globalStart = std::chrono::high_resolution_clock::now();

        std::string currentSuite;

        for (const auto& t : allTests) {
            if (!suiteFilter.empty() && t.suiteName != suiteFilter) {
                continue;
            }
            if (!nameFilter.empty() && (t.suiteName + "." + t.testName).find(nameFilter) == std::string::npos) {
                continue;
            }

            if (t.suiteName != currentSuite) {
                currentSuite = t.suiteName;
                std::cout << "\n" << Color::bold() << Color::blue()
                          << "── Suite: " << currentSuite << " ──"
                          << Color::reset() << "\n";
            }

            ++totalRun;
            std::cout << Color::gray() << "  RUN    " << Color::reset()
                      << currentSuite << "." << t.testName << " ... " << std::flush;

            auto start = std::chrono::high_resolution_clock::now();
            bool testOk = true;
            std::string failMsg;

            try {
                t.runFunc();
            } catch (const TestFailureException& e) {
                testOk = false;
                failMsg = e.what();
            } catch (const std::exception& e) {
                testOk = false;
                failMsg = std::string("Unhandled std::exception: ") + e.what();
            } catch (...) {
                testOk = false;
                failMsg = "Unhandled non-std exception thrown";
            }

            auto finish = std::chrono::high_resolution_clock::now();
            auto durationUs = std::chrono::duration_cast<std::chrono::microseconds>(finish - start).count();

            if (testOk) {
                ++passed;
                std::cout << Color::green() << Color::bold() << "[ PASS ]" << Color::reset()
                          << Color::gray() << " (" << std::fixed << std::setprecision(2) << (durationUs / 1000.0) << " ms)\n"
                          << Color::reset();
            } else {
                ++failed;
                std::cout << Color::red() << Color::bold() << "[ FAIL ]" << Color::reset()
                          << Color::gray() << " (" << std::fixed << std::setprecision(2) << (durationUs / 1000.0) << " ms)\n"
                          << Color::reset();
                std::cout << Color::red() << "         " << failMsg << Color::reset() << "\n";
            }
        }

        auto globalFinish = std::chrono::high_resolution_clock::now();
        auto totalDurationMs = std::chrono::duration_cast<std::chrono::milliseconds>(globalFinish - globalStart).count();

        std::cout << "\n" << Color::bold() << Color::cyan()
                  << "======================================================================\n"
                  << "                          TEST SUMMARY\n"
                  << "======================================================================\n"
                  << Color::reset();

        std::cout << "  Total Executed : " << totalRun << "\n"
                  << "  Passed         : " << Color::green() << Color::bold() << passed << Color::reset() << "\n"
                  << "  Failed         : " << (failed > 0 ? Color::red() : Color::green()) << Color::bold() << failed << Color::reset() << "\n"
                  << "  Total Time     : " << totalDurationMs << " ms\n\n";

        if (failed == 0 && totalRun > 0) {
            std::cout << Color::green() << Color::bold()
                      << "  >>> 100% ALL TESTS PASSED SUCCESSFULLY! <<<\n\n"
                      << Color::reset();
            return 0;
        } else {
            std::cout << Color::red() << Color::bold()
                      << "  >>> TEST SUITE FAILED WITH " << failed << " ERROR(S) <<<\n\n"
                      << Color::reset();
            return 1;
        }
    }
};

// Assertion formatting helpers
inline void failTest(const char* file, int line, const std::string& message) {
    std::ostringstream oss;
    oss << file << ":" << line << " -> " << message;
    throw TestFailureException(oss.str());
}

} // namespace reals::test

// Assertion Macros
#define EXPECT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            ::reals::test::failTest(__FILE__, __LINE__, std::string("Expected true: ") + #cond); \
        } \
    } while (0)

#define EXPECT_FALSE(cond) \
    do { \
        if (cond) { \
            ::reals::test::failTest(__FILE__, __LINE__, std::string("Expected false: ") + #cond); \
        } \
    } while (0)

#define EXPECT_EQ(val1, val2) \
    do { \
        auto _v1 = (val1); \
        auto _v2 = (val2); \
        if (!(_v1 == _v2)) { \
            std::ostringstream _oss; \
            _oss << "Expected equality of " #val1 " and " #val2 "\n" \
                 << "           Actual " #val1 ": " << _v1 << "\n" \
                 << "           Actual " #val2 ": " << _v2; \
            ::reals::test::failTest(__FILE__, __LINE__, _oss.str()); \
        } \
    } while (0)

#define EXPECT_NE(val1, val2) \
    do { \
        auto _v1 = (val1); \
        auto _v2 = (val2); \
        if (_v1 == _v2) { \
            std::ostringstream _oss; \
            _oss << "Expected inequality of " #val1 " and " #val2 " (both equal " << _v1 << ")"; \
            ::reals::test::failTest(__FILE__, __LINE__, _oss.str()); \
        } \
    } while (0)

#define EXPECT_NEAR(val1, val2, eps) \
    do { \
        auto _v1 = static_cast<double>(val1); \
        auto _v2 = static_cast<double>(val2); \
        auto _e  = static_cast<double>(eps); \
        if (std::abs(_v1 - _v2) > _e) { \
            std::ostringstream _oss; \
            _oss << "Expected " #val1 " and " #val2 " within " << _e << "\n" \
                 << "           Actual " #val1 ": " << _v1 << "\n" \
                 << "           Actual " #val2 ": " << _v2 << "\n" \
                 << "           Difference: " << std::abs(_v1 - _v2); \
            ::reals::test::failTest(__FILE__, __LINE__, _oss.str()); \
        } \
    } while (0)

#define EXPECT_LT(val1, val2) \
    do { \
        if (!((val1) < (val2))) { \
            std::ostringstream _oss; \
            _oss << "Expected " #val1 " < " #val2 " (" << (val1) << " >= " << (val2) << ")"; \
            ::reals::test::failTest(__FILE__, __LINE__, _oss.str()); \
        } \
    } while (0)

#define EXPECT_LE(val1, val2) \
    do { \
        if (!((val1) <= (val2))) { \
            std::ostringstream _oss; \
            _oss << "Expected " #val1 " <= " #val2 " (" << (val1) << " > " << (val2) << ")"; \
            ::reals::test::failTest(__FILE__, __LINE__, _oss.str()); \
        } \
    } while (0)

#define EXPECT_GT(val1, val2) \
    do { \
        if (!((val1) > (val2))) { \
            std::ostringstream _oss; \
            _oss << "Expected " #val1 " > " #val2 " (" << (val1) << " <= " << (val2) << ")"; \
            ::reals::test::failTest(__FILE__, __LINE__, _oss.str()); \
        } \
    } while (0)

#define EXPECT_GE(val1, val2) \
    do { \
        if (!((val1) >= (val2))) { \
            std::ostringstream _oss; \
            _oss << "Expected " #val1 " >= " #val2 " (" << (val1) << " < " << (val2) << ")"; \
            ::reals::test::failTest(__FILE__, __LINE__, _oss.str()); \
        } \
    } while (0)

#define EXPECT_THROW(statement, exception_type) \
    do { \
        bool _threw = false; \
        try { \
            statement; \
        } catch (const exception_type&) { \
            _threw = true; \
        } catch (...) { \
            ::reals::test::failTest(__FILE__, __LINE__, "Expected exception type " #exception_type ", but caught different type"); \
        } \
        if (!_threw) { \
            ::reals::test::failTest(__FILE__, __LINE__, "Expected statement to throw " #exception_type ", but nothing was thrown"); \
        } \
    } while (0)

#define EXPECT_NO_THROW(statement) \
    do { \
        try { \
            statement; \
        } catch (const std::exception& _e) { \
            ::reals::test::failTest(__FILE__, __LINE__, std::string("Expected no exception, but caught: ") + _e.what()); \
        } catch (...) { \
            ::reals::test::failTest(__FILE__, __LINE__, "Expected no exception, but non-std exception caught"); \
        } \
    } while (0)

#define ASSERT_TRUE(cond) EXPECT_TRUE(cond)
#define ASSERT_FALSE(cond) EXPECT_FALSE(cond)
#define ASSERT_EQ(val1, val2) EXPECT_EQ(val1, val2)
#define ASSERT_NE(val1, val2) EXPECT_NE(val1, val2)
#define ASSERT_NEAR(val1, val2, eps) EXPECT_NEAR(val1, val2, eps)
#define ASSERT_LT(val1, val2) EXPECT_LT(val1, val2)
#define ASSERT_LE(val1, val2) EXPECT_LE(val1, val2)
#define ASSERT_GT(val1, val2) EXPECT_GT(val1, val2)
#define ASSERT_GE(val1, val2) EXPECT_GE(val1, val2)
#define ASSERT_THROW(stmt, exc) EXPECT_THROW(stmt, exc)
#define ASSERT_NO_THROW(stmt) EXPECT_NO_THROW(stmt)

// Test Registration Macros
#define TEST(SuiteName, TestName) \
    void Test_##SuiteName##_##TestName(); \
    static ::reals::test::TestAutoRegister s_reg_##SuiteName##_##TestName( \
        #SuiteName, #TestName, Test_##SuiteName##_##TestName, __FILE__, __LINE__); \
    void Test_##SuiteName##_##TestName()

#define TEST_F(FixtureClass, TestName) \
    class FixtureClass##_##TestName : public FixtureClass { \
    public: \
        void Run(); \
    }; \
    static void Runner_##FixtureClass##_##TestName() { \
        FixtureClass##_##TestName instance; \
        instance.SetUp(); \
        try { \
            instance.Run(); \
            instance.TearDown(); \
        } catch (...) { \
            instance.TearDown(); \
            throw; \
        } \
    } \
    static ::reals::test::TestAutoRegister s_reg_##FixtureClass##_##TestName( \
        #FixtureClass, #TestName, Runner_##FixtureClass##_##TestName, __FILE__, __LINE__); \
    void FixtureClass##_##TestName::Run()
