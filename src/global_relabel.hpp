#ifndef GLOBAL_RELABEL_HPP
#define GLOBAL_RELABEL_HPP

#include <Kokkos_Core.hpp>
#include "graph.hpp"

template <class DeviceType>
void global_relabel(Graph<DeviceType> &g, int t, int n)
{
   using ExecutionSpace = typename DeviceType::execution_space;
    using RangePolicy = Kokkos::RangePolicy<ExecutionSpace>;

    Kokkos::deep_copy(g.next_queue_size, 0);
    // Kernel Fusion
    // Reset, Sink Setup, and Queue Init merged into one kernel.
    // technically, this is bad because id "diverges" the 1 warp (32 threads) t is in
    // BUT if we were to do it with no divergence, we would need separate kernel launch
    // which is HUGE overhead 
    Kokkos::parallel_for("GlobalRelabel_Fused_Init", RangePolicy(0, n), KOKKOS_LAMBDA(const int v) {
        if (v == t) {
            // This is the Sink
            g.label(v) = 0;
            
            // Initialize BFS Queue with Sink
            g.current_active(0) = t;
            g.current_queue_size() = 1; 
        } else {
            // This is a normal node
            g.label(v) = n; 
        }
    });

    // Fence to ensure the queue is ready before the Host reads size in the loop
    Kokkos::fence();

    // 3. BFS Loop
    //    Run until queue is empty or we exceed N (in N steps we must reach all N nodes...duh)
    for (int dist = 0; dist < n; ++dist)
    {

        // Get host copy of queue size to check termination
        size_t current_q_size = 0;
        // TODO -- is this bad???
        // worst case is N copy operations, if device is not same as host
        Kokkos::deep_copy(current_q_size, g.current_queue_size);

        if (current_q_size == 0)
            break;

        Kokkos::parallel_for("GlobalRelabel_BFS_Expand", RangePolicy(0, current_q_size), KOKKOS_LAMBDA(const int q_idx) {
            int u = g.current_active(q_idx); // u is current node
            
            int start = g.row_map(u);
            int end = g.row_map(u + 1);

            for (int i = start; i < end; ++i) {
                int v = g.entries(i); // v is neighbor of u
                
                // We are looking for edges v -> u that have residual capacity.
                // 'i' is u -> v.
                // The reverse edge index connects v -> u.
                int rev_idx = g.reverse_edge(i);
                
                // Check if flow can push from v to u
                if (g.residual_capacity(rev_idx) > 0) {
                    
                    // Check if v is unvisited (label is n ~ INF)
                    // atomic_compare_exchange to ensure we only update/add once
                    int expected_label = n;
                    int new_label_val = dist + 1;
                    
                    // Try to set label from 'n' to 'dist + 1'
                    if (Kokkos::atomic_compare_exchange(&g.label(v), expected_label, new_label_val) == expected_label) {
                        int next_pos = Kokkos::atomic_fetch_add(&g.next_queue_size(), 1);
                        // add it to next active, as we will be iterating its edges
                        // next iteration
                        g.next_active(next_pos) = v;
                    }
                }
            } });

        Kokkos::fence();

        // Swap Queues
        // We reuse the pointers in the Graph struct
        // this forces additional pass after global relabel
        // but utilizes memory better
        std::swap(g.current_active, g.next_active);
        std::swap(g.current_queue_size, g.next_queue_size);

        // Reset next queue counter for next iteration
        Kokkos::deep_copy(g.next_queue_size, 0);
    }
}

#endif