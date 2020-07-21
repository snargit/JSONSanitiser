// Copyright (C) 2012 Google Inc.
// Copyright (C) 2020 D Bailey
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <gtest/gtest.h>

#include <JSONSanitiser.hpp>

#include <cstddef>
#include <stdexcept>
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
    ASSERT_EQ(asString(JsonSanitizer::sanitize("\"<script>foo()</script>\"")),
              "\"\\u003cscript>foo()\\u003c/script>\"");
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
    ASSERT_EQ(asString(JsonSanitizer::sanitize("\"</ScR\xc4\xb0pT\"")),
              "\"\\u003c/ScR\xc4\xb0pT\"");
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

TEST(SanitizerTests, TestUnescapedFloatingPointKey)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("{1.0:0}")), "{\"1\":0}");
}

TEST(SanitizerTests, TestUnescapedFloatingPointKey2)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("{1.:0}")), "{\"1\":0}");
}

TEST(SanitizerTests, TestUnescapedFloatingPointKeyPadFront)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("{.5:0}")), "{\"0.5\":0}");
}

TEST(SanitizerTests, TestUnescapedNegativeFloatingPointKeyPadFront)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("{-.5:0}")), "{\"-0.5\":0}");
}

TEST(SanitizerTests, TestUnescapedFloatingPointKeyPadFrontRemoveSign)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("{+.5:0}")), "{\"0.5\":0}");
}

TEST(SanitizerTests, TestUnescapedNormalisedExponentKey)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("{+.5e2:0}")), "{\"50\":0}");
}

TEST(SanitizerTests, TestUnescapedNormalisedExponentKey2)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("{+1.5e+2:0}")), "{\"150\":0}");
}

TEST(SanitizerTests, TestRemoveSignAndPadLeadingFloatingPointUnescapedKey)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("{+.1:0}")), "{\"0.1\":0}");
}

TEST(SanitizerTests, TestRemoveSignAndPadLeadingFloatingPointUnescapedKey2DP)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("{+.01:0}")), "{\"0.01\":0}");
}

TEST(SanitizerTests, TestNormaliseUnescapedExponentKeyRemoveSign)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("{+.5e-2:0}")), "{\"0.005\":0}");
}

TEST(SanitizerTests, TestNormaliseUnescapedExponentKey)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("{10e100:0}")), "{\"1e+101\":0}");
}

TEST(SanitizerTests, TestNormaliseUnescapedNegativeExponentKey)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("{10e-100:0}")), "{\"1e-99\":0}");
}

TEST(SanitizerTests, TestNormaliseUnescapedNegativeExponentKey2)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("{10.5e-100:0}")), "{\"1.05e-99\":0}");
}

TEST(SanitizerTests, TestNormaliseUnescapedNegativeExponentKey3)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("{10.500e-100:0}")), "{\"1.05e-99\":0}");
}

TEST(SanitizerTests, TestNormaliseUnescapedExponentKey2)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("{12.34e100:0}")), "{\"1.234e+101\":0}");
}

TEST(SanitizerTests, TestNormaliseUnescapedNegativeExponentKey4)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("{.01234e-100:0}")), "{\"1.234e-102\":0}");
}

TEST(SanitizerTests, TestNormaliseUnescapedNegativeExponentKey5)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("{.01234e-100:0}")), "{\"1.234e-102\":0}");
}

TEST(SanitizerTests, TestEmptyObject)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("{}")), "{}");
}

// Remove grouping parentheses.
TEST(SanitizerTests, TestRemoveGroupingParentheses)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("({})")), "{}");
}

// Escape code-points and isolated surrogates which are not XML embeddable.
TEST(SanitizerTests, TestEscapeNonXMLCodePoints)
{
    std::string testData{"'"};
    testData.push_back('\x00');
    testData.append("\x08\x1f'");
    ASSERT_EQ(asString(JsonSanitizer::sanitize(testData)), "\"\\u0000\\u0008\\u001f\"");
}

TEST(SanitizerTests, TestSurrogates)
{
    // Test for u10000.
    ASSERT_EQ(asString(JsonSanitizer::sanitize("'\xf0\x90\x80\x80\xed\xb0\x80\xed\xa0\x80'")),
              "\"\xf0\x90\x80\x80\\udc00\\ud800\"");
}

TEST(SanitizerTests, TestBOM)
{
    // \ufffd\ufffe\uffff
    ASSERT_EQ(asString(JsonSanitizer::sanitize("'\xef\xbf\xbd\xef\xbf\xbe\xef\xbf\xbf'")),
              "\"\xef\xbf\xbd\\ufffe\\uffff\"");
}

// These control characters should be elided if they appear outside a string
// literal.
TEST(SanitizerTests, TestInvalidOutsideString)
{
    // \uffef\u000042\u0008\ud800\uffff\udc00
    std::string testData{"\xef\xbf\xaf"};
    testData.push_back('\x00');
    testData.append("42\x08\xed\xa0\x80\xef\xbf\xbf\xed\xb0\x80");
    std::string expected{"\"\xef\xbf\xaf\""};
    ASSERT_EQ(asString(JsonSanitizer::sanitize(testData)), expected);
}

TEST(SanitizerTests, TestInvalidOutsideString2)
{
    // \uffef\u0000\u0008\ud800\uffff\udc00
    std::string testData{"\xef\xbf\xaf"};
    testData.push_back('\x00');
    testData.append("\x08\xed\xa0\x80\xef\xbf\xbf\xed\xb0\x80");
    ASSERT_EQ(asString(JsonSanitizer::sanitize(testData)), "\"\xef\xbf\xaf\"");
}

TEST(SanitizerTests, TestInvalidOutsideString3)
{
    // \ufffe\u0000\u0008\ud800\uffff\udc00
    std::string testData{"\xef\xbf\xbe"};
    testData.push_back('\x00');
    testData.append("\x08\xed\xa0\x80\xef\xbf\xbf\xed\xb0\x80");
    ASSERT_EQ(asString(JsonSanitizer::sanitize(testData)), "null");
}

TEST(SanitizerTests, TestInvalidOutsideString4)
{
    // \ufffe\u000042\u0008\ud800\uffff\udc00
    std::string testData{"\xef\xbf\xbe"};
    testData.push_back('\x00');
    testData.append("42\x08\xed\xa0\x80\xef\xbf\xbf\xed\xb0\x80");
    std::string expected{"42"};
    ASSERT_EQ(asString(JsonSanitizer::sanitize(testData)), expected);
}

TEST(SanitizerTests, TestArrayCommaSeparatedEmptyElments)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("[,]")), "[null]");
}

TEST(SanitizerTests, TestArraySingleNullAndEmptyCommaSeparated)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("[null,]")), "[null]");
}

TEST(SanitizerTests, TestNestedObject)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("{\"a\":0,false\"x\":{\"\":-1}}")),
              "{\"a\":0,\"false\":\"x\",\"\":{\"\":-1}}");
}

TEST(SanitizerTests, TestTrueFalseArrayNoComma)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("[true false]")), "[true ,false]");
}

TEST(SanitizerTests, TestEscapedUnicodeInArray)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("[\"\\u00a0\\u1234\"]")), "[\"\\u00a0\\u1234\"]");
}

TEST(SanitizerTests, TestUnclosedObject)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("{a\\b\"c")), "{\"a\\b\":\"c\"}");
}

TEST(SanitizerTests, TestUnclosedObject2)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("{\"a\":\"b\",\"c\":")), "{\"a\":\"b\",\"c\":null}");
}

TEST(SanitizerTests, TestExponentOutOfRange)
{
    // Exponent way out of representable range in a JS double.
    ASSERT_EQ(asString(JsonSanitizer::sanitize("{1e0001234567890123456789123456789123456789:0}")),
              "{\"1e0001234567890123456789123456789123456789\":0}");
}

// This is an odd consequence of the way we recode octal literals.
// Our octal recoder does not fail on digits '8' or '9'.
TEST(SanitizerTests, TestOddOctalRecode)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("-016923547559")), "-2035208041");
}

// These triggered index out of bounds and assertion errors.
TEST(TestIssue3, TestIndexOutOfBounds)
{
    // \u00E4
    ASSERT_EQ(asString(JsonSanitizer::sanitize("[{{},\xc3\xa4")), "[{\"\":{},\"\xC3\xA4\":null}]");
}

TEST(TestIssue3, TestIndexOutOfBoundsAscii)
{
    // \u00E4
    ASSERT_EQ(asString(JsonSanitizer::sanitize("[{{},hello")), "[{\"\":{},\"hello\":null}]");
}

TEST(TestIssue3, TestIndexOutOfBounds2)
{
    // \u00E4\u00E4},\u00E4
    ASSERT_EQ(asString(JsonSanitizer::sanitize("[{{\xc3\xa4\xc3\xa4},\xc3\xa4")),
              "[{\"\":{\"\xC3\xA4\xC3\xA4\":null},\"\xC3\xA4\":null}]");
}

TEST(TestIssue3, TestIndexOutOfBounds2Ascii)
{
    // \u00E4\u00E4},\u00E4
    ASSERT_EQ(asString(JsonSanitizer::sanitize("[{{hello},world")),
              "[{\"\":{\"hello\":null},\"world\":null}]");
}


// Make sure that bare words are quoted.
TEST(TestIssue4, TestDevQuoted)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("dev")), "\"dev\"");
}
TEST(TestIssue4, TestEvalQuoted)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("eval")), "\"eval\"");
}
TEST(TestIssue4, TestCommentQuoted)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("comment")), "\"comment\"");
}
TEST(TestIssue4, TestFasleQuoted)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("fasle")), "\"fasle\"");
}
TEST(TestIssue4, TestFALSEQuoted)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("FALSE")), "\"FALSE\"");
}
TEST(TestIssue4, TestDevSlashCommentQuoted)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("dev/comment")), "\"dev/comment\"");
}
TEST(TestIssue4, TestDevCommentQuoted)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("dev\\comment")), "\"devcomment\"");
}
TEST(TestIssue4, TestDevNewlineCommentQuoted)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("dev\\ncomment")), "\"dev\\ncomment\"");
}
TEST(TestIssue4, TestDevCommentArrayQuoted)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("[dev\\, comment]")), "[\"dev\", \"comment\"]");
}

TEST(TestMaximumNestingLevel, TestNestedMaps)
{
    std::string const nestedMaps{
        "{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{}}}}}}}}}"
        "}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}"};
    std::string const sanitizedNestedMaps{
        "{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":"
        "{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":"
        "{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":"
        "{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":{\"\":"
        "{\"\":{\"\":{\"\":{\"\":{}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}"
        "}"};

    ASSERT_THROW(JsonSanitizer::sanitize(nestedMaps, JsonSanitizer::DEFAULT_NESTING_DEPTH),
                 std::out_of_range);
    ASSERT_EQ(asString(
                  JsonSanitizer::sanitize(nestedMaps, JsonSanitizer::DEFAULT_NESTING_DEPTH + 1)),
              sanitizedNestedMaps);
}

TEST(TestMaximumNestingLevel, TestMaximumNestingLevelAssignment)
{
    JsonSanitizer JS{"", std::numeric_limits<int>::min()};
    EXPECT_EQ(1, JS.getMaximumNestingDepth());
    JsonSanitizer JS2{"", std::numeric_limits<int>::max()};
    EXPECT_EQ(JsonSanitizer::MAXIMUM_NESTING_DEPTH, JS2.getMaximumNestingDepth());
}

TEST(TestFuzzer, TestClosedArray)
{
    // Discovered by fuzzer with seed -Dfuzz.seed=df3b4778ce54d00a
    // \ufeff-01742461140214282
    ASSERT_EQ(asString(JsonSanitizer::sanitize("\xef\xbb\xbf-01742461140214282]")),
              "\"\xef\xbb\xbf-01742461140214282\"");
}

TEST(TestFuzzer, TestClosedArray2)
{
    // Discovered by fuzzer with seed -Dfuzz.seed=df3b4778ce54d00a
    // \ufffe-01742461140214282
    ASSERT_EQ(asString(JsonSanitizer::sanitize("\xef\xbf\xbe-01742461140214282]")),
              "-68348121520322");
}

TEST(TestIssue13, TestDescription)
{
    ASSERT_EQ(asString(
                  JsonSanitizer::sanitize("[ { \"description\": \"aa##############aa\" }, 1 ]")),
              "[ { \"description\": \"aa##############aa\" }, 1 ]");
}

TEST(TestHtmlParserStateChanges, TestScript)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("\"<script\"")), "\"\\u003cscript\"");
}
TEST(TestHtmlParserStateChanges, TestScript2)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("\"<Script\"")), "\"\\u003cScript\"");
}
TEST(TestHtmlParserStateChanges, TestScriptTurkishI)
{
    // \u0130 is a Turkish dotted upper-case 'I' so the lower case version of
    // the tag name is "script".
    ASSERT_EQ(asString(JsonSanitizer::sanitize("\"<ScR\xc4\xb0pT\"")), "\"\\u003cScR\xc4\xb0pT\"");
}
TEST(TestHtmlParserStateChanges, TestScript3)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("\"<SCRIPT\n>\"")), "\"\\u003cSCRIPT\\n>\"");
}
TEST(TestHtmlParserStateChanges, TestScript4)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("<script")), "\"script\"");
}
TEST(TestHtmlParserStateChanges, TestXMLComment)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("\"<!--\"")), "\"\\u003c!--\"");
}
TEST(TestHtmlParserStateChanges, TestXMLComment2)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("<!--")), "-0");
}
TEST(TestHtmlParserStateChanges, TestXMLComment3)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("\"-->\"")), "\"--\\u003e\"");
}
TEST(TestHtmlParserStateChanges, TestXMLComment4)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("-->")), "-0");
}
TEST(TestHtmlParserStateChanges, TestScriptXMLComment)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("\"<!--<script>\"")),
              "\"\\u003c!--\\u003cscript>\"");
}

TEST(TestMyFuzzFail, Test1)
{
    ASSERT_EQ(asString(JsonSanitizer::sanitize("{00\\ud8b6\\udc3000\\u3\xe8\x9e\xbf"
                                               "\xec\xb6\xbb\xc2\x87\xee\x81\xb6\xdb\xa7\x77\xc3"
                                               "\x95\x30\x30\x5c\x75\x38\x64\x39\x32\x3c\x5c"
                                               "\x7c\xc2\x89\xec\xa3\x8a\xc3\xb2\x30\x30\x5c\x75"
                                               "\x63\x30\x30\x5c\x75\x64\x62\x31\x32\x5c\x75"
                                               "\x64\x65\x62\x65\xf1\xae\x9a\x9a\x30\x30\x5c\x75"
                                               "\x62\x33\x2e\x5c\x18\x09\x0d\x5d\x2c\x22\x5c"
                                               "\x4d\xf3\xbf\x9b\x85\xf4\x89\x87\x84\xef\x80\xae"
                                               "\xe6\x98\x82\x34\xc2\xa2\xc2\xaf\x5c\x0a\x30"
                                               "\x30\x5c\x75\x63\xc3\x83\x4c\xc3\x90\xc3\x88\x5c"
                                               "\x28\xed\x84\xb4\xe7\xa4\xb2\x30\x30\x5c\x75"
                                               "\x32\x30\x30\x5c\x75\x61\x30\x3e\xeb\x99\x8f\xed"
                                               "\x81\xbf\x4d\xc3\xbd\xc3\x97\xc3\xac\x5e\xec"
                                               "\x94\xbf\x72\xef\x93\x8f\x52\xec\xaa\x8c\xc2\x9c"
                                               "\xe6\x88\xb5\x5c\x73\xc2\x8f\x30\x30\x5c\x75"
                                               "\x65\x30\x63\x33\x30\x30\x5c\x75\x35\x34\x39\x61"
                                               "\x5c\xc2\xa2\xf1\x80\xa1\xbf\x66\xe7\x9a\xa0"
                                               "\xee\x9d\xb9\xe9\x97\x8e\xc3\x92\xe9\x9d\x8c\x69"
                                               "\xf1\x97\xa9\x95\xc3\xba\xc2\xaf\xe0\xb9\xa3"
                                               "\xe3\xb4\xb3\xc3\xb2\xe1\xb4\xab\x30\x30\x5c\x75"
                                               "\x32\x61\xe4\x81\x80\xc3\x90\xe2\xa8\x9d\xc2"
                                               "\x91\x35\x30\x30\x5c\x75\x31\x30\xc4\xa4\x6d\xc3"
                                               "\x9c\x5c\x5c\x7c\xc2\xbc\xe8\xac\x83\xf1\xbc"
                                               "\xaa\x87\xf0\x94\xb0\xb7\xf1\x9f\xa3\x86\xf4\x8f"
                                               "\x8f\x84\xc2\x93\xc3\xaa\xf3\xa0\xa2\x8d\x22"
                                               "\x2b\xf3\x8e\xb0\x8d\x22\x3a\x08\x20\x0d\x08\x66"
                                               "\x61\x6c\x73\x65\x2c\x22\xc3\xbf\xf1\xa0\xa2"
                                               "\x89\x30\x30\x5c\x75\x31\x30\xe5\x82\x86\xe3\x85"
                                               "\xa1\xe0\xba\x8b\x5a\xc2\xae\xc2\xa3\xef\x95"
                                               "\xaf\xc2\xbd\xe8\x96\x98\xf1\xb5\xa4\x94\xc2\x9f"
                                               "\x5c\x18\xf2\xb1\xb2\xbe\x22\x3a\x20\x0d\x08"
                                               "\x09")),
              "{\"0\":\"ud8b6\","
              "\"udc3000u3\xE8\x9E\xBF\xEC\xB6\xBB\xC2\x87\xEE\x81\xB6\xDB\xA7w\xC3\x95"
              "00\\u8d92<|\xC2\x89\xEC\xA3\x8A\xC3\xB2"
              "00uc00\\udb12\\udebe\xF1\xAE\x9A\x9A"
              "00ub3.\"\t\r:null}");
}
