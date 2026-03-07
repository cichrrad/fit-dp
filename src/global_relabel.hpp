#ifndef GLOBAL_RELABEL_HPP
#define GLOBAL_RELABEL_HPP

#include <Kokkos_Core.hpp>
#include "graph.hpp"

template <class DeviceType>
void global_relabel(Graph<DeviceType> &g, int s, int t, int n)
{
    using ExecutionSpace = typename DeviceType::execution_space;
    using RangePolicy = Kokkos::RangePolicy<ExecutionSpace>;

    Kokkos::deep_copy(g.next_queue_size, 0);
    // Kernel Fusion
    // Reset, Sink Setup, and Queue Init merged into one kernel.
    // technically, this is bad because id "diverges" the 1-2 warp(s) t and s is in
    // BUT if we were to do it with no divergence, we would need separate kernel launch
    // which is HUGE overhead
    Kokkos::parallel_for("GlobalRelabel_Fused_Init", RangePolicy(0, n), KOKKOS_LAMBDA(const int v) {
        if (v == t) {
            // This is the Sink
            g.label(v) = 0;
            
            // Initialize BFS Queue with Sink
            g.current_active(0) = t;
            g.current_queue_size() = 1;
        }
        else if(v == s){
            g.label(v) = n;
        } 
        else {
            // This is a normal node
            g.label(v) = 2*n; 
        } });

    // Fence to ensure the queue is ready before the Host reads size in the loop
    Kokkos::fence();

    // 3. BFS Loop
    //    Run until queue is empty or we exceed N (in N steps we must reach all N nodes...duh)
    size_t host_current_q_size = 1;

    for (int dist = 0; dist < n; ++dist)
    {
        // Break condition evaluated purely on the host!
        if (host_current_q_size == 0)
            break;

        size_t host_next_q_size = 0; // Host variable to receive the reduction result

        Kokkos::parallel_reduce("GlobalRelabel_BFS_Expand", RangePolicy(0, host_current_q_size), KOKKOS_LAMBDA(const int q_idx, size_t &l_added) { // l_added is the thread-local accumulator
            int u = g.current_active(q_idx);
            int start = g.row_map(u);
            int end = g.row_map(u + 1);

            for (int i = start; i < end; ++i)
            {
                int v = g.entries(i);

                if (v == s)
                {
                    continue;
                }
                int rev_idx = g.reverse_edge(i);

                if (g.residual_capacity(rev_idx) > 0)
                {

                    int expected_label = 2 * n;
                    int new_label_val = dist + 1;

                    // TODO: -- try to redo so that atomic contention is not an issue
                    if (Kokkos::atomic_compare_exchange(&g.label(v), expected_label, new_label_val) == expected_label)
                    {
                        int next_pos = Kokkos::atomic_fetch_add(&g.next_queue_size(), 1);
                        g.next_active(next_pos) = v;

                        l_added++;
                    }
                }
            }
        },
                                host_next_q_size); // Reduce directly into host_next_q_size

        Kokkos::fence(); // Ensure kernel and reduction are complete

        host_current_q_size = host_next_q_size;

        // Swap Queues
        std::swap(g.current_active, g.next_active);
        std::swap(g.current_queue_size, g.next_queue_size);

        // We still need to zero out the device counter so the atomics start at 0 next iteration.
        Kokkos::deep_copy(g.next_queue_size, 0);
    }
}

#endif