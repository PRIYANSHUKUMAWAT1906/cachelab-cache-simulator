#include "cachelab/trace_reader.hpp"

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <istream>

namespace cachelab {
namespace {

std::string trim(const std::string& s) {
    std::size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

}  // namespace

std::uint64_t parse_address(const std::string& raw) {
    const std::string s = trim(raw);
    if (s.empty()) throw TraceError("empty address");
    for (char c : s)
        if (std::isspace(static_cast<unsigned char>(c)))
            throw TraceError("unexpected extra text after address '" + s.substr(0, s.find(c)) + "'");

    int base = 10;
    std::size_t off = 0;
    if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
        off = 2;
    }
    if (off >= s.size()) throw TraceError("missing digits after '0x'");

    // Only digits are allowed: this rejects signs, '.', and trailing junk.
    for (std::size_t i = off; i < s.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        const bool ok = (base == 16) ? std::isxdigit(c) != 0 : std::isdigit(c) != 0;
        if (!ok) throw TraceError("invalid address '" + s + "'");
    }

    errno = 0;
    const unsigned long long v = std::strtoull(s.c_str() + off, nullptr, base);
    if (errno == ERANGE) throw TraceError("address '" + s + "' does not fit in 64 bits");
    return static_cast<std::uint64_t>(v);
}

std::vector<std::uint64_t> read_trace(std::istream& in, const std::string& source) {
    std::vector<std::uint64_t> out;
    std::string line;
    std::size_t line_no = 0;
    while (std::getline(in, line)) {
        ++line_no;
        const auto hash = line.find('#');
        if (hash != std::string::npos) line.erase(hash);
        line = trim(line);
        if (line.empty()) continue;
        try {
            out.push_back(parse_address(line));
        } catch (const TraceError& e) {
            throw TraceError(source + ":" + std::to_string(line_no) + ": " + e.what());
        }
    }
    return out;
}

std::vector<std::uint64_t> read_trace_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw TraceError("cannot open trace file '" + path + "'");
    return read_trace(f, path);
}

}  // namespace cachelab
