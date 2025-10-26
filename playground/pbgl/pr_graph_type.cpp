// mpicxx -O2 -std=c++17 pr_graph_type.cpp -o out \
//   -lboost_graph -lboost_mpi -lboost_serialization

#include <boost/graph/use_mpi.hpp>

#include <boost/graph/distributed/adjacency_list.hpp>
#include <boost/graph/distributed/mpi_process_group.hpp>
#include <boost/graph/properties.hpp>
#include <boost/mpi/communicator.hpp>
#include <boost/mpi/environment.hpp>
#include <cstdint>
#include <iostream>
#include <utility>
#include <vector>

namespace bgd = boost::graph::distributed;

// ---- Custom vertex tags (type-only; use get(tag_type(), g)) ----
namespace boost {
struct vertex_height_t {
  typedef vertex_property_tag kind;
};
struct vertex_next_height_t {
  typedef vertex_property_tag kind;
};
struct vertex_excess_t {
  typedef vertex_property_tag kind;
};
struct vertex_added_excess_t {
  typedef vertex_property_tag kind;
};
struct vertex_discovered_t {
  typedef vertex_property_tag kind;
};
struct vertex_work_t {
  typedef vertex_property_tag kind;
};
BOOST_INSTALL_PROPERTY(vertex, height);
BOOST_INSTALL_PROPERTY(vertex, next_height);
BOOST_INSTALL_PROPERTY(vertex, excess);
BOOST_INSTALL_PROPERTY(vertex, added_excess);
BOOST_INSTALL_PROPERTY(vertex, discovered);
BOOST_INSTALL_PROPERTY(vertex, work);
} // namespace boost

using Cap = long long;

// Bundled properties
using VertexProps = boost::property<
    boost::vertex_height_t, int,
    boost::property<
        boost::vertex_next_height_t, int,
        boost::property<
            boost::vertex_excess_t, Cap,
            boost::property<
                boost::vertex_added_excess_t, Cap,
                boost::property<
                    boost::vertex_discovered_t, unsigned char,
                    boost::property<boost::vertex_work_t, std::uint64_t>>>>>>;

using EdgeProps =
    boost::property<boost::edge_capacity_t, Cap,
                    boost::property<boost::edge_residual_capacity_t, Cap,
                                    boost::property<boost::edge_flow_t, Cap>>>;

using Graph = boost::adjacency_list<
    boost::vecS, boost::distributedS<bgd::mpi_process_group, boost::vecS>,
    boost::directedS, VertexProps, EdgeProps>;

using HeightMap = boost::property_map<Graph, boost::vertex_height_t>::type;
using NextHMap = boost::property_map<Graph, boost::vertex_next_height_t>::type;
using ExcessMap = boost::property_map<Graph, boost::vertex_excess_t>::type;
using AddExMap = boost::property_map<Graph, boost::vertex_added_excess_t>::type;
using DiscMap = boost::property_map<Graph, boost::vertex_discovered_t>::type;
using WorkMap = boost::property_map<Graph, boost::vertex_work_t>::type;

using CapMap = boost::property_map<Graph, boost::edge_capacity_t>::type;
using ResidMap =
    boost::property_map<Graph, boost::edge_residual_capacity_t>::type;
using FlowMap = boost::property_map<Graph, boost::edge_flow_t>::type;

static inline void add_arc_with_reverse(Graph &g, std::size_t gu,
                                        std::size_t gv, Cap cap,
                                        Cap init_flow = 0) {
  auto u = vertex(gu, g), v = vertex(gv, g);

  std::pair<Graph::edge_descriptor, bool> pf = add_edge(u, v, g); // forward
  std::pair<Graph::edge_descriptor, bool> pr = add_edge(v, u, g); // reverse
  auto ef = pf.first;
  auto er = pr.first;

  auto C = get(boost::edge_capacity_t(), g);
  auto R = get(boost::edge_residual_capacity_t(), g);
  auto F = get(boost::edge_flow_t(), g);

  // forward
  put(C, ef, cap);
  put(F, ef, init_flow);
  put(R, ef, cap - init_flow);

  // reverse
  put(C, er, 0);
  put(F, er, 0);
  put(R, er, init_flow);
}

int main(int argc, char **argv) {
  boost::mpi::environment env(argc, argv);
  boost::mpi::communicator world;

  const std::size_t N = 8;
  Graph g(N); // global vertices [0..N)

  if (world.rank() == 0) {
    add_arc_with_reverse(g, 0, 1, 10);
    add_arc_with_reverse(g, 0, 2, 5);
    add_arc_with_reverse(g, 1, 3, 9);
    add_arc_with_reverse(g, 2, 3, 4);
    add_arc_with_reverse(g, 3, 7, 8);
    add_arc_with_reverse(g, 1, 2, 15);
    add_arc_with_reverse(g, 2, 4, 8);
    add_arc_with_reverse(g, 4, 7, 10);
  }
  synchronize(g.process_group());

  // ---- Grab maps ----
  HeightMap h = get(boost::vertex_height_t(), g);
  NextHMap h2 = get(boost::vertex_next_height_t(), g);
  ExcessMap ex = get(boost::vertex_excess_t(), g);
  AddExMap ax = get(boost::vertex_added_excess_t(), g);
  DiscMap dc = get(boost::vertex_discovered_t(), g);
  WorkMap wk = get(boost::vertex_work_t(), g);

  CapMap C = get(boost::edge_capacity_t(), g);
  ResidMap R = get(boost::edge_residual_capacity_t(), g);
  FlowMap F = get(boost::edge_flow_t(), g);

  auto lidx = get(boost::vertex_index, g);
  auto owner = get(boost::vertex_owner, g);

  // ---- 1) Vertex property round-trip checks (local slice) ----
  for (auto [vi, ve] = vertices(g); vi != ve; ++vi) {
    auto i = get(lidx, *vi);
    put(h, *vi, 7 + int(i));
    put(h2, *vi, 100 + int(i));
    put(ex, *vi, 1000 + Cap(i));
    put(ax, *vi, 2000 + Cap(i));
    put(dc, *vi, 1); // mark discovered
    put(wk, *vi, get(wk, *vi) + i);

    bool ok = get(h, *vi) == 7 + int(i) && get(h2, *vi) == 100 + int(i) &&
              get(ex, *vi) == 1000 + Cap(i) && get(ax, *vi) == 2000 + Cap(i) &&
              get(dc, *vi) == 1 && get(wk, *vi) >= i; // monotone
    std::cout << "[rank " << world.rank() << "] vertex local " << i
              << " property round-trip: " << (ok ? "PASS" : "FAIL") << "\n";
  }

  // ---- 2) Edge property round-trip checks (local out-edges) ----
  for (auto [vi, ve] = vertices(g); vi != ve; ++vi) {
    for (auto [ei, ee] = out_edges(*vi, g); ei != ee; ++ei) {
      // write then read (don’t break residual invariants too much)
      Cap c0 = get(C, *ei);
      Cap r0 = get(R, *ei);
      Cap f0 = get(F, *ei);

      put(F, *ei, f0); // idempotent write
      put(R, *ei, r0); // idempotent write
      put(C, *ei, c0); // idempotent write

      bool ok = get(C, *ei) == c0 && get(R, *ei) == r0 && get(F, *ei) == f0;

      auto v = target(*ei, g);
      std::cout << "[rank " << world.rank() << "] edge u(local "
                << get(lidx, *vi) << ") -> "
                << (get(owner, v) == world.rank()
                        ? ("v(local " + std::to_string(get(lidx, v)) + ")")
                        : ("v(REMOTE owner " + std::to_string(get(owner, v)) +
                           ")"))
                << " property round-trip: " << (ok ? "PASS" : "FAIL") << "\n";
    }
  }

  // ---- 3) Cross-process push demo (if any remote target exists) ----
  bool did_remote_test = false;
  for (auto [vi, ve] = vertices(g); vi != ve && !did_remote_test; ++vi) {
    auto u = *vi;
    for (auto [ei, ee] = out_edges(u, g); ei != ee; ++ei) {
      auto v = target(*ei, g);
      int v_owner = get(owner, v);
      if (v_owner == world.rank())
        continue; // need a remote target

      Cap r = get(R, *ei);
      if (r <= 0)
        continue;
      Cap delta = 1;

      // 1) forward residual decreases locally (we own u)
      put(R, *ei, r - delta);

      // 2) credit neighbor's added_excess(v) -- PBGL will route this remote
      // write
      put(ax, v, get(ax, v) + delta);

      did_remote_test = true;
      std::cout << "[rank " << world.rank() << "] remote push test: u(local "
                << get(lidx, u) << ") -> v(owner " << v_owner << ") Δ=" << delta
                << "\n";
      break;
    }
  }

  // Flush all remote puts
  synchronize(g.process_group());

  for (int p = 0; p < world.size(); ++p) {
    world.barrier();
    if (p != world.rank())
      continue;
    std::cout << "=== After synchronize: rank " << world.rank() << " ===\n";
    for (auto [vi, ve] = vertices(g); vi != ve; ++vi) {
      Cap a = get(ax, *vi);
      if (a != 2000 + Cap(get(lidx, *vi))) { // changed from initial pattern?
        std::cout << "  vertex local " << get(lidx, *vi)
                  << " added_excess changed to " << a << "\n";
      }
    }
    for (auto [vi, ve] = vertices(g); vi != ve; ++vi) {
      for (auto [ei, ee] = out_edges(*vi, g); ei != ee; ++ei) {
        auto u = *vi, v = target(*ei, g);
        if (get(R, *ei) > 0) {
          std::cout << "  edge u(local " << get(lidx, u) << ") -> "
                    << (get(owner, v) == world.rank()
                            ? ("v(local " + std::to_string(get(lidx, v)) + ")")
                            : ("v(REMOTE owner " +
                               std::to_string(get(owner, v)) + ")"))
                    << " R=" << get(R, *ei) << "\n";
        }
      }
    }
    std::cout.flush();
  }
  world.barrier();

  return 0;
}
