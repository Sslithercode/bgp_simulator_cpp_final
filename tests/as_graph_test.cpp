#include "as_graph.h"
#include <iostream>
#include <chrono>

void printStats(const ASGraph& graph) {
    std::cout << "\n=== AS Graph Statistics ===" << std::endl;
    std::cout << "Nodes (ASes): " << graph.getNodeCount() << std::endl;
    std::cout << "Total edges: " << graph.getEdgeCount() << std::endl;
    std::cout << "Provider-Customer edges: " << graph.getProviderCustomerEdges() << std::endl;
    std::cout << "Peer edges: " << graph.getPeerEdges() << std::endl;
    std::cout << "==========================\n" << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "==================================" << std::endl;
    std::cout << "BGP Simulator - AS Graph Builder" << std::endl;
    std::cout << "Task 2.3: Building AS Graph" << std::endl;
    std::cout << "==================================\n" << std::endl;

    // Determine input file
    std::string input_file = "as-rel.txt";
    if (argc > 1) {
        input_file = argv[1];
    }

    // Create AS Graph
    ASGraph graph;

    // Build from file with timing
    std::cout << "Building AS graph from file: " << input_file << std::endl;
    auto start = std::chrono::high_resolution_clock::now();

    if (!graph.buildFromFile(input_file)) {
        std::cerr << "Failed to build AS graph from file" << std::endl;
        return 1;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "\nGraph construction time: " << duration.count() << " ms" << std::endl;

    // Print statistics
    printStats(graph);

    // Detect cycles
    std::cout << "Running cycle detection..." << std::endl;
    start = std::chrono::high_resolution_clock::now();

    bool has_cycles = graph.detectCycles();

    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Cycle detection time: " << duration.count() << " ms" << std::endl;

    if (has_cycles) {
        std::cerr << "\nERROR: Graph contains cycles!" << std::endl;
        std::cerr << "The AS graph must be a DAG (Directed Acyclic Graph)" << std::endl;
        return 1;
    }

    std::cout << "\n=== SUCCESS ===" << std::endl;
    std::cout << "AS graph built and validated successfully!" << std::endl;
    std::cout << "Graph is a valid DAG with no cycles." << std::endl;

    // Sample some nodes
    std::cout << "\n=== Sample Nodes ===" << std::endl;
    int sample_count = 0;
    for (const auto& pair : graph.getNodes()) {
        const ASNode& node = pair.second;
        std::cout << "AS" << node.asn << ": "
                  << node.providers.size() << " providers, "
                  << node.customers.size() << " customers, "
                  << node.peers.size() << " peers" << std::endl;

        if (++sample_count >= 5) break;
    }

    // Verify specific known ASes from the data file
    std::cout << "\n=== Verification: Check Specific ASes ===" << std::endl;

    // From the data we saw: 1|11537|0|bgp means AS1 peers with AS11537
    if (graph.hasNode(1)) {
        const ASNode* as1 = graph.getNode(1);
        std::cout << "AS1 found: " << as1->providers.size() << " providers, "
                  << as1->customers.size() << " customers, "
                  << as1->peers.size() << " peers" << std::endl;

        // Show first few relationships
        if (!as1->peers.empty()) {
            std::cout << "  First peer: AS" << as1->peers[0].get().asn << std::endl;
        }
        if (!as1->customers.empty()) {
            std::cout << "  First customer: AS" << as1->customers[0].get().asn << std::endl;
        }
    }

    // Check AS3 which should have multiple relationships
    if (graph.hasNode(3)) {
        const ASNode* as3 = graph.getNode(3);
        std::cout << "AS3 found: " << as3->providers.size() << " providers, "
                  << as3->customers.size() << " customers, "
                  << as3->peers.size() << " peers" << std::endl;
    }

    std::cout << "\n=== Graph Build Verified ===" << std::endl;
    std::cout << "Graph successfully built from file with all relationships!" << std::endl;

    return 0;
}
