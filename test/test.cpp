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
    ASSERT_EQ(asString(JsonSanitizer::sanitize("")), "null");
}

TEST(SanitizerTests, TestNull)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("null")), "null");
}

TEST(SanitizerTests, TestFalse)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("false")), "false");
}

TEST(SanitizerTests, TestTrue)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("true")), "true");
}

TEST(SanitizerTests, TestFalsePadBoth)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize(" false ")), " false ");
}

TEST(SanitizerTests, TestFalsePadLeft)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("  false")), "  false");
}

TEST(SanitizerTests, TestFalseReturn)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("false\n")), "false\n");
}

TEST(SanitizerTests, TestFalseTrue)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("false,true")), "false");
}

TEST(SanitizerTests, TestFooQuoted)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("\"foo\"")), "\"foo\"");
}

TEST(SanitizerTests, TestFooSingleQuote)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("'foo'")), "\"foo\"");
}

TEST(SanitizerTests, TestScript)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("\"<script>foo()</script>\"")), "\"\\u003cscript>foo()\\u003c/script>\"");
}

TEST(SanitizerTests, TestScript2)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("\"</SCRIPT\n>\"")), "\"\\u003c/SCRIPT\\n>\"");
}

TEST(SanitizerTests, TestScript3)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("\"</ScRIpT\"")), "\"\\u003c/ScRIpT\"");
}

// \u0130 (\xc4\xb0) is a Turkish dotted upper-case 'I' so the lower case version of
// the tag name is "script".
TEST(SanitizerTests, TestScriptTurkishDottedUpperCase)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("\"</ScR\xc4\xb0pT\"")), "\"\\u003c/ScR\xc4\xb0pT\"");
}

TEST(SanitizerTests, TestHelloHtmlB)
{
     ASSERT_EQ(asString(JsonSanitizer::sanitize("\"<b>Hello</b>\"")), "\"<b>Hello</b>\"");
}

TEST(SanitizerTests, TestHelloHtmlS)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("\"<s>Hello</s>\"")), "\"<s>Hello</s>\"");
}

TEST(SanitizerTests, TestNestedSquareBrackets)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("'<[[]]>'")), "\"<[[\\u005d]>\"");
}

TEST(SanitizerTests, TestDoubleCloseSquareBracket)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("']]>'")), "\"\\u005d]>\"");
}

TEST(SanitizerTests, TestDoubleSquareBracketWithZero)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("[[0]]>")), "[[0]]");
}

TEST(SanitizerTests, TestArrayTrailingCommaUnclosed)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("[1,-1,0.0,-0.5,1e2,")), "[1,-1,0.0,-0.5,1e2]");
}

TEST(SanitizerTests, TestNumberTrailingCommaArray)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("[1,2,3,]")), "[1,2,3]");
}

TEST(SanitizerTests, TestArrayRepeatedAndTrailingComma)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("[1,,3,]")), "[1,null,3]");
}

TEST(SanitizerTests, TestArrayNoElementSeparator)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("[1 2 3]")), "[1 ,2 ,3]");
}

TEST(SanitizerTests, TestDictionary)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("{ \"foo\": \"bar\" }")), "{ \"foo\": \"bar\" }");
}

TEST(SanitizerTests, TestDictionaryTrailingComma)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("{ \"foo\": \"bar\", }")), "{ \"foo\": \"bar\" }");
}

TEST(SanitizerTests, TestDictionaryCommaSeparated)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("{\"foo\",\"bar\"}")), "{\"foo\":\"bar\"}");
}

TEST(SanitizerTests, TestDictionaryUnquoted)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("{ foo: \"bar\" }")), "{ \"foo\": \"bar\" }");
}

TEST(SanitizerTests, TestDictionarySingleQuoteNotClosed)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("{ foo: 'bar")), "{ \"foo\": \"bar\"}");
}

TEST(SanitizerTests, TestDictionaryUnboundedArray)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("{ foo: ['bar")), "{ \"foo\": [\"bar\"]}");
}

TEST(SanitizerTests, TestNewlineLeadingComment)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("// comment\nfalse")), "false");
}

TEST(SanitizerTests, TestTrailingComment)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("false// comment")), "false");
}

TEST(SanitizerTests, TestTrailingCommentNewline)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("false// comment\n")), "false");
}

TEST(SanitizerTests, TestCStyleTrailingComment)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("false/* comment */")), "false");
}

TEST(SanitizerTests, TestCStyleTrailingCommentUnterminated)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("false/* comment *")), "false");
}

TEST(SanitizerTests, TestCStyleTrailingCommentUnterminated2)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("false/* comment ")), "false");
}

TEST(SanitizerTests, TestCStyleCommentMultiAsterix)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("/*/true**/false")), "false");
}

TEST(SanitizerTests, TestPositiveInteger)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("1")), "1");
}

TEST(SanitizerTests, TestNegativeInteger)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("-1")), "-1");
}

TEST(SanitizerTests, TestPositiveFloatingPoint)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("1.0")), "1.0");
}

TEST(SanitizerTests, TestNegativeFloatingPoint)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("-1.0")), "-1.0");
}

TEST(SanitizerTests, TestPositiveFloatingPoint2DP)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("1.05")), "1.05");
}

TEST(SanitizerTests, TestPositiveFloatingPointMultiDP)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("427.0953333")), "427.0953333");
}

TEST(SanitizerTests, TestExplictPositiveExponent)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("6.0221412927e+23")), "6.0221412927e+23");
}

TEST(SanitizerTests, TestPositiveExponent)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("6.0221412927e23")), "6.0221412927e23");
}

TEST(SanitizerTests, TestUnterminatedExponent)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("6.0221412927e")), "6.0221412927e0");
}

TEST(SanitizerTests, TestUnterminatedExponent2)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("6.0221412927e-")), "6.0221412927e-0");
}

TEST(SanitizerTests, TestUnterminatedExponent3)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("6.0221412927e+")), "6.0221412927e+0");
}

TEST(SanitizerTests, TestPositiveLargeNegativeExponent)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("1.660538920287695E-24")), "1.660538920287695E-24");
}

TEST(SanitizerTests, TestNegativeLargeNegativeExponent)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("-6.02e-23")), "-6.02e-23");
}

TEST(SanitizerTests, TestPadTrailingPositiveFloatingPoint)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("1.")), "1.0");
}

TEST(SanitizerTests, TestPadLeadingPositiveFloatingPoint)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize(".5")), "0.5");
}

TEST(SanitizerTests, TestPadLeadingNegativeFloatingPoint)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("-.5")), "-0.5");
}

TEST(SanitizerTests, TestRemoveSignAndPadLeadingFloatingPoint)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("+.5")), "0.5");
}

TEST(SanitizerTests, TestRemoveSignAndPadLeadingExponent)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("+.5e2")), "0.5e2");
}

TEST(SanitizerTests, TestRemoveSignLeadingExponent)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("+1.5e+2")), "1.5e+2");
}

TEST(SanitizerTests, TestRemoveSignAndPadLeadingNegativeExponent)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("+.5e-2")), "0.5e-2");
}

TEST(SanitizerTests, TestUnescapedNumericKey)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("{0:0}")), "{\"0\":0}");
}

TEST(SanitizerTests, TestUnescapedNegativeNumericKey)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("{-0:0}")), "{\"0\":0}");
}

TEST(SanitizerTests, TestUnescapedPositiveNumericKey)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("{+0:0}")), "{\"0\":0}");
}

//TEST(SanitizerTests, TestFooSingleQuote)
//{
//    ASSERT_EQ(asString(JsonSanitizer::sanitize("{\"1\":0}", "{1.0:0}");
//}
//TEST(SanitizerTests, TestFooSingleQuote)
//{
//    ASSERT_EQ(asString(JsonSanitizer::sanitize("{\"1\":0}", "{1.:0}");
//}
//TEST(SanitizerTests, TestFooSingleQuote)
//{
//    ASSERT_EQ(asString(JsonSanitizer::sanitize("{\"0.5\":0}", "{.5:0}");
//}
//TEST(SanitizerTests, TestFooSingleQuote)
//{
//    ASSERT_EQ(asString(JsonSanitizer::sanitize("{\"-0.5\":0}", "{-.5:0}");
//}
//TEST(SanitizerTests, TestFooSingleQuote)
//{
//    ASSERT_EQ(asString(JsonSanitizer::sanitize("{\"0.5\":0}", "{+.5:0}");
//}
//TEST(SanitizerTests, TestFooSingleQuote)
//{
//    ASSERT_EQ(asString(JsonSanitizer::sanitize("{\"50\":0}", "{+.5e2:0}");
//}
//TEST(SanitizerTests, TestFooSingleQuote)
//{
//    ASSERT_EQ(asString(JsonSanitizer::sanitize("{\"150\":0}", "{+1.5e+2:0}");
//}
//TEST(SanitizerTests, TestFooSingleQuote)
//{
//    ASSERT_EQ(asString(JsonSanitizer::sanitize("{\"0.1\":0}", "{+.1:0}");
//}
//TEST(SanitizerTests, TestFooSingleQuote)
//{
//    ASSERT_EQ(asString(JsonSanitizer::sanitize("{\"0.01\":0}", "{+.01:0}");
//}
//TEST(SanitizerTests, TestFooSingleQuote)
//{
//    ASSERT_EQ(asString(JsonSanitizer::sanitize("{\"0.005\":0}", "{+.5e-2:0}");
//}
//TEST(SanitizerTests, TestFooSingleQuote)
//{
//    ASSERT_EQ(asString(JsonSanitizer::sanitize("{\"1e+101\":0}", "{10e100:0}");
//}
//TEST(SanitizerTests, TestFooSingleQuote)
//{
//    ASSERT_EQ(asString(JsonSanitizer::sanitize("{\"1e-99\":0}", "{10e-100:0}");
//}
//TEST(SanitizerTests, TestFooSingleQuote)
//{
//    ASSERT_EQ(asString(JsonSanitizer::sanitize("{\"1.05e-99\":0}", "{10.5e-100:0}");
//}
//TEST(SanitizerTests, TestFooSingleQuote)
//{
//    ASSERT_EQ(asString(JsonSanitizer::sanitize("{\"1.05e-99\":0}", "{10.500e-100:0}");
//}
//TEST(SanitizerTests, TestFooSingleQuote)
//{
//    ASSERT_EQ(asString(JsonSanitizer::sanitize("{\"1.234e+101\":0}", "{12.34e100:0}");
//}
//TEST(SanitizerTests, TestFooSingleQuote)
//{
//    ASSERT_EQ(asString(JsonSanitizer::sanitize("{\"1.234e-102\":0}", "{.01234e-100:0}");
//}
//TEST(SanitizerTests, TestFooSingleQuote)
//{
//    ASSERT_EQ(asString(JsonSanitizer::sanitize("{\"1.234e-102\":0}", "{.01234e-100:0}");
//}

TEST(SanitizerTests, TestEmptyObject)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("{}")), "{}");
}

//// Remove grouping parentheses.
//TEST(SanitizerTests, TestFooSingleQuote)
//{
//    ASSERT_EQ(asString(JsonSanitizer::sanitize("{}", "({})");
//}
//// Escape code-points and isolated surrogates which are not XML embeddable.
//TEST(SanitizerTests, TestFooSingleQuote)
//{
//    ASSERT_EQ(asString(JsonSanitizer::sanitize("\"\\u0000\\u0008\\u001f\"", "'\u0000\u0008\u001f'");
//}
//TEST(SanitizerTests, TestFooSingleQuote)
//{
//    ASSERT_EQ(asString(JsonSanitizer::sanitize("\"\ud800\udc00\\udc00\\ud800\"", "'\ud800\udc00\udc00\ud800'");
//}
//TEST(SanitizerTests, TestFooSingleQuote)
//{
//    ASSERT_EQ(asString(JsonSanitizer::sanitize("\"\ufffd\\ufffe\\uffff\"", "'\ufffd\ufffe\uffff'");
//}
//// These control characters should be elided if they appear outside a string
//// literal.
//TEST(SanitizerTests, TestFooSingleQuote)
//{
//    ASSERT_EQ(asString(JsonSanitizer::sanitize("42", "\uffef\u000042\u0008\ud800\uffff\udc00");
//}
//TEST(SanitizerTests, TestFooSingleQuote)
//{
//    ASSERT_EQ(asString(JsonSanitizer::sanitize("null", "\uffef\u0000\u0008\ud800\uffff\udc00");
//}
//TEST(SanitizerTests, TestFooSingleQuote)
//{
//    ASSERT_EQ(asString(JsonSanitizer::sanitize("[null]", "[,]");
//}
//TEST(SanitizerTests, TestFooSingleQuote)
//{
//    ASSERT_EQ(asString(JsonSanitizer::sanitize("[null]", "[null,]");
//}
//TEST(SanitizerTests, TestFooSingleQuote)
//{
//    ASSERT_EQ(asString(JsonSanitizer::sanitize("{\"a\":0,\"false\":\"x\",\"\":{\"\":-1}}", "{\"a\":0,false\"x\":{\"\":-1}}");
//}
//TEST(SanitizerTests, TestFooSingleQuote)
//{
//    ASSERT_EQ(asString(JsonSanitizer::sanitize("[true ,false]", "[true false]");
//}
//TEST(SanitizerTests, TestFooSingleQuote)
//{
//    ASSERT_EQ(asString(JsonSanitizer::sanitize("[\"\\u00a0\\u1234\"]");
//}
//TEST(SanitizerTests, TestFooSingleQuote)
//{
//    ASSERT_EQ(asString(JsonSanitizer::sanitize("{\"a\\b\":\"c\"}", "{a\\b\"c");
//}
//TEST(SanitizerTests, TestFooSingleQuote)
//{
//    ASSERT_EQ(asString(JsonSanitizer::sanitize("{\"a\":\"b\",\"c\":null}", "{\"a\":\"b\",\"c\":");
//}
//TEST(SanitizerTests, TestFooSingleQuote)
//{
//    ASSERT_EQ(asString(JsonSanitizer::sanitize("{\"1e0001234567890123456789123456789123456789\":0}",
//// Exponent way out of representable range in a JS double.
//                "{1e0001234567890123456789123456789123456789:0}");
//}
//// This is an odd consequence of the way we recode octal literals.
//// Our octal recoder does not fail on digits '8' or '9'.
//TEST(SanitizerTests, TestFooSingleQuote)
//{
//    ASSERT_EQ(asString(JsonSanitizer::sanitize("-2035208041", "-016923547559");
//}
