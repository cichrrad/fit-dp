#include <Kokkos_Core.hpp>
#include <iostream>
#include <vector>
#include <map>
#include <algorithm>
#include <iomanip>

#include "src/preprocessing/csv_loader.hpp"
#include "src/graph.hpp"

void preprocess_edges(std::map<std::pair<int, int>, long long> &capacity_map,
                      std::vector<std::vector<int>> &adj,
                      const std::vector<InputEdge> &raw_edges,
                      const int &num_nodes,
                      size_t &total_edges)
{
    for (const auto &e : raw_edges)
    
    {
        if (e.u == e.v)
            continue;
        if (e.u >= num_nodes || e.v >= num_nodes)
            continue;

        capacity_map[{e.u, e.v}] += e.capacity;

        // Ensure reverse edge exists (cap 0 if not in input)
        if (capacity_map.find({e.v, e.u}) == capacity_map.end())
        {
            capacity_map[{e.v, e.u}] = 0;
        }
    }

    for (const auto &kv : capacity_map)
    {
        adj[kv.first.first].push_back(kv.first.second);
        total_edges++;
    }
}

int main(int argc, char **argv)
{
    // ---------------------------------------------------------
    // LOAD RAW GRAPH DATA
    // ---------------------------------------------------------
    std::string filename = "./input/mock/mock_graph.csv";
    int num_nodes = 0;
    const auto raw_edges = parse_csv(filename, num_nodes);

    if (raw_edges.empty())
    {
        std::cerr << "Error: No edges loaded. Check file path: " << filename << "\n";
        return 1;
    }
    std::cout << "Loaded " << raw_edges.size() << " edges. Detected " << num_nodes << " nodes.\n";
    std::cout << "Raw Input Edges:\n";
    for (const auto &e : raw_edges)
    {
        std::cout << "  " << e.u << " --[" << e.capacity << "]--> " << e.v << '\n';
    }
    int source = 0, sink = num_nodes - 1;
    std::cout << "source is node " << source << "; sink is node " << sink << '\n';

    Kokkos::initialize(argc, argv);
    {
        using defDevice = Kokkos::DefaultExecutionSpace;
        std::cout << "\nKokkos initialized on: " << typeid(defDevice).name() << "\n";

        // ---------------------------------------------------------
        // PREPROCESS (HOST)
        // ---------------------------------------------------------
        std::map<std::pair<int, int>, long long> capacity_map;
        std::vector<std::vector<int>> adj(num_nodes);
        size_t total_edges = 0;
        preprocess_edges(capacity_map, adj, raw_edges, num_nodes, total_edges);

        // ---------------------------------------------------------
        // CONSTRUCT GRAPH ON DEVICE
        // ---------------------------------------------------------
        Graph<defDevice> g;

        // Allocate on DEVICE first
        g.row_map = typename Graph<defDevice>::row_map_type("row_map", num_nodes + 1);
        g.entries = typename Graph<defDevice>::entries_type("entries", total_edges);
        g.capacity = typename Graph<defDevice>::capacity_type("capacity", total_edges);
        g.flow = typename Graph<defDevice>::flow_type("flow", total_edges);
        g.reverse_edge_index = typename Graph<defDevice>::reverse_edge_index_type("rev_idx", total_edges);

        g.excess = typename Graph<defDevice>::excess_type("excess", num_nodes);
        g.height = typename Graph<defDevice>::height_type("height", num_nodes);

        // Create Host Mirrors
        // (References if DEVICE storage is reachable, New Alloc if not)
        auto h_row_map = Kokkos::create_mirror_view(g.row_map);
        auto h_entries = Kokkos::create_mirror_view(g.entries);
        auto h_capacity = Kokkos::create_mirror_view(g.capacity);
        auto h_rev_idx = Kokkos::create_mirror_view(g.reverse_edge_index);

        // Populate Mirrors
        size_t current_idx = 0;
        for (int i = 0; i < num_nodes; ++i)
        {
            h_row_map(i) = current_idx;
            current_idx += adj[i].size();
        }
        h_row_map(num_nodes) = current_idx;

        // Map to track where we put every edge: {u,v} -> index
        std::map<std::pair<int, int>, size_t> edge_idx_map;

        // Fill
        for (int u = 0; u < num_nodes; ++u)
        {
            size_t row_start = h_row_map(u);
            const auto &neighbors = adj[u];
            for (size_t i = 0; i < neighbors.size(); ++i)
            {
                int v = neighbors[i];
                size_t idx = row_start + i;

                h_entries(idx) = v;
                h_capacity(idx) = capacity_map[{u, v}];

                edge_idx_map[{u, v}] = idx;
            }
        }

        // Fill Reverse Index (for O(1) lookup)
        for (int u = 0; u < num_nodes; ++u)
        {
            size_t start = h_row_map(u);
            size_t end = h_row_map(u + 1);
            for (size_t i = start; i < end; ++i)
            {
                int v = h_entries(i);
                h_rev_idx(i) = edge_idx_map.at({v, u});
            }
        }

        // COPY to Device (No-op if reachable)
        Kokkos::deep_copy(g.row_map, h_row_map);
        Kokkos::deep_copy(g.entries, h_entries);
        Kokkos::deep_copy(g.capacity, h_capacity);
        Kokkos::deep_copy(g.reverse_edge_index, h_rev_idx);

        // ---------------------------------------------------------
        // PRINT INITIAL GRAPH
        // ---------------------------------------------------------
        std::cout << "\n Initial Graph on Device\n";
        // We use the host mirrors we already have populated
        for (int u = 0; u < num_nodes; ++u)
        {
            size_t start = h_row_map(u);
            size_t end = h_row_map(u + 1);
            for (size_t e = start; e < end; ++e)
            {
                std::cout << "Node " << u << " -> " << h_entries(e)
                          << " [Cap: " << h_capacity(e)
                          << ", Flow: " << 0 // Known 0 init
                          << ", RevIdx: " << h_rev_idx(e) << "]\n";
            }
        }

        // ---------------------------------------------------------
        // MODIFY GRAPH (KERNEL)
        // ---------------------------------------------------------
        std::cout << "\n Saturating source node " << source << '\n';
        Kokkos::parallel_for("Saturate_Source", Kokkos::RangePolicy<defDevice>(0, num_nodes - 1), KOKKOS_LAMBDA(const int &idx) {
                
                size_t start = g.row_map(idx);
                size_t end = g.row_map(idx+1);

                for (size_t e = start; e < end; ++e) {
                    long long cap = g.capacity(e);
                    if (cap > 0) {
                        // Push Flow
                        g.flow(e) = cap;
                        // Update Reverse Flow
                        size_t rev = g.reverse_edge_index(e);
                        g.flow(rev) = -cap;

                        // Update Excess
                        int v = g.entries(e);
                        Kokkos::atomic_add(&g.excess(v), cap);
                        Kokkos::atomic_sub(&g.excess(idx), cap);
                    }
                } });
        Kokkos::fence();

        // ---------------------------------------------------------
        // PRINT FINAL STATE
        // ---------------------------------------------------------

        // Copy back flow and excess to see changes
        auto h_flow = Kokkos::create_mirror_view(g.flow);
        auto h_excess = Kokkos::create_mirror_view(g.excess);
        Kokkos::deep_copy(h_flow, g.flow);
        Kokkos::deep_copy(h_excess, g.excess);

        std::cout << "\n Graph After Saturation\n";
        for (int u = 0; u < num_nodes; ++u)
        {
            std::cout << "Node " << u << " Excess: " << h_excess(u) << "\n";
            size_t start = h_row_map(u);
            size_t end = h_row_map(u + 1);
            for (size_t e = start; e < end; ++e)
            {
                if (h_capacity(e) > 0 || h_flow(e) != 0)
                { // Show edges with activity
                    std::cout << "  -> " << h_entries(e)
                              << " [Flow: " << h_flow(e) << " / " << h_capacity(e) << "]\n";
                }
            }
        }
    }
    Kokkos::finalize();
    std::cout << "Goodbye World\n";
    return 0;
}