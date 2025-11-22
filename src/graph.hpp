#ifndef GRAPH_HPP
#define GRAPH_HPP

#include <Kokkos_Core.hpp>

template <class DeviceType>
struct Graph
{
    // Derived types for cleaner code
    using ExecutionSpace = typename DeviceType::execution_space;
    using MemorySpace = typename DeviceType::memory_space;

    // We use 'long long' for flow/capacity to prevent overflow on large benchmarks
    using FlowType = long long;
    using NodeIndex = int;
    using EdgeIndex = size_t;

    // ------------------------------------------------------------
    // View Type Definitions
    // ------------------------------------------------------------

    using row_map_type = Kokkos::View<EdgeIndex *, DeviceType>;
    using entries_type = Kokkos::View<NodeIndex *, DeviceType>;
    using capacity_type = Kokkos::View<FlowType *, DeviceType>;
    using flow_type = Kokkos::View<FlowType *, DeviceType>;
    using reverse_edge_index_type = Kokkos::View<EdgeIndex *, DeviceType>;

    using excess_type = Kokkos::View<FlowType *, DeviceType>;
    using height_type = Kokkos::View<int *, DeviceType>;

    // ------------------------------------------------------------
    // Member Variables (The Actual Data)
    // ------------------------------------------------------------

    // Topology
    row_map_type row_map; // Size: num_nodes + 1
    entries_type entries; // Size: num_edges

    // Edge Properties (SoA)
    capacity_type capacity;                     // Size: num_edges
    flow_type flow;                             // Size: num_edges
    reverse_edge_index_type reverse_edge_index; // Size: num_edges

    // Node Properties
    excess_type excess; // Size: num_nodes
    height_type height; // Size: num_nodes

    // ============================================================
    // 4. Helper Methods
    // ============================================================

    // Returns number of nodes
    KOKKOS_INLINE_FUNCTION
    NodeIndex num_nodes() const
    {
        return excess.extent(0);
    }

    // Returns number of edges
    KOKKOS_INLINE_FUNCTION
    EdgeIndex num_edges() const
    {
        return entries.extent(0);
    }
};

#endif // GRAPH_HPP