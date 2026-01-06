#ifndef ANNOUNCEMENT_H
#define ANNOUNCEMENT_H

#include <cstdint>
#include <vector>
#include <string>
#include <cstring>

using ASN = uint32_t;

// Relationship types for received_from
enum class RelationshipType : uint8_t {
    ORIGIN = 0,      // Initial announcement (highest priority)
    CUSTOMER = 1,    // From customer
    PEER = 2,        // From peer
    PROVIDER = 3     // From provider
};

// Compact IPv4 prefix representation
struct IPv4Prefix {
    uint32_t address;  // Network address in host byte order
    uint8_t prefix_len; // CIDR prefix length (0-32)

    IPv4Prefix() : address(0), prefix_len(0) {}
    IPv4Prefix(uint32_t addr, uint8_t len) : address(addr), prefix_len(len) {}

    // Parse from string like "1.2.0.0/16"
    static IPv4Prefix parse(const std::string& str);

    // Convert to string
    std::string toString() const;

    // Comparison for hash map
    bool operator==(const IPv4Prefix& other) const {
        return address == other.address && prefix_len == other.prefix_len;
    }
};

// Compact IPv6 prefix (128 bits)
struct IPv6Prefix {
    uint64_t high;  // Upper 64 bits
    uint64_t low;   // Lower 64 bits
    uint8_t prefix_len; // CIDR prefix length (0-128)

    IPv6Prefix() : high(0), low(0), prefix_len(0) {}
    IPv6Prefix(uint64_t h, uint64_t l, uint8_t len) : high(h), low(l), prefix_len(len) {}

    // Parse from string
    static IPv6Prefix parse(const std::string& str);

    // Convert to string
    std::string toString() const;

    bool operator==(const IPv6Prefix& other) const {
        return high == other.high && low == other.low && prefix_len == other.prefix_len;
    }
};

// Generic prefix that can be IPv4 or IPv6
struct Prefix {
    bool is_ipv6;
    union {
        IPv4Prefix v4;
        IPv6Prefix v6;
    };

    Prefix() : is_ipv6(false), v4() {}

    explicit Prefix(const IPv4Prefix& prefix) : is_ipv6(false), v4(prefix) {}
    explicit Prefix(const IPv6Prefix& prefix) : is_ipv6(true), v6(prefix) {}

    // Parse from string (auto-detect IPv4/IPv6)
    static Prefix parse(const std::string& str);

    std::string toString() const {
        return is_ipv6 ? v6.toString() : v4.toString();
    }

    bool operator==(const Prefix& other) const {
        if (is_ipv6 != other.is_ipv6) return false;
        return is_ipv6 ? (v6 == other.v6) : (v4 == other.v4);
    }
};

// Hash function for Prefix
namespace std {
    template<>
    struct hash<IPv4Prefix> {
        size_t operator()(const IPv4Prefix& p) const {
            return std::hash<uint32_t>()(p.address) ^ (std::hash<uint8_t>()(p.prefix_len) << 1);
        }
    };

    template<>
    struct hash<IPv6Prefix> {
        size_t operator()(const IPv6Prefix& p) const {
            return std::hash<uint64_t>()(p.high) ^ (std::hash<uint64_t>()(p.low) << 1) ^
                   (std::hash<uint8_t>()(p.prefix_len) << 2);
        }
    };

    template<>
    struct hash<Prefix> {
        size_t operator()(const Prefix& p) const {
            if (p.is_ipv6) {
                return std::hash<IPv6Prefix>()(p.v6) ^ 1;
            } else {
                return std::hash<IPv4Prefix>()(p.v4);
            }
        }
    };
}

// Optimized BGP Announcement structure
// Memory layout optimized for cache efficiency
struct Announcement {
    Prefix prefix;                      // 20 bytes (with padding)
    ASN next_hop_asn;                   // 4 bytes
    RelationshipType received_from;     // 1 byte
    bool rov_invalid;                   // 1 byte - ROV invalid flag
    uint8_t _padding[2];                // Alignment padding

    // AS-Path stored as compact vector
    // For performance: use small vector optimization or raw pointer
    std::vector<ASN> as_path;           // 24 bytes (pointer + size + capacity)

    Announcement() : next_hop_asn(0), received_from(RelationshipType::ORIGIN), rov_invalid(false) {
        std::memset(_padding, 0, sizeof(_padding));
    }

    // Create announcement with single AS in path
    Announcement(const Prefix& p, ASN origin, RelationshipType rel = RelationshipType::ORIGIN, bool rov_inv = false)
        : prefix(p), next_hop_asn(origin), received_from(rel), rov_invalid(rov_inv) {
        std::memset(_padding, 0, sizeof(_padding));
        as_path.push_back(origin);
        as_path.shrink_to_fit(); // Save memory for single-element paths
    }

    // Copy announcement with new next_hop and relationship (does NOT prepend to path)
    // Receiver will prepend their ASN when storing
    Announcement copy_with_new_hop(ASN new_next_hop, RelationshipType new_rel) const {
        Announcement new_ann;
        new_ann.prefix = prefix;
        new_ann.next_hop_asn = new_next_hop;
        new_ann.received_from = new_rel;
        new_ann.rov_invalid = rov_invalid;
        new_ann.as_path = as_path; // Copy path unchanged
        return new_ann;
    }

    // Get AS path length (critical for routing decisions)
    size_t getPathLength() const {
        return as_path.size();
    }

    // Check if AS is in path (loop prevention)
    bool containsAS(ASN asn) const {
        for (ASN path_asn : as_path) {
            if (path_asn == asn) return true;
        }
        return false;
    }

    // Compare announcements for route selection
    // Returns: true if this announcement is better than 'other'
    bool isBetterThan(const Announcement& other) const {
        // Rule 1: Best relationship (customer > peer > provider, origin is best)
        if (received_from != other.received_from) {
            return received_from < other.received_from;
        }

        // Rule 2: Shortest AS path
        if (as_path.size() != other.as_path.size()) {
            return as_path.size() < other.as_path.size();
        }

        // Rule 3: Lowest next hop ASN (tie breaker)
        return next_hop_asn < other.next_hop_asn;
    }
};

#endif // ANNOUNCEMENT_H
