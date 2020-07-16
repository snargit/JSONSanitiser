#include <gtest/gtest.h>

#include <JSONSanitiser.hpp>

using namespace com::google::json;

TEST(SanitizerTests, TestEmpty)
{
    ASSERT_EQ(JsonSanitizer::sanitize(""), "null");
}

TEST(SanitizerTests, TestNull)
{
    ASSERT_EQ(JsonSanitizer::sanitize("null"), "null");
}

TEST(SanitizerTests, TestFalse)
{
    ASSERT_EQ(JsonSanitizer::sanitize("false"), "false");
}

TEST(SanitizerTests, TestTrue)
{
     ASSERT_EQ(JsonSanitizer::sanitize("true"), "true");
}

TEST(SanitizerTests, TestFalsePadBoth)
{
     ASSERT_EQ(JsonSanitizer::sanitize(" false ")," false ");
}

TEST(SanitizerTests, TestFalsePadLeft)
{
    ASSERT_EQ(JsonSanitizer::sanitize("  false"), "  false ");
}

TEST(SanitizerTests, TestFalseReturn)
{
     ASSERT_EQ(JsonSanitizer::sanitize("false\n"), "false\n");
}

TEST(SanitizerTests, TestFalseTrue)
{
     ASSERT_EQ(JsonSanitizer::sanitize("false,true"), "false");
}

TEST(SanitizerTests, TestFooQuoted)
{
    ASSERT_EQ(JsonSanitizer::sanitize("\"foo\""), "\"foo\"");
}

TEST(SanitizerTests, TestFooSingleQuote)
{
     ASSERT_EQ(JsonSanitizer::sanitize("'foo'"), "\"foo\"");
}