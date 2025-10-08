#include "ff.hpp"
#include <limits>
#include <stack>

namespace my_ff {

Edge add_edge_with_capacity(Graph &g, Vertex u, Vertex v, long c) {
  CapMap cap = get(boost::edge_capacity, g);
  ResMap res = get(boost::edge_residual_capacity, g);
  RevMap rev = get(boost::edge_reverse, g);

  bool ok;
  Edge e, er;
  tie(e, ok) = add_edge(u, v, g);
  tie(er, ok) = add_edge(v, u, g);

  cap[e] = c;
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
  ResMap res = get(boost::edge_residual_capacity, g);

  std::stack<Vertex> st;
  st.push(s);
  seen.assign(num_vertices(g), 0);
  pred.assign(num_vertices(g), EdgeDesc());

  seen[s] = 1;

  while (!st.empty()) {
    Vertex u = st.top();
    st.pop();

    boost::graph_traits<Graph>::out_edge_iterator ei, ei_end;
    for (tie(ei, ei_end) = out_edges(u, g); ei != ei_end; ++ei) {
      Edge e = *ei;
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

long ford_fulkerson_max_flow(Graph &g, Vertex s, Vertex t) {
  ResMap res = get(boost::edge_residual_capacity, g);
  RevMap rev = get(boost::edge_reverse, g);

  const long INF = std::numeric_limits<long>::max();
  long max_flow = 0;

  std::vector<char> seen(num_vertices(g), 0);
  std::vector<EdgeDesc> pred(num_vertices(g)); // predecessor edge per vertex

  // While there exists an s->t path in the residual graph
  while (dfs_find_augmenting_path(g, s, t, seen, pred)) {
    // 1) Find bottleneck capacity along the path
    long bottleneck = INF;
    for (Vertex v = t; v != s;) {
      Edge e = pred[v];
      bottleneck = std::min(bottleneck, res[e]);
      v = source(e, g);
    }

    // 2) Augment flow along the path and update the residual graph
    for (Vertex v = t; v != s;) {
      Edge e = pred[v];
      Edge er = rev[e];      // reverse edge
      res[e] -= bottleneck;  // consume residual on forward edge
      res[er] += bottleneck; // increase residual on reverse edge
      v = source(e, g);
    }

    max_flow += bottleneck;
  }

  return max_flow;
}

} // namespace my_ff
