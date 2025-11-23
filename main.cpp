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

    // LOAD GRAPH
    std::string filename = "./input/mock/mock_graph.csv";
    int num_nodes = 0;
    const auto raw_edges = parse_csv(filename, num_nodes);

    std::cout << "Loaded " << raw_edges.size() << " edges. Detected " << num_nodes << " nodes.\n";
    std::cout << "Raw Input Edges:\n";
    for (const auto &e : raw_edges)
    {
        std::cout << "  " << e.u << " --[" << e.capacity << "]--> " << e.v << '\n';
    }
    int source = 0, sink = num_nodes - 1;
    std::cout << "source is node " << source << "; sink is node " << sink << '\n';

    // preprocess raw edges
    std::map<std::pair<int, int>, long long> capacity_map;
    std::vector<std::vector<int>> adj(num_nodes);
    size_t total_edges = 0;
    preprocess_edges(capacity_map, adj, raw_edges, num_nodes, total_edges);

    // KOKKOS START
    Kokkos::initialize(argc, argv);
    {
        using defDevice = Kokkos::DefaultExecutionSpace;
        std::cout << "\nKokkos initialized on: " << typeid(defDevice).name() << "\n";

        // construct g on device
        Graph<defDevice> g;

        // !Allocate on DEVICE FIRST!
        // vertex -> edge list map
        g.row_map = typename Graph<defDevice>::row_map_type("row_map", num_nodes + 1);
        // edge list
        g.edge_list = typename Graph<defDevice>::edge_list_type("edge_list", total_edges);
        // edge props
        g.capacity = typename Graph<defDevice>::flow_type("capacity", total_edges);
        g.flow = typename Graph<defDevice>::flow_type("flow", total_edges);
        g.reverse_edge_index = typename Graph<defDevice>::reverse_edge_index_type("rev_idx", total_edges);
        // vertex props
        g.excess = typename Graph<defDevice>::flow_type("excess", num_nodes);
        g.height = typename Graph<defDevice>::height_type("height", num_nodes);

        // !Create Host Mirrors SECOND!
        // (References if DEVICE storage is reachable, New Alloc if not)
        auto h_row_map = Kokkos::create_mirror_view(g.row_map);
        auto h_edge_list = Kokkos::create_mirror_view(g.edge_list);
        auto h_capacity = Kokkos::create_mirror_view(g.capacity);
        auto h_rev_idx = Kokkos::create_mirror_view(g.reverse_edge_index);

        // Populate Mirrors
        // can be done in parallel if on same device?
        size_t current_idx = 0;
        for (int i = 0; i < num_nodes; ++i)
        {
            h_row_map(i) = current_idx;
            current_idx += adj[i].size();
        }
        h_row_map(num_nodes) = current_idx;

        // {u,v} -> index
        std::map<std::pair<int, int>, size_t> edge_idx_map;

        // fill
        for (int u = 0; u < num_nodes; ++u)
        {
            size_t row_start = h_row_map(u);
            const auto &neighbors = adj[u];
            for (size_t i = 0; i < neighbors.size(); ++i)
            {
                int v = neighbors[i];
                size_t idx = row_start + i;

                h_edge_list(idx) = v;
                h_capacity(idx) = capacity_map[{u, v}];

                edge_idx_map[{u, v}] = idx;
            }
        }

        // fill reverse index (for O(1) lookup)
        for (int u = 0; u < num_nodes; ++u)
        {
            size_t start = h_row_map(u);
            size_t end = h_row_map(u + 1);
            for (size_t i = start; i < end; ++i)
            {
                int v = h_edge_list(i);
                h_rev_idx(i) = edge_idx_map.at({v, u});
            }
        }
        // COPY to Device (No-op if reachable)
        Kokkos::deep_copy(g.row_map, h_row_map);
        Kokkos::deep_copy(g.edge_list, h_edge_list);
        Kokkos::deep_copy(g.capacity, h_capacity);
        Kokkos::deep_copy(g.reverse_edge_index, h_rev_idx);

        // print graph (on host)
        std::cout << "\n Initial Graph on Device\n";
        int e_idx_counter = 0;
        for (int u = 0; u < num_nodes; ++u)
        {
            size_t start = h_row_map(u);
            size_t end = h_row_map(u + 1);
            for (size_t e = start; e < end; ++e)
            {
                std::cout << "[Idx: " << e_idx_counter++ << "]Node " << u << " -> " << h_edge_list(e)
                          << " [Cap: " << h_capacity(e)
                          << ", Flow: " << 0 // Known 0 init
                          << ", RevIdx: " << h_rev_idx(e) << "]\n";
            }
        }

        // Kernel
        std::cout << "\n Pushing from all nodes\n";
        Kokkos::parallel_for("mock_push_all", Kokkos::RangePolicy<defDevice>(0, num_nodes - 1), KOKKOS_LAMBDA(const int &idx) {
                
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
                        int v = g.edge_list(e);
                        Kokkos::atomic_add(&g.excess(v), cap);
                        Kokkos::atomic_sub(&g.excess(idx), cap);
                    }
                } });
        Kokkos::fence();

        // Copy back flow and excess to see changes
        auto h_flow = Kokkos::create_mirror_view(g.flow);
        auto h_excess = Kokkos::create_mirror_view(g.excess);
        Kokkos::deep_copy(h_flow, g.flow);
        Kokkos::deep_copy(h_excess, g.excess);

        std::cout << "\n Graph After\n";
        for (int u = 0; u < num_nodes; ++u)
        {
            std::cout << "Node " << u << " Excess: " << h_excess(u) << "\n";
            size_t start = h_row_map(u);
            size_t end = h_row_map(u + 1);
            for (size_t e = start; e < end; ++e)
            {

                std::cout << "  -> " << h_edge_list(e)
                          << " [Flow: " << h_flow(e) << " / " << h_capacity(e) << "]\n";
            }
        }
    }
    Kokkos::finalize();
    std::cout << "Goodbye World\n";
    return 0;
}