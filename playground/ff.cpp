// ford_fulkerson_bgl.cpp
#include <boost/graph/adjacency_list.hpp>
#include <iostream>
#include <limits>
#include <stack>
#include <vector>

using namespace boost;

// Predeclare edge descriptor type with adjacency_list_traits
using Traits = adjacency_list_traits<vecS, vecS, directedS>;
using EdgeDesc = Traits::edge_descriptor;

// Define our graph with interior edge properties:
//   capacity, residual capacity, and a reverse-edge handle
using Graph =
    adjacency_list<vecS, vecS, directedS, no_property,
                   property<edge_capacity_t, long,
                            property<edge_residual_capacity_t, long,
                                     property<edge_reverse_t, EdgeDesc>>>>;

using Vertex = graph_traits<Graph>::vertex_descriptor;

// Helper: add (u->v) with capacity 'c' and its reverse edge (v->u) with 0
// capacity. Also wire up the reverse-edge map both ways, and initialize
// residuals.
EdgeDesc add_edge_with_capacity(Vertex u, Vertex v, long c, Graph &g) {
  auto cap = get(edge_capacity, g);
  auto res = get(edge_residual_capacity, g);
  auto rev = get(edge_reverse, g);

  bool ok;
  EdgeDesc e, er;

  tie(e, ok) = add_edge(u, v, g);
  tie(er, ok) = add_edge(v, u, g);

  cap[e] = c;
  cap[er] = 0;

  // Residuals start equal to capacity on forward edges; zero on reverse edges
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

int main() {
  // Example network:
  // 0->1(16), 0->2(13)
  // 1->2(10), 1->3(12)
  // 2->1(4),  2->4(14)
  // 3->2(9),  3->5(20)
  // 4->3(7),  4->5(4)
  Graph g(6);

  add_edge_with_capacity(0, 1, 16, g);
  add_edge_with_capacity(0, 2, 13, g);
  add_edge_with_capacity(1, 2, 10, g);
  add_edge_with_capacity(1, 3, 12, g);
  add_edge_with_capacity(2, 1, 4, g);
  add_edge_with_capacity(2, 4, 14, g);
  add_edge_with_capacity(3, 2, 9, g);
  add_edge_with_capacity(3, 5, 20, g);
  add_edge_with_capacity(4, 3, 7, g);
  add_edge_with_capacity(4, 5, 4, g);

  Vertex s = 0, t = 5;

  long flow = ford_fulkerson_max_flow(g, s, t);
  std::cout << "Max flow = " << flow << "\n";
  // expect 23
  return 0;
}
