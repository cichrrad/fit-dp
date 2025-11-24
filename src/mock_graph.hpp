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

    using FlowType = long long;
    using NodeIndex = int;
    using EdgeIndex = size_t;
    // "vertex edge start index"
    using row_map_type = Kokkos::View<EdgeIndex *, DeviceType>;
    // edges -- edge on position [n] holds index of endpoint vertex [v] of edge from
    // vertex [u] (row_map(u) <= [n] < row_map(u+1))
    using edge_list_type = Kokkos::View<NodeIndex *, DeviceType>;
    
    using flow_type = Kokkos::View<FlowType *, DeviceType>;
    using reverse_edge_index_type = Kokkos::View<EdgeIndex *, DeviceType>;
    using height_type = Kokkos::View<int *, DeviceType>;


    // properties
    // graph
    row_map_type row_map;  
    edge_list_type edge_list; 
    
    // edge
    flow_type capacity;                        
    flow_type flow;                            
    reverse_edge_index_type reverse_edge_index;

    // vertes
    flow_type excess;   
    height_type height; 

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
        return edge_list.extent(0);
    }
};

#endif // GRAPH_HPP