#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "../../include/kastr.hpp"

#include <doctest/doctest.h>

using namespace kstring;

TEST_CASE("KAStr basic ASCII-only operations") {
    SUBCASE("empty string") {
        KAStr s;
        CHECK(s.empty());
        CHECK(s.byte_size() == 0);
        CHECK(s.char_size() == 0);
        CHECK(s == "");
        CHECK(s == "");
    }

    SUBCASE("from const char* constructor") {
        const char* raw = "hello";
        KAStr s(raw);
        CHECK(! s.empty());
        CHECK(s.byte_size() == 5);
        CHECK(s.char_size() == 5);
        CHECK(s == "hello");
        CHECK(s[0] == 'h');
        CHECK(s[4] == 'o');
        CHECK_THROWS_AS(s[5], std::out_of_range);
    }

    SUBCASE("from (char*, size_t) constructor") {
        const char* raw = "worldwide";
        KAStr s(raw, 5); // "world"
        CHECK(s.byte_size() == 5);
        CHECK(s == "world");
    }

    SUBCASE("byte_at access") {
        KAStr s = "abc";
        CHECK(s.byte_at(0) == 'a');
        CHECK(s.byte_at(1) == 'b');
        CHECK(s.byte_at(2) == 'c');
        CHECK_THROWS_AS(s.byte_at(3), std::out_of_range);
    }

    SUBCASE("begin()/end() iterators") {
        KAStr s("xyz");
        std::string collect(s.begin(), s.end());
        CHECK(collect == "xyz");
    }

    SUBCASE("rbegin()/rend() iterators") {
        KAStr s("xyz");
        std::string collect(s.rbegin(), s.rend());
        CHECK(collect == "zyx");
    }
}

TEST_CASE("KAStr: brutal match and slicing tests") {

    SUBCASE("find: basic cases") {
        KAStr s("abracadabra");
        CHECK(s.find("dasdasdasdasdasdwqedqwd") == kstring::knpos);
        CHECK(s.find("abra") == 0);
        CHECK(s.find("cad") == 4);
        CHECK(s.find("xyz") == std::string::npos);
        CHECK(s.find("") == 0); // 空串永远匹配位置 0

        KAStr empty("");
        CHECK(empty.find("anything") == std::string::npos);
        CHECK(s.find("a") == 0);
    }

    SUBCASE("rfind: rightmost match") {
        KAStr s("abracadabra");
        CHECK(s.rfind("dasdasdasdasdasdwqedqwd") == kstring::knpos);
        CHECK(s.rfind("abra") == 7);
        CHECK(s.rfind("a") == 10);
        CHECK(s.rfind("xyz") == std::string::npos);
        CHECK(s.rfind("") == s.byte_size()); // 空串 rfind 返回长度
    }

    SUBCASE("contains") {
        KAStr s("hello world");
        CHECK(s.contains("hello"));
        CHECK(s.contains("world"));
        CHECK_FALSE(s.contains("bye"));
        CHECK(s.contains("")); // 空串也认为包含
    }

    SUBCASE("starts_with") {
        KAStr s("banana");
        CHECK(s.starts_with("ban"));
        CHECK_FALSE(s.starts_with("nan"));
        CHECK(s.starts_with(""));                // 空前缀总是匹配
        CHECK(KAStr("").starts_with(""));        // empty starts_with empty → true
        CHECK_FALSE(KAStr("").starts_with("a")); // empty 不会 start with 非空
    }

    SUBCASE("ends_with") {
        KAStr s("banana");
        CHECK(s.ends_with("ana"));
        CHECK_FALSE(s.ends_with("ban"));
        CHECK(s.ends_with(""));         // 空后缀总是匹配
        CHECK(KAStr("").ends_with("")); // empty ends_with empty → true
        CHECK_FALSE(KAStr("").ends_with("x"));
    }

    SUBCASE("substr slicing torture") {
        KAStr s("abcdefgh");

        CHECK(s.substr(0, 3) == "abc");
        CHECK(s.substr(2, 4) == "cdef");
        CHECK(s.substr(5) == "fgh");
        CHECK(s.subrange(1, 6) == "bcdef");
        CHECK(s.subrange(3) == "defgh");

        // 超范围切片：允许 start==size()，返回空串
        CHECK(s.substr(8) == "");
        CHECK(s.subrange(8) == "");

        // 崩溃边缘：非法切片越界
        CHECK(s.substr(9).empty());
        CHECK(s.substr(100).empty());
        CHECK(s.subrange(4, 20) == "efgh");
        CHECK(s.subrange(6, 3).empty()); // 逆序非法
    }

    SUBCASE("== and !=") {
        KAStr a("test");
        KAStr b("test");
        KAStr c("TEST");
        KAStr d("test1");

        CHECK(a == b);
        CHECK_FALSE(a != b);
        CHECK(a != c);
        CHECK(a != d);
        CHECK(a == "test");
        CHECK_FALSE(a == "nope");
    }

    SUBCASE("operator<< stream output") {
        KAStr s("streaming");
        std::ostringstream oss;
        oss << s;
        CHECK(oss.str() == "streaming");

        KAStr empty("");
        std::ostringstream oss2;
        oss2 << empty;
        CHECK(oss2.str() == "");
    }

    SUBCASE("trick edge cases with special chars") {
        KAStr s("abc\0def", 7); // 带 '\0'
        CHECK(s.byte_size() == 7);
        CHECK(s[3] == '\0');
        CHECK(s == std::string("abc\0def", 7));
        CHECK(std::string("abc\0def", 7) == s);
        CHECK(s.substr(3, 1) == std::string("\0", 1));
        CHECK(s.contains(KAStr("\0", 1)));
    }
}

TEST_CASE("KAStr: brutal split/split_once/split_at testing") {
    KAStr base("a,b,c,d,e");

    SUBCASE("split_at & split_exclusive_at") {
        auto splited = base.split_at(3);
        auto l = splited.first;
        auto r = splited.second;
        CHECK(l == "a,b");
        CHECK(r == ",c,d,e");

        auto splited2 = base.split_exclusive_at(3);
        auto lx = splited2.first;
        auto rx = splited2.second;
        CHECK(lx == "a,b");
        CHECK(rx == "c,d,e");

        CHECK_THROWS(base.split_at(100));
        CHECK_THROWS(base.split_exclusive_at(100));
    }

    SUBCASE("split_count: basic, full split") {
        auto result = base.split_count(",", 10);
        std::vector<std::string> expected = {"a", "b", "c", "d", "e"};
        REQUIRE(result.size() == expected.size());
        for (std::size_t i = 0; i < expected.size(); ++i) CHECK(result[i] == expected[i]);
    }

    SUBCASE("split_count: limited split") {
        auto result = base.split_count(",", 2);
        CHECK(result.size() == 3);
        CHECK(result[0] == "a");
        CHECK(result[1] == "b");
        CHECK(result[2] == "c,d,e");
    }

    SUBCASE("split_count: empty delimiter with limit") {
        KAStr s("abcde");

        auto r0 = s.split_count("", 0);
        CHECK(r0.size() == 1);
        CHECK(r0[0] == "abcde");

        auto r1 = s.split_count("", 1);
        CHECK(r1.size() == 2);
        CHECK(r1[0] == "a");
        CHECK(r1[1] == "bcde");

        auto r4 = s.split_count("", 4);
        CHECK(r4.size() == 5);
        CHECK(r4[0] == "a");
        CHECK(r4[1] == "b");
        CHECK(r4[2] == "c");
        CHECK(r4[3] == "d");
        CHECK(r4[4] == "e");

        auto r10 = s.split_count("", 10);
        CHECK(r10.size() == 5); // 最多 len 个字符
    }

    SUBCASE("rsplit_count: basic, full reverse split") {
        auto result = base.rsplit_count(",", 10);
        std::vector<std::string> expected = {"e", "d", "c", "b", "a"};
        REQUIRE(result.size() == expected.size());
        for (std::size_t i = 0; i < expected.size(); ++i) CHECK(result[i] == expected[i]);
    }

    SUBCASE("rsplit_count: limited split") {
        auto result = base.rsplit_count(",", 2);
        CHECK(result.size() == 3);
        CHECK(result[0] == "e");
        CHECK(result[1] == "d");
        CHECK(result[2] == "a,b,c");
    }

    SUBCASE("rsplit_count: empty delimiter with limit") {
        KAStr s("abcde");

        auto r0 = s.rsplit_count("", 0);
        CHECK(r0.size() == 1);
        CHECK(r0[0] == "abcde");

        auto r2 = s.rsplit_count("", 2);
        CHECK(r2.size() == 3);
        CHECK(r2[0] == "e");
        CHECK(r2[1] == "d");
        CHECK(r2[2] == "abc");

        auto r5 = s.rsplit_count("", 5);
        CHECK(r5.size() == 5);
        CHECK(r5[0] == "e");
        CHECK(r5[1] == "d");
        CHECK(r5[2] == "c");
        CHECK(r5[3] == "b");
        CHECK(r5[4] == "a");
    }


    SUBCASE("split / rsplit = split_count / rsplit_count with no limit") {
        auto a = base.split(",");
        auto b = base.split_count(",", -1);
        REQUIRE(a.size() == b.size());
        for (std::size_t i = 0; i < a.size(); ++i) CHECK(a[i] == b[i]);

        auto ar = base.rsplit(",");
        auto br = base.rsplit_count(",", -1);
        REQUIRE(ar.size() == br.size());
        for (std::size_t i = 0; i < ar.size(); ++i) CHECK(ar[i] == br[i]);
    }

    SUBCASE("split_once") {
        std::pair<KAStr, KAStr> result = base.split_once(",");
        CHECK(result.first == "a");
        CHECK(result.second == "b,c,d,e");

        std::pair<KAStr, KAStr> not_found = base.split_once("z");
        CHECK(not_found.first == base);
        CHECK(not_found.second == "");
    }

    SUBCASE("rsplit_once") {
        std::pair<KAStr, KAStr> result = base.rsplit_once(",");
        CHECK(result.first == "e");
        CHECK(result.second == "a,b,c,d");

        std::pair<KAStr, KAStr> not_found = base.rsplit_once("z");
        CHECK(not_found.first == base);
        CHECK(not_found.second == "");
    }

    SUBCASE("edge: delimiter at boundaries") {
        KAStr s(",a,b,");
        auto result = s.split_count(",", 10);
        std::vector<std::string> expected = {"", "a", "b", ""};
        REQUIRE(result.size() == expected.size());
        for (std::size_t i = 0; i < expected.size(); ++i) CHECK(result[i] == expected[i]);
    }

    SUBCASE("edge: empty input") {
        KAStr empty("");
        auto result = empty.split(",");
        CHECK(result.size() == 1);
        CHECK(result[0] == "");
    }

    SUBCASE("edge: delimiter longer than input") {
        KAStr s("x");
        auto result = s.split("xyz");
        CHECK(result.size() == 1);
        CHECK(result[0] == "x");
    }

    SUBCASE("edge: split by full string") {
        KAStr s("hello");
        auto result = s.split("hello");
        CHECK(result.size() == 2);
        CHECK(result[0] == "");
        CHECK(result[1] == "");
    }

    SUBCASE("edge: repeated delimiter") {
        KAStr s("a--b--c");
        auto result = s.split("--");
        std::vector<std::string> expected = {"a", "b", "c"};
        REQUIRE(result.size() == expected.size());
        for (std::size_t i = 0; i < expected.size(); ++i) CHECK(result[i] == expected[i]);
    }
}

TEST_CASE("KAStr whitespace & line splitting torture") {

    SUBCASE("split_whitespace: basic + tabs + newlines") {
        KAStr s(" \t  abc \n def  \r ghi\t\n  ");
        auto parts = s.split_whitespace();

        REQUIRE(parts.size() == 3);
        CHECK(parts[0] == "abc");
        CHECK(parts[1] == "def");
        CHECK(parts[2] == "ghi");
    }

    SUBCASE("split_whitespace: multiple spaces") {
        KAStr s("a   b    c");
        auto parts = s.split_whitespace();

        REQUIRE(parts.size() == 3);
        CHECK(parts[0] == "a");
        CHECK(parts[1] == "b");
        CHECK(parts[2] == "c");
    }

    SUBCASE("split_whitespace: empty input") {
        KAStr s("");
        auto parts = s.split_whitespace();
        CHECK(parts.empty());
    }

    SUBCASE("split_whitespace: only whitespace") {
        KAStr s(" \t\n\v\f\r ");
        auto parts = s.split_whitespace();
        CHECK(parts.empty());
    }

    SUBCASE("split_whitespace: no split") {
        KAStr s("singleword");
        auto parts = s.split_whitespace();
        REQUIRE(parts.size() == 1);
        CHECK(parts[0] == "singleword");
    }

    SUBCASE("lines: \\n splitting") {
        KAStr s("a\nb\nc");
        auto lines = s.lines();

        REQUIRE(lines.size() == 3);
        CHECK(lines[0] == "a");
        CHECK(lines[1] == "b");
        CHECK(lines[2] == "c");
    }

    SUBCASE("lines: mixed \\n \\r\\n \\r") {
        KAStr s("a\nb\r\nc\rd");
        auto lines = s.lines();

        REQUIRE(lines.size() == 4);
        CHECK(lines[0] == "a");
        CHECK(lines[1] == "b");
        CHECK(lines[2] == "c");
        CHECK(lines[3] == "d");
    }

    SUBCASE("lines: ending with newline") {
        KAStr s("a\nb\n");
        auto lines = s.lines();

        REQUIRE(lines.size() == 2);
        CHECK(lines[0] == "a");
        CHECK(lines[1] == "b");
    }

    SUBCASE("lines: empty and blank lines") {
        KAStr s("\n\nabc\n\ndef");
        auto lines = s.lines();

        REQUIRE(lines.size() == 5);
        CHECK(lines[0] == "");
        CHECK(lines[1] == "");
        CHECK(lines[2] == "abc");
        CHECK(lines[3] == "");
        CHECK(lines[4] == "def");
    }
}

TEST_CASE("KAStr prefix/suffix/trim") {

    SUBCASE("strip_prefix: match & no match") {
        KAStr s("foobar");

        CHECK(s.strip_prefix("foo") == "bar");
        CHECK(s.strip_prefix("bar") == "foobar");   // no match
        CHECK(s.strip_prefix("") == "foobar");      // empty prefix
        CHECK(KAStr("").strip_prefix("foo") == ""); // empty input
    }

    SUBCASE("strip_prefix: full match") {
        KAStr s("abc");
        CHECK(s.strip_prefix("abc") == "");
    }

    SUBCASE("strip_suffix: match & no match") {
        KAStr s("helloworld");

        CHECK(s.strip_suffix("world") == "hello");
        CHECK(s.strip_suffix("hello") == "helloworld"); // no match
        CHECK(s.strip_suffix("") == "helloworld");
        CHECK(KAStr("").strip_suffix("x") == "");
    }

    SUBCASE("strip_suffix: full match") {
        KAStr s("xyz");
        CHECK(s.strip_suffix("xyz") == "");
    }

    SUBCASE("trim_start: crazy spaces") {
        KAStr s(" \t\n\r\v\fHello");
        CHECK(s.trim_start() == "Hello");

        KAStr s2(" \t \n\r");
        CHECK(s2.trim_start() == ""); // all whitespace

        CHECK(KAStr("").trim_start() == "");
    }

    SUBCASE("trim_end: chaotic endings") {
        KAStr s("Goodbye \t\n\r\v\f");
        CHECK(s.trim_end() == "Goodbye");

        KAStr s2(" \t \n\r");
        CHECK(s2.trim_end() == "");

        CHECK(KAStr("").trim_end() == "");
    }

    SUBCASE("trim: full frontal assault") {
        KAStr s(" \n  \tHello World  \v \r\n ");
        CHECK(s.trim() == "Hello World");

        KAStr s2(" \t\r\n");
        CHECK(s2.trim() == "");

        CHECK(KAStr("").trim() == "");
    }

    SUBCASE("trim vs strip_* conflict") {
        KAStr s("  prefixmiddlepostfix  ");
        auto trimmed = s.trim();

        CHECK(trimmed == "prefixmiddlepostfix");

        auto stripped = trimmed.strip_prefix("prefix").strip_suffix("postfix");

        CHECK(stripped == "middle");
    }

    SUBCASE("user uses partial prefix/suffix") {
        KAStr s("banana");
        CHECK(s.strip_prefix("bananaz") == "banana");
        CHECK(s.strip_suffix("anana") == "b"); // only suffix match fails
        CHECK(s.strip_suffix("na") == "bana");
    }

    SUBCASE("user inputs longer than string") {
        KAStr s("abc");
        CHECK(s.strip_prefix("abcdef") == "abc");
        CHECK(s.strip_suffix("abcdef") == "abc");
    }
}
