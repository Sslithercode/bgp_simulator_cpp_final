#include "as_graph.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <chrono>

ASGraph::ASGraph() {
    // Reserve space for expected ~100k nodes to avoid rehashing
    nodes.reserve(120000);
    asn_set.reserve(120000);
}

void ASGraph::reserveNodes(size_t count) {
    nodes.reserve(count);
    asn_set.reserve(count);
}

ASNode& ASGraph::getOrCreateNode(ASN asn) {
    // Fast path: check if exists
    auto it = nodes.find(asn);
    if (it != nodes.end()) {
        return it->second;
    }

    // Create new node
    asn_set.insert(asn);
    auto result = nodes.emplace(asn, ASNode(asn));
    return result.first->second;
}

void ASGraph::addRelationship(ASN as1, ASN as2, RelationType rel_type) {
    ASNode& node1 = getOrCreateNode(as1);
    ASNode& node2 = getOrCreateNode(as2);

    edge_count++;

    switch (rel_type) {
        case RelationType::PROVIDER:
            // as1 is customer, as2 is provider
            node1.providers.push_back(std::ref(node2));
            node2.customers.push_back(std::ref(node1));
            provider_customer_edges++;
            break;

        case RelationType::CUSTOMER:
            // as1 is provider, as2 is customer
            node1.customers.push_back(std::ref(node2));
            node2.providers.push_back(std::ref(node1));
            provider_customer_edges++;
            break;

        case RelationType::PEER:
            // Peer to peer
            node1.peers.push_back(std::ref(node2));
            node2.peers.push_back(std::ref(node1));
            peer_edges++;
            break;
    }
}

bool ASGraph::buildFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file " << filename << std::endl;
        return false;
    }

    std::cout << "Parsing AS relationships from " << filename << "..." << std::endl;

    std::string line;
    line.reserve(128); // Pre-allocate reasonable line size

    size_t line_count = 0;
    size_t parsed_count = 0;

    // Fast parsing - avoid string operations where possible
    while (std::getline(file, line)) {
        line_count++;

        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Fast parsing: ASN1|ASN2|relationship|source
        // We only need first 3 fields
        const char* str = line.c_str();
        char* end;

        // Parse AS1
        ASN as1 = static_cast<ASN>(std::strtoul(str, &end, 10));
        if (end == str || *end != '|') continue;

        // Parse AS2
        str = end + 1;
        ASN as2 = static_cast<ASN>(std::strtoul(str, &end, 10));
        if (end == str || *end != '|') continue;

        // Parse relationship type
        str = end + 1;
        int rel_val = std::atoi(str);

        RelationType rel_type;
        switch (rel_val) {
            case -1:
                rel_type = RelationType::CUSTOMER;
                break;
            case 0:
                rel_type = RelationType::PEER;
                break;
            case 1:
                rel_type = RelationType::PROVIDER;
                break;
            default:
                continue; // Invalid relationship type
        }

        addRelationship(as1, as2, rel_type);
        parsed_count++;

        // Progress indicator every 100k lines
        if (parsed_count % 100000 == 0) {
            std::cout << "  Parsed " << parsed_count << " relationships..." << std::endl;
        }
    }

    file.close();

    std::cout << "Parsing complete:" << std::endl;
    std::cout << "  Total lines: " << line_count << std::endl;
    std::cout << "  Parsed relationships: " << parsed_count << std::endl;
    std::cout << "  Nodes (ASes): " << nodes.size() << std::endl;
    std::cout << "  Provider-Customer edges: " << provider_customer_edges << std::endl;
    std::cout << "  Peer edges: " << peer_edges << std::endl;

    return true;
}

bool ASGraph::hasCycleDFS(ASN current, ASN parent, std::unordered_set<ASN>& visited,
                          std::unordered_set<ASN>& recursion_stack, bool check_providers) {
    visited.insert(current);
    recursion_stack.insert(current);

    auto it = nodes.find(current);
    if (it == nodes.end()) {
        recursion_stack.erase(current);
        return false;
    }

    const ASNode& node = it->second;

    // Check either providers (going up) or customers (going down)
    const auto& neighbors = check_providers ? node.providers : node.customers;

    for (const auto& neighbor_ref : neighbors) {
        ASN neighbor = neighbor_ref.get().asn;
        // Skip parent in undirected traversal
        if (neighbor == parent) {
            continue;
        }

        // If neighbor is in recursion stack, we found a cycle
        if (recursion_stack.find(neighbor) != recursion_stack.end()) {
            return true;
        }

        // If not visited, recurse
        if (visited.find(neighbor) == visited.end()) {
            if (hasCycleDFS(neighbor, current, visited, recursion_stack, check_providers)) {
                return true;
            }
        }
    }

    recursion_stack.erase(current);
    return false;
}

bool ASGraph::detectCycles() {
    std::cout << "Checking for cycles in provider-customer relationships..." << std::endl;

    std::unordered_set<ASN> visited;
    std::unordered_set<ASN> recursion_stack;

    // Check for cycles by traversing provider relationships
    for (const auto& pair : nodes) {
        ASN asn = pair.first;

        if (visited.find(asn) == visited.end()) {
            if (hasCycleDFS(asn, 0, visited, recursion_stack, true)) {
                std::cerr << "ERROR: Cycle detected in provider-customer relationships!" << std::endl;
                std::cerr << "The AS graph contains cycles, which violates the DAG assumption." << std::endl;
                return true; // Cycle found
            }
        }
    }

    // Also check customer relationships (downward)
    visited.clear();
    recursion_stack.clear();

    for (const auto& pair : nodes) {
        ASN asn = pair.first;

        if (visited.find(asn) == visited.end()) {
            if (hasCycleDFS(asn, 0, visited, recursion_stack, false)) {
                std::cerr << "ERROR: Cycle detected in customer-provider relationships!" << std::endl;
                std::cerr << "The AS graph contains cycles, which violates the DAG assumption." << std::endl;
                return true; // Cycle found
            }
        }
    }

    std::cout << "No cycles detected. Graph is a valid DAG." << std::endl;
    return false; // No cycles
}

bool ASGraph::hasNode(ASN asn) const {
    return asn_set.find(asn) != asn_set.end();
}

// BGP Functionality Implementation

#include "bgp_policy.h"
#include <fstream>
#include <queue>
#include <algorithm>

ASNode::~ASNode() {
    delete policy;
}

void ASGraph::initializeBGP() {
    std::cout << "Initializing BGP policies for all nodes..." << std::endl;

    for (auto& pair : nodes) {
        ASNode& node = pair.second;
        if (node.policy == nullptr) {
            node.policy = new BGP();
        }
    }

    std::cout << "BGP policies initialized for " << nodes.size() << " nodes" << std::endl;
}

void ASGraph::flattenGraph() {
    std::cout << "Flattening graph (assigning propagation ranks)..." << std::endl;

    // BFS to assign ranks
    // Rank 0 = ASes with no customers (edges)
    // Rank increases as we go up the provider chain
    // Each AS rank = MAX(all customer ranks) + 1

    std::unordered_map<ASN, int> ranks;
    std::unordered_map<ASN, int> customer_count; // Track how many customers are unranked

    // Initialize: rank 0 for nodes with no customers, count customers for others
    std::queue<ASN> ready_queue;
    for (auto& pair : nodes) {
        ASNode& node = pair.second;
        if (node.customers.empty()) {
            ranks[node.asn] = 0;
            ready_queue.push(node.asn);
        } else {
            customer_count[node.asn] = node.customers.size();
        }
    }

    // Process nodes in topological order
    int max_rank = 0;
    while (!ready_queue.empty()) {
        ASN current = ready_queue.front();
        ready_queue.pop();

        ASNode* node = getNode(current);
        if (!node) continue;

        int current_rank = ranks[current];

        // Update all providers of this node
        for (auto& provider_ref : node->providers) {
            ASNode& provider = provider_ref.get();
            ASN provider_asn = provider.asn;

            auto count_it = customer_count.find(provider_asn);
            if (count_it == customer_count.end()) continue; // Already processed or no customers

            // Update the rank if this customer has higher rank
            int new_provider_rank = current_rank + 1;
            auto rank_it = ranks.find(provider_asn);
            if (rank_it == ranks.end()) {
                ranks[provider_asn] = new_provider_rank;
            } else {
                ranks[provider_asn] = std::max(ranks[provider_asn], new_provider_rank);
            }

            // Decrement customer count
            count_it->second--;
            if (count_it->second == 0) {
                // All customers of this provider are ranked, so provider is ready
                ready_queue.push(provider_asn);
                customer_count.erase(count_it);
                max_rank = std::max(max_rank, ranks[provider_asn]);
            }
        }
    }

    // Assign propagation_rank to each node and build ranked_ases vector
    ranked_ases.clear();
    ranked_ases.resize(max_rank + 1);

    for (auto& pair : nodes) {
        ASNode& node = pair.second;
        int rank = ranks[node.asn];
        node.propagation_rank = rank;
        ranked_ases[rank].push_back(node.asn);
    }

    std::cout << "Graph flattened. Max rank: " << max_rank << std::endl;
    for (size_t i = 0; i < ranked_ases.size(); i++) {
        std::cout << "  Rank " << i << ": " << ranked_ases[i].size() << " ASes" << std::endl;
    }
}


size_t ASGraph::propagateAnnouncements() {
    std::cout << "Propagating announcements..." << std::endl;

    size_t total_propagated = 0;

    // Phase 1: UP (to providers)
    propagateUp();

    // Phase 2: ACROSS (to peers, one hop only)
    propagateAcross();

    // Phase 3: DOWN (to customers)
    propagateDown();

    // Count total announcements
    for (const auto& pair : nodes) {
        total_propagated += pair.second.policy->getLocalRIBSize();
    }

    std::cout << "Propagation complete. Total announcements: " << total_propagated << std::endl;
    return total_propagated;
}

void ASGraph::propagateUp() {
    std::cout << "  Phase 1: Propagating UP (to providers)..." << std::endl;

    // Go from rank 0 upwards
    for (size_t rank = 0; rank < ranked_ases.size(); rank++) {
        // Send announcements from this rank
        for (ASN asn : ranked_ases[rank]) {
            auto node_it = nodes.find(asn);
            if (node_it == nodes.end() || !node_it->second.policy) continue;

            ASNode& node = node_it->second;
            if (node.providers.empty()) continue;

            const auto& local_rib = node.policy->getLocalRIB();
            if (local_rib.empty()) continue;

            // Send to providers (only announcements from customers or origin)
            for (const auto& rib_pair : local_rib) {
                const Announcement& ann = rib_pair.second;

                // Valley-free routing: only send to providers if learned from customer or origin
                if (ann.received_from != RelationshipType::CUSTOMER &&
                    ann.received_from != RelationshipType::ORIGIN) {
                    continue;
                }

                for (auto& provider_ref : node.providers) {
                    ASNode& provider = provider_ref.get();

                    // Don't send if provider is in AS path (loop prevention)
                    if (ann.containsAS(provider.asn)) continue;
                    if (!provider.policy) continue;

                    Announcement new_ann = ann.copy_with_new_hop(asn, RelationshipType::CUSTOMER);
                    provider.policy->receiveAnnouncement(new_ann);
                }
            }
        }

        // Process received queue for next rank
        if (rank + 1 < ranked_ases.size()) {
            for (ASN asn : ranked_ases[rank + 1]) {
                auto node_it = nodes.find(asn);
                if (node_it != nodes.end() && node_it->second.policy) {
                    node_it->second.policy->processReceivedQueue(asn);
                    node_it->second.policy->clearReceivedQueue();
                }
            }
        }
    }
}

void ASGraph::propagateAcross() {
    std::cout << "  Phase 2: Propagating ACROSS (to peers)..." << std::endl;

    // Send from ALL ASes - optimize by avoiding repeated lookups
    for (auto& pair : nodes) {
        ASNode& node = pair.second;
        if (!node.policy || node.peers.empty()) continue;

        const auto& local_rib = node.policy->getLocalRIB();
        if (local_rib.empty()) continue;

        for (const auto& rib_pair : local_rib) {
            const Announcement& ann = rib_pair.second;

            // Valley-free routing: only send to peers if learned from customer or origin
            if (ann.received_from != RelationshipType::CUSTOMER &&
                ann.received_from != RelationshipType::ORIGIN) {
                continue;
            }

            // Cache to avoid redundant containsAS checks for same peer
            for (auto& peer_ref : node.peers) {
                ASNode& peer = peer_ref.get();

                // Don't send if peer is in AS path (loop prevention)
                if (ann.containsAS(peer.asn)) continue;
                if (!peer.policy) continue;

                Announcement new_ann = ann.copy_with_new_hop(node.asn, RelationshipType::PEER);
                peer.policy->receiveAnnouncement(new_ann);
            }
        }
    }

    // Process ALL at once (to prevent multiple hops)
    for (auto& pair : nodes) {
        if (pair.second.policy) {
            pair.second.policy->processReceivedQueue(pair.second.asn);
            pair.second.policy->clearReceivedQueue();
        }
    }
}

void ASGraph::propagateDown() {
    std::cout << "  Phase 3: Propagating DOWN (to customers)..." << std::endl;

    // Go from highest rank downwards
    for (int rank = ranked_ases.size() - 1; rank >= 0; rank--) {
        // Send announcements
        for (ASN asn : ranked_ases[rank]) {
            auto node_it = nodes.find(asn);
            if (node_it == nodes.end() || !node_it->second.policy) continue;

            ASNode& node = node_it->second;
            if (node.customers.empty()) continue;

            const auto& local_rib = node.policy->getLocalRIB();
            if (local_rib.empty()) continue;

            // Send all announcements to customers
            for (const auto& rib_pair : local_rib) {
                const Announcement& ann = rib_pair.second;

                for (auto& customer_ref : node.customers) {
                    ASNode& customer = customer_ref.get();

                    // Don't send if customer is in AS path
                    if (ann.containsAS(customer.asn)) continue;
                    if (!customer.policy) continue;

                    Announcement new_ann = ann.copy_with_new_hop(asn, RelationshipType::PROVIDER);
                    customer.policy->receiveAnnouncement(new_ann);
                }
            }
        }

        // Process received queue for next rank down
        if (rank - 1 >= 0) {
            for (ASN asn : ranked_ases[rank - 1]) {
                auto node_it = nodes.find(asn);
                if (node_it != nodes.end() && node_it->second.policy) {
                    node_it->second.policy->processReceivedQueue(asn);
                    node_it->second.policy->clearReceivedQueue();
                }
            }
        }
    }
}

bool ASGraph::exportToCSV(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file " << filename << " for writing" << std::endl;
        return false;
    }

    // Write header
    file << "asn,prefix,as_path\n";

    // Write all announcements
    size_t count = 0;
    for (const auto& node_pair : nodes) {
        const ASNode& node = node_pair.second;
        if (!node.policy) continue;

        for (const auto& rib_pair : node.policy->getLocalRIB()) {
            const Prefix& prefix = rib_pair.first;
            const Announcement& ann = rib_pair.second;

            // Format: asn,prefix,"as1 as2 as3"
            file << node.asn << "," << prefix.toString() << ",\"";

            for (size_t i = 0; i < ann.as_path.size(); i++) {
                if (i > 0) file << " ";
                file << ann.as_path[i];
            }

            file << "\"\n";
            count++;
        }
    }

    file.close();
    std::cout << "Exported " << count << " announcements to " << filename << std::endl;
    return true;
}

// ROV Functionality Implementation

void ASGraph::seedAnnouncement(ASN origin_asn, const std::string& prefix_str, bool rov_invalid) {
    ASNode* node = getNode(origin_asn);
    if (!node || !node->policy) {
        std::cerr << "Error: Cannot seed announcement at AS" << origin_asn << std::endl;
        return;
    }

    Prefix prefix = Prefix::parse(prefix_str);
    Announcement ann(prefix, origin_asn, RelationshipType::ORIGIN, rov_invalid);

    node->policy->seedAnnouncement(ann);
    
    if (rov_invalid) {
        std::cout << "Seeded ROV INVALID announcement " << prefix_str << " at AS" << origin_asn << std::endl;
    } else {
        std::cout << "Seeded announcement " << prefix_str << " at AS" << origin_asn << std::endl;
    }
}

bool ASGraph::loadROVASNs(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open ROV ASN file " << filename << std::endl;
        return false;
    }

    std::cout << "Loading ROV ASNs from " << filename << "..." << std::endl;

    std::string line;
    size_t loaded = 0;
    size_t upgraded = 0;

    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Parse ASN
        ASN asn = static_cast<ASN>(std::strtoul(line.c_str(), nullptr, 10));
        
        if (asn == 0) {
            continue; // Invalid ASN
        }

        rov_asns.insert(asn);
        loaded++;

        // Upgrade policy to ROV if AS exists
        ASNode* node = getNode(asn);
        if (node && node->policy) {
            // Replace BGP with ROV
            delete node->policy;
            node->policy = new ROV();
            upgraded++;
        }
    }

    file.close();

    std::cout << "Loaded " << loaded << " ROV ASNs" << std::endl;
    std::cout << "Upgraded " << upgraded << " ASes to ROV policy" << std::endl;

    return true;
}

size_t ASGraph::getROVASNCount() const {
    return rov_asns.size();
}
