#include <cstdio>
#include <fstream>
#include <sstream>

#include "cachelab/trace_reader.hpp"
#include "test_util.hpp"

using namespace cachelab;

static std::vector<std::uint64_t> parse(const std::string& text) {
    std::istringstream in(text);
    return read_trace(in, "test");
}

static void test_valid_input() {
    auto v = parse("0x0000\n0x40\n128\n0XfF\n");
    CHECK_EQ(v.size(), 4);
    CHECK_EQ(v[0], 0);
    CHECK_EQ(v[1], 64);
    CHECK_EQ(v[2], 128);
    CHECK_EQ(v[3], 255);

    CHECK_EQ(parse("").size(), 0);                       // empty trace
    CHECK_EQ(parse("\n\n   \n# only comments\n").size(), 0);
    CHECK_EQ(parse("0x10 # trailing comment\n  32  \r\n").size(), 2);
    CHECK_EQ(parse("0x10").size(), 1);                   // no trailing newline
    CHECK_EQ(parse("0xFFFFFFFFFFFFFFFF")[0], 0xFFFFFFFFFFFFFFFFull);
    CHECK_EQ(parse("18446744073709551615")[0], 0xFFFFFFFFFFFFFFFFull);
}

static void test_invalid_input() {
    CHECK_THROWS(parse("hello\n"));
    CHECK_THROWS(parse("0x\n"));
    CHECK_THROWS(parse("0xZZ\n"));
    CHECK_THROWS(parse("-5\n"));
    CHECK_THROWS(parse("+5\n"));
    CHECK_THROWS(parse("1.5\n"));
    CHECK_THROWS(parse("12abc\n"));
    CHECK_THROWS(parse("0x10 0x20\n"));                    // two tokens on one line
    CHECK_THROWS(parse("0x1FFFFFFFFFFFFFFFF\n"));          // > 64 bits
    CHECK_THROWS(parse("18446744073709551616\n"));         // 2^64
    CHECK_THROWS(parse_address(""));

    // Error messages point at the offending line.
    try {
        parse("0x10\n0x20\nbad\n");
        CHECK(false);
    } catch (const TraceError& e) {
        CHECK(std::string(e.what()).find("test:3:") != std::string::npos);
    }
}

static void test_files() {
    CHECK_THROWS(read_trace_file("/definitely/not/a/real/file.trace"));

    const std::string path = "cachelab_test_tmp.trace";
    { std::ofstream f(path); f << "# t\n0x0\n0x40\n"; }
    auto v = read_trace_file(path);
    CHECK_EQ(v.size(), 2);
    std::remove(path.c_str());
}

int main() {
    test_valid_input();
    test_invalid_input();
    test_files();
    return testutil::finish("test_trace_reader");
}
