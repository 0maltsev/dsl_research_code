#pragma once

// Minimal standalone test harness shared by every BoundFin test binary.
// Deliberately small: one assertion macro pair and a main() that reports a
// single named check's pass/fail. No external test-framework dependency.

#include <exception>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace boundfin::testing {

class CheckFailure : public std::runtime_error {
public:
  explicit CheckFailure(const std::string &message) : std::runtime_error(message) {}
};

} // namespace boundfin::testing

#define BOUNDFIN_CHECK(condition)                                                                                    \
  do {                                                                                                               \
    if (!(condition)) {                                                                                             \
      std::ostringstream boundfin_check_message;                                                                    \
      boundfin_check_message << __FILE__ << ":" << __LINE__ << ": CHECK failed: " #condition;                       \
      throw boundfin::testing::CheckFailure(boundfin_check_message.str());                                          \
    }                                                                                                                \
  } while (false)

#define BOUNDFIN_CHECK_EQ(lhs, rhs)                                                                                  \
  do {                                                                                                               \
    const auto &boundfin_check_lhs = (lhs);                                                                         \
    const auto &boundfin_check_rhs = (rhs);                                                                         \
    if (!(boundfin_check_lhs == boundfin_check_rhs)) {                                                              \
      std::ostringstream boundfin_check_message;                                                                    \
      boundfin_check_message << __FILE__ << ":" << __LINE__ << ": CHECK_EQ failed: " #lhs " == " #rhs << "\n  lhs=" \
                              << boundfin_check_lhs << "\n  rhs=" << boundfin_check_rhs;                             \
      throw boundfin::testing::CheckFailure(boundfin_check_message.str());                                          \
    }                                                                                                                \
  } while (false)

#define BOUNDFIN_TEST_MAIN(test_function)                                                                            \
  int main() {                                                                                                       \
    try {                                                                                                            \
      test_function();                                                                                              \
    } catch (const std::exception &ex) {                                                                             \
      std::cerr << "FAIL [" #test_function "]: " << ex.what() << "\n";                                              \
      return 1;                                                                                                      \
    }                                                                                                                \
    std::cout << "OK [" #test_function "]\n";                                                                       \
    return 0;                                                                                                        \
  }
