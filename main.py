import bgp_simulator as bgp

g = bgp.ASGraph()
g.build_from_file("20250901.as-rel2.txt")

g.initialize_bgp()
g.flatten_graph()

g.seed_announcement(3, "10.0.0.0/24")
g.propagate_announcements()

print("Graph:", g)
print("Node 1 RIB:", g.get_rib(1))
print("Node 5 RIB:", g.get_rib(5))
