# BGP Simulator Python API Reference

Quick reference guide for the Python bindings.

## Installation

```bash
pip install pybind11
cd bgp_simulator
pip install .
```

## Quick Reference

### Import

```python
import bgp_simulator as bgp
```

### ASGraph Class

Main class for BGP simulation.

#### Creation and Setup

```python
graph = bgp.ASGraph()                    # Create new graph
graph.build_from_file(filename)          # Load CAIDA topology
graph.detect_cycles()                    # Returns True if cycles exist
graph.initialize_bgp()                   # Initialize BGP policies
graph.flatten_graph()                    # Assign propagation ranks
graph.reserve_nodes(count)               # Pre-allocate space
```

#### Announcements

```python
graph.seed_announcement(asn, prefix, rov_invalid)  # Seed announcement
total = graph.propagate_announcements()            # Propagate all
graph.export_to_csv(filename)                      # Export results
```

#### ROV Operations

```python
graph.load_rov_asns(filename)      # Load ROV-deploying ASes
count = graph.get_rov_asn_count()  # Get ROV deployment count
```

#### Queries

```python
# Node information
info = graph.get_node_info(asn)
# Returns dict with: asn, propagation_rank, providers, customers, peers, rib_size, rib

# All nodes
all_nodes = graph.get_all_nodes_info()
# Returns dict: {asn_str: node_info}

# RIB for specific AS
rib = graph.get_rib(asn)
# Returns dict: {prefix_str: announcement_dict}

# Specific announcement
ann = graph.get_announcement(asn, prefix)
# Returns announcement dict or None

# Graph structure
ranked = graph.get_ranked_ases()  # Returns list of lists by rank
```

#### Statistics

```python
# Basic counts
graph.get_node_count()                  # Number of ASes
graph.get_edge_count()                  # Number of relationships
graph.get_provider_customer_edges()     # P-C edges
graph.get_peer_edges()                  # Peer edges

# Check existence
graph.has_node(asn)  # Returns bool
```

### Prefix Class

Represents IPv4 or IPv6 prefix.

```python
# Parsing
prefix = bgp.Prefix.parse("10.0.0.0/8")
prefix = bgp.parse_prefix("192.168.0.0/16")  # Convenience function

# Properties
prefix.is_ipv6        # Bool
prefix.to_string()    # String representation

# Comparison
prefix1 == prefix2
```

### IPv4Prefix / IPv6Prefix Classes

```python
# IPv4
ipv4 = bgp.IPv4Prefix.parse("1.2.3.0/24")
ipv4.address       # uint32
ipv4.prefix_len    # uint8
ipv4.to_string()

# IPv6
ipv6 = bgp.IPv6Prefix.parse("2001:db8::/32")
ipv6.high          # uint64
ipv6.low           # uint64
ipv6.prefix_len    # uint8
ipv6.to_string()
```

### Announcement Class

Represents a BGP announcement.

```python
# Creation
ann = bgp.Announcement(
    prefix=bgp.Prefix.parse("10.0.0.0/8"),
    origin=1,
    rel=bgp.RelationshipType.ORIGIN,
    rov_invalid=False
)

# Properties (read-only)
ann.prefix           # Prefix object
ann.next_hop_asn     # uint32
ann.received_from    # RelationshipType enum
ann.rov_invalid      # bool
ann.as_path          # List[int] (property)

# Methods
length = ann.get_path_length()        # Returns int
has_as = ann.contains_as(asn)         # Returns bool
is_better = ann.is_better_than(other) # Returns bool
dict_repr = ann.to_dict()             # Returns dict
```

### Enums

#### RelationType (AS relationships)

```python
bgp.RelationType.CUSTOMER   # -1: AS1 is provider, AS2 is customer
bgp.RelationType.PEER       #  0: Peer-to-peer
bgp.RelationType.PROVIDER   #  1: AS1 is customer, AS2 is provider
```

#### RelationshipType (announcement source)

```python
bgp.RelationshipType.ORIGIN     # 0: Initial announcement (highest priority)
bgp.RelationshipType.CUSTOMER   # 1: From customer
bgp.RelationshipType.PEER       # 2: From peer
bgp.RelationshipType.PROVIDER   # 3: From provider
```

### Utility Functions

```python
# Statistics
stats = bgp.get_graph_statistics(graph)
# Returns dict with:
#   total_nodes, total_edges, provider_customer_edges, peer_edges,
#   rov_deploying_ases, avg_providers, avg_customers, avg_peers, stub_ases
```

## Data Structure Details

### node_info Dictionary

```python
{
    'asn': int,
    'propagation_rank': int,
    'providers': List[int],
    'customers': List[int],
    'peers': List[int],
    'rib_size': int,
    'rib': Dict[str, announcement_dict]
}
```

### announcement_dict Dictionary

```python
{
    'prefix': str,
    'next_hop_asn': int,
    'received_from': int,  # RelationshipType as int
    'rov_invalid': bool,
    'as_path': List[int]
}
```

### stats Dictionary

```python
{
    'total_nodes': int,
    'total_edges': int,
    'provider_customer_edges': int,
    'peer_edges': int,
    'rov_deploying_ases': int,
    'avg_providers': float,
    'avg_customers': float,
    'avg_peers': float,
    'stub_ases': int
}
```

## Complete Example

```python
import bgp_simulator as bgp

# Setup
graph = bgp.ASGraph()
graph.build_from_file("topology.txt")

if graph.detect_cycles():
    print("ERROR: Cycles detected")
    exit(1)

print(f"Loaded {graph.get_node_count()} ASes")

# Initialize
graph.initialize_bgp()
graph.load_rov_asns("rov_asns.txt")
graph.flatten_graph()

# Seed announcements
graph.seed_announcement(1, "10.0.0.0/8", False)      # Valid
graph.seed_announcement(100, "192.168.0.0/16", True)  # Invalid

# Propagate
total = graph.propagate_announcements()
print(f"Total announcements: {total}")

# Query specific AS
info = graph.get_node_info(50)
print(f"AS50 has {info['rib_size']} routes")

rib = graph.get_rib(50)
for prefix, ann in rib.items():
    path_str = ' → '.join(f"AS{asn}" for asn in ann['as_path'])
    print(f"  {prefix}: {path_str}")

# Get specific route
ann = graph.get_announcement(50, "10.0.0.0/8")
if ann:
    print(f"Route to 10.0.0.0/8: {ann['as_path']}")

# Statistics
stats = bgp.get_graph_statistics(graph)
print(f"Average path length can be calculated from RIBs")
print(f"Stub ASes: {stats['stub_ases']} ({stats['stub_ases']/stats['total_nodes']*100:.1f}%)")

# Export
graph.export_to_csv("results.csv")
print("Results exported")
```

## Common Patterns

### Iterate All ASes and Their Routes

```python
all_nodes = graph.get_all_nodes_info()
for asn_str, node_info in all_nodes.items():
    asn = int(asn_str)
    for prefix, ann in node_info['rib'].items():
        print(f"AS{asn} has route to {prefix} via {ann['as_path']}")
```

### Find ASes with Specific Prefix

```python
prefix = "10.0.0.0/8"
ases_with_route = []

all_nodes = graph.get_all_nodes_info()
for asn_str, node_info in all_nodes.items():
    if prefix in node_info['rib']:
        ases_with_route.append(int(asn_str))

print(f"{len(ases_with_route)} ASes have route to {prefix}")
```

### Calculate Average Path Length

```python
from collections import defaultdict

path_lengths = []
all_nodes = graph.get_all_nodes_info()

for node_info in all_nodes.values():
    for ann in node_info['rib'].values():
        path_lengths.append(len(ann['as_path']))

if path_lengths:
    avg_length = sum(path_lengths) / len(path_lengths)
    print(f"Average AS path length: {avg_length:.2f}")
```

### Find Most Popular Transit ASes

```python
from collections import Counter

transit_count = Counter()
all_nodes = graph.get_all_nodes_info()

for node_info in all_nodes.values():
    for ann in node_info['rib'].values():
        # Count all ASes except destination (first in path)
        for asn in ann['as_path'][1:]:
            transit_count[asn] += 1

# Top 10 transit ASes
for asn, count in transit_count.most_common(10):
    print(f"AS{asn}: appears in {count} paths")
```

### Compare Routes Before/After ROV

```python
# Run without ROV
graph1 = bgp.ASGraph()
graph1.build_from_file("topology.txt")
graph1.initialize_bgp()
graph1.flatten_graph()
graph1.seed_announcement(100, "192.168.0.0/16", True)
graph1.propagate_announcements()

ases_without_rov = sum(
    1 for info in graph1.get_all_nodes_info().values()
    if "192.168.0.0/16" in info['rib']
)

# Run with ROV
graph2 = bgp.ASGraph()
graph2.build_from_file("topology.txt")
graph2.initialize_bgp()
graph2.load_rov_asns("rov_asns.txt")
graph2.flatten_graph()
graph2.seed_announcement(100, "192.168.0.0/16", True)
graph2.propagate_announcements()

ases_with_rov = sum(
    1 for info in graph2.get_all_nodes_info().values()
    if "192.168.0.0/16" in info['rib']
)

reduction = ases_without_rov - ases_with_rov
print(f"ROV blocked invalid route at {reduction} ASes")
```

## Error Handling

```python
# File operations return bool
if not graph.build_from_file("topology.txt"):
    print("Failed to load topology")
    exit(1)

if not graph.export_to_csv("output.csv"):
    print("Failed to export results")

# Query operations return None if not found
ann = graph.get_announcement(999999, "10.0.0.0/8")
if ann is None:
    print("AS or announcement not found")

node_info = graph.get_node_info(999999)
if not node_info:  # Empty dict if AS doesn't exist
    print("AS not found")
```

## Performance Tips

1. **Pre-allocate** if you know graph size:
   ```python
   graph.reserve_nodes(100000)
   ```

2. **Batch queries** instead of repeated calls:
   ```python
   # Good: Get all at once
   all_nodes = graph.get_all_nodes_info()
   for asn_str, info in all_nodes.items():
       process(info)

   # Bad: Repeated queries
   for asn in range(1, 100000):
       info = graph.get_node_info(asn)  # Many failed lookups
   ```

3. **Cache lookups**:
   ```python
   # Cache frequently accessed data
   stats = bgp.get_graph_statistics(graph)
   node_count = stats['total_nodes']
   ```

## Version Information

```python
print(bgp.__version__)  # Prints version string
```

## See Also

- **README.md**: General documentation and C++ API
- **DESIGN_DECISIONS.txt**: Architecture details
- **examples/**: Complete working examples
