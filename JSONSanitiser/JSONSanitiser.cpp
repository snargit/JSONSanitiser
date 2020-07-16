#include "JSONSanitiser.hpp"

#include <array>
#include <cassert>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <iterator>

namespace {
std::array<char, 16> const HEX_DIGITS = {'0', '1', '2', '3', '4', '5', '6', '7',
                                         '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

} // namespace

namespace utf8 {

// Reused from Boost
inline bool invalid_continuing_octet(unsigned char octet_1)
{
    return (octet_1 < 0x80 || 0xbf < octet_1);
}

inline bool invalid_leading_octet(unsigned char octet_1)
{
    return (0x7f < octet_1 && octet_1 < 0xc0) || (octet_1 > 0xfd);
}

size_t backup_one_character_octect_count(unsigned char const *c, size_t max)
{
    size_t i = 1;
    while (i != max) {
        auto &ch = *(c - i);
        if (!invalid_continuing_octet(ch)) {
            ++i;
            continue;
        } else if (!invalid_leading_octet(ch)) {
            break;
        } else {
            break;
        }
    }
    return i;
}

inline unsigned int get_octet_count(unsigned char lead_octet)
{
    // if the 0-bit (MSB) is 0, then 1 character
    if (lead_octet <= 0x7f)
        return 1;

    // Otherwise the count number of consecutive 1 bits starting at MSB
    //    assert(0xc0 <= lead_octet && lead_octet <= 0xfd);

    if (0xc0 <= lead_octet && lead_octet <= 0xdf)
        return 2;
    else if (0xe0 <= lead_octet && lead_octet <= 0xef)
        return 3;
    else if (0xf0 <= lead_octet && lead_octet <= 0xf7)
        return 4;
    else if (0xf8 <= lead_octet && lead_octet <= 0xfb)
        return 5;
    else
        return 6;
}

inline std::string_view char_at(std::string const &s, size_t start)
{
    return std::string_view(s.data() + start, get_octet_count(s[start]));
}

inline std::string_view char_at(std::string_view const s, size_t start)
{
    return std::string_view(s.data() + start, get_octet_count(s[start]));
}

uint32_t to_utf32(std::string_view s)
{
    constexpr uint8_t UTF8_ONE_BYTE_MASK   = 0b10000000;
    constexpr uint8_t UTF8_TWO_BYTE_MASK   = 0b11100000;
    constexpr uint8_t UTF8_THREE_BYTE_MASK = 0b11110000;
    constexpr uint8_t UTF8_FOUR_BYTE_MASK  = 0b11111000;
    constexpr uint8_t UTF8_FIVE_BYTE_MASK  = 0b11111100;
    constexpr uint8_t UTF8_SIX_BYTE_MASK   = 0b11111110;

    constexpr uint8_t UTF8_CONTINUATION_MASK = 0b00111111;

    uint32_t c = 0;
    switch (s.length()) {
        case 1:
            c = static_cast<uint8_t>(s[0]) & ~UTF8_ONE_BYTE_MASK;
            break;
        case 2:
            c = static_cast<uint32_t>(static_cast<uint8_t>(s[0]) & ~UTF8_TWO_BYTE_MASK) << 6 |
                static_cast<uint32_t>(static_cast<uint8_t>(s[1]) & UTF8_CONTINUATION_MASK);
            break;
        case 3:
            c = static_cast<uint32_t>(static_cast<uint8_t>(s[0]) & ~UTF8_THREE_BYTE_MASK) << 12 |
                static_cast<uint32_t>(static_cast<uint8_t>(s[1]) & UTF8_CONTINUATION_MASK) << 6 |
                static_cast<uint32_t>(static_cast<uint8_t>(s[2]) & UTF8_CONTINUATION_MASK);
            break;
        case 4:
            c = static_cast<uint32_t>(static_cast<uint8_t>(s[0]) & ~UTF8_FOUR_BYTE_MASK) << 18 |
                static_cast<uint32_t>(static_cast<uint8_t>(s[1]) & UTF8_CONTINUATION_MASK) << 12 |
                static_cast<uint32_t>(static_cast<uint8_t>(s[2]) & UTF8_CONTINUATION_MASK) << 6 |
                static_cast<uint32_t>(static_cast<uint8_t>(s[3]) & UTF8_CONTINUATION_MASK);
            break;
        case 5:
            c = static_cast<uint32_t>(static_cast<uint8_t>(s[0]) & ~UTF8_FIVE_BYTE_MASK) << 24 |
                static_cast<uint32_t>(static_cast<uint8_t>(s[1]) & UTF8_CONTINUATION_MASK) << 18 |
                static_cast<uint32_t>(static_cast<uint8_t>(s[2]) & UTF8_CONTINUATION_MASK) << 12 |
                static_cast<uint32_t>(static_cast<uint8_t>(s[3]) & UTF8_CONTINUATION_MASK) << 6 |
                static_cast<uint32_t>(static_cast<uint8_t>(s[4]) & UTF8_CONTINUATION_MASK);
            break;
        case 6:
            c = static_cast<uint32_t>(static_cast<uint8_t>(s[0]) & ~UTF8_FIVE_BYTE_MASK) << 30 |
                static_cast<uint32_t>(static_cast<uint8_t>(s[1]) & UTF8_CONTINUATION_MASK) << 24 |
                static_cast<uint32_t>(static_cast<uint8_t>(s[2]) & UTF8_CONTINUATION_MASK) << 18 |
                static_cast<uint32_t>(static_cast<uint8_t>(s[3]) & UTF8_CONTINUATION_MASK) << 12 |
                static_cast<uint32_t>(static_cast<uint8_t>(s[4]) & UTF8_CONTINUATION_MASK) << 6 |
                static_cast<uint32_t>(static_cast<uint8_t>(s[5]) & UTF8_CONTINUATION_MASK);
            break;
    }
    return c;
}

} // namespace utf8

namespace com::google::json {

void JsonSanitizer::sanitize()
{
    _bracketDepth = 0u;
    _cleaned      = 0u;
    _sanitizedJson.clear();

    State state = State::START_ARRAY;
    if (_jsonish.empty()) {
        _sanitizedJson = "null";
        return;
    }

    // n is count of code points, not bytes
    auto const n = _jsonish.length();
    for (size_t i = 0u; i < n; ++i) {

        try {
            auto ch = utf8::char_at(_jsonish, i);
            if (SUPER_VERBOSE_AND_SLOW_LOGGING) {
                auto sanitizedJsonStr = _sanitizedJson;
                sanitizedJsonStr.append(_jsonish.substr(_cleaned, i - _cleaned));
                std::cerr << "i=" << i << ", ch =" << ch << ", state=" << toString(state)
                          << ", sanitized=" << sanitizedJsonStr << "\n";
            }
            auto abortLoop = false;
            if (ch.length() == 1) {
                switch (ch.front()) {
                    case '\t':
                    case '\n':
                    case '\r':
                    case ' ':
                        break;

                    case '"':
                    case '\'': {
                        state       = requireValueState(i, state, true);
                        auto strEnd = endOfQuotedString(_jsonish, i);
                        sanitizeString(i, strEnd);
                        i = strEnd - i;
                    } break;
                    case '(':
                    case ')':
                        elide(i, i + 1);
                        break;
                    case '{':
                    case '[': {

                        state = requireValueState(i, state, false);
                        if (_isMap.empty()) {
                            _isMap.resize(_maximumNestingDepth, false);
                        }
                        auto map              = ch.front() == '{';
                        _isMap[_bracketDepth] = map;
                        ++_bracketDepth;
                        state = map ? State::START_MAP : State::START_ARRAY;
                    } break;
                    case '}':
                    case ']':
                        if (_bracketDepth == 0) {
                            elide(i, _jsonish.length());
                            abortLoop = true;
                            break;
                        }
                        switch (state) {
                            case State::BEFORE_VALUE:
                                insert(i, "null");
                                break;
                            case State::BEFORE_ELEMENT:
                            case State::BEFORE_KEY:
                                elideTrailingComma(i);
                                break;
                            case State::AFTER_KEY:
                                insert(i, ":null");
                                break;
                            case State::START_MAP:
                            case State::START_ARRAY:
                            case State::AFTER_ELEMENT:
                            case State::AFTER_VALUE:
                                break;
                        }
                        --_bracketDepth;
                        {
                            auto closeBracket = _isMap[_bracketDepth] ? '}' : ']';
                            if (ch.front() != closeBracket) {
                                replace(i, i + 1, closeBracket);
                            }
                            state = ((_bracketDepth == 0) || (!_isMap[_bracketDepth - 1])) ?
                                        State::AFTER_ELEMENT :
                                        State::AFTER_VALUE;
                        }
                        break;
                    case ',':
                        if (_bracketDepth == 0) {
                            throw UnbracketedComma{};
                        }
                        switch (state) {
                            // Normal
                            case State::AFTER_ELEMENT:
                                state = State::BEFORE_ELEMENT;
                                break;
                            case State::AFTER_VALUE:
                                state = State::BEFORE_KEY;
                                break;
                            // Array elision.
                            case State::START_ARRAY:
                            case State::BEFORE_ELEMENT:
                                insert(i, "null");
                                state = State::BEFORE_ELEMENT;
                                break;
                            // Ignore
                            case State::START_MAP:
                            case State::BEFORE_KEY:
                            case State::AFTER_KEY:
                                elide(i, i + 1);
                                break;
                            // Supply missing value.
                            case State::BEFORE_VALUE:
                                insert(i, "null");
                                state = State::BEFORE_KEY;
                                break;
                        }
                        break;
                    case ':':
                        if (state == State::AFTER_KEY) {
                            state = State::BEFORE_VALUE;
                        } else {
                            elide(i, i + 1);
                        }
                        break;
                    case '/': {

                        auto end = i + 1;
                        if (end < n) {
                            auto jca = utf8::char_at(_jsonish, end);
                            if (jca.length() == 1) {
                                switch (jca.front()) {
                                    case '/':
                                        end = n; // Worst case.
                                        for (auto j = i + 2; j < n;
                                             j += utf8::get_octet_count(_jsonish[j])) {
                                            auto cch = utf8::char_at(_jsonish, j);
                                            if (cch == "\n" || cch == "\r" ||
                                                cch == "\xe2\x80\xa8" || cch == "\xe2\x80\xa9") {
                                                end = j + cch.length();
                                                break;
                                            }
                                        }
                                        break;
                                    case '*':
                                        end = n;
                                        if (i + 3 < n) {
                                            for (auto j = i + 2; (j = _jsonish.find('/', j + 1)) !=
                                                                 std::string_view::npos;) {
                                                if (_jsonish[j - 1] == '*') {
                                                    end = j + 1;
                                                    break;
                                                }
                                            }
                                        }
                                        break;
                                }
                            }
                        }
                        elide(i, end);
                        i = end - 1;
                    } break;
                    default:
                        auto runEnd = i;
                        for (; runEnd < n; ++runEnd) {
                            auto tch = utf8::char_at(_jsonish, runEnd);
                            if (tch.size() == 1) {
                                auto &tchf = tch.front();
                                if ((('a' <= tchf) && (tchf <= 'z')) ||
                                    (('0' <= tchf) && (tchf <= '9')) || (tchf == '+') ||
                                    (tchf == '-') || (tchf == '.') ||
                                    (('A' <= tchf) && (tchf <= 'Z')) || (tchf == '_') ||
                                    (tchf == '$')) {
                                    continue;
                                }
                            }
                            break;
                        }

                        if (runEnd == i) {
                            elide(i, i + 1);
                            break;
                        }

                        state          = requireValueState(i, state, true);
                        auto &chf      = ch.front();
                        auto  isNumber = (('0' <= chf) && (chf <= '9')) || (chf == '.') ||
                                        (chf == '+') || (chf == '-');
                        auto bisKeyword = !isNumber && isKeyword(i, runEnd);

                        if (!(isNumber || bisKeyword)) {
                            // We're going to have to quote the output.  Further expand to
                            // include more of an unquoted token in a string.
                            for (; runEnd < n; runEnd += utf8::get_octet_count(_jsonish[runEnd])) {
                                if (isJsonSpecialChar(runEnd)) {
                                    break;
                                }
                            }
                            if ((runEnd < n) && (utf8::char_at(_jsonish, runEnd) == "\"")) {
                                ++runEnd;
                            }
                        }
                        if (state == State::AFTER_KEY) {
                            // We need to quote whatever we have since it is used as a
                            // property name in a map and only quoted strings can be used that
                            // way in JSON.
                            insert(i, '"');
                            if (isNumber) {
                                // By JS rules,
                                //   { .5e-1: "bar" }
                                // is the same as
                                //   { "0.05": "bar" }
                                // because a number literal is converted to its string form
                                // before being used as a property name.
                                canonicalizeNumber(i, runEnd);
                                // We intentionally ignore the return value of canonicalize.
                                // Uncanonicalizable numbers just get put straight through as
                                // string values.
                                insert(runEnd, '"');
                            } else {
                                sanitizeString(i, runEnd);
                            }
                        } else {
                            if (isNumber) {
                                // Convert hex and octal constants to decimal and ensure that
                                // integer and fraction portions are not empty.
                                normalizeNumber(i, runEnd);
                            } else if (!bisKeyword) {
                                // Treat as an unquoted string literal.
                                insert(i, '"');
                                sanitizeString(i, runEnd);
                            }
                        }
                        i = runEnd - 1;
                        break;
                }
            }
            if (abortLoop) {
                break;
            }
        } catch (UnbracketedComma const &) {
            elide(i, _jsonish.length());
            break;
        }
    }
    if ((state == State::START_ARRAY) && (_bracketDepth == 0)) {
        // No tokens.  Only whitespace
        insert(n, "null");
        state = State::AFTER_ELEMENT;
    }

    if (SUPER_VERBOSE_AND_SLOW_LOGGING) {
        std::cerr << "state=" << toString(state) << ", sanitizedJson=" << _sanitizedJson
                  << ", cleaned=" << _cleaned << ", bracketDepth=" << _bracketDepth << std::endl;
    }

    if (!_sanitizedJson.empty() || (_cleaned != 0) || (_bracketDepth != 0)) {
        _sanitizedJson.append(_jsonish.substr(_cleaned, n - _cleaned));
        _cleaned = n;

        switch (state) {
            case State::BEFORE_ELEMENT:
            case State::BEFORE_KEY:
                elideTrailingComma(n);
                break;
            case State::AFTER_KEY:
                _sanitizedJson.append(":null");
                break;
            case State::BEFORE_VALUE:
                _sanitizedJson.append("null");
                break;
            default:
                break;
        }

        // Insert brackets to close unclosed content.
        while (_bracketDepth != 0) {
            _sanitizedJson.push_back(_isMap[--_bracketDepth] ? '}' : ']');
        }
    }
}

std::string_view JsonSanitizer::toString() const noexcept
{
    return !_sanitizedJson.empty() ? std::string_view{_sanitizedJson} : _jsonish;
}

void JsonSanitizer::sanitizeString(size_t start, size_t end)
{
    auto closed = false;
    auto i      = start;
    while (i < end) {
        auto ch = utf8::char_at(_jsonish, i);
        if (ch == "\xe2\x80\xa8") {
            replace(i, i + ch.length(), "\\u2028");
        } else if (ch == "\xe2\x80\xa9") {
            replace(i, i + ch.length(), "\\u2029");
        } else if (ch.length() == 1) {
            auto &chf = ch.front();
            if (chf < '\x20') {
                if ((chf == '\x09') || (chf == '\x0a') || (chf == '\x0d')) {
                    ++i;
                    continue;
                } else {
                    replace(i, i + 1, "\\u");
                    auto uch = static_cast<uint32_t>(chf);
                    for (auto j = 4; --j >= 0;) {
                        _sanitizedJson.push_back(HEX_DIGITS[uch >> (j << 2) & 0x0f]);
                    }
                    ++i;
                    continue;
                }
            }
            switch (chf) {
                case '\n':
                    replace(i, i + 1, "\\n");
                    break;
                case '\r':
                    replace(i, i + 1, "\\r");
                    break;
                case '"':
                case '\'':
                    if (i == start) {
                        if (ch == "\'") {
                            replace(i, i + 1, "\"");
                        }
                    } else {
                        if ((i + ch.length()) == end) {
                            auto startDelim = utf8::char_at(_jsonish, start);
                            if (startDelim != "\'") {
                                startDelim = "\"";
                            }
                            closed = startDelim == ch;
                        }
                        if (closed) {
                            if (ch == "\'") {
                                replace(i, i + 1, "\"");
                            } else if (ch == "\"") {
                                insert(i, "\\");
                            }
                        }
                    }
                    break;
                case '<':
                    if ((i + 3) >= end) {
                        break;
                    }
                    {
                        auto const ofst = i + 1;
                        auto       c1   = utf8::char_at(_jsonish, ofst);
                        auto       c2   = utf8::char_at(_jsonish, ofst + c1.length());
                        auto       c3   = utf8::char_at(_jsonish, ofst + c1.length() + c2.length());
                        if ((c1.length() == 1) && (c2.length() == 1) && (c3.length() == 1)) {
                            auto lc1 = static_cast<char>(c1.front() | 32);
                            auto lc2 = static_cast<char>(c2.front() | 32);
                            auto lc3 = static_cast<char>(c3.front() | 32);
                            if ((c1 == "!" && c2 == "-" && c3 == "-") ||
                                (lc1 == 's' && lc2 == 'c' && lc3 == 'r') ||
                                (c1 == "/" && lc2 == 's' && lc3 == 'c')) {
                                replace(i, ofst, "\\u003c");
                            }
                        }
                    }
                    break;
                case '>':
                    if (((i - 2) >= start) && (utf8::char_at(_jsonish, i - 2) == "-") &&
                        (utf8::char_at(_jsonish, i - 1) == "-")) {
                        replace(i, i + 1, "\\u005d");
                    }
                    break;
                case ']':
                    if ((i + 2 < end) && (utf8::char_at(_jsonish, i + 1) == "]") &&
                        (utf8::char_at(_jsonish, i + 2) == ">")) {
                        replace(i, i + 1, "\\u005d");
                    }
                    break;
                case '\\':
                    if (i + 1 == end) {
                        elide(i, i + 1);
                        break;
                    }
                    {
                        auto sch = utf8::char_at(_jsonish, i + 1);
                        if (sch.length() == 1) {
                            switch (sch.front()) {
                                case 'b':
                                case 'f':
                                case 'n':
                                case 'r':
                                case 't':
                                case '\\':
                                case '/':
                                case '"':
                                    ++i;
                                    break;
                                case 'x':
                                    if (((i + 4) < end) && isHexAt(i + 2) && isHexAt(i + 3)) {
                                        replace(i, i + 2, "\\u00"); // \xab -> \u00ab
                                        i += 3;
                                        break;
                                    }
                                    elide(i, i + 1);
                                    break;
                                case 'u':
                                    if (((i + 6) < end) && isHexAt(i + 2) && isHexAt(i + 3) &&
                                        isHexAt(i + 4) && isHexAt(i + 5)) {
                                        i += 5;
                                        break;
                                    }
                                    elide(i, i + 1);
                                    break;
                                case '0':
                                case '1':
                                case '2':
                                case '3':
                                case '4':
                                case '5':
                                case '6':
                                case '7': {
                                    auto octalEnd = i + 1;
                                    if (((octalEnd + 1) < end) && isOctAt(octalEnd + 1)) {
                                        ++octalEnd;
                                        if (((ch.front() <= '3')) && ((octalEnd + 1) < end) &&
                                            isOctAt(octalEnd + 1)) {
                                            ++octalEnd;
                                        }
                                        int value = 0;
                                        for (auto j = i; j < octalEnd; ++j) {
                                            auto  jca  = utf8::char_at(_jsonish, j);
                                            auto &jcaf = jca.front();
                                            value      = (value << 3) | (jcaf - '0');
                                        }
                                        replace(i + 1, octalEnd, "u00");
                                        appendHex(value, 2);
                                    }
                                    i = octalEnd - 1;
                                } break;
                                default:
                                    elide(i, i + 1);
                                    break;
                            }
                        }
                    }
                    break;
                default:
                    break;
            }
        } else {
            auto const u32ch = utf8::to_utf32(ch);
            if ((u32ch >= 0xD800) && (u32ch < 0xE000)) {
                // This must be a lone surrogate - otherwise it would have been
                // combined with another to make a valid UTF8 character
                replace(i, i + ch.length(), "\\u");
                auto u16ch = static_cast<uint16_t>(u32ch); // Safe
                for (int j = 4; --j >= 0;) {
                    _sanitizedJson.push_back(HEX_DIGITS[(u16ch >> (j << 2)) & 0xf]);
                }
            } else if (u32ch >= 0xFFFE) {
                // These have to be split into surrogate pairs
                uint32_t u32chprime = u32ch - 0x10000;
                uint16_t w1 = 0xD800u + static_cast<uint16_t>((u32chprime & 0x000FFC00) >> 10);
                uint16_t w2 = 0xDC00u + static_cast<uint16_t>(u32chprime & 0x000003FF);
                replace(i, i + ch.length(), "\\u");
                for (int j = 4; --j >= 0;) {
                    _sanitizedJson.push_back(HEX_DIGITS[(w1 >> (j << 2)) & 0xf]);
                }
                _sanitizedJson.append("\\u");
                for (int j = 4; --j >= 0;) {
                    _sanitizedJson.push_back(HEX_DIGITS[(w2 >> (j << 2)) & 0xf]);
                }
            }
        }
        i += ch.length();
    }
    if (!closed) {
        insert(end, "\"");
    }
}

JsonSanitizer::State JsonSanitizer::requireValueState(size_t pos, State state, bool canBeKey)
{
    switch (state) {
        case State::START_MAP:
        case State::BEFORE_KEY:
            if (canBeKey) {
                return State::AFTER_KEY;
            } else {
                insert(pos, "\"\":");
                return State::AFTER_KEY;
            }
            break;

        case State::AFTER_KEY:
            insert(pos, ":");
            return State::AFTER_VALUE;
            break;

        case State::BEFORE_VALUE:
            return State::AFTER_VALUE;
            break;

        case State::AFTER_VALUE:
            if (canBeKey) {
                insert(pos, ",");
                return State::AFTER_KEY;
            } else {
                insert(pos, ",\"\":");
                return State::AFTER_VALUE;
            }
            break;

        case State::START_ARRAY:
        case State::BEFORE_ELEMENT:
            return State::AFTER_ELEMENT;
            break;

        case State::AFTER_ELEMENT:
            if (_bracketDepth == 0) {
                throw UnbracketedComma{};
            }
            insert(pos, ",");
            return State::AFTER_ELEMENT;

        default:
            break;
    }
    throw AssertionError{};
}

void JsonSanitizer::insert(size_t pos, std::string_view s)
{
    replace(pos, pos, s);
}

void JsonSanitizer::insert(size_t pos, char s)
{
    replace(pos, pos, s);
}

void JsonSanitizer::elide(size_t start, size_t end)
{
    if (_sanitizedJson.empty()) {
        _sanitizedJson.reserve(_jsonish.length() + 32);
    }
    _sanitizedJson.append(_jsonish.substr(_cleaned, start - _cleaned));
    _cleaned = end;
}

void JsonSanitizer::replace(size_t start, size_t end, std::string_view s)
{
    elide(start, end);
    _sanitizedJson.append(s);
}

void JsonSanitizer::replace(size_t start, size_t end, char s)
{
    elide(start, end);
    _sanitizedJson.push_back(s);
}

size_t JsonSanitizer::endOfQuotedString(std::string_view s, size_t start) const
{
    auto quote = utf8::char_at(s, start);
    auto i     = s.find(quote, start + quote.length());
    while (i != std::string_view::npos) {
        auto slashRunStart = i;
        while ((slashRunStart > start) && (utf8::char_at(s, slashRunStart) == "\\")) {
            --slashRunStart;
        }
        if (((i - slashRunStart) & 1) == 0) {
            return i + quote.length();
        }
        i = s.find(quote, i + quote.length());
    }
    return s.length();
}

void JsonSanitizer::elideTrailingComma(size_t closeBracketPos)
{
    // The content before closeBracketPos is stored in two places.
    // 1. sanitizedJson
    // 2. jsonish.substring(cleaned, closeBracketPos)
    // We walk over whitespace characters in both right-to-left looking for a
    // comma.
    for (auto i = closeBracketPos; i >= _cleaned;
         i -= utf8::backup_one_character_octect_count(reinterpret_cast<unsigned char const *>(
                                                          &_jsonish[i]),
                                                      i - _cleaned)) {
        auto ch = utf8::char_at(_jsonish, i);
        if (ch.length() == 1) {
            auto &chf = ch.front();
            switch (chf) {
                case '\t':
                case '\n':
                case '\r':
                case ' ':
                    continue;
                case ',':
                    elide(i, i + 1);
                    return;
                default:
                    throw AssertionError{std::string{utf8::char_at(_jsonish, i)}};
            }
        }
    }
    for (auto i = _sanitizedJson.length(); i >= 0;
         i -= utf8::backup_one_character_octect_count(reinterpret_cast<unsigned char const *>(
                                                          &_sanitizedJson[i]),
                                                      _sanitizedJson.length() - i)) {
        auto ch = utf8::char_at(_sanitizedJson, i);
        if (ch.length() == 1) {
            auto &chf = ch.front();
            switch (chf) {
                case '\t':
                case '\n':
                case '\r':
                case ' ':
                    continue;
                case ',':
                    _sanitizedJson.resize(i);
                    return;
                default:
                    throw AssertionError{std::string{utf8::char_at(_sanitizedJson, i)}};
            }
        }
    }
    throw AssertionError{"Trailing comma not found in " + std::string{_jsonish} + " or " +
                         _sanitizedJson};
}

void JsonSanitizer::normalizeNumber(size_t start, size_t end)
{
    auto pos = start;
    // Sign
    if (pos < end) {
        auto ch = utf8::char_at(_jsonish, pos);
        if (ch.length() == 1) {
            auto &chf = ch.front();
            switch (chf) {
                case '+':
                    elide(pos, pos + 1);
                    ++pos;
                    break;
                case '-':
                    ++pos;
                    break;
                default:
                    break;
            }
        }
    }

    // Integer part
    auto intEnd = endOfDigitRun(pos, end);
    if (auto ch = utf8::char_at(_jsonish, pos);
        pos == intEnd) { // No empty integer parts allowed in JSON.
        insert(pos, '0');
    } else if ((ch.length() == 1) && ('0' == ch.front())) {
        auto reencoded = false;
        long value     = 0;
        if (auto tch = utf8::char_at(_jsonish, intEnd);
            ((intEnd - pos) == 1) && (intEnd < end) && (tch.length() == 1) &&
            ('x' == (tch.front() | 32))) { // Recode hex.
            for (auto tintEnd = intEnd + 1; tintEnd < end;
                 tintEnd += utf8::get_octet_count(_jsonish[tintEnd])) {
                auto nch = utf8::char_at(_jsonish, tintEnd);
                if (nch.length() == 1) {
                    auto nchf   = nch.front();
                    auto digVal = 0;
                    if (('0' <= nchf) && (nchf <= '9')) {
                        digVal = nchf - '0';
                    } else {
                        nchf |= 32;
                        if (('a' <= nchf) && (nchf <= 'f')) {
                            digVal = nchf - ('a' - 10);
                        } else {
                            break;
                        }
                    }
                    value = (value << 4) | digVal;
                }
            }
            reencoded = true;
        } else if (intEnd - pos > 1) { // Recode octal.
            for (auto i = pos; i < intEnd; i += utf8::get_octet_count(_jsonish[i])) {
                int digVal = utf8::char_at(_jsonish, i).front() - '0';
                if (digVal < 0) {
                    break;
                }
                value = (value << 3) | digVal;
            }
            reencoded = true;
        }
        if (reencoded) {
            elide(pos, intEnd);
            if (value < 0) {
                // Underflow.
                // Avoid multiple signs.
                // Putting out the underflowed value is the least bad option.
                //
                // We could use BigInteger, but that won't help many clients,
                // and there is a valid use case for underflow: hex-encoded uint64s.
                //
                // First, consume any sign so that we don't put out strings like
                // --1
                auto lastIndex = _sanitizedJson.length() - 1;
                if (lastIndex >= 0) {
                    auto &last = _sanitizedJson[lastIndex];
                    if (last == '-' || last == '+') {
                        elide(lastIndex, lastIndex + 1);
                        if (last == '-') {
                            value = -value;
                        }
                    }
                }
            }
            _sanitizedJson.append(std::to_string(value));
        }
    }
    pos = intEnd;

    // Optional fraction.
    if (pos < end) {
        if (auto ch = utf8::char_at(_jsonish, pos); (ch.length() == 1) && (ch.front() == '.')) {
            ++pos;
            auto fractionEnd = endOfDigitRun(pos, end);
            if (fractionEnd == pos) {
                insert(pos, '0');
            }
            // JS eval will discard digits after 24(?) but will not treat them as a
            // syntax error, and JSON allows arbitrary length fractions.
            pos = fractionEnd;
        }
    }

    // Optional exponent.
    if (pos < end) {
        if (auto ch = utf8::char_at(_jsonish, pos);
            (ch.length() == 1) && 'e' == (ch.front() | 32)) {
            ++pos;
            if (pos < end) {
                auto nch = utf8::char_at(_jsonish, pos);
                if (nch.length() == 1) {
                    switch (nch.front()) {
                        // JSON allows explicit + in exponent but not for number as a whole.
                        case '+':
                        case '-':
                            ++pos;
                            break;
                        default:
                            break;
                    }
                }
            }
            // JSON allows leading zeros on exponent part.
            auto expEnd = endOfDigitRun(pos, end);
            if (expEnd == pos) {
                insert(pos, '0');
            }
            pos = expEnd;
        }
    }
    if (pos != end) {
        elide(pos, end);
    }
}

bool JsonSanitizer::canonicalizeNumber(size_t start, size_t end)
{
    elide(start, start);
    auto sanStart = _sanitizedJson.length();

    normalizeNumber(start, end);

    // Ensure that the number is on the output buffer.  Since this method is
    // only called when we are quoting a number that appears where a property
    // name is expected, we can force the sanitized form to contain it without
    // affecting the fast-track for already valid inputs.
    elide(end, end);
    auto sanEnd = _sanitizedJson.length();

    return canonicalizeNumber(_sanitizedJson, sanStart, sanEnd);
}

bool JsonSanitizer::canonicalizeNumber(std::string &sanitizedJson, size_t sanStart, size_t sanEnd)
{
    // Now we perform several steps.
    // 1. Convert from scientific notation to regular or vice-versa based on
    //    normalized exponent.
    // 2. Remove trailing zeroes from the fraction and truncate it to 24 digits.
    // 3. Elide the fraction entirely if it is ".0".
    // 4. Convert any 'E' that separates the exponent to lower-case.
    // 5. Elide any minus sign on a zero value.
    // to convert the number to its canonical JS string form.

    // Figure out where the parts of the number start and end.
    size_t intEnd, fractionStart, fractionEnd, expStart, expEnd;
    auto   ch       = utf8::char_at(sanitizedJson, sanStart);
    size_t offset   = ((ch.size() == 1) && (ch.front() == '-')) ? 1 : 0;
    auto   intStart = sanStart + offset;
    for (intEnd = intStart; intEnd < sanEnd;
         intEnd += utf8::get_octet_count(sanitizedJson[intEnd])) {
        ch = utf8::char_at(sanitizedJson, intEnd);
        if (ch.size() == 1) {
            auto &chf = ch.front();
            if (!('0' <= chf && chf <= '9')) {
                break;
            }
        } else {
            break;
        }
    }
    if (intEnd != sanEnd) {
        ch = utf8::char_at(sanitizedJson, intEnd);
    } else {
        ch = {};
    }
    if ((intEnd == sanEnd) || ((ch.size() == 1) && ('.' != ch.front()))) {
        fractionStart = fractionEnd = intEnd;
    } else {
        fractionStart = intEnd + 1;
        for (fractionEnd = fractionStart; fractionEnd < sanEnd;
             fractionEnd += utf8::get_octet_count(sanitizedJson[fractionEnd])) {
            ch = utf8::char_at(sanitizedJson, fractionEnd);
            if (ch.size() == 1) {
                auto &chf = ch.front();
                if (!('0' <= chf && chf <= '9')) {
                    break;
                }
            } else {
                break;
            }
        }
    }
    if (fractionEnd == sanEnd) {
        expStart = expEnd = sanEnd;
    } else {
        ch = utf8::char_at(sanitizedJson, fractionEnd);
        assert((ch.size() == 1) && ('e' == (ch.front() | 32)));
        expStart = fractionEnd + 1;
        ch       = utf8::char_at(sanitizedJson, expStart);
        if ((ch.size() == 1) && (ch.front() == '+')) {
            ++expStart;
        }
        expEnd = sanEnd;
    }

    assert((intStart <= intEnd) && (intEnd <= fractionStart) && (fractionStart <= fractionEnd) &&
           (fractionEnd <= expStart) && (expStart <= expEnd));

    auto exp = 0;
    if (expEnd != expStart) {
        if (auto [p, ec] =
                std::from_chars(&sanitizedJson[expStart], &sanitizedJson[expEnd], exp, 10);
            ec != std::errc{}) {
            return false;
        }
    }
    // Numbered Comments below come from the EcmaScript 5 language specification
    // section 9.8.1 : ToString Applied to the Number Type
    // http://es5.github.com/#x9.8.1

    // 5. let n, k, and s be integers such that k >= 1, 10k-1 <= s < 10k, the
    // Number value for s * 10n-k is m, and k is as small as possible.
    // Note that k is the number of digits in the decimal representation of s,
    // that s is not divisible by 10, and that the least significant digit of s
    // is not necessarily uniquely determined by these criteria.
    auto n = exp; // Exponent

    // s, the string of decimal digits in the representation of m are stored in
    // sanitizedJson.substring(intStart).
    // k, the number of digits in s is computed later.

    // Leave only the number representation on the output buffer after intStart.
    // This leaves any sign on the digit per
    // 3. If m is less than zero, return the String concatenation of the
    //    String "-" and ToString(-m).
    auto sawDecimal     = false;
    auto zero           = true;
    auto digitOutPos    = intStart;
    auto nZeroesPending = 0;
    for (auto i = intStart; i < fractionEnd; i += utf8::get_octet_count(sanitizedJson[i])) {
        ch = utf8::char_at(sanitizedJson, i);
        if (ch.size() == 1) {
            auto &chf = ch.front();
            if (chf == '.') {
                sawDecimal = true;
                if (zero) {
                    nZeroesPending = 0;
                }
                continue;
            }
        } else {
            continue;
        }

        auto digit = ch.front();
        if ((!zero || digit != '0') && !sawDecimal) {
            ++n;
        }

        if (digit == '0') {
            // Keep track of runs of zeros so that we can take them into account
            // if we later see a non-zero digit.
            ++nZeroesPending;
        } else {
            if (zero) { // First non-zero digit.
                // Discard runs of zeroes at the front of the integer part, but
                // any after the decimal point factor into the exponent, n.
                if (sawDecimal) {
                    n -= nZeroesPending;
                }
                nZeroesPending = 0;
            }
            zero = false;
            while (nZeroesPending != 0 || digit != 0) {
                char vdigit;
                if (nZeroesPending == 0) {
                    vdigit = digit;
                    digit  = (char)0;
                } else {
                    vdigit = '0';
                    --nZeroesPending;
                }

                // TODO: limit s to 21 digits?
                sanitizedJson[digitOutPos++] = vdigit;
            }
        }
    }
    sanitizedJson.resize(digitOutPos);
    // Number of digits in decimal representation of s.
    auto const k = static_cast<int>(digitOutPos - intStart);

    // Now we have computed n, k, and s as defined above.  Time to add decimal
    // points, exponents, and leading zeroes per the rest of the JS number
    // formatting specification.

    if (zero) { // There are no non-zero decimal digits.
        // 2. If m is +0 or -0, return the String "0".
        sanitizedJson.resize(sanStart); // Elide any sign.
        sanitizedJson.push_back('0');
        return true;
    }

    // 6. If k <= n <= 21, return the String consisting of the k digits of the
    // decimal representation of s (in order, with no leading zeroes),
    // followed by n-k occurrences of the character '0'.
    if ((k <= n) && (n <= 21)) {
        for (auto i = k; i < n; ++i) {
            sanitizedJson.push_back('0');
        }

        // 7. If 0 < n <= 21, return the String consisting of the most significant n
        // digits of the decimal representation of s, followed by a decimal point
        // '.', followed by the remaining k-n digits of the decimal representation
        // of s.
    } else if ((0 < n) && (n <= 21)) {
        sanitizedJson.insert(std::next(std::cbegin(sanitizedJson), intStart + n), '.');

        // 8. If -6 < n <= 0, return the String consisting of the character '0',
        // followed by a decimal point '.', followed by -n occurrences of the
        // character '0', followed by the k digits of the decimal representation of
        // s.
    } else if (-6 < n && n <= 0) {
        auto tmp = std::string_view{"0.000000"}.substr(0, 2 - n);
        sanitizedJson.insert(intStart, tmp.data(), tmp.size());
    } else {

        // 9. Otherwise, if k = 1, return the String consisting of the single
        // digit of s, followed by lowercase character 'e', followed by a plus
        // sign '+' or minus sign '-' according to whether n-1 is positive or
        // negative, followed by the decimal representation of the integer
        // abs(n-1) (with no leading zeros).
        if (k == 1) {
            // Sole digit already on sanitizedJson.

            // 10. Return the String consisting of the most significant digit of the
            // decimal representation of s, followed by a decimal point '.', followed
            // by the remaining k-1 digits of the decimal representation of s,
            // followed by the lowercase character 'e', followed by a plus sign '+'
            // or minus sign '-' according to whether n-1 is positive or negative,
            // followed by the decimal representation of the integer abs(n-1) (with
            // no leading zeros).
        } else {
            sanitizedJson.insert(std::next(std::cbegin(sanitizedJson), intStart + 1), '.');
        }
        auto const nLess1 = n - 1;
        sanitizedJson.push_back('e');
        sanitizedJson.push_back((nLess1 < 0) ? '-' : '+');
        sanitizedJson.append(std::to_string(std::abs(nLess1)));
    }
    return true;
}

bool JsonSanitizer::isKeyword(size_t start, size_t end) const
{
    auto n = end - start;
    if (n == 5) {
        auto pos = _jsonish.find("false", start, n);
        return pos != std::string_view::npos;
    } else if (n == 4) {
        auto pos = _jsonish.find("true", start, n);
        if (pos != std::string_view::npos) {
            return true;
        }
        pos = _jsonish.find("null", start, n);
        return pos != std::string_view::npos;
    }
    return false;
}

bool JsonSanitizer::isOctAt(size_t i) const
{
    auto ch = utf8::char_at(_jsonish, i);
    if (ch.length() == 1) {
        return ('0' <= ch.front()) && (ch.front() <= '7');
    }
    return false;
}

bool JsonSanitizer::isHexAt(size_t i) const
{
    auto ch = utf8::char_at(_jsonish, i);
    if (ch.length() == 1) {
        if (('0' <= ch.front()) && (ch.front() <= '9')) {
            return true;
        }
        auto c = static_cast<char>(ch.front() | 32);
        return ('a' <= c) && (c <= 'f');
    }
    return false;
}

bool JsonSanitizer::isJsonSpecialChar(size_t i) const
{
    auto ch = utf8::char_at(_jsonish, i);
    if (ch.length() == 1) {
        auto &chf = ch.front();
        if (chf <= ' ') {
            return true;
        }
        switch (chf) {
            case '"':
            case ',':
            case ':':
            case '[':
            case ']':
            case '{':
            case '}':
                return true;
            default:
                return false;
        }
    }
    return false;
}

// clang-format off
void JsonSanitizer::appendHex(int n, int nDigits)
{
    for (unsigned int i = 0, x = static_cast<unsigned int>(n); i<static_cast<unsigned int>(nDigits); ++i, x >> 4) {
        auto dig = static_cast<char>(x & 0xf);
        _sanitizedJson.push_back(dig +
                                 (dig < static_cast<char>(10) ? '0' : static_cast<char>('a' - 10)));
    }
}
// clang-format on

size_t JsonSanitizer::endOfDigitRun(size_t start, size_t limit) const
{
    for (auto end = start; end < limit; end += utf8::get_octet_count(_jsonish[end])) {
        auto ch = utf8::char_at(_jsonish, end);
        if (ch.length() == 1) {
            auto &chf = ch.front();
            if (!(('0' <= chf) && (chf <= '9'))) {
                return end;
            }
        } else {
            return end;
        }
    }
    return limit;
}

} // namespace com::google::json