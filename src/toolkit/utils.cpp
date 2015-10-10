#include "utils.h"

void break_string(size_t n, std::string& s) {
    size_t pos = 0;
    while (true) {
        size_t newline = s.find('\n', pos);
        if (newline == std::string::npos)
            return;
        if (newline - pos > n) {
            s.insert(pos + n, "\n");
            pos++;
        }
        pos += newline;
    }
}

std::size_t hash_comb(std::size_t h, std::size_t v)
{
    h ^= v + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
}

std::map<std::string, std::unique_ptr<std::ostream>> DebugInterface::streams;