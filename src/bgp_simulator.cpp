#include "as_graph.h"
#include "announcement.h"
#include <iostream>
#include <chrono>

int main(int argc, char* argv[]) {
    std::cout << "======================================" << std::endl;
    std::cout << "BGP Simulator - Section 3" << std::endl;
    std::cout << "======================================\n" << std::endl;

    // Parse arguments
    std::string as_rel_file = "as-rel.txt";
    std::string output_file = "ribs.csv";
    std::string rov_asns_file = "";

    if (argc > 1) as_rel_file = argv[1];
    if (argc > 2) output_file = argv[2];
    if (argc > 3) rov_asns_file = argv[3];

    auto total_start = std::chrono::high_resolution_clock::now();

    // Step 1: Build AS Graph
    std::cout << "Step 1: Building AS Graph..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();

    ASGraph graph;
    if (!graph.buildFromFile(as_rel_file)) {
        std::cerr << "Failed to build AS graph" << std::endl;
        return 1;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "  Time: " << duration.count() << " ms\n" << std::endl;

    // Step 2: Detect Cycles
    std::cout << "Step 2: Detecting cycles..." << std::endl;
    start = std::chrono::high_resolution_clock::now();

    if (graph.detectCycles()) {
        std::cerr << "Graph contains cycles!" << std::endl;
        return 1;
    }

    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "  Time: " << duration.count() << " ms\n" << std::endl;

    // Step 3: Initialize BGP
    std::cout << "Step 3: Initializing BGP..." << std::endl;
    start = std::chrono::high_resolution_clock::now();

    graph.initializeBGP();

    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "  Time: " << duration.count() << " ms\n" << std::endl;

    // Step 3.5: Load ROV ASNs (if provided)
    if (!rov_asns_file.empty()) {
        std::cout << "Step 3.5: Loading ROV ASNs..." << std::endl;
        start = std::chrono::high_resolution_clock::now();

        if (!graph.loadROVASNs(rov_asns_file)) {
            std::cerr << "Warning: Failed to load ROV ASNs" << std::endl;
        }

        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "  Time: " << duration.count() << " ms\n" << std::endl;
    }

    // Step 4: Flatten Graph
    std::cout << "Step 4: Flattening graph..." << std::endl;
    start = std::chrono::high_resolution_clock::now();

    graph.flattenGraph();

    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "  Time: " << duration.count() << " ms\n" << std::endl;

    // Step 5: Seed Announcements
    std::cout << "Step 5: Seeding announcements..." << std::endl;
    start = std::chrono::high_resolution_clock::now();

    // Seed a test announcement
    graph.seedAnnouncement(1, "1.2.0.0/16");

    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "  Time: " << duration.count() << " ms\n" << std::endl;

    // Step 6: Propagate
    std::cout << "Step 6: Propagating announcements..." << std::endl;
    start = std::chrono::high_resolution_clock::now();

    size_t total_announcements = graph.propagateAnnouncements();

    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "  Time: " << duration.count() << " ms" << std::endl;
    std::cout << "  Total announcements: " << total_announcements << "\n" << std::endl;

    // Step 7: Export to CSV
    std::cout << "Step 7: Exporting to CSV..." << std::endl;
    start = std::chrono::high_resolution_clock::now();

    if (!graph.exportToCSV(output_file)) {
        std::cerr << "Failed to export to CSV" << std::endl;
        return 1;
    }

    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "  Time: " << duration.count() << " ms\n" << std::endl;

    // Total time
    auto total_end = std::chrono::high_resolution_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(total_end - total_start);

    std::cout << "======================================" << std::endl;
    std::cout << "SUCCESS!" << std::endl;
    std::cout << "Total time: " << total_duration.count() << " ms" << std::endl;
    std::cout << "Output file: " << output_file << std::endl;
    std::cout << "======================================" << std::endl;

    return 0;
}
