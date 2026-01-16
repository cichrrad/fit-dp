#ifndef GLOBAL_RELABEL_HPP
#define GLOBAL_RELABEL_HPP

#include <Kokkos_Core.hpp>
#include "graph.hpp"

template <class DeviceType>
void global_relabel(Graph<DeviceType>& g, int t, int n) {
    using ExecutionSpace = typename DeviceType::execution_space;
    using RangePolicy = Kokkos::RangePolicy<ExecutionSpace>;

    // 1. Reset all labels to 'n' (infinity) and handle the sink 't'.
    //    We effectively use 'n' as the "unvisited" marker.
    Kokkos::parallel_for("GlobalRelabel_Reset", RangePolicy(0, n), KOKKOS_LAMBDA(const int v) {
        g.label(v) = n; // Set to unreachable
    });
    
    // Explicitly set sink distance to 0
    Kokkos::parallel_for("GlobalRelabel_SetSink", RangePolicy(0, 1), KOKKOS_LAMBDA(const int) {
        g.label(t) = 0;
    });

    // 2. Initialize BFS Queue
    //    We can reuse the graph's queue views if we are careful, or just use them 
    //    as temporary scratch space since this step is synchronous and blocking.
    //    Let's clear the queue counters first.
    Kokkos::deep_copy(g.current_queue_size, 0);
    Kokkos::deep_copy(g.next_queue_size, 0);

    // Add sink 't' to current_active (acting as BFS queue)
    Kokkos::parallel_for("GlobalRelabel_InitQueue", RangePolicy(0, 1), KOKKOS_LAMBDA(const int) {
        g.current_active(0) = t;
        g.current_queue_size() = 1;
    });
    Kokkos::fence();

    // 3. BFS Loop
    //    Run until queue is empty or we exceed N (sanity check)
    //    'dist' tracks the current distance from sink
    for (int dist = 0; dist < n; ++dist) {
        
        // Get host copy of queue size to check termination
        size_t current_q_size = 0;
        Kokkos::deep_copy(current_q_size, g.current_queue_size);

        if (current_q_size == 0) break;

        Kokkos::parallel_for("GlobalRelabel_BFS_Expand", RangePolicy(0, current_q_size), KOKKOS_LAMBDA(const int q_idx) {
            int u = g.current_active(q_idx); // u is current node
            
            int start = g.row_map(u);
            int end = g.row_map(u + 1);

            for (int i = start; i < end; ++i) {
                int v = g.entries(i); // v is neighbor of u
                
                // We are looking for edges v -> u that have residual capacity.
                // In our graph, 'i' is u -> v.
                // The reverse edge index connects v -> u.
                int rev_idx = g.reverse_edge(i);
                
                // Check if flow can push from v to u (backward BFS direction)
                if (g.residual_capacity(rev_idx) > 0) {
                    
                    // Check if v is unvisited (label is n)
                    // We use atomic_compare_exchange to ensure we only update/add once
                    int expected_label = n;
                    int new_label_val = dist + 1;
                    
                    // Try to set label from 'n' to 'dist + 1'
                    if (Kokkos::atomic_compare_exchange(&g.label(v), expected_label, new_label_val) == expected_label) {
                        // Success: We claimed v. Add to next queue.
                        int next_pos = Kokkos::atomic_fetch_add(&g.next_queue_size(), 1);
                        g.next_active(next_pos) = v;
                    }
                }
            }
        });
        
        Kokkos::fence();

        // Swap Queues
        // We reuse the pointers in the Graph struct temporarily
        // Note: This does not affect the main algorithm state because Global Relabel
        // completely refreshes the active sets anyway.
        std::swap(g.current_active, g.next_active);
        std::swap(g.current_queue_size, g.next_queue_size);
        
        // Reset next queue counter for next iteration
        Kokkos::deep_copy(g.next_queue_size, 0);
    }
}

#endif