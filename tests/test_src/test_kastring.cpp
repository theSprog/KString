#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "kastr.hpp"

#include <unordered_map>
#include <doctest/doctest.h>
#include "../../include/kastring.hpp"

using namespace kstring;

TEST_CASE("KAString basic ASCII-only operations") {
    SUBCASE("empty string") {
        KAString s;
        CHECK(s.empty());
        CHECK(s.byte_size() == 0);
        CHECK(s.char_size() == 0);
        CHECK(s == "");
        CHECK(static_cast<std::string>(s) == "");
    }

    SUBCASE("construct from c-string and std::string") {
        KAString s1("hello");
        KAString s2(std::string("world"));

        CHECK(s1.byte_size() == 5);
        CHECK(s2.byte_size() == 5);

        CHECK(s1[0] == 'h');
        CHECK(s2[4] == 'd');

        CHECK(static_cast<std::string>(s1) == "hello");
        CHECK(static_cast<std::string>(s2) == "world");
    }

    SUBCASE("initializer list construction") {
        KAString s = {'A', 'B', 'C'};
        CHECK(s.byte_size() == 3);
        CHECK(s[0] == 'A');
        CHECK(s[2] == 'C');
        CHECK(s.byte_at(1) == 'B');
    }

    SUBCASE("output to ostream") {
        KAString s("print");
        std::ostringstream oss;
        oss << s;
        CHECK(oss.str() == "print");
    }
}

TEST_CASE("KAString mutable interface works as expected") {
    KAString s("hello");

    SUBCASE("mutable operator[]") {
        s[0] = 'H';
        s[4] = 'O';
        CHECK(static_cast<std::string>(s) == "HellO");
    }

    SUBCASE("modify via begin()/end()") {
        for (Byte& b : s) {
            b = static_cast<Byte>(std::toupper(b));
        }
        CHECK(static_cast<std::string>(s) == "HELLO");
    }

    SUBCASE("reverse iteration and mutation") {
        auto it = s.rbegin();
        *it = '!';
        ++it;
        *it = 'O';
        CHECK(static_cast<std::string>(s) == "helO!");
    }

    SUBCASE("out-of-bound access throws") {
        CHECK_THROWS_AS(s[100] = 'x', std::out_of_range);
        CHECK_THROWS_AS(s.byte_at(100), std::out_of_range);
    }
}

TEST_CASE("KAString operator overloads work correctly") {
    KAString a("hello");
    KAString b("world");
    KAString empty;

    SUBCASE("operator== and operator!=") {
        CHECK(a == "hello");
        CHECK("hello" == a);
        CHECK_FALSE(a == "hellO");
        CHECK_FALSE("HELLO" == a);

        CHECK(a != "hellO");
        CHECK("HELLO" != a);

        CHECK(a == std::string("hello"));
        CHECK(std::string("hello") == a);
        CHECK(a != std::string("hell"));
        CHECK(std::string("hell") != a);

        CHECK(empty == "");
        CHECK("" == empty);
        CHECK(empty != "nonempty");
    }

    SUBCASE("operator+ KAString + KAString") {
        KAString c = a + b;
        CHECK(c == "helloworld");
    }

    SUBCASE("operator+ KAString + const char*") {
        KAString c = a + "!";
        CHECK(c == "hello!");
    }

    SUBCASE("operator+ const char* + KAString") {
        KAString c = "Say " + a;
        CHECK(c == "Say hello");
    }

    SUBCASE("operator+ KAString + std::string") {
        std::string suffix = "!";
        KAString c = a + suffix;
        CHECK(c == "hello!");
    }

    SUBCASE("operator+ std::string + KAString") {
        std::string prefix = "Say ";
        KAString c = prefix + a;
        CHECK(c == "Say hello");
    }

    SUBCASE("operator+ KAString + char") {
        KAString c = a + '!';
        CHECK(c == "hello!");
    }

    SUBCASE("operator+ char + KAString") {
        KAString c = '*' + a;
        CHECK(c == "*hello");
    }

    SUBCASE("operator+= with KAString") {
        KAString s = a;
        s += b;
        CHECK(s == "helloworld");
    }

    SUBCASE("operator+= with const char*") {
        KAString s = a;
        s += "!";
        CHECK(s == "hello!");
    }

    SUBCASE("operator+= with std::string") {
        KAString s = a;
        std::string suffix = " world";
        s += suffix;
        CHECK(s == "hello world");
    }

    SUBCASE("operator+= with char") {
        KAString s = a;
        s += '!';
        CHECK(s == "hello!");
    }

    SUBCASE("operator+= with KAStr") {
        KAString s = a;
        s += KAStr("!");
        CHECK(s == "hello!");
    }
}

TEST_CASE("KAString compare, operator< and std::hash") {
    KAString a("apple");
    KAString b("banana");
    KAString a2("apple");
    KAString empty;

    SUBCASE("compare behaves like strcmp") {
        CHECK(a.compare(b) < 0);
        CHECK(b.compare(a) > 0);
        CHECK(a.compare(a2) == 0);
        CHECK(a.compare(empty) > 0);
        CHECK(empty.compare(a) < 0);
        CHECK(empty.compare(empty) == 0);
    }

    SUBCASE("operator< enables sorting") {
        std::set<KAString> sorted = {b, a, a2, empty};
        std::vector<std::string> expected = {"", "apple", "banana"};

        std::vector<std::string> actual;
        for (const auto& s : sorted) {
            actual.push_back(static_cast<std::string>(s));
        }

        CHECK(actual == expected);
    }

    SUBCASE("std::hash supports unordered_map") {
        std::unordered_map<KAString, int> map;
        map[a] = 1;
        map[b] = 2;
        map[empty] = 0;

        CHECK(map[a] == 1);
        CHECK(map[b] == 2);
        CHECK(map[a2] == 1); // same content
        CHECK(map[empty] == 0);

        // Sanity: keys with different content must not collide
        CHECK(map.find(KAString("nonexistent")) == map.end());
    }
}
