#ifndef GRAPH_DEFS_HPP
#define GRAPH_DEFS_HPP

#include <Kokkos_Core.hpp>

struct TempEdge {
    int u;
    int v;
    long long capacity;


    // overload ops so we can run Kokkos::sort to do
    // work for us :)

    KOKKOS_INLINE_FUNCTION
    bool operator<(const TempEdge& other) const {
        if (u != other.u) return u < other.u;
        return v < other.v;
    }
    
    KOKKOS_INLINE_FUNCTION
    bool operator==(const TempEdge& other) const {
        return u == other.u && v == other.v;
    }
    
    KOKKOS_INLINE_FUNCTION
    bool operator!=(const TempEdge& other) const {
        return !(*this == other);
    }
};

// Define the Host View type used by the loader
using HostEdgeList = Kokkos::View<TempEdge*, Kokkos::HostSpace>;

#endif // GRAPH_DEFS_HPP