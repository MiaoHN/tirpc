#include <gtest/gtest.h>

TEST(SimpleTest, Addition) { EXPECT_EQ(1 + 1, 2); }

TEST(SimpleTest, StringCompare) {
  std::string s1 = "tirpc";
  std::string s2 = "tirpc";
  EXPECT_EQ(s1, s2);
}

TEST(SimpleTest, TrueIsTrue) { EXPECT_TRUE(true); }
