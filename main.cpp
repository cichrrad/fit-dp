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
        std::cout << "Running on: " << typeid(Device).name() << "\n";

        // [TIMER] Start Graph Init
        Kokkos::Timer timer;

        // build graph on device
        auto g = GraphBuilder::build_graph<Device>(raw_edges, N, s, t);

        // kick of the algorithm
        // (push from S, add neighbours to intial queue)
        initialize_algorithm(g, s, t, N);

        // [TIMER] End Graph Init
        // NOTE: -- There is Kokkos::Fence in initialize_algorithm
        // so reading time here is always after its done
        double time_init = timer.seconds();
        std::cout << ">> Graph Build & Init Time: " << time_init << " seconds.\n";

        // [HOST] Variables
        int iteration = 1;
        size_t h_current_q_size = 0;
        const long long gr_iter_trigger = 1500;

        long long iterations_since_last_gr = 0;
        Kokkos::deep_copy(h_current_q_size, g.current_queue_size);

        // [TIMER] Start Algorithm
        timer.reset();

        while (h_current_q_size > 0)
        {

            if (iterations_since_last_gr > gr_iter_trigger)
            {
                global_relabel(g, s, t, N);
                iterations_since_last_gr = 0;
                // reset size
                Kokkos::deep_copy(g.current_queue_size, 0);

                // global_relabel uses queues, so we need to rebuild the
                // set of active vertices for this iteration
                Kokkos::parallel_for("Rebuild_Active_Set", Kokkos::RangePolicy<Device>(0, N), KOKKOS_LAMBDA(const int v) {
                    if (v != s && v != t && g.excess(v) > 0 && g.label(v) < N) {
                            int pos = Kokkos::atomic_fetch_add(&g.current_queue_size(), 1);
                            g.current_active(pos) = v;
                    } });
                Kokkos::fence();

                Kokkos::deep_copy(h_current_q_size, g.current_queue_size);
            }
            int next_iter_mask = iteration + 1;


            // PROCESS =================================================
            Kokkos::parallel_for(
                "process_kernel",
                Kokkos::RangePolicy<Device>(0, h_current_q_size),
                KOKKOS_LAMBDA(const int &i) {
                    int u = g.current_active(i);
                    if (u == s || u == t)
                    {
                        return;
                    }

                    long long e_u = g.excess(u);

                    // label at the moment kernel was
                    // launched (this wont change)
                    const int d_u_start = g.label(u);

                    // current label reflecting
                    // local relabeling when discharging
                    int d_u_current = d_u_start;

                    // edges from u
                    int row_start = g.row_map(u);
                    int row_end = g.row_map(u + 1);

                    // We loop until discharged or blocked by a conflict
                    while (e_u > 0)
                    {
                        // GR contribution
                        // this is inside while loop
                        // se add this every time
                        // we re-enter it, because
                        // we rescan edges

                        // "Infinity"
                        // (N should do)
                        unsigned long min_d_neighbor = N + 1;
                        bool skipped_admissible_edge = false;

                        // go over all the edges from u and try
                        // to push along them if possible
                        for (int idx = row_start; idx < row_end; ++idx)
                        {
                            // get target of edge idx
                            // and cap (residual) of idx
                            int v = g.entries(idx);
                            long long cap = g.residual_capacity(idx);

                            // can we still push?
                            if (cap > 0)
                            {
                                int d_v = g.label(v);

                                // note min neighbour for relabeling later
                                if (d_v < min_d_neighbor)
                                    min_d_neighbor = d_v;

                                // admissible ?
                                if (d_u_current == d_v + 1)
                                {
                                    bool wins = true;

                                    if (g.active_phase(v) == iteration)
                                    {
                                        // Conflict detected! Fall back to the deterministic tie-breaker
                                        // using the labels from the start of the iteration.
                                        wins = (d_u_start < d_v - 1) ||
                                               (d_u_start == d_v + 1) ||
                                               (d_u_start == d_v && u < v);
                                    }

                                    if (wins)
                                    {
                                        // u won, so u can now safely
                                        // discharge along edge idx
                                        // => win condition check
                                        // allows us to NOT use atomics
                                        // for the pushing
                                        long long delta = (e_u < cap) ? e_u : cap;

                                        g.residual_capacity(idx) -= delta;
                                        g.residual_capacity(g.reverse_edge(idx)) += delta;
                                        e_u -= delta;
                                        // update so that v has it next iteration
                                        // this has to be atomic as u' might exist
                                        // also pushing into v at the same time
                                        Kokkos::atomic_add(&g.added_excess(v), delta);

                                        // Enqueue v
                                        //(it either did not have excess and now it does = active)
                                        // OR
                                        //(it did and whatever it did with it -- pushed all or not
                                        // -- we just added more)
                                        if (v != s && v != t)
                                        {
                                            // mark vertices active in NEXT turn with "generational mask"
                                            // which uses iteration count
                                            // => removes need for clear between iterations
                                            // like with bit/bool masks
                                            int seen_mask = Kokkos::atomic_exchange(&g.active_iteration_mask(v), next_iter_mask);
                                            if (seen_mask != next_iter_mask)
                                            {
                                                size_t insert_pos = Kokkos::atomic_fetch_add(&g.next_queue_size(), 1);
                                                g.next_active(insert_pos) = v;
                                            }
                                        }

                                        // did we just push all excess of u?
                                        // if so, break now (from the for loop)
                                        if (e_u == 0)
                                            break;
                                    }
                                    // we lost = wait it out
                                    else
                                    {
                                        // Found admissible edge, but lost conflict.
                                        skipped_admissible_edge = true;
                                        // NOTE -- this might be too strict
                                        // -- continue might be good too
                                        // and it would allow other edges
                                        // to attempt to push to them

                                        // break;
                                        continue; // seems to work as expected
                                    }
                                }
                            }
                            // we cannot push along this edge
                            // as it is full
                        }

                        // did we break from the for loop
                        // because we have nothing left to
                        // push ? if so, break
                        // (from the while loop)
                        if (e_u == 0)
                            break;

                        // the moment we lost the win check
                        // = we know we will lose it
                        // every time this iteration
                        // (because we use "old" label)
                        // = break
                        // TODO verify????
                        if (skipped_admissible_edge)
                            break;

                        // If we are here, we have excess AND no admissible edges.
                        // => elabel to (min_neighbor + 1), so we are "uphill"
                        // from it and can potentially push to it
                        int new_d = min_d_neighbor + 1;

                        // Apply relabel if valid and increasing
                        if (new_d < N + 1 && new_d > d_u_start)
                        {
                            d_u_current = new_d;
                            g.new_label(u) = new_d;
                        }
                        else
                        {
                            // If we can't push and can't relabel (e.g. disconnected), we are stuck.
                            break;
                        }
                    }

                    // Write back remaining excess
                    g.excess(u) = e_u;

                    // enqueue Self if still active / relabeled
                    if (e_u > 0 || d_u_current > d_u_start)
                    {
                        int seen_mask = Kokkos::atomic_exchange(&g.active_iteration_mask(u), next_iter_mask);
                        if (seen_mask != next_iter_mask)
                        {
                            size_t insert_pos = Kokkos::atomic_fetch_add(&g.next_queue_size(), 1);
                            g.next_active(insert_pos) = u;
                        }
                    }
                });

            // wait for all threads
            Kokkos::fence("after_process_fence");
            // update work metric
            iterations_since_last_gr++;
            // PROCESS END ==============================================

            // Check Next Queue
            size_t h_next_q_size = 0;
            Kokkos::deep_copy(h_next_q_size, g.next_queue_size);

            // APPLY =================================================
            if (h_next_q_size > 0)
            {
                Kokkos::parallel_for(
                    "apply_kernel",
                    Kokkos::RangePolicy<Device>(0, h_next_q_size),
                    KOKKOS_LAMBDA(const int &i) {
                        int u = g.next_active(i);
                        long long incoming = g.added_excess(u);
                        g.active_phase(u) = next_iter_mask;
                        // update excess added by process
                        // kernel prior, so that excess
                        // is up-to-date for next iter
                        if (incoming > 0)
                        {
                            g.excess(u) += incoming;
                            g.added_excess(u) = 0;
                        }
                        int d_proposed = g.new_label(u);
                        int d_current = g.label(u);
                        // if we relabeled during the process
                        // kernel, update
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
            std::swap(g.current_active, g.next_active);
            std::swap(g.current_queue_size, g.next_queue_size);
            // TODO -- is this necessary ? Its just 1 number, but the overhead
            // must be big -- maybe there is a way to do this just on device?
            Kokkos::deep_copy(g.next_queue_size, 0);

            h_current_q_size = h_next_q_size;
            iteration++;
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