#include <boost/graph/adjacency_list.hpp>
#include <iostream>
#include <limits>
#include <stack>
#include <vector>

using namespace boost;

// LINK
//  > https://www.youtube.com/watch?v=LdOnanfc5TM

// for defining <*edge container*,*vertex container*,*orientation*> of graph(s)
// we will use
using Traits = adjacency_list_traits<vecS, vecS, directedS>;
// => we will store edges and vertices in vector selectors, graph will be
// directed

// for when accessing edge
using EdgeDesc = Traits::edge_descriptor;

// Define graph with wanted edge properties -- capacity, residual capacity,
// reverse-edge (for `backflow`)
using Graph =
    adjacency_list<vecS, vecS, directedS, no_property,
                   property<edge_capacity_t, long,
                            property<edge_residual_capacity_t, long,
                                     property<edge_reverse_t, EdgeDesc>>>>;

// for when accessing vertex
using Vertex = graph_traits<Graph>::vertex_descriptor;

// Helper: add (u->v) with capacity 'c' and its reverse edge (v->u) with 0
// capacity. Also wire up the reverse-edge map both ways, and initialize
// residuals.
EdgeDesc add_edge_with_capacity(Vertex u, Vertex v, long c, Graph &g) {

  // capacity
  auto cap = get(edge_capacity, g);
  // residual
  auto res = get(edge_residual_capacity, g);
  // reverse edge to original one
  auto rev = get(edge_reverse, g);

  bool ok;
  EdgeDesc e, er;

  // makes tuples, add_edge returns {edge_descriptor, bool}
  tie(e, ok) = add_edge(u, v, g);
  tie(er, ok) = add_edge(v, u, g);

  // set cap (capacity) of edge 'e' (u-->v) to c
  cap[e] = c;
  // backflow is 0
  cap[er] = 0;

  res[e] = c;
  res[er] = 0;

  rev[e] = er;
  rev[er] = e;

  return e;
}

// Depth-First-Search to find any s->t path with positive residual capacity.
// Fills 'pred' with the incoming edge used to reach each vertex.
static bool dfs_find_augmenting_path(Graph &g, Vertex s, Vertex t,
                                     std::vector<char> &seen,
                                     std::vector<EdgeDesc> &pred) {
  auto res = get(edge_residual_capacity, g);

  std::stack<Vertex> st;
  st.push(s);
  seen.assign(num_vertices(g), 0);
  pred.assign(num_vertices(g), EdgeDesc());

  seen[s] = 1;

  while (!st.empty()) {
    Vertex u = st.top();
    st.pop();

    graph_traits<Graph>::out_edge_iterator ei, ei_end;
    // out_edges return tuple of iterators {[first, last)}
    for (tie(ei, ei_end) = out_edges(u, g); ei != ei_end; ++ei) {
      EdgeDesc e = *ei;
      Vertex v = target(e, g);
      if (!seen[v] && res[e] > 0) {
        seen[v] = 1;
        pred[v] = e;
        if (v == t)
          return true; // found augmenting path
        st.push(v);
      }
    }
  }
  return false;
}

// Ford–Fulkerson using DFS augmenting paths.
// Returns the max flow value and leaves residual capacities updated.
long ford_fulkerson_max_flow(Graph &g, Vertex s, Vertex t) {
  auto res = get(edge_residual_capacity, g);
  auto rev = get(edge_reverse, g);

  const long INF = std::numeric_limits<long>::max();
  long max_flow = 0;

  std::vector<char> seen(num_vertices(g), 0);
  std::vector<EdgeDesc> pred(num_vertices(g)); // predecessor edge per vertex

  // While there exists an s->t path in the residual graph
  while (dfs_find_augmenting_path(g, s, t, seen, pred)) {
    // 1) Find bottleneck capacity along the path
    long bottleneck = INF;
    for (Vertex v = t; v != s;) {
      EdgeDesc e = pred[v];
      bottleneck = std::min(bottleneck, res[e]);

      // tail of the edge (ie for  a-->b, v would store 'a' )
      // compliments 'target(edgeDescriptor,graph)'
      // only deterministic in directed graphs (duh)
      v = source(e, g);
    }

    // 2) Augment flow along the path and update residual graph
    for (Vertex v = t; v != s;) {
      EdgeDesc e = pred[v];
      EdgeDesc er = rev[e];  // reverse edge
      res[e] -= bottleneck;  // consume residual on forward edge
      res[er] += bottleneck; // increase residual on reverse edge
      v = source(e, g);
    }

    max_flow += bottleneck;
  }

  return max_flow;
}
