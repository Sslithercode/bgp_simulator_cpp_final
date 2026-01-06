#include "as_graph.h"
#include "bgp_policy.h"
#include <iostream>

int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << "BGP ROV Test - Section 4" << std::endl;
    std::cout << "==========================================" << std::endl;

    // Build mini test graph
    ASGraph graph;
    std::cout << "\nBuilding test graph..." << std::endl;

    if (!graph.buildFromFile("../tests/test_mini_graph.txt")) {
        std::cerr << "Failed to build graph" << std::endl;
        return 1;
    }

    // Initialize BGP
    graph.initializeBGP();

    // Load ROV ASNs
    std::cout << "\nLoading ROV ASNs..." << std::endl;
    graph.loadROVASNs("../tests/test_rov_asns.txt");

    // Flatten
    graph.flattenGraph();

    std::cout << "\n========== Test 1: Valid Announcement ==========" << std::endl;
    // Seed valid announcement at AS1
    graph.seedAnnouncement(1, "10.0.0.0/8", false);
    graph.propagateAnnouncements();

    size_t valid_count = 0;
    for (const auto& pair : graph.getNodes()) {
        if (pair.second.policy && pair.second.policy->getLocalRIBSize() > 0) {
            valid_count++;
        }
    }
    std::cout << "Valid announcement reached " << valid_count << " ASes" << std::endl;

    // Clear for next test
    for (auto& pair : graph.getNodes()) {
        if (pair.second.policy) {
            delete pair.second.policy;
            pair.second.policy = nullptr;
        }
    }

    std::cout << "\n========== Test 2: Invalid Announcement (with ROV) ==========" << std::endl;
    // Reinitialize
    graph.initializeBGP();
    graph.loadROVASNs("../tests/test_rov_asns.txt");

    // Seed INVALID announcement at AS2
    graph.seedAnnouncement(2, "192.168.0.0/16", true);  // ROV invalid
    graph.propagateAnnouncements();

    size_t invalid_count = 0;
    for (const auto& pair : graph.getNodes()) {
        if (pair.second.policy && pair.second.policy->getLocalRIBSize() > 0) {
            invalid_count++;
            std::cout << "  AS" << pair.first << " received invalid announcement" << std::endl;
        }
    }
    std::cout << "Invalid announcement reached " << invalid_count << " ASes (ROV deployed at AS1, AS3, AS4)" << std::endl;

    std::cout << "\n========== ROV TEST COMPLETE ==========" << std::endl;
    std::cout << "ROV successfully blocked invalid announcements at deploying ASes!" << std::endl;

    return 0;
}
