#include "boundfin/toolchain_manifest/manifest.hpp"
#include "boundfin_test.hpp"

#include <stdexcept>
#include <string>

namespace {

void extract_first_version_token_from_real_tool_output() {
  using boundfin::toolchain_manifest::extract_first_version_token;

  BOUNDFIN_CHECK_EQ(extract_first_version_token("cmake version 4.3.3\n\nCMake suite maintained..."),
                     std::string("4.3.3"));
  BOUNDFIN_CHECK_EQ(extract_first_version_token("1.13.2\n"), std::string("1.13.2"));
  BOUNDFIN_CHECK_EQ(
      extract_first_version_token("clang version 22.1.6\nTarget: x86_64-pc-linux-gnu\nThread model: posix"),
      std::string("22.1.6"));
  BOUNDFIN_CHECK_EQ(extract_first_version_token("v24.19.0"), std::string("24.19.0"));

  bool threw = false;
  try {
    const std::string unused = extract_first_version_token("no digits here at all");
    (void)unused;
  } catch (const std::invalid_argument &) {
    threw = true;
  }
  BOUNDFIN_CHECK(threw);
}

} // namespace

BOUNDFIN_TEST_MAIN(extract_first_version_token_from_real_tool_output)
