#ifndef INITIALIZE_ALGORITHM_HPP
#define INITIALIZE_ALGORITHM_HPP

#include <Kokkos_Core.hpp>
#include "graph.hpp"

template <class DeviceType>
void initialize_algorithm(Graph<DeviceType> &g, int s, int t, int n, int C = 1)
{
    using ExecutionSpace = typename DeviceType::execution_space;
    using RangePolicy = Kokkos::RangePolicy<ExecutionSpace>;
    using ValueType = typename Graph<DeviceType>::ValueType;
    auto pseudo_warp_size = Kokkos::TeamPolicy<DeviceType>::vector_length_max();

    int s_row_start, s_row_end;
    {
        auto s_map_subview = Kokkos::subview(g.row_map, std::make_pair(s, s + 2));
        auto h_s_map = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), s_map_subview);
        s_row_start = h_s_map(0);
        s_row_end = h_s_map(1);
    }

    Kokkos::deep_copy(Kokkos::subview(g.label, s), n);

    // Saturate Edges
    long long total_pushed_from_s = 0;

    Kokkos::parallel_reduce(
        "Saturate_Source_Edges",
        RangePolicy(s_row_start, s_row_end),
        KOKKOS_LAMBDA(const int &edge_idx, long long &local_pushed_flow) {
            // Get edge target and capacity
            int v = g.entries(edge_idx);
            long long cap = g.residual_capacity(edge_idx);

            // Technically not needed, as each edge is managed by 1 thread
            // and hould have capacity as we just
            // loaded the graph
            if (cap > 0)
            {
                // Push Flow
                g.residual_capacity(edge_idx) = 0;

                // Update Reverse: v -> s
                int rev_idx = g.reverse_edge(edge_idx);
                g.residual_capacity(rev_idx) += cap;
                // Update Excess at V
                // NOTE: -- this can be done without atomics
                // ONLY because we merged all edges u -> v into one
                // in graph building process
                // (else multiple threads might touch v)
                g.excess(v) += cap;

                if (v != s && v != t)
                {
                    // Add to current queue
                    g.active_iteration_mask(v) = 1;
                    g.active_phase(v) = 1;

                    // THIS IS FOR THE FUTURE EDGE-PARALLEL SHIFT
                    int row_start = g.row_map(v);
                    int row_end = g.row_map(v + 1);
                    if (row_end - row_start > pseudo_warp_size * C)
                    {
                        size_t qh_pos = Kokkos::atomic_fetch_add(&g.current_high_size(), 1);
                        g.current_high(qh_pos) = v;
                    }
                    else
                    {
                        size_t ql_pos = Kokkos::atomic_fetch_add(&g.current_low_size(), 1);
                        g.current_low(ql_pos) = v;
                    }
                    // Mark as active in iteration mask for start of algo
                }

                // Accumulate total flow pushed for the reduction
                local_pushed_flow += cap;
            }
        },
        total_pushed_from_s);

    Kokkos::deep_copy(Kokkos::subview(g.excess, s), -total_pushed_from_s);

    Kokkos::fence();
}

#endif