#include <Kokkos_Core.hpp>
#include <vector>
#include <iostream>
#include <iomanip>
#include <map>
#include <algorithm>
#include <chrono>

#include "src/graph.hpp"
#include "src/graph_builder.hpp"
#include "src/initialize_algorithm.hpp"
#include "src/preprocessing/dimacs_par_loader.hpp"
#include "src/global_relabel.hpp"

// #define DEBUG_VERIFY

#ifdef DEBUG_VERIFY
#include "src/debug/debug_verifier.hpp"
#endif

int main(int argc, char *argv[])
{

    std::string graph_path = "./input/mock/generated_graph.dimacs";
    unsigned int tc = 0;
    if (argc > 1)
    {
        graph_path = argv[1];
    }
    if (argc > 2)
    {
        tc = (unsigned int)std::stoi(argv[2]);
    }

    Kokkos::initialize(argc, argv);
    {
        // [TIMER] Start Graph Read
        auto start_io = std::chrono::high_resolution_clock::now();

        int N, s, t;
        const auto raw_edges = parallel_load_dimacs(graph_path, N, s, t, tc);

        // [TIMER] End Graph Read
        auto end_io = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> io_duration = end_io - start_io;

        std::cout << "Loading graph from: " << graph_path << "\n";
        std::cout << "Parsed graph with " << N << " vertices and " << raw_edges.size() << " edges. Source is " << s << " and sink is " << t << ".\n";
        std::cout << ">> IO Time (CSV Read): " << io_duration.count() << " seconds.\n";

        using Device = Kokkos::DefaultExecutionSpace;
        auto pseudo_warp_size = Kokkos::TeamPolicy<Device>::vector_length_max();
        std::cout << "Running on    : " << typeid(Device).name() << "\n";
        std::cout << "\"Warp\" size   : " << pseudo_warp_size << "\n";

        // [TIMER] Start Graph Init
        Kokkos::Timer timer;

        // build graph on device
        auto g = GraphBuilder::build_graph<Device>(raw_edges, N, s, t);

        // kick of the algorithm
        // (push from S, add neighbours to intial queue)
        initialize_algorithm(g, s, t, N);

        // THIS IS FOR THE FUTURE EDGE-PARALLEL SHIFT
        size_t h_current_low_size = 0;
        size_t h_current_high_size = 0;
        Kokkos::deep_copy(h_current_low_size, g.current_low_size);
        Kokkos::deep_copy(h_current_high_size, g.current_high_size);

        std::cout << "--- Initial Queue Routing ---\n";
        std::cout << "Low Queue Size (<= " << pseudo_warp_size << " edges): " << h_current_low_size << "\n";
        std::cout << "High Queue Size (> " << pseudo_warp_size << " edges): " << h_current_high_size << "\n";
        // ==========================================

        // [TIMER] End Graph Init
        // NOTE: -- There is Kokkos::Fence in initialize_algorithm
        // so reading time here is always after its done
        double time_init = timer.seconds();
        std::cout << ">> Graph Build & Init Time: " << time_init << " seconds.\n";

        // [HOST] Variables
        int iteration = 1;
        const long long gr_iter_trigger = 1500;
        long long iterations_since_last_gr = 0;

        // [TIMER] Start Algorithm
        timer.reset();

        // will be: h_current_low_size + h_current_high_size > 0
        while (h_current_low_size + h_current_high_size > 0)
        {

            if (iterations_since_last_gr > gr_iter_trigger)
            {
                global_relabel(g, s, t, N);
                iterations_since_last_gr = 0;
                // reset size
                // THIS IS FOR THE FUTURE EDGE-PARALLEL SHIFT
                Kokkos::deep_copy(g.current_low_size, 0);
                Kokkos::deep_copy(g.current_high_size, 0);

                // global_relabel uses queues, so we need to rebuild the
                // set of active vertices for this iteration
                Kokkos::parallel_for("Rebuild_Active_Set", Kokkos::RangePolicy<Device>(0, N), KOKKOS_LAMBDA(const int v) {
                    
                    // THIS IS FOR THE FUTURE EDGE-PARALLEL SHIFT
                    g.current_arc(v) = 0;
                    
                    if (v != s && v != t && g.excess(v) > 0 && g.label(v) < N) {

                        g.active_iteration_mask(v)=iteration;
                        // THIS IS FOR THE FUTURE EDGE-PARALLEL SHIFT
                        int row_start = g.row_map(v);
                        int row_end = g.row_map(v + 1);
                        if (row_end - row_start > pseudo_warp_size)
                        {
                            size_t qh_pos = Kokkos::atomic_fetch_add(&g.current_high_size(), 1);
                            g.current_high(qh_pos) = v;
                        }
                        else
                        {
                            size_t ql_pos = Kokkos::atomic_fetch_add(&g.current_low_size(), 1);
                            g.current_low(ql_pos) = v;
                        }

                    } });
                Kokkos::fence();

                // THIS IS FOR THE FUTURE EDGE-PARALLEL SHIFT
                Kokkos::deep_copy(h_current_high_size, g.current_high_size);
                Kokkos::deep_copy(h_current_low_size, g.current_low_size);
            }
            int next_iter_mask = iteration + 1;

            // PROCESS =================================================

            // if (iteration % 50 == 0)
            // {
            //     std::cout << "current high queue size is " << h_current_high_size << "\n";
            //     std::cout << "current low queue size is " << h_current_low_size << "\n";
            // }
            // low vertices
            if (h_current_low_size > 0)
            {

                // 1. Define standard 1D RangePolicy
                Kokkos::RangePolicy<Device> policy_low(0, h_current_low_size);

                Kokkos::parallel_for("process_low_kernel", policy_low, KOKKOS_LAMBDA(const int &i) {
                    int u = g.current_low(i);

                    if (u == s || u == t) return;
                
                    long long e_u = g.excess(u);
                    const int d_u_start = g.label(u);
                    int d_u_current = d_u_start;
                
                    int row_start = g.row_map(u);
                    int row_end = g.row_map(u + 1);


                    // Load the saved arc (edge offset)
                    int current_edge_idx = row_start + g.current_arc(u);
                
                    while (e_u > 0) {
                        unsigned long min_d_neighbor = N + 1;
                        int first_skipped_idx = -1;
                        bool skipped_admissible_edge = false;
                    
                        // 2. Resume from current_edge_idx instead of row_start
                        for (int idx = current_edge_idx; idx < row_end; ++idx) {
                            int v = g.entries(idx);
                            long long cap = g.residual_capacity(idx);
                        
                            if (cap > 0) {
                                int d_v = g.label(v);
                                if (d_v < min_d_neighbor) min_d_neighbor = d_v;
                            
                                if (d_u_current == d_v + 1) {
                                    bool wins = true;

                                    if (g.active_phase(v) == iteration) {
                                        wins =  (d_u_start < d_v - 1) || 
                                                (d_u_start == d_v + 1) || 
                                                (d_u_start == d_v && u < v);
                                    }
                                
                                    if (wins) {
                                        long long delta = (e_u < cap) ? e_u : cap;
                                    
                                        g.residual_capacity(idx) -= delta;
                                        g.residual_capacity(g.reverse_edge(idx)) += delta;
                                        e_u -= delta;

                                        // Must be atomic because other threads might push to v simultaneously 
                                        Kokkos::atomic_add(&g.added_excess(v), delta);
                                    
                                        // 3. Dynamic Routing during Enqueue
                                        if (v != s && v != t) {
                                            int seen_mask = Kokkos::atomic_exchange(&g.active_iteration_mask(v), next_iter_mask);
                                            // If we are the thread that activated v 
                                            if (seen_mask != next_iter_mask) {
                                                int degree_v = g.row_map(v + 1) - g.row_map(v);

                                                // Route to the correct queue!
                                                if (degree_v <= pseudo_warp_size) {
                                                    size_t pos = Kokkos::atomic_fetch_add(&g.next_low_size(), 1);
                                                    g.next_low(pos) = v;
                                                } else {
                                                    size_t pos = Kokkos::atomic_fetch_add(&g.next_high_size(), 1);
                                                    g.next_high(pos) = v;
                                                }
                                            }
                                        }
                                    
                                        if (e_u == 0) {
                                            // 4. Save the arc!
                                            // If we skipped an edge earlier, anchor the arc to it.
                                            // Otherwise, anchor it to the current edge where we ran out of excess
                                            current_edge_idx = (first_skipped_idx != -1) ? first_skipped_idx : idx;
                                            break;
                                        }
                                    } else {
                                        skipped_admissible_edge = true;
                                        // Record the VERY FIRST skipped index.
                                        if (first_skipped_idx == -1) {
                                            first_skipped_idx = idx;
                                        }
                                        continue; 
                                    }
                                }
                            }
                        } // end for
                    
                        if (e_u == 0 || skipped_admissible_edge) break;
                    
                        // 5. Relabel
                        int new_d = min_d_neighbor + 1;
                        if (new_d < N + 1 && new_d > d_u_start) {
                            d_u_current = new_d;
                            g.new_label(u) = new_d;

                            // CRITICAL: Reset the arc on relabel!
                            current_edge_idx = row_start;
                            g.current_arc(u) = 0; 
                        } else {
                            break;
                        }
                    }
                
                    // Write back state
                    g.excess(u) = e_u;

                    // Update the global arc tracker if we exited the while loop without relabeling
                    if (d_u_current == d_u_start) {
                        g.current_arc(u) = current_edge_idx - row_start;
                    }
                
                    // 6. Self-Enqueue (u is a low-degree node, so it stays in the low queue)
                    if (e_u > 0 || d_u_current > d_u_start) {
                        int seen_mask = Kokkos::atomic_exchange(&g.active_iteration_mask(u), next_iter_mask);
                        if (seen_mask != next_iter_mask) {
                            size_t pos = Kokkos::atomic_fetch_add(&g.next_low_size(), 1);
                            g.next_low(pos) = u;
                        }
                    } });
                // update work metric
                iterations_since_last_gr++;
                Kokkos::fence("after_process_low_fence");
            }

            // high vertices
            if (h_current_high_size > 0)
            {

                Kokkos::TeamPolicy<Device> policy_high(h_current_high_size, pseudo_warp_size);
                Kokkos::parallel_for("process_high_kernel", policy_high, KOKKOS_LAMBDA(const Kokkos::TeamPolicy<Device>::member_type &team) {
                // League rank maps exactly to the active vertex index
                int u = g.current_high(team.league_rank());
                if (u == s || u == t) return;
                
                // e_u is kept in a local register for EVERY thread in the team.
                // We will update it uniformly so it stays synchronized.
                long long e_u = g.excess(u);
                const int d_u_start = g.label(u);
                int d_u_current = d_u_start;
                
                int row_start = g.row_map(u);
                int row_end = g.row_map(u + 1);
                int degree = row_end - row_start;
                
                // Load the current arc (Thread 0 does it, but we broadcast or just let all threads read it)
                int arc = g.current_arc(u);
                int first_skipped_chunk = -1;
                
                // Loop until discharged or blocked
                while (e_u > 0) {
                    unsigned long local_min_d = N + 1;
                    first_skipped_chunk = -1;
                    int skipped_admissible = 0; 
                
                    // 2. CHUNKING LOGIC
                    // We process edges in jumps of team_size()
                    for (int chunk_start = arc; chunk_start < degree; chunk_start += team.team_size()) {

                        // Each thread grabs an edge
                        int edge_offset = chunk_start + team.team_rank();
                        int idx = row_start + edge_offset;
                        bool valid_edge = (edge_offset < degree);
                    
                        long long cap = 0;
                        int v = -1, d_v = N + 1;
                        bool admissible = false, wins = false;
                    
                        if (valid_edge) {
                            v = g.entries(idx);
                            cap = g.residual_capacity(idx);
                            if (cap > 0) {
                                d_v = g.label(v);
                                if (d_u_current == d_v + 1) {
                                    admissible = true;
                                    wins = true;

                                    if (g.active_phase(v) == iteration) {
                                        wins =  (d_u_start < d_v - 1) || 
                                                (d_u_start == d_v + 1) || 
                                                (d_u_start == d_v && u < v);
                                    }
                                }
                            }
                        }
                    
                        // --- Team Reductions for state ---
                        // Get the minimum neighbor distance across the chunk

                        // NOTE: - ?? Could we merge this reduce with skipped_admissible to 
                        // handle both in 1 reduce ??
                        // TODO: - Merge the reductions
                        unsigned long thread_min_d = (valid_edge && cap > 0) ? d_v : (N + 1);
                        unsigned long team_min_d;
                        Kokkos::parallel_reduce(Kokkos::TeamThreadRange(team, team.team_size()), 
                            [&](const int dummy, unsigned long& update) {
                                if (thread_min_d < update) update = thread_min_d;
                            }, Kokkos::Min<unsigned long>(team_min_d));
                    
                        if (team_min_d < local_min_d) {
                           local_min_d = team_min_d;
                        }

                        // Did anyone skip an admissible edge?
                        int thread_skipped = (valid_edge && admissible && !wins) ? 1 : 0;
                        int team_skipped = 0;
                        Kokkos::parallel_reduce(Kokkos::TeamThreadRange(team, team.team_size()), 
                            [&](const int dummy, int& update) {
                                update += thread_skipped;
                            }, team_skipped);
                    
                        if (team_skipped > 0) {
                            skipped_admissible = 1;
                            if (first_skipped_chunk == -1) {
                                first_skipped_chunk = chunk_start;
                            }
                        }
                    
                        // 3. TEAM PREFIX SUM FOR PUSHING
                        long long req_flow = (valid_edge && admissible && wins && cap > 0) ? cap : 0;
                        long long exclusive_prefix = 0;
                        long long total_chunk_req = 0;
                    
                        // Scan to distribute flow correctly
                        Kokkos::parallel_scan(Kokkos::TeamThreadRange(team, team.team_size()), 
                            [&](const int thread_rank, long long& update, const bool final_pass) {
                                if (final_pass) { exclusive_prefix = update; }
                                update += req_flow;
                        });

                        // Reduce to get total requested flow by the team
                        Kokkos::parallel_reduce(Kokkos::TeamThreadRange(team, team.team_size()), 
                            [&](const int dummy, long long& update) {
                                update += req_flow;
                            }, total_chunk_req);
                        // Calculate actual push based on prefix sum and remaining excess
                        long long actual_push = 0;
                        if (req_flow > 0) {
                            long long available = e_u - exclusive_prefix;
                            if (available > 0) {
                                actual_push = (req_flow < available) ? req_flow : available;
                            }
                        }
                    
                        // Apply the flow
                        if (actual_push > 0) {
                            g.residual_capacity(idx) -= actual_push;
                            g.residual_capacity(g.reverse_edge(idx)) += actual_push;
                            Kokkos::atomic_add(&g.added_excess(v), actual_push);
                        }
                    
                        // Uniformly update e_u for ALL threads in the team
                        long long total_pushed = (total_chunk_req < e_u) ? total_chunk_req : e_u;
                        e_u -= total_pushed;
                    
                    
                        // 4. DYNAMIC ENQUEUE WITH BLOCK ATOMICS
                        int is_high = 0, is_low = 0;
                    
                        if (actual_push > 0 && v != s && v != t) {
                            int seen = Kokkos::atomic_exchange(&g.active_iteration_mask(v), next_iter_mask);
                            if (seen != next_iter_mask) {
                                // NOTE: - This is never OOB access, though it looks
                                // like one (CSR format perk)
                                int n_degree = g.row_map(v + 1) - g.row_map(v);
                                if (n_degree > pseudo_warp_size) is_high = 1;
                                else is_low = 1;
                            }
                        }
                    
                        // processing high nodes
                        int high_offset = 0;
                        Kokkos::parallel_scan(Kokkos::TeamThreadRange(team, team.team_size()), 
                            [&](const int dummy, int& update, const bool final_pass) {
                                if (final_pass) high_offset = update;
                                update += is_high;
                            });
                        
                        // last thread knows the total
                        int my_total_h = high_offset + is_high;
                        
                        // Broadcast from the last thread (rank = team_size - 1) to the whole team
                        team.team_broadcast(my_total_h, team.team_size() - 1);
                        int total_high_enq = my_total_h;
                        int high_base = 0;
                        if (team.team_rank() == 0 && total_high_enq > 0) {
                            // ONE atomic per chunk for the whole team!
                            high_base = Kokkos::atomic_fetch_add(&g.next_high_size(), total_high_enq);
                        }
                        team.team_broadcast(high_base, 0); // Share base index with the team
                        if (is_high) {
                            g.next_high(high_base + high_offset) = v;
                        }
                    
                        // [Repeat Block Atomic logic for `is_low` and `next_queue_size_low`]
                                    // processing high nodes
                        int low_offset = 0;
                        Kokkos::parallel_scan(Kokkos::TeamThreadRange(team, team.team_size()), 
                            [&](const int dummy, int& update, const bool final_pass) {
                                if (final_pass) low_offset = update;
                                update += is_low;
                            });
                        
                        // last thread knows the total
                        int my_total_l = low_offset + is_low;
                        
                        // Broadcast from the last thread (rank = team_size - 1) to the whole team
                        team.team_broadcast(my_total_l, team.team_size() - 1);
                        int total_low_enq = my_total_l;
                        int low_base = 0;
                        if (team.team_rank() == 0 && total_low_enq > 0) {
                            // ONE atomic per chunk for the whole team!
                            low_base = Kokkos::atomic_fetch_add(&g.next_low_size(), total_low_enq);
                        }
                        team.team_broadcast(low_base, 0); // Share base index with the team
                        if (is_low) {
                            g.next_low(low_base + low_offset) = v;
                        }
                    
                        // 5. EARLY EXIT & CURRENT ARC
                        if (e_u == 0) {
                            // Thread 0 saves the chunk index to resume later
                            if (team.team_rank() == 0) {
                                // Anchor to the first skipped chunk, or the current chunk if no skips occurred
                                g.current_arc(u) = (first_skipped_chunk != -1) ? first_skipped_chunk : chunk_start;
                            }
                            break; // Break the chunk loop
                        }
                    } // End of chunk loop
                
                    if (e_u == 0 || skipped_admissible) break;
                
                    // 6. RELABEL
                    int new_d = local_min_d + 1;
                    if (new_d < N + 1 && new_d > d_u_start) {
                        d_u_current = new_d;
                        if (team.team_rank() == 0) {
                            g.new_label(u) = new_d;
                            g.current_arc(u) = 0; // CRITICAL: Reset arc on relabel!
                        }
                        arc = 0; // Rescan from chunk 0
                    } else {
                        break; // Stuck
                    }
                }
            
                // 7. WRITEBACK & SELF-ENQUEUE
                if (team.team_rank() == 0) {
                    g.excess(u) = e_u;
                    if (e_u > 0 || d_u_current > d_u_start) {
                        int seen = Kokkos::atomic_exchange(&g.active_iteration_mask(u), next_iter_mask);
                        if (seen != next_iter_mask) {
                            // Self enqueue (since u is already high-degree, put it in high queue)
                            size_t pos = Kokkos::atomic_fetch_add(&g.next_high_size(), 1);
                            g.next_high(pos) = u;
                        }
                    }
                    if (e_u > 0 && d_u_current == d_u_start) {
                        // We hit the end of the edge list, didn't relabel, but still have excess.
                        // This only happens if we skipped edges. Reset arc to the skipped chunk.
                        if (team.team_rank() == 0 && first_skipped_chunk != -1) {
                            g.current_arc(u) = first_skipped_chunk;
                        }
                    }
                } });
                // update work metric
                iterations_since_last_gr++;
                Kokkos::fence("after_process_high_fence");
            }

            // wait for all threads
            // PROCESS END ==============================================

            // THIS IS FOR THE FUTURE EDGE-PARALLEL SHIFT
            size_t h_next_low_size = 0;
            size_t h_next_high_size = 0;

            Kokkos::deep_copy(h_next_low_size, g.next_low_size);
            Kokkos::deep_copy(h_next_high_size, g.next_high_size);

            // APPLY =================================================
            // will be: h_next_low_size + h_next_high_size > 0
            if (h_next_low_size + h_next_high_size > 0)
            {
                Kokkos::parallel_for(
                    "apply_kernel",
                    Kokkos::RangePolicy<Device>(0, h_next_low_size + h_next_high_size),
                    KOKKOS_LAMBDA(const int &i) {
                        // fetch from the correct queue based on the global index
                        int u;
                        if (i < h_next_low_size)
                        {
                            u = g.next_low(i);
                        }
                        else
                        {
                            u = g.next_high(i - h_next_low_size);
                        }

                        // same logic
                        long long incoming = g.added_excess(u);

                        if (incoming > 0 || g.excess(u))
                        {
                            g.excess(u) += incoming;
                            g.added_excess(u) = 0;
                            g.active_phase(u) = next_iter_mask;
                        }

                        int d_proposed = g.new_label(u);
                        int d_current = g.label(u);

                        if (d_proposed > d_current)
                        {
                            g.label(u) = d_proposed;
                            g.new_label(u) = 0;
                        }
                    });

                Kokkos::fence("after_apply_fence");
            }
            // APPLY END ==============================================

            // QUEUE SWAP
            iteration++;

            // THIS IS FOR THE FUTURE EDGE-PARALLEL SHIFT
            std::swap(g.current_high, g.next_high);
            std::swap(g.current_low, g.next_low);
            std::swap(g.current_high_size, g.next_high_size);
            std::swap(g.current_low_size, g.next_low_size);

            Kokkos::deep_copy(g.next_high_size, 0);
            Kokkos::deep_copy(g.next_low_size, 0);
            h_current_high_size = h_next_high_size;
            h_current_low_size = h_next_low_size;
        }

        // [TIMER] End Algorithm
        Kokkos::fence();
        double time_algo = timer.seconds();

        std::cout << "\n[FINISHED] Total Iterations: " << iteration << "\n";
        std::cout << ">> Algorithm Runtime: " << time_algo << " seconds.\n";
        std::cout << "------------------------------------------\n";
        std::cout << "TOTAL Runtime (IO + Init + Algo): " << (io_duration.count() + time_init + time_algo) << " seconds.\n";

        long long h_final_excess = 0;
        long long h_final_added = 0;
        Kokkos::deep_copy(h_final_excess, Kokkos::subview(g.excess, t));
        Kokkos::deep_copy(h_final_added, Kokkos::subview(g.added_excess, t));

        std::cout << "MAX FLOW IS " << (h_final_excess + h_final_added) << "\n";
#ifdef DEBUG_VERIFY
        Verifier<Device>::check_optimality(g, s, t, N);
#endif
    }
    Kokkos::finalize();
}