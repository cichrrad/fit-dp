#ifndef GRAPH_BUILDER_HPP
#define GRAPH_BUILDER_HPP

#include <vector>
#include <map>
#include <algorithm>
#include <iostream>
#include <stdexcept>

#include <Kokkos_Core.hpp>

#include "graph.hpp"
#include "preprocessing/input_edge.hpp"

// A helper struct for building the graph on the Host
struct HostEdge {
    int target;
    long long capacity;
    int reverse_edge; // To be filled later
    int original_index;     // Temp helper to track sorting
};

class GraphBuilder {
public:
    // Main function to construct the Device Graph from input data
    template <class DeviceType>
    static Graph<DeviceType> build_graph(
        const std::vector<InputEdge>& raw_edges, 
        int num_nodes, 
        int source_node, 
        int sink_node
    ) {
        // 1. [HOST] Build Adjacency List & Saturate Graph
        // We use a vector of vectors to represent the graph temporarily.
        // We also handle duplicate edge inputs by summing capacities.
        std::vector<std::vector<HostEdge>> adj(num_nodes);
        
        // Map to track existing edges to handle duplicates and saturation
        // Key: {u, v}, Value: index in adj[u]
        std::map<std::pair<int, int>, size_t> edge_map;

        for (const auto& e : raw_edges) {
            if (e.u >= num_nodes || e.v >= num_nodes) throw std::runtime_error("Node index out of bounds");
            if (e.u == e.v) continue; // skip self-loops (there should not be any in the first place!)

            std::pair<int, int> forward_key = {e.u, e.v};

            // Add or Update Forward Edge
            if (edge_map.find(forward_key) == edge_map.end()) {
                edge_map[forward_key] = adj[e.u].size();
                adj[e.u].push_back({e.v, e.capacity, -1, 0});
            } else {
                adj[e.u][edge_map[forward_key]].capacity += e.capacity;
            }

            // Ensure Reverse Edge Exists
            std::pair<int, int> backward_key = {e.v, e.u};
            if (edge_map.find(backward_key) == edge_map.end()) {
                edge_map[backward_key] = adj[e.v].size();
                adj[e.v].push_back({e.u, 0, -1, 0}); // 0 capacity for pure reverse edges
            }
        }

        // 2. [HOST] Sort Adjacency Lists
        // Sorting neighbors improves locality and allows binary search for reverse indexing
        for (auto& neighbors : adj) {
            std::sort(neighbors.begin(), neighbors.end(), 
                [](const HostEdge& a, const HostEdge& b) {
                    return a.target < b.target;
                });
        }

        // 3. [HOST] Generate CSR Layouts & Calculate Reverse Indices
        size_t total_edges = 0;
        for (const auto& neighbors : adj) total_edges += neighbors.size();

        // Host-side CSR arrays
        std::vector<int> h_row_map(num_nodes + 1);
        std::vector<int> h_entries(total_edges);
        std::vector<long long> h_capacities(total_edges);
        std::vector<int> h_reverse_map(total_edges);

        int current_edge_idx = 0;
        h_row_map[0] = 0;

        // Flatten pass 1: Fill topology
        for (int u = 0; u < num_nodes; ++u) {
            for (const auto& edge : adj[u]) {
                h_entries[current_edge_idx] = edge.target;
                h_capacities[current_edge_idx] = edge.capacity;
                current_edge_idx++;
            }
            h_row_map[u + 1] = current_edge_idx;
        }

        // Flatten pass 2: Calculate Reverse Edge Index
        // For every edge u -> v at index 'i', we must find v -> u at index 'j'
        for (int u = 0; u < num_nodes; ++u) {
            for (int i = h_row_map[u]; i < h_row_map[u + 1]; ++i) {
                int v = h_entries[i];
                
                // Find 'u' in 'v's adjacency list
                // Since we sorted, we can theoretically use binary search, 
                // or just scan since neighbor lists are usually small.
                // We need the GLOBAL index of that edge.
                
                int v_start = h_row_map[v];
                int v_end = h_row_map[v + 1];
                
                // Optimized search assuming sorted entries
                auto it = std::lower_bound(
                    h_entries.begin() + v_start, 
                    h_entries.begin() + v_end, 
                    u
                );

                if (it != h_entries.begin() + v_end && *it == u) {
                    int j = std::distance(h_entries.begin(), it);
                    h_reverse_map[i] = j;
                } else {
                    throw std::runtime_error("Graph structure corruption: Reverse edge not found.");
                }
            }
        }

        // 4. [DEVICE] Allocate and Populate Graph Struct
        Graph<DeviceType> g;

        // Allocate Views
        g.row_map = typename Graph<DeviceType>::RowMapType("row_map", num_nodes + 1);
        g.entries = typename Graph<DeviceType>::EntriesType("entries", total_edges);
        g.residual_capacity = typename Graph<DeviceType>::ValueViewType("residual_capacity", total_edges);
        g.reverse_edge = typename Graph<DeviceType>::IndexViewType("reverse_edge", total_edges);
        
        g.excess = typename Graph<DeviceType>::ValueViewType("excess", num_nodes);
        g.label = typename Graph<DeviceType>::LabelViewType("label", num_nodes);
        g.new_label = typename Graph<DeviceType>::LabelViewType("new_label", num_nodes);
        g.added_excess = typename Graph<DeviceType>::ValueViewType("added_excess", num_nodes);
        g.active_iteration_mask = typename Graph<DeviceType>::MaskViewType("active_mask", num_nodes);
        
        // Queue management
        g.current_queue_size = Kokkos::View<size_t, DeviceType>("curr_q_size");
        g.next_queue_size = Kokkos::View<size_t, DeviceType>("next_q_size");
        g.current_active = typename Graph<DeviceType>::EntriesType("curr_active", num_nodes); // Worse case size
        g.next_active = typename Graph<DeviceType>::EntriesType("next_active", num_nodes);

        // Copy Data to Device
        // We use create_mirror_view_and_copy for convenience. 
        // Note: For row_map, entries, reverse_map we can use the vectors directly if layout matches,
        // but explicit copy is safer for portability (e.g. CudaUVM vs Host).
        
        HostToDeviceCopy(g.row_map, h_row_map);
        HostToDeviceCopy(g.entries, h_entries);
        HostToDeviceCopy(g.residual_capacity, h_capacities);
        HostToDeviceCopy(g.reverse_edge, h_reverse_map);

        // Initialize Node State
        Kokkos::deep_copy(g.excess, 0);
        Kokkos::deep_copy(g.label, 0);
        Kokkos::deep_copy(g.new_label, 0);
        Kokkos::deep_copy(g.added_excess, 0);
        Kokkos::deep_copy(g.active_iteration_mask, 0);
        Kokkos::deep_copy(g.current_queue_size, 0);
        Kokkos::deep_copy(g.next_queue_size, 0);

        return g;
    }

private:
    // Helper to copy std::vector to Kokkos View
    template <typename ViewType, typename T>
    static void HostToDeviceCopy(ViewType& view, const std::vector<T>& data) {
        // Create a temporary host view wrapping the raw vector data
        // This avoids one allocation on the host side
        Kokkos::View<const T*, Kokkos::HostSpace, Kokkos::MemoryTraits<Kokkos::Unmanaged>> 
            host_view(data.data(), data.size());
        Kokkos::deep_copy(view, host_view);
    }
};

#endif // GRAPH_BUILDER_HPP