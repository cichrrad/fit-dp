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
#include "src/preprocessing/csv_loader.hpp"
#include "src/global_relabel.hpp"

// #define DEBUG_PRINT_ON_HOST

int main(int argc, char *argv[])
{

    // [TIMER] Start Graph Read
    auto start_io = std::chrono::high_resolution_clock::now();

    int N, s, t;
    // const auto raw_edges = parse_csv("./helpers/format_convertors/graph_output.csv", N);
    const auto raw_edges = parse_csv("./input/mock/generated_graph.csv", N, s, t);

    // [TIMER] End Graph Read
    auto end_io = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> io_duration = end_io - start_io;

    std::cout << "Parsed graph with " << N << " vertices and "<< raw_edges.size() <<" edges. Source is " << s << " and sink is " << t << ".\n";
    std::cout << ">> IO Time (CSV Read): " << io_duration.count() << " seconds.\n";

    Kokkos::initialize(argc, argv);
    {
        using Device = Kokkos::DefaultExecutionSpace;
        std::cout << "\nKokkos initialized on: " << typeid(Device).name() << "\n";
        
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

#ifdef DEBUG_PRINT_ON_HOST
        // HOST MIRRORS=================================================
        auto h_excess = Kokkos::create_mirror_view(g.excess);
        auto h_added_excess = Kokkos::create_mirror_view(g.added_excess);
        auto h_label = Kokkos::create_mirror_view(g.label);
        auto h_new_label = Kokkos::create_mirror_view(g.new_label);
        auto h_row_map = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), g.row_map);
        auto h_entries = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), g.entries);
        auto h_residual = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), g.residual_capacity);
        // =============================================================

        // [DEBUG] Print Graph Structure
        std::cout << "\n=== [GRAPH ADJACENCY CHECK] ===\n";
        for (int u = 0; u < N; ++u)
        {
            std::cout << "Node " << u << " Neighbors: ";
            for (int i = h_row_map(u); i < h_row_map(u + 1); ++i)
            {
                int v = h_entries(i);
                std::cout << v << " (Res " << h_residual(i) << ") ";
            }
            std::cout << "\n";
        }

        auto print_state = [&](const char *phase_name, size_t q_size, bool show_pending = false, long long &t_excess)
        {
            std::cout << "\n=== [" << phase_name << "] ===\n";
            Kokkos::deep_copy(h_excess, g.excess);
            Kokkos::deep_copy(h_label, g.label);

            // Create a temporary snapshot of the CURRENT active queue
            // This ensures we don't accidentally overwrite the 'next' buffer if they alias in HostSpace
            // (such as OPENMP)
            auto h_active_snapshot = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), g.current_active);

            if (show_pending)
            {
                Kokkos::deep_copy(h_added_excess, g.added_excess);
                Kokkos::deep_copy(h_new_label, g.new_label);
            }

            std::cout << "Active Queue (" << q_size << "): [ ";
            for (size_t i = 0; i < q_size; ++i)
                std::cout << h_active_snapshot(i) << " ";
            std::cout << "]\n";

            std::cout << " Node | Label | Excess ";
            if (show_pending)
                std::cout << "| +Buf | NewL ";
            std::cout << "\n------+-------+--------";
            if (show_pending)
                std::cout << "+------+------";
            std::cout << "\n";

            for (int i = 0; i < N; ++i)
            {
                std::cout << std::setw(5) << i << " | "
                          << std::setw(5) << h_label(i) << " | "
                          << std::setw(6) << h_excess(i) << " ";
                if (show_pending)
                {
                    std::cout << "| " << std::setw(4) << h_added_excess(i) << " | "
                              << std::setw(4) << h_new_label(i) << " ";
                }
                if (i == s)
                    std::cout << "(S)";
                if (i == t)
                {
                    std::cout << "(T)";
                    t_excess = h_excess(i) + h_added_excess(i);
                }
                std::cout << "\n";
            }
        };

        long long final_excess = 0LL;
#endif
        // [HOST] Variables
        int iteration = 1;
        size_t h_current_q_size = 0;
        const long long gr_trigger = 12 * N + 2 * g.num_edges();
        long long work_since_last_gr = 0;
#ifdef DEBUG_PRINT_ON_HOST
        std::cout << "GLOBAL RELABEL TRIGGER = " << gr_trigger << "\n";
#endif
        Kokkos::deep_copy(h_current_q_size, g.current_queue_size);
#ifdef DEBUG_PRINT_ON_HOST
        std::cout << "\n[STARTING ALGORITHM] Initial Active Nodes: " << h_current_q_size << "\n";
#endif

#ifdef DEBUG_PRINT_ON_HOST
        print_state("INITIAL STATE", h_current_q_size, false, final_excess);
#endif

        // [TIMER] Start Algorithm
        timer.reset();

        while (h_current_q_size > 0)
        {

#ifdef DEBUG_PRINT_ON_HOST
            std::cout << "\n--- Iteration " << iteration << " ---\n";
#endif

            if (work_since_last_gr > gr_trigger)
            {
                work_since_last_gr = 0;
#ifdef DEBUG_PRINT_ON_HOST
                std::cout << "GR TRIGGERED\n";
#endif
                global_relabel(g, t, N);

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
            long long step_work = 0;
            int next_iter_mask = iteration + 1;

            // PROCESS =================================================
            Kokkos::parallel_reduce(
                "process_kernel",
                Kokkos::RangePolicy<Device>(0, h_current_q_size),
                KOKKOS_LAMBDA(const int &i, long long &l_work) {
                    int u = g.current_active(i);
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
                        l_work += (row_end - row_start);

                        // "Infinity"
                        // (> N should do)
                        int min_d_neighbor = 2 * g.num_nodes();
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
                                    // win condition
                                    // note that this is calculated
                                    // with const label from kernel launch
                                    // moment, not current (possibly relabeled)
                                    // label d_u_current
                                    bool wins = (d_u_start < d_v - 1) ||
                                                (d_u_start == d_v + 1) ||
                                                (d_u_start == d_v && u < v);

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
                        if (new_d < 2 * g.num_nodes() && new_d > d_u_start)
                        {
                            d_u_current = new_d;
                            // Buffer update
                            g.new_label(u) = new_d;
                            // add Beta -- relabel tax
                            // to support global relabel
                            // sooner if we are in this
                            // situation
                            l_work += 12;
                        }
                        else
                        {
                            // If we can't push and can't relabel (e.g. disconnected), we are stuck.
                            break;
                        }
                    }

                    // Write back remaining excess
                    g.excess(u) = e_u;

                    // ?TODO FIX?
                    // ?TEMP? we add anything that changed label
                    // during kernel, because we need to fix it in
                    // apply kernel -- there might be better way
                    // to do this and not bloat the working set

                    // removed -- should still work fine ?

                    // enqueue Self if still active
                    if (e_u > 0 || d_u_current > d_u_start)
                    {
                        int seen_mask = Kokkos::atomic_exchange(&g.active_iteration_mask(u), next_iter_mask);
                        if (seen_mask != next_iter_mask)
                        {
                            size_t insert_pos = Kokkos::atomic_fetch_add(&g.next_queue_size(), 1);
                            g.next_active(insert_pos) = u;
                        }
                    }
                },
                step_work);

            // wait for all threads
            Kokkos::fence("after_process_fence");
            // update work metric
            work_since_last_gr += step_work;
            // PROCESS END ==============================================

            // Check Next Queue
            size_t h_next_q_size = 0;
            Kokkos::deep_copy(h_next_q_size, g.next_queue_size);

#ifdef DEBUG_PRINT_ON_HOST
            print_state("POST-PROCESS (Pending Updates)", h_current_q_size, true, final_excess);
            std::cout << "Next Queue Size will be: " << h_next_q_size << "\n";
#endif
            // APPLY =================================================
            if (h_next_q_size > 0)
            {
                Kokkos::parallel_for(
                    "apply_kernel",
                    Kokkos::RangePolicy<Device>(0, h_next_q_size),
                    KOKKOS_LAMBDA(const int &i) {
                        int u = g.next_active(i);
                        long long incoming = g.added_excess(u);
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

#ifdef DEBUG_PRINT_ON_HOST
            //[DEBUG] Double Check - Print the NEW current queue (which was next)
            print_state("POST-APPLY (State Committed - Ready for Next)", h_next_q_size, false, final_excess);
#endif
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

        // print from device
        // single thread launched to just print flow
        // NOTE -- THIS WONT WORK ON GPUs -- WE NEED TO DEEP COPY 'g.added_excess(i) + g.excess(i)' ONTO HOST AND COUT
        // GOOD ENOUGH FOR NOW (OpenMP)
        Kokkos::parallel_for("print_max_flow", Kokkos::RangePolicy<Device>(t, t + 1), KOKKOS_LAMBDA(const int &i) { std::cout << "MAX FLOW IS " << g.added_excess(i) + g.excess(i) << "\n"; });
    }
    Kokkos::finalize();
}