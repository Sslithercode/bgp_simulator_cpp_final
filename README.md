# BGP Simulator

A high-performance BGP (Border Gateway Protocol) route propagation simulator with Route Origin Validation (ROV) support. This simulator operates on real-world AS topology data from CAIDA and models valley-free routing, relationship-aware propagation, and security mechanisms.

## Features

- **Realistic BGP Simulation**: Models valley-free routing and economic relationships (customer, peer, provider)
- **Route Origin Validation (ROV)**: Simulate RPKI-based filtering of invalid route announcements
- **High Performance**: Optimized C++ implementation with O(1) lookups and cache-friendly data structures
- **Large-Scale Support**: Handles full Internet topology (~78k ASes, ~490k relationships)
- **Python Bindings**: Easy-to-use Python API via pybind11
- **Comprehensive Analysis**: Built-in tools for route analysis and statistics

## Table of Contents

- [Installation](#installation)
- [Quick Start](#quick-start)
- [Usage](#usage)
  - [C++ Interface](#c-interface)
  - [Python Interface](#python-interface)
- [Examples](#examples)
- [Architecture](#architecture)
- [Benchmarks](#benchmarks)
- [Documentation](#documentation)
- [Contributing](#contributing)
- [License](#license)

## Installation

### Prerequisites

- **C++ Compiler**: GCC 7+ or Clang 5+ with C++17 support
- **CMake**: Version 3.15 or higher
- **CURL**: For CAIDA data downloader (optional)
- **Python 3.6+**: For Python bindings (optional)
- **pybind11**: For Python bindings (optional)

### Building from Source

```bash
# Clone the repository
git clone https://github.com/yourusername/bgp_simulator.git
cd bgp_simulator

# Create build directory
mkdir -p build && cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . -j$(nproc)

# Run tests
./as_graph_test ../tests/test_mini_graph.txt
./bgp_rov_test
```

### Installing Python Bindings

```bash
# Install pybind11
pip install pybind11

# Build and install
cd bgp_simulator
pip install .

# Or for development
pip install -e .
```

## Quick Start

### C++ Quick Start

```cpp
#include "as_graph.h"

int main() {
    // Create graph and load topology
    ASGraph graph;
    graph.buildFromFile("CAIDAASGraphCollector_2025.10.16.txt");

    // Initialize BGP
    graph.initializeBGP();
    graph.flattenGraph();

    // Seed announcement
    graph.seedAnnouncement(1, "10.0.0.0/8", false);

    // Propagate
    graph.propagateAnnouncements();

    // Export results
    graph.exportToCSV("ribs.csv");

    return 0;
}
```

### Python Quick Start

```python
import bgp_simulator as bgp

# Create graph and load topology
graph = bgp.ASGraph()
graph.build_from_file("CAIDAASGraphCollector_2025.10.16.txt")

# Initialize BGP
graph.initialize_bgp()
graph.flatten_graph()

# Seed announcement
graph.seed_announcement(1, "10.0.0.0/8", False)

# Propagate
graph.propagate_announcements()

# Query results
rib = graph.get_rib(100)  # Get RIB for AS100
for prefix, ann in rib.items():
    print(f"{prefix}: {ann['as_path']}")

# Export
graph.export_to_csv("ribs.csv")
```

## Usage

### C++ Interface

#### Command-Line Tool

The main simulator accepts the following arguments:

```bash
./bgp_simulator --relationships <topology_file> \
                --announcements <announcements_csv> \
                [--rov-asns <rov_asns_file>] \
                [--output <output_csv>]
```

**Example:**

```bash
./bgp_simulator --relationships data/caida_graph.txt \
                --announcements data/anns.csv \
                --rov-asns data/rov_asns.csv \
                --output results.csv
```

#### File Formats

**AS Relationships File** (CAIDA format):
```
# AS relationships
1|2|-1|source
2|3|0|source
```
- Format: `AS1|AS2|relationship|source`
- Relationships: `-1` = customer, `0` = peer, `1` = provider

**Announcements CSV:**
```csv
seed_asn,prefix,rov_invalid
1,10.0.0.0/8,False
2,192.168.0.0/16,True
```

**ROV ASNs File:**
```
1
2
3
```
One ASN per line.

**Output CSV:**
```csv
asn,prefix,as_path
1,10.0.0.0/8,"(1,)"
2,10.0.0.0/8,"(2, 1)"
```

### Python Interface

#### Basic Operations

```python
import bgp_simulator as bgp

# Create and configure graph
graph = bgp.ASGraph()
graph.build_from_file("topology.txt")
graph.detect_cycles()  # Returns True if cycles found
graph.initialize_bgp()
graph.flatten_graph()

# Seed announcements
graph.seed_announcement(origin_asn=1, prefix="10.0.0.0/8", rov_invalid=False)

# Load ROV-deploying ASes
graph.load_rov_asns("rov_asns.txt")

# Propagate
total_anns = graph.propagate_announcements()
print(f"Total announcements: {total_anns}")
```

#### Query Operations

```python
# Get node information
node_info = graph.get_node_info(100)
print(f"AS100 has {len(node_info['customers'])} customers")
print(f"Propagation rank: {node_info['propagation_rank']}")

# Get local RIB for an AS
rib = graph.get_rib(100)
for prefix, announcement in rib.items():
    print(f"{prefix}: {announcement['as_path']}")

# Get specific announcement
ann = graph.get_announcement(100, "10.0.0.0/8")
if ann:
    print(f"AS path: {ann['as_path']}")
    print(f"Next hop: AS{ann['next_hop_asn']}")

# Get all nodes
all_nodes = graph.get_all_nodes_info()
for asn, info in all_nodes.items():
    print(f"AS{asn}: {info['rib_size']} routes")

# Statistics
stats = bgp.get_graph_statistics(graph)
print(f"Total nodes: {stats['total_nodes']}")
print(f"Stub ASes: {stats['stub_ases']}")
print(f"Average customers: {stats['avg_customers']:.2f}")
```

#### Advanced Usage

```python
# Prefix operations
prefix = bgp.Prefix.parse("192.168.1.0/24")
print(f"Prefix: {prefix.to_string()}")
print(f"Is IPv6: {prefix.is_ipv6}")

# Create announcements manually
ann = bgp.Announcement(
    prefix=bgp.Prefix.parse("10.0.0.0/8"),
    origin=1,
    rel=bgp.RelationshipType.ORIGIN,
    rov_invalid=False
)
print(f"Path length: {ann.get_path_length()}")
print(f"Contains AS5: {ann.contains_as(5)}")

# Relationship types
print(bgp.RelationType.CUSTOMER)   # -1
print(bgp.RelationType.PEER)       # 0
print(bgp.RelationType.PROVIDER)   # 1

print(bgp.RelationshipType.ORIGIN)   # 0 (highest priority)
print(bgp.RelationshipType.CUSTOMER) # 1
print(bgp.RelationshipType.PEER)     # 2
print(bgp.RelationshipType.PROVIDER) # 3
```

## Examples

### C++ Examples

See `tests/` directory for examples:
- `as_graph_test.cpp`: Graph construction and cycle detection
- `bgp_rov_test.cpp`: ROV filtering demonstration

### Python Examples

See `examples/` directory:

1. **Basic Example** (`python_example_basic.py`):
   - Load topology
   - Seed announcements
   - Query results

2. **ROV Example** (`python_example_rov.py`):
   - Compare propagation with/without ROV
   - Measure ROV effectiveness

3. **Analysis Example** (`python_example_analysis.py`):
   - Path length distribution
   - Transit AS popularity
   - Route choice analysis
   - Influential AS identification

Run examples:
```bash
cd examples
python3 python_example_basic.py
python3 python_example_rov.py
python3 python_example_analysis.py
```

## Architecture

### Key Components

1. **Data Layer**: Prefix representation, AS paths, announcements
2. **Policy Layer**: BGP and ROV routing policies
3. **Graph Layer**: AS topology with relationship-aware propagation

### Propagation Algorithm

The simulator uses a three-phase propagation model:

1. **UP Phase**: Propagate to providers (rank 0 → highest)
   - Only forward customer/origin routes

2. **ACROSS Phase**: Propagate to peers (all ranks simultaneously)
   - Only forward customer/origin routes
   - Process all at once to prevent multi-hop peer paths

3. **DOWN Phase**: Propagate to customers (highest → rank 0)
   - Forward all routes

### Route Selection

Routes are selected based on:
1. **Relationship preference**: ORIGIN > CUSTOMER > PEER > PROVIDER
2. **Shortest AS path**
3. **Lowest next-hop ASN** (tie breaker)

### Data Structures

- **AS Graph**: `unordered_map<ASN, ASNode>` with pre-allocated capacity
- **Neighbors**: `vector<reference_wrapper<ASNode>>` for direct access
- **RIB**: `unordered_map<Prefix, Announcement>` for O(1) lookup
- **AS Path**: `vector<ASN>` for cache-friendly traversal

See `DESIGN_DECISIONS.txt` for detailed architectural documentation.

## Benchmarks

Performance on real-world CAIDA topology (78,370 ASes, 489,407 relationships):

| Benchmark | Announcements | RIB Entries | Time | Throughput |
|-----------|---------------|-------------|------|------------|
| Prefix | 2 | 78,022 | 579ms | 135k/sec |
| Subprefix | 2 | 155,385 | 735ms | 211k/sec |
| Many | 40 | 2,963,832 | 13.5 - optimized down to 6s | 493k/sec |

Tested on: Intel CPU, Linux 6.14, GCC 11.4, compiled with `-O3 -march=native`

### Running Benchmarks

```bash
cd bench

# Prefix hijack scenario
../build/bgp_simulator --relationships prefix/CAIDAASGraphCollector_2025.10.16.txt \
                       --announcements prefix/anns.csv \
                       --rov-asns prefix/rov_asns.csv \
                       --output my_results.csv

# Verify results
./compare_output.sh prefix/ribs.csv my_results.csv
```

## Documentation

- **DESIGN_DECISIONS.txt**: Detailed architectural decisions and rationale
- **Project Instructions**: See `PROJECT_INSTRUCTIONS.txt` for assignment context
- **API Documentation**: Generated from code comments (Doxygen compatible)

### Key Concepts

**Valley-Free Routing**: ASes only provide transit between:
- Customer ↔ Customer
- Customer ↔ Peer/Provider
- Customer ↔ Internet

**Route Origin Validation (ROV)**: ASes with ROV deployed filter announcements marked as `rov_invalid`, preventing propagation of hijacked routes.

**Propagation Ranks**: ASes are assigned ranks based on customer relationships:
- Rank 0: Stub networks (no customers)
- Rank N: ASes whose customers are all rank < N

## Project Structure

```
bgp_simulator/
├── include/              # Header files
│   ├── announcement.h    # Prefix and announcement structures
│   ├── bgp_policy.h      # BGP and ROV policy classes
│   └── as_graph.h        # AS graph and propagation
├── src/                  # Implementation files
│   ├── announcement.cpp
│   ├── bgp_policy.cpp
│   ├── as_graph.cpp
│   ├── bgp_simulator_main.cpp
│   ├── caida_downloader.cpp
│   └── python_bindings.cpp
├── tests/                # Test files and data
│   ├── as_graph_test.cpp
│   ├── test_mini_graph.txt
│   ├── test_cycle_graph.txt
│   └── test_rov_asns.txt
├── bench/                # Benchmark scenarios
│   ├── prefix/
│   ├── subprefix/
│   ├── many/
│   └── compare_output.sh
├── examples/             # Python example scripts
│   ├── python_example_basic.py
│   ├── python_example_rov.py
│   └── python_example_analysis.py
├── build/                # Build directory (generated)
├── CMakeLists.txt        # Build configuration
├── setup.py              # Python package setup
├── README.md             # This file
└── DESIGN_DECISIONS.txt  # Architecture documentation
```

## Testing

### C++ Tests

```bash
cd build

# AS graph test
./as_graph_test ../tests/test_mini_graph.txt

# Cycle detection test
./as_graph_test ../tests/test_cycle_graph.txt

# ROV test
./bgp_rov_test
```

### Python Tests

```bash
# Run examples
cd examples
python3 python_example_basic.py
python3 python_example_rov.py
python3 python_example_analysis.py
```

### Benchmark Tests

```bash
cd bench

# Run all benchmarks
for scenario in prefix subprefix many; do
    echo "Testing $scenario..."
    ../build/bgp_simulator \
        --relationships $scenario/CAIDAASGraphCollector_2025.10.16.txt \
        --announcements $scenario/anns.csv \
        --rov-asns $scenario/rov_asns.csv \
        --output test_$scenario.csv
    ./compare_output.sh $scenario/ribs.csv test_$scenario.csv
done
```


