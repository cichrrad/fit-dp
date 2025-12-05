#include <Kokkos_Core.hpp>
#include <vector>
#include <iostream>
#include <iomanip>
#include <map>
#include <algorithm>

#include "src/graph.hpp"
#include "src/graph_builder.hpp"
#include "src/initialize_algorithm.hpp"
#include "src/preprocessing/csv_loader.hpp"

int main(int argc, char *argv[])
{
    int N;
    const auto raw_edges = parse_csv("./input/mock/mock_graph.csv", N);
    int s = 0;
    int t = N - 1;

    std::cout << "Parsed graph with " << N << " vertices. Source is " << s << " and sink is " << t << ".\n";

    Kokkos::initialize(argc, argv);
    {
        using Device = Kokkos::DefaultExecutionSpace;
        std::cout << "\nKokkos initialized on: " << typeid(Device).name() << "\n";

        auto g = GraphBuilder::build_graph<Device>(raw_edges, N, s, t);
        initialize_algorithm(g, s, t, N);

        // Mirrors for Node Properties (Excess, Label) - These are safe to keep persistent
        // because we only ever copy Device -> Host.
        auto h_excess = Kokkos::create_mirror_view(g.excess);
        auto h_label = Kokkos::create_mirror_view(g.label);
        auto h_added_excess = Kokkos::create_mirror_view(g.added_excess); 
        auto h_new_label = Kokkos::create_mirror_view(g.new_label);
        
        // Static structure mirrors
        auto h_row_map = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), g.row_map);
        auto h_entries = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), g.entries);
        auto h_residual = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), g.residual_capacity);

        // [DEBUG] Print Graph Structure
        std::cout << "\n=== [GRAPH ADJACENCY CHECK] ===\n";
        for (int u = 0; u < N; ++u) {
            std::cout << "Node " << u << " Neighbors: ";
            for (int i = h_row_map(u); i < h_row_map(u + 1); ++i) {
                int v = h_entries(i);
                std::cout << v << " (Res " << h_residual(i) << ") ";
            }
            std::cout << "\n";
        }

        auto print_state = [&](const char* phase_name, size_t q_size, bool show_pending = false) {
            std::cout << "\n=== [" << phase_name << "] ===\n";
            Kokkos::deep_copy(h_excess, g.excess);
            Kokkos::deep_copy(h_label, g.label);
            
            // Create a temporary snapshot of the CURRENT active queue
            // This ensures we don't accidentally overwrite the 'next' buffer if they alias in HostSpace
            // (such as OPENMP)
            auto h_active_snapshot = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), g.current_active);

            if (show_pending) {
                Kokkos::deep_copy(h_added_excess, g.added_excess);
                Kokkos::deep_copy(h_new_label, g.new_label);
            }

            std::cout << "Active Queue (" << q_size << "): [ ";
            for (size_t i = 0; i < q_size; ++i) std::cout << h_active_snapshot(i) << " ";
            std::cout << "]\n";

            std::cout << " Node | Label | Excess ";
            if (show_pending) std::cout << "| +Buf | NewL ";
            std::cout << "\n------+-------+--------";
            if (show_pending) std::cout << "+------+------";
            std::cout << "\n";

            for (int i = 0; i < N; ++i) {
                std::cout << std::setw(5) << i << " | "
                          << std::setw(5) << h_label(i) << " | "
                          << std::setw(6) << h_excess(i) << " ";
                if (show_pending) {
                    std::cout << "| " << std::setw(4) << h_added_excess(i) << " | "
                              << std::setw(4) << h_new_label(i) << " ";
                }
                if (i == s) std::cout << "(S)";
                if (i == t) std::cout << "(T)";
                std::cout << "\n";
            }
        };

        // [HOST] Control Variables
        int iteration = 1; 
        size_t h_current_q_size = 0;
        Kokkos::deep_copy(h_current_q_size, g.current_queue_size);

        std::cout << "\n[STARTING ALGORITHM] Initial Active Nodes: " << h_current_q_size << "\n";
        print_state("INITIAL STATE", h_current_q_size);

        while (h_current_q_size > 0) {
            
            std::cout << "\n--- Iteration " << iteration << " ---\n";

            // 1. PROCESS PHASE
            long long step_work = 0;
            int next_iter_mask = iteration + 1;

            Kokkos::parallel_reduce(
                "process_kernel",
                Kokkos::RangePolicy<Device>(0, h_current_q_size),
                KOKKOS_LAMBDA(const int& i, long long& l_work) {
                    int u = g.current_active(i);
                    long long e_u = g.excess(u);
                    int d_u = g.label(u);
                    int min_d_neighbor = 2 * g.num_nodes(); 
                    
                    int row_start = g.row_map(u);
                    int row_end   = g.row_map(u + 1);
                    l_work += (row_end - row_start);

                    for (int idx = row_start; idx < row_end; ++idx) {
                        if (e_u == 0) break; 
                        int v = g.entries(idx);
                        long long cap = g.residual_capacity(idx);
                        
                        if (cap > 0) {
                            int d_v = g.label(v);
                            if (d_v < min_d_neighbor) min_d_neighbor = d_v;
                            
                            if (d_u == d_v + 1) {
                                // Admissible
                                long long delta = (e_u < cap) ? e_u : cap;
                                
                                g.residual_capacity(idx) -= delta;
                                int rev_idx = g.reverse_edge(idx);
                                g.residual_capacity(rev_idx) += delta;
                                e_u -= delta;
                                Kokkos::atomic_add(&g.added_excess(v), delta);
                                
                                // Wavefront Logic
                                if (v != s && v != t) {
                                    int seen_mask = Kokkos::atomic_exchange(&g.active_iteration_mask(v), next_iter_mask);
                                    if (seen_mask != next_iter_mask) {
                                        size_t insert_pos = Kokkos::atomic_fetch_add(&g.next_queue_size(), 1);
                                        g.next_active(insert_pos) = v;
                                        printf("  QUEUE: Added %d to next_active at pos %lu\n", v, insert_pos);
                                    }
                                }
                            }
                        }
                    } 
                    
                    // Relabel Logic
                    if (e_u > 0) {
                        int new_d = min_d_neighbor + 1;
                        if (new_d < g.num_nodes() * 2 && new_d > d_u) {
                            g.new_label(u) = new_d;
                        }
                        int seen_mask = Kokkos::atomic_exchange(&g.active_iteration_mask(u), next_iter_mask);
                        if (seen_mask != next_iter_mask) {
                            size_t insert_pos = Kokkos::atomic_fetch_add(&g.next_queue_size(), 1);
                            g.next_active(insert_pos) = u;
                            printf("  QUEUE: Re-added self %d to next_active at pos %lu\n", u, insert_pos);
                        }
                    }
                    g.excess(u) = e_u;

                }, step_work);
            
            Kokkos::fence();

            // Check Next Queue
            size_t h_next_q_size = 0;
            Kokkos::deep_copy(h_next_q_size, g.next_queue_size);
            
            print_state("POST-PROCESS (Pending Updates)", h_current_q_size, true);
            std::cout << "Next Queue Size will be: " << h_next_q_size << "\n";

            // 3. APPLY PHASE
            if (h_next_q_size > 0) {
                Kokkos::parallel_for(
                    "apply_kernel",
                    Kokkos::RangePolicy<Device>(0, h_next_q_size),
                    KOKKOS_LAMBDA(const int& i) {
                        int u = g.next_active(i);
                        long long incoming = g.added_excess(u);
                        if (incoming > 0) {
                            g.excess(u) += incoming;
                            g.added_excess(u) = 0; 
                        }
                        int d_proposed = g.new_label(u);
                        int d_current  = g.label(u);
                        if (d_proposed > d_current) {
                            g.label(u) = d_proposed;
                            g.new_label(u) = 0; 
                        }
                    });
                Kokkos::fence();
            }

            // 4. QUEUE SWAP
            std::swap(g.current_active, g.next_active);
            std::swap(g.current_queue_size, g.next_queue_size);
            Kokkos::deep_copy(g.next_queue_size, 0);

            // [DEBUG] Double Check - Print the NEW current queue (which was next)
            print_state("POST-APPLY (State Committed - Ready for Next)", h_next_q_size);

            h_current_q_size = h_next_q_size;
            iteration++;
        }

        std::cout << "\n[FINISHED] Total Iterations: " << iteration << "\n";
    }
    Kokkos::finalize();
    std::cout << "Goodbye!";
}