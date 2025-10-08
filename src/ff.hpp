#ifndef FORD_FULKERSON_BGL_HPP
#define FORD_FULKERSON_BGL_HPP

#include "dinic.hpp"
#include <vector>

namespace my_ff {

using my_dinic::Edge;
using my_dinic::Graph;
using my_dinic::Vertex;
using EdgeDesc = Edge;

using my_dinic::CapMap;
using my_dinic::ResMap;
using my_dinic::RevMap;

Edge add_edge_with_capacity(Graph &g, Vertex u, Vertex v, long c);

// Ford–Fulkerson using DFS-found augmenting paths.
// Returns max-flow value; updates residual capacities in-place.
long ford_fulkerson_max_flow(Graph &g, Vertex s, Vertex t);

} // namespace my_ff

#endif // FORD_FULKERSON_BGL_HPP
