#ifndef GRAPH_BUILDER_HPP
#define GRAPH_BUILDER_HPP

#include <Kokkos_Core.hpp>
#include <Kokkos_Sort.hpp> 
#include <vector>
#include <stdexcept>
#include <iostream>

#include "graph.hpp"
#include "preprocessing/graph_defs.hpp"

class GraphBuilder {
public:
    template <class DeviceType>
    static Graph<DeviceType> build_graph(
        const HostEdgeList& h_raw_edges,
        int num_nodes, 
        int source_node, 
        int sink_node
    ) {
        using ExecutionSpace = typename DeviceType::execution_space;
        using RangePolicy = Kokkos::RangePolicy<ExecutionSpace>;
        using TempView = Kokkos::View<TempEdge*, DeviceType>;
        using IntView = Kokkos::View<int*, DeviceType>;

        size_t input_size = h_raw_edges.extent(0);
        if (input_size == 0) throw std::runtime_error("Graph is empty.");
        
        // [HOST -> DEVICE]
        // Allocate device memory -- 2x the # of edges for residuals
        size_t potential_size = input_size * 2;
        TempView raw_edges(Kokkos::ViewAllocateWithoutInitializing("raw_edges_gpu"), potential_size);

        // copy the first half over
        Kokkos::deep_copy(
            Kokkos::subview(raw_edges, std::make_pair((size_t)0, input_size)), 
            h_raw_edges
        );

        // [DEVICE] Graph Construction
        // Symmetrize -- add inverse of each edge (for i at i + input_size --> filling second half)
        Kokkos::parallel_for("Generate_Reverse_Edges", RangePolicy(0, input_size), 
            KOKKOS_LAMBDA(const size_t i) {
                TempEdge e = raw_edges(i);
                raw_edges(i + input_size) = { e.v, e.u, 0 };
        });

        // Sort
        Kokkos::sort(raw_edges);

        // Deduplicate & Compress
        IntView flags(Kokkos::ViewAllocateWithoutInitializing("edge_flags"), potential_size);
        Kokkos::parallel_for("Mark_Unique", RangePolicy(0, potential_size), 
            KOKKOS_LAMBDA(const size_t i) {
                if (i == 0) {
                    flags(i) = 1;
                } else {
                    flags(i) = (raw_edges(i) != raw_edges(i - 1)) ? 1 : 0;
                }
        });

        IntView write_indices(Kokkos::ViewAllocateWithoutInitializing("write_indices"), potential_size);
        
        // inclusive scan, so that duplicates have same idx
        Kokkos::parallel_scan("Scan_Indices", RangePolicy(0, potential_size), 
            KOKKOS_LAMBDA(const size_t i, int& update, const bool final) {
                update += flags(i);
                if (final) {
                    write_indices(i) = update - 1; // Convert to 0-based index
                }
        });

        int total_edges = 0;
        int last_idx = potential_size - 1;
        Kokkos::deep_copy(total_edges, Kokkos::subview(write_indices, last_idx));
        total_edges += 1; // 0-based index to count

        // Allocate fields
        Graph<DeviceType> g;
        g.row_map = typename Graph<DeviceType>::RowMapType("row_map", num_nodes + 1);
        g.entries = typename Graph<DeviceType>::EntriesType("entries", total_edges);
        g.residual_capacity = typename Graph<DeviceType>::ValueViewType("residual", total_edges);
        g.reverse_edge = typename Graph<DeviceType>::IndexViewType("reverse_edge", total_edges);
        
        g.excess = typename Graph<DeviceType>::ValueViewType("excess", num_nodes);
        g.label = typename Graph<DeviceType>::LabelViewType("label", num_nodes);
        g.new_label = typename Graph<DeviceType>::LabelViewType("new_label", num_nodes);
        g.added_excess = typename Graph<DeviceType>::ValueViewType("added_excess", num_nodes);
        g.active_iteration_mask = typename Graph<DeviceType>::MaskViewType("active_mask", num_nodes);
        g.current_queue_size = Kokkos::View<size_t, DeviceType>("curr_q_size");
        g.next_queue_size = Kokkos::View<size_t, DeviceType>("next_q_size");
        g.current_active = typename Graph<DeviceType>::EntriesType("curr_active", num_nodes);
        g.next_active = typename Graph<DeviceType>::EntriesType("next_active", num_nodes);
        g.active_phase = typename Graph<DeviceType>::MaskViewType("active_phase", num_nodes);


        // THIS IS FOR THE FUTURE EDGE-PARALLEL SHIFT
        g.current_low_size = Kokkos::View<size_t, DeviceType>("curr_low_size");
        g.current_high_size = Kokkos::View<size_t, DeviceType>("curr_high_size");
        g.next_high_size =  Kokkos::View<size_t, DeviceType>("next_high_size");
        g.next_low_size =  Kokkos::View<size_t, DeviceType>("next_low_size");

        g.current_low = typename Graph<DeviceType>::EntriesType("curr_low", num_nodes);
        g.current_high = typename Graph<DeviceType>::EntriesType("curr_high", num_nodes);
        g.next_low = typename Graph<DeviceType>::EntriesType("next_lowh", num_nodes);
        g.next_high = typename Graph<DeviceType>::EntriesType("next_high", num_nodes);

        g.gr_current_size = Kokkos::View<size_t, DeviceType>("gr_curr_size");
        g.gr_next_size = Kokkos::View<size_t, DeviceType>("gr_next_size");

        g.gr_current_active = typename Graph<DeviceType>::EntriesType("gr_curr_active", num_nodes);
        g.gr_next_active = typename Graph<DeviceType>::EntriesType("gr_next_active", num_nodes);

        g.current_arc = typename Graph<DeviceType>::MaskViewType("curr_arc", num_nodes);

        IntView compressed_u(Kokkos::ViewAllocateWithoutInitializing("compressed_u"), total_edges);
        Kokkos::deep_copy(g.residual_capacity, 0);

        // Populate
        Kokkos::parallel_for("Populate_CSR_Data", RangePolicy(0, potential_size), 
            KOKKOS_LAMBDA(const size_t i) {
                int idx = write_indices(i); 
                
                // Only FIRST thread of a duplicate group writes the topology
                if (flags(i) == 1) {
                    g.entries(idx) = raw_edges(i).v;
                    compressed_u(idx) = raw_edges(i).u;
                }

                // ALL threads add their capacity
                Kokkos::atomic_add(&g.residual_capacity(idx), raw_edges(i).capacity);
        });

        // Build Row Map
        Kokkos::deep_copy(g.row_map, 0);
        
        Kokkos::parallel_for("Histogram_Degrees", RangePolicy(0, total_edges), 
            KOKKOS_LAMBDA(const size_t i) {
                int u = compressed_u(i);
                if (u < num_nodes) {
                    Kokkos::atomic_inc(&g.row_map(u + 1));
                }
        });

        Kokkos::parallel_scan("Scan_RowMap", RangePolicy(0, num_nodes + 1), 
            KOKKOS_LAMBDA(const size_t i, int& update, const bool final) {
                update += g.row_map(i);
                if (final) g.row_map(i) = update;
        });

        // Link Reverse Edges
        Kokkos::parallel_for("Find_Reverse_Edges", RangePolicy(0, total_edges), 
            KOKKOS_LAMBDA(const size_t i) {
                int u = compressed_u(i);
                int v = g.entries(i);
                
                int start = g.row_map(v);
                int end = g.row_map(v + 1);
                
                int low = start;
                int high = end - 1;
                int rev_idx = -1;
                
                while (low <= high) {
                    int mid = low + (high - low) / 2;
                    int neighbor = g.entries(mid);
                    
                    if (neighbor == u) {
                        rev_idx = mid;
                        break;
                    } else if (neighbor < u) {
                        low = mid + 1;
                    } else {
                        high = mid - 1;
                    }
                }
                g.reverse_edge(i) = rev_idx;
        });

        // Init State
        Kokkos::deep_copy(g.excess, 0);
        Kokkos::deep_copy(g.label, 0);
        Kokkos::deep_copy(g.new_label, 0);
        Kokkos::deep_copy(g.added_excess, 0);
        Kokkos::deep_copy(g.active_iteration_mask, 0);
        Kokkos::deep_copy(g.current_queue_size, 0);
        Kokkos::deep_copy(g.next_queue_size, 0);
        
        Kokkos::deep_copy(g.active_phase, 0);
        
        // THIS IS FOR THE FUTURE EDGE-PARALLEL SHIFT

        Kokkos::deep_copy(g.current_arc, 0);
        Kokkos::deep_copy(g.gr_current_size, 0);
        Kokkos::deep_copy(g.gr_next_size, 0);
        Kokkos::deep_copy(g.next_low_size, 0);
        Kokkos::deep_copy(g.current_low_size, 0);
        Kokkos::deep_copy(g.current_high_size, 0);
        Kokkos::deep_copy(g.next_high_size, 0);

        return g;
    }
};

#endif // GRAPH_BUILDER_HPP