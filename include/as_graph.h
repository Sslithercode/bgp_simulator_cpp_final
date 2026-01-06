#ifndef AS_GRAPH_H
#define AS_GRAPH_H

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>

// Optimal architecture for AS Graph with memory and speed constraints
// - Use uint32_t for ASN (maximum 32-bit as per BGP RFC)
// - Use flat vectors instead of smart pointers for cache locality
// - Use hash maps for O(1) lookups
// - Preallocate to avoid reallocation

using ASN = uint32_t;

// Relationship types from CAIDA data
enum class RelationType : int8_t {
    CUSTOMER = -1,  // AS1 is provider, AS2 is customer
    PEER = 0,       // Peer-to-peer
    PROVIDER = 1    // AS1 is customer, AS2 is provider
};

// Forward declaration
class BGPPolicy;

// Forward declare for reference wrapper
struct ASNode;

// Compact AS node structure - only what we need
// Store references to neighbors instead of ASNs to avoid hash lookups
struct ASNode {
    ASN asn;

    // Store references to neighbor nodes for direct access (no hash lookups!)
    // Using reference_wrapper because references can't be reassigned
    std::vector<std::reference_wrapper<ASNode>> providers;
    std::vector<std::reference_wrapper<ASNode>> customers;
    std::vector<std::reference_wrapper<ASNode>> peers;

    // For cycle detection and propagation
    int propagation_rank = -1;
    bool visited = false;  // For DFS cycle detection
    bool in_stack = false; // For DFS cycle detection

    // BGP Policy (owned by this node, use reference not pointer)
    BGPPolicy* policy = nullptr;

    ASNode() : asn(0) {}
    explicit ASNode(ASN asn_val) : asn(asn_val) {
        // Reserve reasonable sizes to reduce reallocations
        providers.reserve(4);
        customers.reserve(8);
        peers.reserve(4);
    }

    ~ASNode();
};

class ASGraph {
private:
    // Main storage: ASN -> ASNode
    // Using unordered_map for sparse ASN space (not all ASNs from 0-2^32 exist)
    std::unordered_map<ASN, ASNode> nodes;

    // Quick existence check
    std::unordered_set<ASN> asn_set;

    // Statistics
    size_t edge_count = 0;
    size_t provider_customer_edges = 0;
    size_t peer_edges = 0;

    // Helper: Get or create node
    ASNode& getOrCreateNode(ASN asn);

    // Cycle detection using DFS
    bool hasCycleDFS(ASN current, ASN parent, std::unordered_set<ASN>& visited,
                     std::unordered_set<ASN>& recursion_stack, bool check_providers);

public:
    ASGraph();

    // Build graph from CAIDA file
    bool buildFromFile(const std::string& filename);

    // Add relationship between two ASes
    void addRelationship(ASN as1, ASN as2, RelationType rel_type);

    // Check for cycles in provider-customer relationships
    bool detectCycles();

    // Get node by ASN (nullptr if doesn't exist) - inline for hot paths
    inline const ASNode* getNode(ASN asn) const {
        auto it = nodes.find(asn);
        return (it != nodes.end()) ? &(it->second) : nullptr;
    }

    inline ASNode* getNode(ASN asn) {
        auto it = nodes.find(asn);
        return (it != nodes.end()) ? &(it->second) : nullptr;
    }

    // Check if ASN exists
    bool hasNode(ASN asn) const;

    // Statistics
    size_t getNodeCount() const { return nodes.size(); }
    size_t getEdgeCount() const { return edge_count; }
    size_t getProviderCustomerEdges() const { return provider_customer_edges; }
    size_t getPeerEdges() const { return peer_edges; }

    // For iteration
    const std::unordered_map<ASN, ASNode>& getNodes() const { return nodes; }
    std::unordered_map<ASN, ASNode>& getNodes() { return nodes; }

    // Memory optimization: reserve space if we know approximate size
    void reserveNodes(size_t count);

    // BGP Functionality (Section 3)

    // Initialize BGP policies for all nodes
    void initializeBGP();

    // Flatten graph: assign propagation ranks
    void flattenGraph();

    // Get flattened graph (vector of vectors by rank)
    const std::vector<std::vector<ASN>>& getRankedASes() const { return ranked_ases; }

    // Seed announcement at a specific AS
    void seedAnnouncement(ASN origin_asn, const std::string& prefix_str, bool rov_invalid = false);

    // Propagate announcements through the entire graph
    // Returns: number of announcements propagated
    size_t propagateAnnouncements();

    // Export local RIBs to CSV
    bool exportToCSV(const std::string& filename) const;

    // ROV Functionality (Section 4)

    // Load ROV ASNs from file and upgrade their policies
    bool loadROVASNs(const std::string& filename);

    // Get count of ASes deploying ROV
    size_t getROVASNCount() const;

private:
    // Flattened graph for efficient propagation
    std::vector<std::vector<ASN>> ranked_ases;

    // ROV tracking
    std::unordered_set<ASN> rov_asns;

    // Propagation helpers
    void propagateUp();      // Send to providers
    void propagateAcross();  // Send to peers (one hop only)
    void propagateDown();    // Send to customers
};

#endif // AS_GRAPH_H
