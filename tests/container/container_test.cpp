#include "container/string_convert.h"

#include <gtest/gtest.h>

#include <string>

TEST(string_convert, to_wstring) {
  std::string src  =  "Hello World";
  std::wstring exp = L"Hello World";
  EXPECT_EQ(exp, convert::to_wstring(src));
}

TEST(string_convert, to_string) {
  std::wstring src = L"Hello World";
  std::string exp  =  "Hello World";
  EXPECT_EQ(exp, convert::to_string(src));
}
