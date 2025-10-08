#include "../src/dinic.hpp"
#include "../src/ff.hpp"
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/edmonds_karp_max_flow.hpp>
#include <chrono>
#include <ctime>
#include <iostream>
#include <limits>
#include <queue>
#include <random>
#include <stack>
#include <vector>

using namespace boost;

const int VERTEX_COUNT = 10000;
const int CAPACITY_MIN = 1;
const int CAPACITY_MAX = 1e4;
const int EDGES_PER_V = 5.0;
const int RUNS = 10;

using Traits = adjacency_list_traits<vecS, vecS, directedS>;
using EdgeDesc = Traits::edge_descriptor;

using Graph =
    adjacency_list<vecS, vecS, directedS, no_property,
                   property<edge_capacity_t, long,
                            property<edge_residual_capacity_t, long,
                                     property<edge_reverse_t, EdgeDesc>>>>;

using Vertex = graph_traits<Graph>::vertex_descriptor;

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
  res[e] = c;
  res[er] = 0;
  rev[e] = er;
  rev[er] = e;
  return e;
}

void build_sample_graph(Graph &g1, Graph &g2, Graph &g3) {
  std::random_device rd;
  std::mt19937 gen(rd());

  const int vertices = VERTEX_COUNT;
  const int num_edges = static_cast<int>(vertices * EDGES_PER_V);

  std::uniform_int_distribution<int> vertex_dist(0, vertices - 1);
  std::uniform_int_distribution<int> capacity_dist(CAPACITY_MIN, CAPACITY_MAX);

  for (int i = 0; i < num_edges; ++i) {
    int u = vertex_dist(gen);
    int v = vertex_dist(gen);
    if (u == v)
      continue; // no self-loops
    long c = capacity_dist(gen);
    add_edge_with_capacity(u, v, c, g1);
    add_edge_with_capacity(u, v, c, g2);
    add_edge_with_capacity(u, v, c, g3);
  }

  // std::cout << "====================================================\n";
  // std::cout << "[INFO] Generated random graph with " << num_edges
  //           << " edges and " << vertices << " vertices.\n";
}

int main() {
  using Clock = std::chrono::steady_clock;
  using ns = std::chrono::nanoseconds;

  long long total_ff_ns = 0;
  long long total_ek_ns = 0;
  long long total_dn_ns = 0;

  for (int run_id = 0; run_id < RUNS; ++run_id) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> vertex_dist(0, VERTEX_COUNT - 1);

    Vertex s, t;
    do {
      s = vertex_dist(gen);
      t = vertex_dist(gen);
    } while (s == t);

    std::cout << "[INFO] Using s=" << s << ", t=" << t << "\n";

    // Build three identical random graphs (FF, EK, Dinic)
    Graph g_ff(VERTEX_COUNT), g_ek(VERTEX_COUNT), g_dn(VERTEX_COUNT);
    build_sample_graph(g_ff, g_ek, g_dn);

    // FF
    auto start = Clock::now();
    long flow_ff = my_ff::ford_fulkerson_max_flow(g_ff, s, t);
    auto end = Clock::now();
    auto ff_ns = std::chrono::duration_cast<ns>(end - start).count();
    total_ff_ns += ff_ns;

    // EK
    start = Clock::now();
    long flow_ek = edmonds_karp_max_flow(g_ek, s, t);
    end = Clock::now();
    auto ek_ns = std::chrono::duration_cast<ns>(end - start).count();
    total_ek_ns += ek_ns;

    // Dinic
    start = Clock::now();
    long flow_dn = my_dinic::Dinic(g_dn).max_flow(s, t);
    end = Clock::now();
    auto dn_ns = std::chrono::duration_cast<ns>(end - start).count();
    total_dn_ns += dn_ns;

    std::cout << "> FF    max flow = " << flow_ff << "  (" << ff_ns << " ns, "
              << (ff_ns / 1000) << " us)\n";
    std::cout << "> EK    max flow = " << flow_ek << "  (" << ek_ns << " ns, "
              << (ek_ns / 1000) << " us)\n";
    std::cout << "> Dinic max flow = " << flow_dn << "  (" << dn_ns << " ns, "
              << (dn_ns / 1000) << " us)\n";

    if (flow_ff == flow_ek && flow_ek == flow_dn) {
      // life is good
    } else {
      std::cout << "[" << run_id << "][WRONG] Results differ\n";
      return -1;
    }
  }

  auto avg = [](long long total_ns) {
    return total_ns / static_cast<double>(RUNS);
  };

  std::cout << "[OK] All " << RUNS << " runs passed.\n";
  std::cout << "Totals over " << RUNS << " runs:\n";
  std::cout << ">FF total:   " << total_ff_ns
            << " ns  | avg: " << avg(total_ff_ns) << " ns  (~"
            << (avg(total_ff_ns) / 1e6) << " ms)\n";
  std::cout << ">EK total:   " << total_ek_ns
            << " ns  | avg: " << avg(total_ek_ns) << " ns  (~"
            << (avg(total_ek_ns) / 1e6) << " ms)\n";
  std::cout << ">Dinic total:" << total_dn_ns
            << " ns  | avg: " << avg(total_dn_ns) << " ns  (~"
            << (avg(total_dn_ns) / 1e6) << " ms)\n";
  return 0;
}
