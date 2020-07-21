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
#include <JSONSanitiser.hpp>
#include <boost/coroutine2/coroutine.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cuchar>
#include <iostream>
#include <iterator>
#include <optional>
#include <memory>
#include <random>
#include <string>
#include <sstream>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

using namespace com::google::json;

namespace utf8 {

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

size_t character_count(std::string const &s)
{
    size_t i   = 0;
    size_t nch = 0;
    while (i < s.length()) {
        i += get_octet_count(static_cast<unsigned char>(s[i]));
        ++nch;
    }
    return nch;
}

size_t nchars(size_t start, size_t end, std::string const &s)
{
    size_t nch = 0;
    while ((start < s.length()) && (start < end)) {
        start += get_octet_count(static_cast<unsigned char>(s[start]));
        ++nch;
    }
    return nch;
}

// Character count starts from 1
size_t offset_of(size_t charn, std::string const &s)
{
    if (charn == 1u) {
        return 0;
    }
    size_t i = 0, char_start = 0;
    size_t nch = 0;
    while ((nch < charn) && (i < s.length())) {
        char_start = i;
        i += get_octet_count(static_cast<unsigned char>(s[i]));
        ++nch;
    }
    return char_start;
}

} // namespace utf8

namespace {
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
} // namespace

using coro_t          = ::boost::coroutines2::coroutine<std::string>;
using StringGenerator = coro_t::pull_type;

class RandomJSONGenerator
{
    size_t                                         _nIterations;
    unsigned int                                   _seed;
    std::mt19937_64                                _generator;
    std::unique_ptr<StringGenerator>               _stringGenerator;
    inline static const std::array<std::string, 5> FLOAT_FORMAT_STRING = {"%g", "%G", "%e", "%E",
                                                                          "%f"};
    inline static const std::array<char, 3> INT_FORMAT_STRING   = {'x', 'X', 'd'};

public:
    RandomJSONGenerator(size_t nIterations, std::optional<unsigned int> seed = std::nullopt)
        : _nIterations{nIterations}
    {
        if (seed) {
            _seed = seed.value();
        } else {
            std::random_device                          rd;
            std::uniform_int_distribution<unsigned int> d{0,
                                                          std::numeric_limits<unsigned int>::max()};
            _seed = d(rd);
        }
        _generator.seed(_seed);
    }

    void init()
    {
        _stringGenerator = std::make_unique<StringGenerator>(
            [this](coro_t::push_type &sink) mutable {
            std::uniform_int_distribution<> d{0, 15};
            std::string                     basis;
            std::string                     pending;
            for (auto i = _nIterations; i > 0; --i) {
                if (pending.empty()) {
                    if (basis.empty()) {
                        basis   = this->operator()();
                        pending = basis;
                    } else {
                        pending = this->mutate(basis);
                    }
                }
                sink(std::move(pending));
                pending.clear();
                if (d(_generator) == 0) {
                    basis.clear();
                }
            }
        });
    }

    auto begin()
    {
        return ::boost::coroutines2::detail::begin(*_stringGenerator);
    }

    auto end()
    {
        return ::boost::coroutines2::detail::end(*_stringGenerator);
    }

    std::string operator()()
    {
        return makeRandomJson();
    }

    std::string mutate(std::string const &s)
    {
        static std::uniform_int_distribution<unsigned int> d16{1, 16};
        size_t                                             n   = d16(_generator);
        auto const                                         len = utf8::character_count(s);
        // Pick the places where we mutate, so we can sort, de-dupe, and then
        // derive s' in a left-to-right pass.
        std::vector<size_t> locations;
        locations.reserve(n);
        std::uniform_int_distribution<unsigned int> d{1u, static_cast<unsigned int>(len)};
        for (auto i = n; i != 0; --i) {
            auto const charloc = d(_generator);
            locations.push_back(utf8::offset_of(charloc, s));
        }
        std::sort(std::begin(locations), std::end(locations));
        auto last = std::unique(std::begin(locations), std::end(locations));
        locations.erase(last, std::end(locations));
        n = locations.size();

        std::string delta;
        size_t      left = 0;
        for (size_t i = 0; i < n; ++i) {
            auto &loc             = locations[i];
            auto  nextLoc         = ((i + 1) == n) ? s.length() : locations[i + 1];
            auto  size            = utf8::nchars(loc, nextLoc, s);
            auto  rndSliceCharLen = 1;
            auto  rndSliceLen     = nextLoc - loc;
            if (size > 1) {
                std::uniform_int_distribution<unsigned int> d{0,
                                                              static_cast<unsigned int>(size - 1)};
                rndSliceCharLen = d(_generator);
            }

            delta.append(std::next(std::cbegin(s), left), std::next(std::cbegin(s), loc));
            left = loc;

            switch (std::uniform_int_distribution{0, 2}(_generator)) {
                case 0: // insert
                    appendRandomChars(rndSliceCharLen, delta);
                    break;
                case 1: // replace
                {
                    appendRandomChars(rndSliceCharLen, delta);
                    left += rndSliceLen;
                } break;
                case 2: // remove
                default:
                    left += rndSliceLen;
                    break;
            }
        }
        delta.append(std::next(std::cbegin(s), left), std::cend(s));
        return delta;
    }

private:
    std::string makeRandomJson()
    {
        static std::uniform_int_distribution<unsigned int> d8{1, 8};
        static std::uniform_int_distribution<unsigned int> d20{4, 19};
        auto                                               maxDepth   = d8(_generator);
        auto                                               maxBreadth = d20(_generator);
        std::string                                        output;

        appendWhitespace(output);
        appendRandomJson(maxDepth, maxBreadth, output);
        appendWhitespace(output);
        return output;
    }

    void appendWhitespace(std::string &s)
    {
        static std::uniform_int_distribution<unsigned int> d4{0, 3};
        static std::string                                 wsChars{" \t\r\b"};
        if (d4(_generator) == 0) {
            for (auto i = static_cast<int>(d4(_generator)); i >= 0; --i) {
                s.push_back(wsChars[d4(_generator)]);
            }
        }
    }

    void appendRandomJson(unsigned int maxDepth, unsigned int maxBreadth, std::string &s)
    {
        static std::uniform_int_distribution<unsigned int> d8{0, 7};
        static std::uniform_int_distribution<unsigned int> d6{0, 5};
        auto const r = (maxDepth != 0) ? d8(_generator) : d6(_generator);
        switch (r) {
            case 0:
                s.append("null");
                break;
            case 1:
                s.append("true");
                break;
            case 2:
                s.append("false");
                break;
            case 3: {
                static std::uniform_int_distribution<unsigned int>
                                             df{0, static_cast<unsigned int>(FLOAT_FORMAT_STRING.size() - 1)};
                auto const &                 fmt    = FLOAT_FORMAT_STRING[df(_generator)];
                static std::array<char, 256> buf    = {};
                auto const                   nchars = snprintf(buf.data(), buf.size(), fmt.c_str(),
                                             1.0 / std::normal_distribution{}(_generator));
                s.append(buf.data(), buf.data() + nchars);
            } break;
            case 4: {
                static std::uniform_int_distribution<unsigned int> d{0, 2};
                switch (d(_generator)) {
                    case 0:
                        break;
                    case 1:
                        s.push_back('-');
                        break;
                    case 2:
                    default:
                        s.push_back('+');
                        break;
                }
                static std::uniform_int_distribution<unsigned int>
                                             di{0, static_cast<unsigned int>(INT_FORMAT_STRING.size() - 1)};
                auto const &                 fmt = INT_FORMAT_STRING[di(_generator)];
                auto                   digitString = randomDecimalDigits(maxBreadth * 2);
                // Trim any leading 0s as boost mulitprecision ints can't deal with them
                digitString.erase(0, std::min(digitString.find_first_not_of('0'), digitString.size() - 1));
                try {
                    ::boost::multiprecision::cpp_int num{digitString};
                    std::ostringstream               oss;
                    switch (fmt) {
                        case 'x':
                            oss << std::hex;
                            break;
                        case 'X':
                            oss << std::hex << std::uppercase;
                            break;
                        case 'd':
                        default:
                            oss << std::dec;
                    }
                    oss << num;
                    s.append(oss.str());
                } catch (std::runtime_error const &) {
                    std::cerr << digitString << std::endl;
                    throw;
                }
            } break;
            case 5:
                appendRandomString(maxBreadth, s);
                break;
            case 6: {
                s.push_back('[');
                appendWhitespace(s);
                static std::uniform_int_distribution<unsigned int> d{0, maxBreadth};
                for (auto i = d(_generator); i != 0; --i) {
                    appendWhitespace(s);
                    appendRandomJson(maxDepth - 1, std::max(1u, maxBreadth - 1), s);
                    if (i != 1) {
                        appendWhitespace(s);
                        s.push_back(',');
                    }
                }
                appendWhitespace(s);
                s.push_back(']');
            } break;
            case 7:
            default: {
                s.push_back('{');
                appendWhitespace(s);
                static std::uniform_int_distribution<unsigned int> d{0, maxBreadth};
                for (int i = d(_generator); i != 0; --i) {
                    appendWhitespace(s);
                    appendRandomString(maxBreadth, s);
                    appendWhitespace(s);
                    s.push_back(':');
                    appendWhitespace(s);
                    appendRandomJson(maxDepth - 1, std::max(1u, maxBreadth - 1), s);
                    if (i != 1) {
                        appendWhitespace(s);
                        s.push_back(',');
                    }
                }
                appendWhitespace(s);
                s.push_back('}');
            } break;
        }
    }

    std::string randomDecimalDigits(unsigned int maxDigits)
    {
        std::uniform_int_distribution<int> d{1, static_cast<int>(maxDigits)};
        auto                                        nDigits = d(_generator);
        std::string                                 output;
        output.reserve(nDigits);
        static std::uniform_int_distribution<unsigned int> d10{0, 9};
        for (; nDigits >= 0; --nDigits) {
            output.push_back('0' + d10(_generator));
        }
        return output;
    }

    void appendRandomString(unsigned int maxBreadth, std::string &s)
    {
        s.push_back('"');
        std::uniform_int_distribution<unsigned int> d{0, maxBreadth * 4};
        appendRandomChars(d(_generator), s);
        s.push_back('"');
    }

    void appendRandomChars(unsigned int nChars, std::string &s)
    {
        for (; nChars != 0; --nChars) {
            appendRandomChar(s);
        }
    }

    size_t appendRandomChar(std::string &s)
    {
        static std::uniform_int_distribution<unsigned int> d8{0, 7};
        static std::uniform_int_distribution<unsigned int> d7{0, 6};
        auto const delim = (d8(_generator) == 0) ? '\'' : '"';
        uint32_t   cpMax;
        switch (d7(_generator)) {
            case 0:
            case 1:
            case 2:
            case 3:
                cpMax = 0x100;
                break;
            case 4:
            case 5:
                cpMax = 0x10000;
                break;
            default:
                cpMax = 0x10FFFF;
                break;
        }
        std::uniform_int_distribution<uint32_t> d{0, cpMax};
        auto                                    cp     = d(_generator);
        auto                                    encode = false;
        if ((cp == static_cast<uint32_t>(delim)) || (cp < 0x20) ||
            (cp == static_cast<uint32_t>('\\'))) {
            encode = true;
        }
        if (!encode && (d8(_generator) == 0u)) {
            encode = true;
        }
        if (encode) {
            size_t                                 bytesAdded = 0;
            static std::uniform_int_distribution<> boolDist{0, 1};
            auto rndBool = static_cast<bool>(boolDist(_generator));
            if (rndBool) {
                if (cp >= 0x10000) {
                    // These have to be split into surrogate pairs
                    uint32_t u32chprime = cp - 0x10000;
                    uint16_t w1 = 0xD800u + static_cast<uint16_t>((u32chprime & 0x000FFC00) >> 10);
                    uint16_t w2 = 0xDC00u + static_cast<uint16_t>(u32chprime & 0x000003FF);
                    std::ostringstream oss;
                    oss << std::hex << std::setw(4) << std::setfill('0');
                    oss << "\\u" << w1 << "\\u" << w2;
                    s.append(oss.str());
                    bytesAdded = oss.str().size();
                } else {
                    uint16_t           w = static_cast<uint16_t>(cp);
                    std::ostringstream oss;
                    oss << std::hex << std::setw(4) << std::setfill('0');
                    oss << "\\u" << w;
                    s.append(oss.str());
                    bytesAdded = oss.str().size();
                }
            } else {
                s.push_back('\\');
                switch (cp) {
                    case 0x0a:
                        s.push_back('\n');
                        bytesAdded = 1;
                        break;
                    case 0x0d:
                        s.push_back('\r');
                        bytesAdded = 1;
                        break;
                    default:
                        bytesAdded = appendUTF8(cp, s);
                        break;
                }
            }
            return bytesAdded;
        } else {
            return appendUTF8(cp, s);
        }
    }

    size_t appendUTF8(uint32_t cp, std::string &s)
    {
        if (cp <= 0x7F) {
            s.push_back(static_cast<char>(static_cast<unsigned char>(cp)));
            return 1u;
        } else if (cp <= 0x7FF) {
            s.push_back(static_cast<char>(static_cast<unsigned char>(0xC0u) |
                                          static_cast<unsigned char>(cp >> 6))); // 110xxxxx
            s.push_back(static_cast<char>(static_cast<unsigned char>(0x80u) |
                                          static_cast<unsigned char>(cp & 0x3Fu))); // 10xxxxxx
            return 2u;
        } else if (cp <= 0xFFFF) {
            s.push_back(static_cast<char>(static_cast<unsigned char>(0xE0u) |
                                          static_cast<unsigned char>((cp >> 12)))); // 1110xxxx
            s.push_back(
                static_cast<char>(static_cast<unsigned char>(0x80u) |
                                  static_cast<unsigned char>(((cp >> 6) & 0x3Fu)))); // 10xxxxxx
            s.push_back(static_cast<char>(static_cast<unsigned char>(0x80u) |
                                          static_cast<unsigned char>((cp & 0x3Fu)))); // 10xxxxxx
            return 3u;
        } else if (cp <= 0x10FFFF) {
            s.push_back(static_cast<char>(static_cast<unsigned char>(0xF0u) |
                                          static_cast<unsigned char>((cp >> 18)))); // 11110xxx
            s.push_back(
                static_cast<char>(static_cast<unsigned char>(0x80u) |
                                  static_cast<unsigned char>(((cp >> 12) & 0x3Fu)))); // 10xxxxxx
            s.push_back(
                static_cast<char>(static_cast<unsigned char>(0x80u) |
                                  static_cast<unsigned char>(((cp >> 6) & 0x3Fu)))); // 10xxxxxx
            s.push_back(static_cast<char>(static_cast<unsigned char>(0x80u) |
                                          static_cast<unsigned char>((cp & 0x3Fu)))); // 10xxxxxx
            return 4u;
        } else {
            return 0;
        }
    }
};

std::string asHex(std::string const &s)
{
    std::ostringstream oss;
    oss.put('"');
    for (auto c : s) {
        oss << "\\x";
        oss << std::hex << std::setw(2) << std::setfill('0');
        oss << static_cast<uint16_t>(static_cast<uint8_t>(c));
    }
    oss.put('"');
    return oss.str();
}


using coro_t          = ::boost::coroutines2::coroutine<std::string>;
using StringGenerator = coro_t::pull_type;


TEST(TestFuzzer, Fuzz)
{
    auto const          nIterations = 10000;
    RandomJSONGenerator rjg{nIterations};
    rjg.init();

    for (auto s : rjg) {
        std::string sanitised;
        std::string sanitised1;
        try {
            sanitised = asString(JsonSanitizer::sanitize(s));
            std::cout << s << " ==> " << sanitised << "\n";
            ASSERT_NO_THROW(sanitised1 = asString(JsonSanitizer::sanitize(sanitised)));
        } catch (...) {
        }
        ASSERT_EQ(sanitised, sanitised1) << "Failed on " << asHex(s) << " ==> " << asHex(sanitised);
    }
}
