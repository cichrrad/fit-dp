#ifndef DEBUG_TRACKERS_HPP
#define DEBUG_TRACKERS_HPP

#include <Kokkos_Core.hpp>
#include <iostream>
#include "../graph.hpp"

template <class DeviceType>
struct DebugTrackers
{
    using ExecutionSpace = typename DeviceType::execution_space;
    using RangePolicy = Kokkos::RangePolicy<ExecutionSpace>;
    // RESIDUAL CONSERVATION
    // The sum of all residual capacities in the graph must be CONSTANT.
    // (Pushing decreases forward edge and increases reverse edge by same amount)
    static long long check_residual_conservation(Graph<DeviceType> &g, const std::string &tag, bool will_print)
    {
        long long total_residual = 0;
        long long negative_edges = 0;

        Kokkos::parallel_reduce("Audit_Sum", RangePolicy(0, g.num_edges()), KOKKOS_LAMBDA(const int i, long long &sum) { sum += g.residual_capacity(i); }, total_residual);

        Kokkos::parallel_reduce("Audit_Neg", RangePolicy(0, g.num_edges()), KOKKOS_LAMBDA(const int i, long long &count) { 
            if(g.residual_capacity(i) < 0) count++; }, negative_edges);

        Kokkos::fence();

        if (will_print)
        {
            std::cout << "[ " << tag << "] Residual Sum: " << total_residual << "\n";
        }
        if (negative_edges > 0)
        {
            std::cout << "!!! [FAIL] FOUND " << negative_edges << " NEGATIVE EDGES !!!\n";
            throw std::runtime_error("Negative Capacity Detected");
        }
        else
        {
            if (will_print)
            {
                std::cout << "[ " << tag << "] No negative capacities" << "\n";
            }
        }
        return total_residual;
    }

    // EXCESS CONSERVATION
    // Sum of all excess (including s and t) should be 0/constant
    static long long check_excess_conservation(Graph<DeviceType> &g, const std::string &tag, bool will_print)
    {
        long long total_excess = 0;
        long long total_added = 0;

        // Sum current excess
        Kokkos::parallel_reduce(
            "Audit_Excess_Sum",
            RangePolicy(0, g.num_nodes()),
            KOKKOS_LAMBDA(const int u, long long &sum) {
                sum += g.excess(u);
            },
            total_excess);

        // Sum added_excess (in flight buffers)
        Kokkos::parallel_reduce(
            "Audit_Added_Sum",
            RangePolicy(0, g.num_nodes()),
            KOKKOS_LAMBDA(const int u, long long &sum) {
                sum += g.added_excess(u);
            },
            total_added);

        Kokkos::fence();
        if (will_print)
        {
            std::cout << "[ " << tag << "] System Excess: " << total_excess
                      << " | In-Flight Excess: " << total_added
                      << " | TOTAL: " << (total_excess + total_added) << "\n";
        }
        return (total_excess + total_added);
    }

    // LABEL VALIDITY (STEEPNESS)
    // For every residual edge (u,v), d(u) <= d(v) + 1 must hold.
    static int check_label_validity(Graph<DeviceType> &g, const std::string &tag, bool will_print)
    {
        int violations = 0;

        Kokkos::parallel_reduce(
            "Audit_Steepness",
            RangePolicy(0, g.num_nodes()),
            KOKKOS_LAMBDA(const int u, int &v_count) {
                int d_u = g.label(u);
                // Iterate edges
                int start = g.row_map(u);
                int end = g.row_map(u + 1);

                for (int i = start; i < end; ++i)
                {
                    long long cap = g.residual_capacity(i);
                    if (cap > 0)
                    {
                        int v = g.entries(i);
                        int d_v = g.label(v);

                        // Check invariant
                        if (d_u > d_v + 1)
                        {
                            v_count++;
                        }
                    }
                }
            },
            violations);

        Kokkos::fence();
        if (will_print)
        {

            if (violations > 0)
            {
                std::cout << "[ " << tag << "] !!! STEEPNESS VIOLATIONS: " << violations << " !!!\n";
            }
            else
            {
                std::cout << "[ " << tag << "] no steepness violations" << "\n";
            }
        }
        return violations;
    }

    static void run_all_debug_checks(Graph<DeviceType> &g, const std::string &tag, bool will_print, long long &previous_residual, bool is_after_apply)
    {
        bool will_throw = false;

        auto res = check_residual_conservation(g, tag, will_print);
        auto exs = check_excess_conservation(g, tag, will_print);
        auto v = check_label_validity(g, tag, will_print);

        if (is_after_apply)
        {

            if (v)
            {
                will_throw = true;
            }
        }

        if (exs)
        {
            will_throw = true;
        }

        if (previous_residual != -1 && previous_residual != res)
        {
            will_throw = true;
        }

        if (will_throw)
        {
            auto res = check_residual_conservation(g, tag, true);
            auto exs = check_excess_conservation(g, tag, true);
            auto v = check_label_validity(g, tag, true);
            // throw std::runtime_error("SOMETHING IS WRONG. STOPING");
        }
        previous_residual = res;
    }
};

#endif