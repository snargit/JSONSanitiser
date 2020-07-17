#include <gtest/gtest.h>

#include <JSONSanitiser.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

using namespace com::google::json;

std::string asString(std::variant<std::string_view, std::string> const &vrnt)
{
    return std::visit(
        [](auto &&v) -> std::string {
            if constexpr (std::is_same_v<std::decay_t<decltype(v)>, std::string>) {
                return v;
            } else {
                return std::string{v};
            }
        },
        vrnt);
}

TEST(SanitizerTests, TestEmpty)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("", true)), "null");
}

TEST(SanitizerTests, TestNull)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("null", true)), "null");
}

TEST(SanitizerTests, TestFalse)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("false", true)), "false");
}

TEST(SanitizerTests, TestTrue)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("true", true)), "true");
}

TEST(SanitizerTests, TestFalsePadBoth)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize(" false ", true)), " false ");
}

TEST(SanitizerTests, TestFalsePadLeft)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("  false", true)), "  false");
}

TEST(SanitizerTests, TestFalseReturn)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("false\n", true)), "false\n");
}

TEST(SanitizerTests, TestFalseTrue)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("false,true", true)), "false");
}

TEST(SanitizerTests, TestFooQuoted)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("\"foo\"", true)), "\"foo\"");
}

TEST(SanitizerTests, TestFooSingleQuote)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("'foo'", true)), "\"foo\"");
}