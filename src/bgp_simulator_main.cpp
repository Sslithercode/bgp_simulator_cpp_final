#include "as_graph.h"
#include "announcement.h"
#include "bgp_policy.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <getopt.h>

struct Config {
    std::string relationships_file;
    std::string announcements_file;
    std::string rov_asns_file;
    std::string output_file = "ribs.csv";
};

void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [options]\n"
              << "Options:\n"
              << "  --relationships <file>   AS relationships file (required)\n"
              << "  --announcements <file>   Announcements CSV file (required)\n"
              << "  --rov-asns <file>       ROV ASNs file (optional)\n"
              << "  --output <file>         Output CSV file (default: ribs.csv)\n"
              << "  -h, --help              Show this help\n";
}

bool parse_args(int argc, char* argv[], Config& config) {
    static struct option long_options[] = {
        {"relationships", required_argument, 0, 'r'},
        {"announcements", required_argument, 0, 'a'},
        {"rov-asns", required_argument, 0, 'v'},
        {"output", required_argument, 0, 'o'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;

    while ((opt = getopt_long(argc, argv, "r:a:v:o:h", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'r':
                config.relationships_file = optarg;
                break;
            case 'a':
                config.announcements_file = optarg;
                break;
            case 'v':
                config.rov_asns_file = optarg;
                break;
            case 'o':
                config.output_file = optarg;
                break;
            case 'h':
                print_usage(argv[0]);
                exit(0);
            default:
                print_usage(argv[0]);
                return false;
        }
    }

    if (config.relationships_file.empty() || config.announcements_file.empty()) {
        std::cerr << "Error: --relationships and --announcements are required\n\n";
        print_usage(argv[0]);
        return false;
    }

    return true;
}

bool load_announcements(ASGraph& graph, const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open announcements file " << filename << std::endl;
        return false;
    }

    std::cout << "Loading announcements from " << filename << "..." << std::endl;

    std::string line;
    size_t count = 0;

    // Skip header
    std::getline(file, line);

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string asn_str, prefix, rov_str;

        // Parse CSV: seed_asn,prefix,rov_invalid
        if (std::getline(iss, asn_str, ',') &&
            std::getline(iss, prefix, ',') &&
            std::getline(iss, rov_str)) {

            ASN seed_asn = std::stoul(asn_str);

            // Trim whitespace and carriage returns from rov_str
            rov_str.erase(rov_str.find_last_not_of(" \t\r\n") + 1);

            bool rov_invalid = (rov_str == "True" || rov_str == "true" || rov_str == "TRUE");

            graph.seedAnnouncement(seed_asn, prefix, rov_invalid);
            count++;
        }
    }

    file.close();
    std::cout << "Loaded " << count << " announcements" << std::endl;
    return true;
}

bool export_to_csv_tuples(const ASGraph& graph, const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file " << filename << " for writing" << std::endl;
        return false;
    }

    // Write header
    file << "asn,prefix,as_path\n";

    // Write all announcements
    size_t count = 0;
    for (const auto& node_pair : graph.getNodes()) {
        const ASNode& node = node_pair.second;
        if (!node.policy) continue;

        for (const auto& rib_pair : node.policy->getLocalRIB()) {
            const Prefix& prefix = rib_pair.first;
            const Announcement& ann = rib_pair.second;

            // Format: asn,prefix,"(as1, as2, as3)" or "(as1,)" for single element
            file << node.asn << "," << prefix.toString() << ",\"(";

            for (size_t i = 0; i < ann.as_path.size(); i++) {
                if (i > 0) file << ", ";
                file << ann.as_path[i];
            }

            // Add trailing comma for single-element paths
            if (ann.as_path.size() == 1) {
                file << ",";
            }

            file << ")\"\n";
            count++;
        }
    }

    file.close();
    std::cout << "Exported " << count << " announcements to " << filename << std::endl;
    return true;
}

int main(int argc, char* argv[]) {
    Config config;

    if (!parse_args(argc, argv, config)) {
        return 1;
    }

    std::cout << "======================================" << std::endl;
    std::cout << "BGP Simulator" << std::endl;
    std::cout << "======================================\n" << std::endl;

    auto total_start = std::chrono::high_resolution_clock::now();

    // Step 1: Build AS Graph
    std::cout << "Step 1: Building AS Graph..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();

    ASGraph graph;
    if (!graph.buildFromFile(config.relationships_file)) {
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
        std::cerr << "ERROR: Graph contains cycles!" << std::endl;
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

    // Step 4: Load ROV ASNs (if provided)
    if (!config.rov_asns_file.empty()) {
        std::cout << "Step 4: Loading ROV ASNs..." << std::endl;
        start = std::chrono::high_resolution_clock::now();

        if (!graph.loadROVASNs(config.rov_asns_file)) {
            std::cerr << "Warning: Failed to load ROV ASNs" << std::endl;
        }

        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "  Time: " << duration.count() << " ms\n" << std::endl;
    }

    // Step 5: Flatten Graph
    std::cout << "Step 5: Flattening graph..." << std::endl;
    start = std::chrono::high_resolution_clock::now();

    graph.flattenGraph();

    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "  Time: " << duration.count() << " ms\n" << std::endl;

    // Step 6: Load and Seed Announcements
    std::cout << "Step 6: Loading announcements..." << std::endl;
    start = std::chrono::high_resolution_clock::now();

    if (!load_announcements(graph, config.announcements_file)) {
        std::cerr << "Failed to load announcements" << std::endl;
        return 1;
    }

    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "  Time: " << duration.count() << " ms\n" << std::endl;

    // Step 7: Propagate
    std::cout << "Step 7: Propagating announcements..." << std::endl;
    start = std::chrono::high_resolution_clock::now();

    size_t total_announcements = graph.propagateAnnouncements();

    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "  Time: " << duration.count() << " ms" << std::endl;
    std::cout << "  Total announcements: " << total_announcements << "\n" << std::endl;

    // Step 8: Export to CSV
    std::cout << "Step 8: Exporting to CSV..." << std::endl;
    start = std::chrono::high_resolution_clock::now();

    if (!export_to_csv_tuples(graph, config.output_file)) {
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
    std::cout << "Output file: " << config.output_file << std::endl;
    std::cout << "======================================" << std::endl;

    return 0;
}
