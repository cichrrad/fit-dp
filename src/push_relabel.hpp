#ifndef PUSH_RELABEL_HPP
#define PUSH_RELABEL_HPP

#include <Kokkos_Core.hpp>
#include "graph.hpp"
#include "global_relabel.hpp"

// PROCESS (flow pushing & local relabels)
template <class DeviceType>
struct ProcessKernel
{
    using ExecutionSpace = typename DeviceType::execution_space;
    using MemorySpace = typename DeviceType::memory_space;

    // Standard Views (Read/Write)
    using ValueView = typename Graph<DeviceType>::ValueViewType;
    using EntriesView = typename Graph<DeviceType>::EntriesType;
    using LabelView = typename Graph<DeviceType>::LabelViewType;
    using MaskView = typename Graph<DeviceType>::MaskViewType;

    using ConstRandomAccessInt = Kokkos::View<const int *,
                                              MemorySpace,
                                              Kokkos::MemoryTraits<Kokkos::RandomAccess>>;

    using ConstRandomAccessEdge = Kokkos::View<const int *,
                                               MemorySpace,
                                               Kokkos::MemoryTraits<Kokkos::RandomAccess>>;

    // Member Variables
    // We hold the RandomAccess versions of the read-only graph parts
    ConstRandomAccessEdge row_map;
    ConstRandomAccessInt entries;
    ConstRandomAccessInt label;
    ConstRandomAccessEdge reverse_edge;

    // Standard R/W views
    ValueView residual_capacity;
    ValueView excess;
    ValueView added_excess;
    LabelView new_label;

    EntriesView current_active;
    EntriesView next_active;
    Kokkos::View<size_t, DeviceType> next_queue_size;
    MaskView active_iteration_mask;

    int next_iter_mask;
    int s, t, n;

    ProcessKernel(Graph<DeviceType> g, int _mask, int _s, int _t, int _n)
        : row_map(g.row_map),           // Implicit conversion to RandomAccess
          entries(g.entries),           // -||-
          label(g.label),               // -||-
          reverse_edge(g.reverse_edge), // -||-
          // Standard copies for the rest
          residual_capacity(g.residual_capacity),
          excess(g.excess),
          added_excess(g.added_excess),
          new_label(g.new_label),
          current_active(g.current_active),
          next_active(g.next_active),
          next_queue_size(g.next_queue_size),
          active_iteration_mask(g.active_iteration_mask),
          next_iter_mask(_mask), s(_s), t(_t), n(_n)
    {
    }
    // actual kernel
    KOKKOS_INLINE_FUNCTION
    void operator()(const int i, long long &l_work) const
    {
        int u = current_active(i);
        long long e_u = excess(u);

        // read-only snapshot Label
        const int d_u_start = label(u);
        int d_u_current = d_u_start;

        // edges
        int row_start = row_map(u);
        int row_end = row_map(u + 1);
        bool relabeled = false;

        // DISCHARGE LOOP
        while (e_u > 0)
        {

            // NOTE,TODO -- this might be innacurate with
            // arc optimization later
            l_work += (row_end - row_start);

            // ~inf
            int min_d_neighbor = 2 * n;
            bool skipped_admissible = false;

            // scan over the edges
            // NOTE,TODO -- this is not a good thing to have
            // sequential in a kernel -- suppose most of warp threads have nodes with degree ~k
            // BUT one has K >>>>>> k --> halt + its slow because its goddamn sequential loop
            // --> Team policy / nested par. would be nice here
            for (int idx = row_start; idx < row_end; ++idx)
            {
                int v = entries(idx);
                long long cap = residual_capacity(idx);

                if (cap > 0)
                {
                    int d_v = label(v);
                    if (d_v < min_d_neighbor)
                        min_d_neighbor = d_v;

                    // admissibility check
                    // (using local var for d_u -- this allows more pushes WITHOUT atomics! :))
                    if (d_u_current == d_v + 1)
                    {

                        // local only deterministic check
                        // to break what node wins
                        bool wins = (d_u_start < d_v - 1) ||
                                    (d_u_start == d_v + 1) ||
                                    (d_u_start == d_v && u < v);

                        if (wins)
                        {
                            // PUSH
                            long long delta = (e_u < cap) ? e_u : cap;

                            // NOT ATOMIC
                            residual_capacity(idx) -= delta;
                            residual_capacity(reverse_edge(idx)) += delta;
                            e_u -= delta;

                            // this still has to be atomic, beause other threads might
                            // be pushing INTO v;
                            Kokkos::atomic_add(&added_excess(v), delta);

                            // activate [v]
                            if (v != s && v != t)
                            {
                                activate_node(v);
                            }

                            if (e_u == 0)
                                break;
                        }
                        else
                        {
                            // we could be pushing, but lost
                            // against [v]
                            skipped_admissible = true;
                        }
                    }
                }
            } // End Edge Scan

            if (e_u == 0)
                break;
            if (skipped_admissible)
                // Lost Conflict - Wait it out till next kernel run
                break;

            // (local) RELABEL
            int new_d = min_d_neighbor + 1;

            //  check to ensure we don't wrap around or do useless work
            if (new_d < 2 * n && new_d > d_u_start)
            {
                d_u_current = new_d;
                new_label(u) = new_d; // buffer the update
                l_work += 12;         // relabel tax for heuristic
                relabeled = true;
            }
            else
            {
                // disconnected or stuck
                break;
            }
        } // End Discharge Loop

        // write Back
        excess(u) = e_u;

        // re-activate self if excess remains or relabeled
        if (e_u > 0 || relabeled)
        {
            activate_node(u);
        }
    }

    // helper
    KOKKOS_INLINE_FUNCTION
    void activate_node(int v) const
    {
        int seen = Kokkos::atomic_exchange(&active_iteration_mask(v), next_iter_mask);
        if (seen != next_iter_mask)
        {
            size_t pos = Kokkos::atomic_fetch_add(&next_queue_size(), 1);
            next_active(pos) = v;
        }
    }
};

// APPLY (update excess with added one from PROCESS, same with label)
template <class DeviceType>
struct ApplyKernel
{
    Graph<DeviceType> g;

    ApplyKernel(Graph<DeviceType> _g) : g(_g) {}

    KOKKOS_INLINE_FUNCTION
    void operator()(const int i) const
    {
        int u = g.next_active(i);

        // take on new flow
        long long incoming = g.added_excess(u);
        if (incoming > 0)
        {
            g.excess(u) += incoming;
            g.added_excess(u) = 0;
        }

        // commit label update
        int d_prop = g.new_label(u);
        int d_curr = g.label(u);
        if (d_prop > d_curr)
        {
            g.label(u) = d_prop;
            // g.new_label(u) = 0; //(CAN BE MOVED TO GR)
        }
    }
};

// Kokkos parallel synchronous push relabel solver entry point
template <class DeviceType>
class PushRelabelSolver
{
public:
    static void solve(Graph<DeviceType> &g, int s, int t, int N, long long &final_max_flow)
    {

        using ExecutionSpace = typename DeviceType::execution_space;

        // Host control variables
        int iteration = 1;
        size_t h_current_q_size = 0;
        // fetch queue size, which was set in the init phase
        // (== degree of of s)
        Kokkos::deep_copy(h_current_q_size, g.current_queue_size);

        // Global Relabel Heuristics
        const long long gr_trigger = 12 * N + 2 * g.num_edges();
        long long work_since_last_gr = 0;

        while (h_current_q_size > 0)
        {

            // Check Global Relabel
            if (work_since_last_gr > gr_trigger)
            {
                work_since_last_gr = 0;
                GlobalRelabel<DeviceType>::run(g, t, N);
                GlobalRelabel<DeviceType>::rebuild_active_queue(g, s, t, N);
                Kokkos::deep_copy(h_current_q_size, g.current_queue_size);

                // If queue empty after GR (graph disconnected?), break
                if (h_current_q_size == 0)
                    break;
            }

            long long step_work = 0;
            int next_iter_mask = iteration + 1;

            // PROCESS Phase
            ProcessKernel<DeviceType> p_kernel(g, next_iter_mask, s, t, N);

            Kokkos::parallel_reduce(
                "PR_Process",
                Kokkos::RangePolicy<ExecutionSpace>(0, h_current_q_size),
                p_kernel,
                step_work);

            work_since_last_gr += step_work;
            Kokkos::fence();

            // APPLY Phase
            size_t h_next_q_size = 0;
            Kokkos::deep_copy(h_next_q_size, g.next_queue_size);

            if (h_next_q_size > 0)
            {
                ApplyKernel<DeviceType> a_kernel(g);
                Kokkos::parallel_for(
                    "PR_Apply",
                    Kokkos::RangePolicy<ExecutionSpace>(0, h_next_q_size),
                    a_kernel);
                    Kokkos::fence();
            }

            // Swap Queues
            std::swap(g.current_active, g.next_active);
            std::swap(g.current_queue_size, g.next_queue_size);
            Kokkos::deep_copy(g.next_queue_size, 0);

            h_current_q_size = h_next_q_size;
            iteration++;
        }
        Kokkos::fence();

        // Calculate Result
        long long h_final_excess = 0;
        long long h_final_added = 0;
        Kokkos::deep_copy(h_final_excess, Kokkos::subview(g.excess, t));
        Kokkos::deep_copy(h_final_added, Kokkos::subview(g.added_excess, t));
        final_max_flow = h_final_excess + h_final_added;
    }
};

#endif