#include "announcement.h"
#include <sstream>
#include <arpa/inet.h>

// IPv4 Prefix parsing
IPv4Prefix IPv4Prefix::parse(const std::string& str) {
    size_t slash = str.find('/');
    if (slash == std::string::npos) {
        return IPv4Prefix(); // Invalid
    }

    std::string addr_str = str.substr(0, slash);
    uint8_t prefix_len = static_cast<uint8_t>(std::stoi(str.substr(slash + 1)));

    struct in_addr addr;
    if (inet_pton(AF_INET, addr_str.c_str(), &addr) != 1) {
        return IPv4Prefix(); // Invalid
    }

    return IPv4Prefix(ntohl(addr.s_addr), prefix_len);
}

std::string IPv4Prefix::toString() const {
    struct in_addr addr;
    addr.s_addr = htonl(address);

    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr, buf, INET_ADDRSTRLEN);

    return std::string(buf) + "/" + std::to_string(prefix_len);
}

// IPv6 Prefix parsing
IPv6Prefix IPv6Prefix::parse(const std::string& str) {
    size_t slash = str.find('/');
    if (slash == std::string::npos) {
        return IPv6Prefix(); // Invalid
    }

    std::string addr_str = str.substr(0, slash);
    uint8_t prefix_len = static_cast<uint8_t>(std::stoi(str.substr(slash + 1)));

    struct in6_addr addr;
    if (inet_pton(AF_INET6, addr_str.c_str(), &addr) != 1) {
        return IPv6Prefix(); // Invalid
    }

    // Convert to host byte order (big endian to native)
    uint64_t high = 0, low = 0;
    for (int i = 0; i < 8; i++) {
        high = (high << 8) | addr.s6_addr[i];
    }
    for (int i = 8; i < 16; i++) {
        low = (low << 8) | addr.s6_addr[i];
    }

    return IPv6Prefix(high, low, prefix_len);
}

std::string IPv6Prefix::toString() const {
    struct in6_addr addr;

    // Convert back to network byte order
    for (int i = 0; i < 8; i++) {
        addr.s6_addr[i] = (high >> (56 - i * 8)) & 0xFF;
    }
    for (int i = 0; i < 8; i++) {
        addr.s6_addr[i + 8] = (low >> (56 - i * 8)) & 0xFF;
    }

    char buf[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &addr, buf, INET6_ADDRSTRLEN);

    return std::string(buf) + "/" + std::to_string(prefix_len);
}

// Generic Prefix parsing
Prefix Prefix::parse(const std::string& str) {
    // Simple heuristic: if contains ':', it's IPv6
    if (str.find(':') != std::string::npos) {
        return Prefix(IPv6Prefix::parse(str));
    } else {
        return Prefix(IPv4Prefix::parse(str));
    }
}
