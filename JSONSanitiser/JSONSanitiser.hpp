﻿// Copyright (C) 2012 Google Inc.
// Copyright (C) 2020 D. Bailey
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
//
// This is a port of the Java version of JSONSanitizer to C++ using UTF-8
// as the character set. As far as possible the original source code has
// been directly reused.

#include "jsonsanitiser_export.h"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace com::google::json {

class AssertionError final : public std::runtime_error
{
public:
    AssertionError(std::string message = {}) noexcept
        : std::runtime_error{message}
    {}
};

class NumberFormatException final : public std::runtime_error
{
public:
    NumberFormatException(std::string message = {}) noexcept
        : std::runtime_error{message}
    {}
};

class UnbracketedComma final : public std::runtime_error
{
public:
    UnbracketedComma() noexcept
        : std::runtime_error{"Unbracketed Comma"}
    {}
};

class JSONSANITISER_EXPORT JsonSanitizer final
{
    std::string_view  _jsonish;
    int               _maximumNestingDepth           = MAXIMUM_NESTING_DEPTH;
    bool              SUPER_VERBOSE_AND_SLOW_LOGGING = false;
    std::string       _sanitizedJson;
    size_t            _bracketDepth = 0;
    size_t            _cleaned      = 0;
    std::vector<bool> _isMap;

public:
    static inline constexpr int DEFAULT_NESTING_DEPTH = 64;
    static inline constexpr int MAXIMUM_NESTING_DEPTH = 4096;

    JsonSanitizer(std::string_view jsonish) noexcept
        : _jsonish{jsonish}
    {}

    JsonSanitizer(std::string_view jsonish, int maximumNestingDepth, bool log) noexcept
        : _jsonish{jsonish}
        , _maximumNestingDepth{std::min(std::max(1, maximumNestingDepth), MAXIMUM_NESTING_DEPTH)}
        , SUPER_VERBOSE_AND_SLOW_LOGGING{log}
    {}

    JsonSanitizer(std::string_view jsonish, int maximumNestingDepth) noexcept
        : JsonSanitizer{jsonish, maximumNestingDepth, false}
    {}

    int getMaximumNestingDepth() const noexcept
    {
        return _maximumNestingDepth;
    }

    static std::variant<std::string_view, std::string> sanitize(std::string_view jsonish,
                                                                bool             log = false)
    {
        return sanitize(jsonish, DEFAULT_NESTING_DEPTH, log);
    }

    static std::variant<std::string_view, std::string> sanitize(std::string_view jsonish,
                                                                int  maximumNestingDepth,
                                                                bool log = false)
    {
        JsonSanitizer s{jsonish, maximumNestingDepth, log};
        s.sanitize();
        return s.toString();
    }

    void                                        sanitize();
    std::variant<std::string_view, std::string> toString() const noexcept;

private:
    enum class State
    {
        /**
         * Immediately after '[' and
         * {@link #BEFORE_ELEMENT before the first element}.
         */
        START_ARRAY,
        /** Before a JSON value in an array or at the top level. */
        BEFORE_ELEMENT,
        /**
         * After a JSON value in an array or at the top level, and before any
         * following comma or close bracket.
         */
        AFTER_ELEMENT,
        /** Immediately after '{' and {@link #BEFORE_KEY before the first key}. */
        START_MAP,
        /** Before a key in a key-value map. */
        BEFORE_KEY,
        /** After a key in a key-value map but before the required colon. */
        AFTER_KEY,
        /** Before a value in a key-value map. */
        BEFORE_VALUE,
        /**
         * After a value in a key-value map but before any following comma or
         * close bracket.
         */
        AFTER_VALUE
    };

    std::string toString(State const &s) const noexcept
    {
        switch (s) {
            case State::START_ARRAY:
                return "START_ARRAY";
                break;
            case State::BEFORE_ELEMENT:
                return "BEFORE_ELEMENT";
                break;
            case State::AFTER_ELEMENT:
                return "AFTER_ELEMENT";
                break;
            case State::START_MAP:
                return "START_MAP";
                break;
            case State::BEFORE_KEY:
                return "BEFORE_KEY";
                break;
            case State::AFTER_KEY:
                return "AFTER_KEY";
                break;
            case State::BEFORE_VALUE:
                return "BEFORE_VALUE";
                break;
            case State::AFTER_VALUE:
                return "AFTER_VALUE";
                break;
            default:
                return "UNKNOWN";
                break;
        }
    }

    void   sanitizeString(size_t start, size_t end);
    State  requireValueState(size_t pos, State state, bool canBeKey);
    void   insert(size_t pos, std::string_view s);
    void   insert(size_t pos, char s);
    void   elide(size_t start, size_t end);
    void   replace(size_t start, size_t end, std::string_view s);
    void   replace(size_t start, size_t end, char s);
    size_t endOfQuotedString(std::string_view s, size_t start) const;
    void   elideTrailingComma(size_t closeBracketPos);
    void   normalizeNumber(size_t start, size_t end);
    bool   canonicalizeNumber(size_t start, size_t end);
    bool   canonicalizeNumber(std::string &sanitizedJson, size_t sanStart, size_t sanEnd);
    bool   isKeyword(size_t start, size_t end) const;
    bool   isOctAt(size_t i) const;
    bool   isHexAt(size_t i) const;
    bool   isJsonSpecialChar(size_t i) const;
    void   appendHex(int n, int nDigits);
    size_t endOfDigitRun(size_t start, size_t limit) const;
    bool   isMaybeNumeric(size_t start, size_t end) const;
};
} // namespace com::google::json