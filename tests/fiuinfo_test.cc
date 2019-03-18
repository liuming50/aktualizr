#include <gtest/gtest.h>
#include <unistd.h>

#include "utilities/fault_injection.h"
#include "utilities/utils.h"

std::string fiu_script;

TEST(Fiuinfo, PassInfoWithRun) {
  EXPECT_NE(fiu_fail("fail"), 0);
  EXPECT_EQ(fault_injection_last_info(), "test");
}

TEST(Fiuinfo, PassInfoWithCtrl) {
  EXPECT_EQ(fiu_fail("failctrl"), 0);
  Utils::shell(fiu_script + " ctrl -c 'enable name=failctrl,failinfo=test_ctrl' " + std::to_string(getpid()), nullptr);
  EXPECT_NE(fiu_fail("failctrl"), 0);
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  if (argc != 2) {
    std::cerr << "Error: " << argv[0] << " requires the path to the fiu wrapper script.\n";
    return EXIT_FAILURE;
  }
  fiu_script = argv[1];

  return RUN_ALL_TESTS();
}
#endif
