#ifndef DEBUG_VERIFIER_HPP
#define DEBUG_VERIFIER_HPP

#include <Kokkos_Core.hpp>
#include <iostream>
#include "../graph.hpp"

template <class DeviceType>
struct Verifier
{
    using ExecutionSpace = typename DeviceType::execution_space;
    using RangePolicy = Kokkos::RangePolicy<ExecutionSpace>;

    // Conservation of Flow
    // Ensures all intermediate nodes have exactly 0 excess.
    static bool check_excess_drained(Graph<DeviceType> &g, int s, int t, int n)
    {
        int undrained_nodes = 0;

        Kokkos::parallel_reduce(
            "Verify_Drain",
            RangePolicy(0, n),
            KOKKOS_LAMBDA(const int u, int &count) {
                // Ignore s and t (they are allowed to have excess/deficit)
                if (u != s && u != t) {
                    if (g.excess(u) != 0) {
                        count++;
                    }
                }
            },
            undrained_nodes);

        Kokkos::fence();

        if (undrained_nodes > 0) {
            std::cout << "[FAIL] Conservation Check: " << undrained_nodes 
                      << " nodes still have excess (flow did not drain).\n";
            return false;
        }
        std::cout << "[PASS] Conservation Check: All nodes drained.\n";
        return true;
    }

    // Optimality (The Min-Cut Condition)
    // Run a BFS from S. If we can reach T, the flow is NOT maximum.
    static bool check_optimality(Graph<DeviceType> &g, int s, int t, int n)
    {
        using BoolView = Kokkos::View<bool*, DeviceType>;
        BoolView visited("visited_mask", n);
        
        // Init S
        Kokkos::parallel_for("Init_BFS", 1, KOKKOS_LAMBDA(const int) {
            visited(s) = true;
        });

        int frontier_size = 1;
        while (frontier_size > 0) {

            int changes = 0;
            Kokkos::parallel_reduce("BFS_Relax", RangePolicy(0, n), 
                KOKKOS_LAMBDA(const int u, int& l_change) {
                if (visited(u)) {
                    int start = g.row_map(u);
                    int end = g.row_map(u+1);
                    for(int i=start; i<end; ++i) {
                        int v = g.entries(i);
                        // If edge has capacity AND v not visited
                        if (g.residual_capacity(i) > 0 && !visited(v)) {
                            visited(v) = true;
                            l_change = 1;
                        }
                    }
                }
            }, changes);
            
            if (changes == 0) break; // converged
        }
        Kokkos::fence();

        // Check if T was visited
        bool t_vis = false;
        Kokkos::parallel_reduce("Check_T", 1, KOKKOS_LAMBDA(const int, bool& l_res){
            l_res = visited(t);
        }, Kokkos::LOr<bool>(t_vis));

        if (t_vis) {
            std::cout << "[FAIL] Optimality Check: Path s->t exists in residual graph!\n";
            return false;
        }
        std::cout << "[PASS] Optimality Check: No path s->t. Min-cut found.\n";
        return true;
    }

    // Run all
    static void validate(Graph<DeviceType> &g, int s, int t, int n) {
        std::cout << "--- STARTING FINAL VERIFICATION ---\n";
        bool p1 = check_excess_drained(g, s, t, n);
        
        long long f_s=0, f_t=0;
        bool p2 = check_source_sink_balance(g, s, t, f_s, f_t);
        
        bool p3 = check_optimality(g, s, t, n);

        if (p1 && p2 && p3) {
            std::cout << ">>> VERIFICATION SUCCESSFUL. Result is valid.\n";
        } else {
            std::cout << ">>> VERIFICATION FAILED.\n";
        }
        std::cout << "-----------------------------------\n";
    }
};

#endif