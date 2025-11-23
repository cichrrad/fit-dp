#ifndef GRAPH_HPP
#define GRAPH_HPP

#include <Kokkos_Core.hpp>

// CSR-ISH SoA
template <class DeviceType>
struct Graph
{
    // types
    using ExecutionSpace = typename DeviceType::execution_space;
    using MemorySpace = typename DeviceType::memory_space;
    using NodeIndex = int;
    using EdgeIndex = size_t;

    using HostMirror = Graph<Kokkos::HostSpace>;

    // CSR arrays
    //  map into edge_list (to show edge list slice begin
    //  for indexed vertex)
    Kokkos::View<EdgeIndex *, DeviceType> row_map;
    Kokkos::View<NodeIndex *, DeviceType> edge_list;

    // Edge properties
    // flow
    // capacity
    // (residual = cap - flow)
    // reverse edge idx (for O(1))
    Kokkos::View<long long *, DeviceType> flow;
    Kokkos::View<long long *, DeviceType> capacity;
    Kokkos::View<EdgeIndex *, DeviceType> reverse_edge;

    // Vertex properties
    // label
    // new_label
    // excess
    // added_excess
    // active (list or fixed size mask ?)
    // new_active
    // discovered ? (now sure how)
    Kokkos::View<int *, DeviceType> label;
    Kokkos::View<int *, DeviceType> new_label;
    Kokkos::View<long long *, DeviceType> excess;
    Kokkos::View<long long *, DeviceType> added_excess;
    Kokkos::View<NodeIndex *, DeviceType> active;
    Kokkos::View<NodeIndex *, DeviceType> new_active;
    

    // Returns number of nodes
    KOKKOS_INLINE_FUNCTION
    NodeIndex num_nodes() const
    {
        return excess.size();
    }

    // Returns number of edges
    KOKKOS_INLINE_FUNCTION
    EdgeIndex num_edges() const
    {
        return edge_list.size();
    }
};

#endif // GRAPH_HPP