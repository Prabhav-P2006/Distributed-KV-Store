#ifndef MINI_GTEST_H
#define MINI_GTEST_H

#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace testing {
class Test {
public:
    virtual ~Test() = default;
    virtual void SetUp() {}
    virtual void TearDown() {}
};

class AssertionFailure : public std::runtime_error {
public:
    explicit AssertionFailure(const std::string& message)
        : std::runtime_error(message) {}
};

struct TestInfo {
    std::string suite;
    std::string name;
    std::function<void()> runner;
};

inline std::vector<TestInfo>& registry() {
    static std::vector<TestInfo> tests;
    return tests;
}

inline int& failureCount() {
    static int failures = 0;
    return failures;
}

inline bool registerTest(const std::string& suite, const std::string& name, std::function<void()> runner) {
    registry().push_back(TestInfo{suite, name, std::move(runner)});
    return true;
}

inline void addFailure(const std::string& message, const char* file, int line) {
    ++failureCount();
    std::cerr << file << ':' << line << ": " << message << '\n';
}

inline int InitGoogleTest(int*, char**) {
    return 0;
}

inline int RunAllTests() {
    int executed = 0;
    for (const auto& test : registry()) {
        try {
            test.runner();
        } catch (const AssertionFailure&) {
        } catch (const std::exception& exception) {
            addFailure(std::string("Unhandled exception in test ") + test.suite + "." + test.name + ": " + exception.what(), __FILE__, __LINE__);
        } catch (...) {
            addFailure(std::string("Unknown exception in test ") + test.suite + "." + test.name, __FILE__, __LINE__);
        }
        ++executed;
    }
    if (failureCount() == 0) {
        std::cout << "[mini-gtest] " << executed << " tests passed\n";
    } else {
        std::cout << "[mini-gtest] " << failureCount() << " assertion(s) failed across " << executed << " test(s)\n";
    }
    return failureCount() == 0 ? 0 : 1;
}

template <typename Left, typename Right>
std::string compareMessage(const char* op, const Left& left, const Right& right) {
    std::ostringstream stream;
    static_cast<void>(left);
    static_cast<void>(right);
    stream << "Comparison failed with operator " << op;
    return stream.str();
}

inline std::string truthMessage(const bool expected_true, const bool actual) {
    std::ostringstream stream;
    stream << "Expected condition to be " << (expected_true ? "true" : "false") << " but was " << std::boolalpha << actual;
    return stream.str();
}
}  // namespace testing

#define TEST_F(Fixture, Name)                                                                                         \
    class Fixture##_##Name##_Test : public Fixture {                                                                  \
    public:                                                                                                           \
        void TestBody();                                                                                              \
        static void Run() {                                                                                           \
            Fixture##_##Name##_Test instance;                                                                         \
            instance.SetUp();                                                                                         \
            try {                                                                                                     \
                instance.TestBody();                                                                                  \
            } catch (...) {                                                                                           \
                instance.TearDown();                                                                                  \
                throw;                                                                                                \
            }                                                                                                         \
            instance.TearDown();                                                                                      \
        }                                                                                                             \
    };                                                                                                                \
    static bool Fixture##_##Name##_registered =                                                                       \
        ::testing::registerTest(#Fixture, #Name, &Fixture##_##Name##_Test::Run);                                     \
    void Fixture##_##Name##_Test::TestBody()

#define EXPECT_TRUE(condition)                                                                                        \
    do {                                                                                                              \
        if (!(condition)) {                                                                                           \
            ::testing::addFailure(::testing::truthMessage(true, static_cast<bool>(condition)), __FILE__, __LINE__);  \
        }                                                                                                             \
    } while (false)

#define ASSERT_TRUE(condition)                                                                                        \
    do {                                                                                                              \
        if (!(condition)) {                                                                                           \
            ::testing::addFailure(::testing::truthMessage(true, static_cast<bool>(condition)), __FILE__, __LINE__);  \
            throw ::testing::AssertionFailure("ASSERT_TRUE failed");                                                  \
        }                                                                                                             \
    } while (false)

#define EXPECT_EQ(left, right)                                                                                        \
    do {                                                                                                              \
        const auto& _left = (left);                                                                                   \
        const auto& _right = (right);                                                                                 \
        if (!(_left == _right)) {                                                                                     \
            ::testing::addFailure(::testing::compareMessage("==", _left, _right), __FILE__, __LINE__);              \
        }                                                                                                             \
    } while (false)

#define ASSERT_EQ(left, right)                                                                                        \
    do {                                                                                                              \
        const auto& _left = (left);                                                                                   \
        const auto& _right = (right);                                                                                 \
        if (!(_left == _right)) {                                                                                     \
            ::testing::addFailure(::testing::compareMessage("==", _left, _right), __FILE__, __LINE__);              \
            throw ::testing::AssertionFailure("ASSERT_EQ failed");                                                    \
        }                                                                                                             \
    } while (false)

#define EXPECT_LT(left, right)                                                                                        \
    do {                                                                                                              \
        const auto& _left = (left);                                                                                   \
        const auto& _right = (right);                                                                                 \
        if (!(_left < _right)) {                                                                                      \
            ::testing::addFailure(::testing::compareMessage("<", _left, _right), __FILE__, __LINE__);               \
        }                                                                                                             \
    } while (false)

#define EXPECT_GT(left, right)                                                                                        \
    do {                                                                                                              \
        const auto& _left = (left);                                                                                   \
        const auto& _right = (right);                                                                                 \
        if (!(_left > _right)) {                                                                                      \
            ::testing::addFailure(::testing::compareMessage(">", _left, _right), __FILE__, __LINE__);               \
        }                                                                                                             \
    } while (false)

#define EXPECT_GE(left, right)                                                                                        \
    do {                                                                                                              \
        const auto& _left = (left);                                                                                   \
        const auto& _right = (right);                                                                                 \
        if (!(_left >= _right)) {                                                                                     \
            ::testing::addFailure(::testing::compareMessage(">=", _left, _right), __FILE__, __LINE__);              \
        }                                                                                                             \
    } while (false)

#endif
