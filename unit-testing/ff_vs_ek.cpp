#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/edmonds_karp_max_flow.hpp>
#include <ctime>
#include <iostream>
#include <limits>
#include <random>
#include <stack>
#include <vector>

using namespace boost;

const int VERTEX_COUNT = 1000;
const int CAPACITY_MIN = 1;
const int CAPACITY_MAX = 500;
const float EDGES_PER_V = 4.0;
const int RUNS = 100;

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

// TODO
// wire-in boost graph generators to produce random
// graph and place it into g1,g2
// graph generators links:
// https://github.com/boostorg/graph/blob/boost-1.89.0/include/boost/graph/plod_generator.hpp
// https://github.com/boostorg/graph/blob/boost-1.89.0/include/boost/graph/mesh_graph_generator.hpp
// https://github.com/boostorg/graph/blob/boost-1.89.0/include/boost/graph/rmat_graph_generator.hpp
// https://github.com/boostorg/graph/blob/boost-1.89.0/include/boost/graph/ssca_graph_generator.hpp
// https://github.com/boostorg/graph/blob/boost-1.89.0/include/boost/graph/erdos_renyi_generator.hpp
// https://github.com/boostorg/graph/blob/boost-1.89.0/include/boost/graph/small_world_generator.hpp
void build_sample_graph(Graph &g1, Graph &g2) {

  std::random_device rd;
  std::mt19937 gen(rd());

  const int vertices = VERTEX_COUNT;
  const int num_edges = vertices * EDGES_PER_V;

  std::uniform_int_distribution<int> vertex_dist(0, vertices - 1);
  std::uniform_int_distribution<int> capacity_dist(CAPACITY_MIN, CAPACITY_MAX);

  // add random directed edges with random capacities
  for (int i = 0; i < num_edges; ++i) {
    int u = vertex_dist(gen);
    int v = vertex_dist(gen);
    if (u == v)
      continue; // skip self-loops

    long c = capacity_dist(gen);
    add_edge_with_capacity(u, v, c, g1);
    add_edge_with_capacity(u, v, c, g2);
  }

  std::cout << "====================================================\n";
  std::cout << "[INFO] Generated random graph with " << num_edges
            << " edges and " << vertices << " vertices.\n";
}

int main() {

  for (int run_id = 0; run_id < RUNS; run_id++) {

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> vertex_dist(0, VERTEX_COUNT - 1);

    Vertex s, t;
    do {
      s = vertex_dist(gen);
      t = vertex_dist(gen);
    } while (s == t); // s ≠ t
    std::cout << "[INFO] Using s=" << s << ", t=" << t << "\n";

    // Build two identical random graphs
    Graph g_ff(VERTEX_COUNT), g_ek(VERTEX_COUNT);
    build_sample_graph(g_ff, g_ek);

    long flow_ff = ford_fulkerson_max_flow(g_ff, s, t);
    long flow_ek = edmonds_karp_max_flow(g_ek, s, t);

    std::cout << "> FF max flow = " << flow_ff << "\n";
    std::cout << "> EK max flow = " << flow_ek << "\n";

    if (flow_ff == flow_ek) {
      std::cout << "[" << run_id << "][OK] Results match\n";
    } else {
      std::cout << "[" << run_id << "][WRONG] Results differ\n";
      return -1;
    }
    std::cout << "====================================================\n";
  }
  std::cout << "   [OK] All " << RUNS << " runs passed.\n";
  return 0;
}