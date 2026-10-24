#pragma once
#include <cstdint>
#include <iosfwd>
#include <stdexcept>
#include <string>
#include <vector>

namespace cachelab {

class TraceError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Parses one address token: "0x1F40" (hex) or "8000" (decimal).
// Throws TraceError on malformed input, negatives, extra tokens or overflow.
std::uint64_t parse_address(const std::string& token);

// One address per line. Blank lines and '#' comments are ignored.
// Errors carry "<source>:<line>: message".
std::vector<std::uint64_t> read_trace(std::istream& in, const std::string& source = "<stream>");
std::vector<std::uint64_t> read_trace_file(const std::string& path);

}  // namespace cachelab
