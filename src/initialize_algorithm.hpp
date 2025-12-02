#ifndef INITIALIZE_ALGORITHM_HPP
#define INITIALIZE_ALGORITHM_HPP

#include <Kokkos_Core.hpp>
#include "graph.hpp"

template <class DeviceType>
void initialize_algorithm(Graph<DeviceType>& g, int s, int t, int n) {
    
    using ExecutionSpace = typename DeviceType::execution_space;
    using RangePolicy = Kokkos::RangePolicy<ExecutionSpace>;

    // Set Initial Labels: d(s) = n
    // NOTE: -- this might be better to do before moving onto the device
    // to not have to launch kernel
    Kokkos::parallel_for("Init_Source_Label", RangePolicy(0, 1), KOKKOS_LAMBDA(const int&) {
        g.label(s) = n;
        // g.label(t) = 0; // Already 0 from builder
    });

    // Saturate Source Edges
    Kokkos::parallel_for("Saturate_Source", RangePolicy(0, 1), KOKKOS_LAMBDA(const int&) {
        int start = g.row_map(s);
        int end = g.row_map(s+1);
        
        int queue_idx = 0; // Local counter for this thread

        for (int i = start; i < end; ++i) {
            int v = g.entries(i);
            long long cap = g.residual_capacity(i);

            if (cap > 0) {
                // Push Flow: s -> v
                // Residual capacity s->v becomes 0
                g.residual_capacity(i) = 0;

                // Update Reverse: v -> s
                // Residual capacity v->s increases by 'cap'
                int rev_idx = g.reverse_edge(i);
                g.residual_capacity(rev_idx) += cap; 
                // Note: No atomic needed for residual here, only 's' touches 's->v' 
                // and 'v->s' is owned by 'v' which is inactive.

                // Update Excess
                // s loses excess (doesn't matter much for s, but good accounting)
                // v gains excess
                // Direct update to v is safe because v is only a neighbor of s once (simple graph)
                // or if multi-edge, this thread processes sequentially.
                g.excess(v) += cap; 
                g.excess(s) -= cap;

                // Activate Neighbor 'v'
                // We add v to 'current_active' because it now has excess > 0.
                // We don't need to check existing excess because everyone started at 0.
                if (v != s && v != t) {
                    // Add to queue
                    int q_pos = Kokkos::atomic_fetch_add(&g.current_queue_size(), 1);
                    g.current_active(q_pos) = v;
                    
                    // Mark as active in the mask for iteration 0
                    // Using iteration 1 ensures it's seen as "active in current round"
                    g.active_iteration_mask(v) = 1; 
                }
            }
        }
    });
    
    Kokkos::fence();
}

#endif