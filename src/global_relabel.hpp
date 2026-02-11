#ifndef GLOBAL_RELABEL_HPP
#define GLOBAL_RELABEL_HPP

#include <Kokkos_Core.hpp>
#include "graph.hpp"

// Global relabel 
// (reverse BFS from sink using 2 queue swapping)
template <class DeviceType>
struct GlobalRelabel {

    using ExecutionSpace = typename DeviceType::execution_space;
    using RangePolicy = Kokkos::RangePolicy<ExecutionSpace>;

    static void run(Graph<DeviceType> &g, int t, int n)
    {
        
        Kokkos::deep_copy(g.new_label, 0);
        Kokkos::deep_copy(g.next_queue_size, 0);

        // Fused Init: Reset Labels to 'inf' (~ anything >= n) and setup sink at once
        Kokkos::parallel_for("GlobalRelabel_Fused_Init", RangePolicy(0, n), KOKKOS_LAMBDA(const int v) {
            // this diverges warp t is in, but thats fine 
            if (v == t) {
                g.label(v) = 0;
                g.current_active(0) = t;
                g.current_queue_size() = 1; 
            } else {
                // inf
                g.label(v) = n+10;
            }
        });
        Kokkos::fence();

        // reverse BFS from t
        // AT MOST n iterations, because worst case
        // (something like giant linked list)
        // would mean most distant node is n edges
        // away from t
        for (int dist = 0; dist < n; ++dist)
        {
            size_t current_q_size = 0;
            Kokkos::deep_copy(current_q_size, g.current_queue_size);

            if (current_q_size == 0) break;

            Kokkos::parallel_for("GlobalRelabel_BFS", RangePolicy(0, current_q_size), KOKKOS_LAMBDA(const int q_idx) {
                
                // u is visited node
                int u = g.current_active(q_idx); 
                int start = g.row_map(u);
                int end   = g.row_map(u + 1);

                for (int i = start; i < end; ++i) {
                    int rev_idx = g.reverse_edge(i);
                    int v = g.entries(i); 

                    // If we can push flow v -> u
                    if (g.residual_capacity(rev_idx) > 0) {
                        int expected = n+10;
                        // If v is unvisited, set label and enqueue
                        if (Kokkos::atomic_compare_exchange(&g.label(v), expected, dist + 1) == expected) {
                            int pos = Kokkos::atomic_fetch_add(&g.next_queue_size(), 1);
                            g.next_active(pos) = v;
                        }
                    }
                } 
            });
            Kokkos::fence();

            // Swap for next BFS level
            std::swap(g.current_active, g.next_active);
            std::swap(g.current_queue_size, g.next_queue_size);
            Kokkos::deep_copy(g.next_queue_size, 0);
        }
    }

    // Helper to rebuild active queue after Global Relabel
    // dpending on how much memory we got to spare,
    // we could just have 2 additional queues for relabel and not do this
    // (might very well be worth it, unless graph is very big)
    static void rebuild_active_queue(Graph<DeviceType> &g, int s, int t, int n) {
        Kokkos::deep_copy(g.current_queue_size, 0);
        
        Kokkos::parallel_for("Rebuild_Active_Set", RangePolicy(0, n), KOKKOS_LAMBDA(const int v) {
            // anything but s/t with excess is active
            if (v != s && v != t && g.excess(v) > 0) {
                int pos = Kokkos::atomic_fetch_add(&g.current_queue_size(), 1);
                g.current_active(pos) = v;
            } 
        });
        Kokkos::fence();
    }
};

#endif