#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include "as_graph.h"
#include "announcement.h"
#include "bgp_policy.h"

namespace py = pybind11;

// Helper to convert Announcement to dict for Python
py::dict announcement_to_dict(const Announcement& ann) {
    py::dict result;
    result["prefix"] = ann.prefix.toString();
    result["next_hop_asn"] = ann.next_hop_asn;
    result["received_from"] = static_cast<int>(ann.received_from);
    result["rov_invalid"] = ann.rov_invalid;

    py::list path;
    for (ASN asn : ann.as_path) {
        path.append(asn);
    }
    result["as_path"] = path;

    return result;
}

// Helper to get node information
py::dict get_node_info(const ASNode* node) {
    if (!node) {
        return py::dict();
    }

    py::dict result;
    result["asn"] = node->asn;
    result["propagation_rank"] = node->propagation_rank;

    py::list providers, customers, peers;
    for (const auto& p : node->providers) {
        providers.append(p.get().asn);
    }
    for (const auto& c : node->customers) {
        customers.append(c.get().asn);
    }
    for (const auto& p : node->peers) {
        peers.append(p.get().asn);
    }

    result["providers"] = providers;
    result["customers"] = customers;
    result["peers"] = peers;

    // Get RIB info if policy exists
    if (node->policy) {
        result["rib_size"] = node->policy->getLocalRIBSize();

        py::dict rib;
        for (const auto& pair : node->policy->getLocalRIB()) {
            rib[pair.first.toString().c_str()] = announcement_to_dict(pair.second);
        }
        result["rib"] = rib;
    } else {
        result["rib_size"] = 0;
        result["rib"] = py::dict();
    }

    return result;
}

PYBIND11_MODULE(bgp_simulator, m) {
    m.doc() = "BGP Simulator Python Bindings - Simulate BGP route propagation with ROV support";

    // Enums
    py::enum_<RelationType>(m, "RelationType")
        .value("CUSTOMER", RelationType::CUSTOMER, "AS1 is provider, AS2 is customer")
        .value("PEER", RelationType::PEER, "Peer-to-peer relationship")
        .value("PROVIDER", RelationType::PROVIDER, "AS1 is customer, AS2 is provider")
        .export_values();

    py::enum_<RelationshipType>(m, "RelationshipType")
        .value("ORIGIN", RelationshipType::ORIGIN, "Initial announcement (highest priority)")
        .value("CUSTOMER", RelationshipType::CUSTOMER, "From customer")
        .value("PEER", RelationshipType::PEER, "From peer")
        .value("PROVIDER", RelationshipType::PROVIDER, "From provider")
        .export_values();

    // IPv4Prefix
    py::class_<IPv4Prefix>(m, "IPv4Prefix")
        .def(py::init<>())
        .def(py::init<uint32_t, uint8_t>())
        .def_readwrite("address", &IPv4Prefix::address)
        .def_readwrite("prefix_len", &IPv4Prefix::prefix_len)
        .def_static("parse", &IPv4Prefix::parse, "Parse IPv4 prefix from string (e.g., '1.2.0.0/16')")
        .def("to_string", &IPv4Prefix::toString, "Convert to string representation")
        .def("__str__", &IPv4Prefix::toString)
        .def("__repr__", [](const IPv4Prefix& p) {
            return "IPv4Prefix('" + p.toString() + "')";
        })
        .def("__eq__", &IPv4Prefix::operator==);

    // IPv6Prefix
    py::class_<IPv6Prefix>(m, "IPv6Prefix")
        .def(py::init<>())
        .def(py::init<uint64_t, uint64_t, uint8_t>())
        .def_readwrite("high", &IPv6Prefix::high)
        .def_readwrite("low", &IPv6Prefix::low)
        .def_readwrite("prefix_len", &IPv6Prefix::prefix_len)
        .def_static("parse", &IPv6Prefix::parse, "Parse IPv6 prefix from string")
        .def("to_string", &IPv6Prefix::toString, "Convert to string representation")
        .def("__str__", &IPv6Prefix::toString)
        .def("__repr__", [](const IPv6Prefix& p) {
            return "IPv6Prefix('" + p.toString() + "')";
        })
        .def("__eq__", &IPv6Prefix::operator==);

    // Prefix
    py::class_<Prefix>(m, "Prefix")
        .def(py::init<>())
        .def(py::init<const IPv4Prefix&>())
        .def(py::init<const IPv6Prefix&>())
        .def_readonly("is_ipv6", &Prefix::is_ipv6)
        .def_static("parse", &Prefix::parse, "Parse prefix from string (auto-detect IPv4/IPv6)")
        .def("to_string", &Prefix::toString, "Convert to string representation")
        .def("__str__", &Prefix::toString)
        .def("__repr__", [](const Prefix& p) {
            return "Prefix('" + p.toString() + "')";
        })
        .def("__eq__", &Prefix::operator==);

    // Announcement
    py::class_<Announcement>(m, "Announcement")
        .def(py::init<>())
        .def(py::init<const Prefix&, ASN, RelationshipType, bool>(),
             py::arg("prefix"), py::arg("origin"),
             py::arg("rel") = RelationshipType::ORIGIN,
             py::arg("rov_invalid") = false)
        .def_readonly("prefix", &Announcement::prefix)
        .def_readonly("next_hop_asn", &Announcement::next_hop_asn)
        .def_readonly("received_from", &Announcement::received_from)
        .def_readonly("rov_invalid", &Announcement::rov_invalid)
        .def_property_readonly("as_path", [](const Announcement& ann) {
            py::list path;
            for (ASN asn : ann.as_path) {
                path.append(asn);
            }
            return path;
        })
        .def("get_path_length", &Announcement::getPathLength, "Get AS path length")
        .def("contains_as", &Announcement::containsAS, "Check if AS is in path")
        .def("is_better_than", &Announcement::isBetterThan, "Compare announcements for route selection")
        .def("to_dict", &announcement_to_dict, "Convert announcement to dictionary")
        .def("__repr__", [](const Announcement& ann) {
            return "Announcement(prefix='" + ann.prefix.toString() +
                   "', origin=" + std::to_string(ann.next_hop_asn) +
                   ", path_len=" + std::to_string(ann.as_path.size()) + ")";
        });

    // ASGraph
    py::class_<ASGraph>(m, "ASGraph")
        .def(py::init<>(), "Create a new AS graph")
        .def("build_from_file", &ASGraph::buildFromFile,
             py::arg("filename"),
             "Build graph from CAIDA AS relationships file")
        .def("add_relationship", &ASGraph::addRelationship,
             py::arg("as1"), py::arg("as2"), py::arg("rel_type"),
             "Add a relationship between two ASes")
        .def("detect_cycles", &ASGraph::detectCycles,
             "Check for cycles in provider-customer relationships")
        .def("has_node", &ASGraph::hasNode,
             py::arg("asn"),
             "Check if ASN exists in graph")
        .def("get_node_count", &ASGraph::getNodeCount, "Get number of nodes")
        .def("get_edge_count", &ASGraph::getEdgeCount, "Get number of edges")
        .def("get_provider_customer_edges", &ASGraph::getProviderCustomerEdges,
             "Get number of provider-customer edges")
        .def("get_peer_edges", &ASGraph::getPeerEdges, "Get number of peer edges")
        .def("reserve_nodes", &ASGraph::reserveNodes,
             py::arg("count"),
             "Reserve space for nodes to avoid rehashing")
        .def("initialize_bgp", &ASGraph::initializeBGP,
             "Initialize BGP policies for all nodes")
        .def("flatten_graph", &ASGraph::flattenGraph,
             "Assign propagation ranks to nodes")
        .def("get_ranked_ases", &ASGraph::getRankedASes,
             "Get flattened graph as vector of vectors by rank")
        .def("seed_announcement", &ASGraph::seedAnnouncement,
             py::arg("origin_asn"), py::arg("prefix_str"),
             py::arg("rov_invalid") = false,
             "Seed an announcement at a specific AS")
        .def("propagate_announcements", &ASGraph::propagateAnnouncements,
             "Propagate announcements through the entire graph")
        .def("export_to_csv", &ASGraph::exportToCSV,
             py::arg("filename"),
             "Export local RIBs to CSV file")
        .def("load_rov_asns", &ASGraph::loadROVASNs,
             py::arg("filename"),
             "Load ROV ASNs from file and upgrade their policies")
        .def("get_rov_asn_count", &ASGraph::getROVASNCount,
             "Get count of ASes deploying ROV")
        .def("get_node_info", [](ASGraph& graph, ASN asn) {
            return get_node_info(graph.getNode(asn));
        }, py::arg("asn"), "Get detailed information about a node")
        .def("get_all_nodes_info", [](ASGraph& graph) {
            py::dict result;
            for (const auto& pair : graph.getNodes()) {
                result[std::to_string(pair.first).c_str()] = get_node_info(&pair.second);
            }
            return result;
        }, "Get information about all nodes in the graph")
        .def("get_rib", [](ASGraph& graph, ASN asn) {
            const ASNode* node = graph.getNode(asn);
            if (!node || !node->policy) {
                return py::dict();
            }

            py::dict rib;
            for (const auto& pair : node->policy->getLocalRIB()) {
                rib[pair.first.toString().c_str()] = announcement_to_dict(pair.second);
            }
            return rib;
        }, py::arg("asn"), "Get the local RIB for a specific AS")
        .def("get_announcement", [](ASGraph& graph, ASN asn, const std::string& prefix_str) -> py::object {
            const ASNode* node = graph.getNode(asn);
            if (!node || !node->policy) {
                return py::none();
            }

            Prefix prefix = Prefix::parse(prefix_str);
            const Announcement* ann = node->policy->getAnnouncement(prefix);
            if (!ann) {
                return py::none();
            }

            py::dict result = announcement_to_dict(*ann);
            return result;
        }, py::arg("asn"), py::arg("prefix"),
           "Get specific announcement from AS's RIB")
        .def("__repr__", [](const ASGraph& graph) {
            return "ASGraph(nodes=" + std::to_string(graph.getNodeCount()) +
                   ", edges=" + std::to_string(graph.getEdgeCount()) + ")";
        });

    // Utility functions
    m.def("parse_prefix", &Prefix::parse,
          py::arg("prefix_str"),
          "Parse a prefix string (auto-detect IPv4/IPv6)");

    // Statistics helper
    m.def("get_graph_statistics", [](const ASGraph& graph) {
        py::dict stats;
        stats["total_nodes"] = graph.getNodeCount();
        stats["total_edges"] = graph.getEdgeCount();
        stats["provider_customer_edges"] = graph.getProviderCustomerEdges();
        stats["peer_edges"] = graph.getPeerEdges();
        stats["rov_deploying_ases"] = graph.getROVASNCount();

        // Calculate degree statistics
        size_t total_providers = 0;
        size_t total_customers = 0;
        size_t total_peers = 0;
        size_t stub_count = 0;

        for (const auto& pair : graph.getNodes()) {
            const ASNode& node = pair.second;
            total_providers += node.providers.size();
            total_customers += node.customers.size();
            total_peers += node.peers.size();

            if (node.customers.empty() && node.peers.empty()) {
                stub_count++;
            }
        }

        stats["avg_providers"] = static_cast<double>(total_providers) / graph.getNodeCount();
        stats["avg_customers"] = static_cast<double>(total_customers) / graph.getNodeCount();
        stats["avg_peers"] = static_cast<double>(total_peers) / graph.getNodeCount();
        stats["stub_ases"] = stub_count;

        return stats;
    }, py::arg("graph"), "Get comprehensive graph statistics");

    // Version info
    m.attr("__version__") = "1.0.0";
}
