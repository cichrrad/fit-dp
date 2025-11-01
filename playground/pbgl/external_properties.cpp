// mwe_ext_props.cpp
#include <boost/graph/use_mpi.hpp>

#include <boost/graph/distributed/adjacency_list.hpp>
#include <boost/graph/distributed/mpi_process_group.hpp>
#include <boost/property_map/parallel/parallel_property_maps.hpp>
#include <boost/property_map/parallel/vector_property_map.hpp>

#include <boost/serialization/serialization.hpp>

using namespace boost;
using boost::graph::distributed::mpi_process_group;

/*
https://www.boost.org/doc/libs/latest/libs/graph_parallel/doc/html/distributed_property_map.html#consistency-models
*/
using boost::parallel::cm_backward;
using boost::parallel::cm_bidirectional;
using boost::parallel::cm_forward;

// we *add* edge_index_t so we can make external edge maps
using e_static = property<edge_index_t, std::size_t>;

using Graph = adjacency_list<vecS, distributedS<mpi_process_group, vecS>,
                             directedS, no_property, e_static>;

int main(int argc, char **argv) {
  mpi::environment env(argc, argv);
  mpi_process_group pg;
  auto pid = process_id(pg);

  const std::size_t N = 12;
  if (N < 2 || pg.size > N) {
    return -1;
  }

  Graph g(N, pg);

  // every process gets their local v/e indexes ranges
  auto v_index = get(vertex_index, g);
  auto e_index = get(edge_index, g);

  // use them to map local storage onto properties
  vector_property_map<long, decltype(v_index)> excess(v_index);
  vector_property_map<long, decltype(e_index)> capacity(e_index);

  excess.set_consistency_model(cm_bidirectional);
  capacity.set_consistency_model(cm_bidirectional);

  // SETUP PASS (Master only)
  if (pid == 0) {
    std::cout << "Building graph..\n";
    std::size_t eid = 0;
    // make a semi ring so that we have
    // cross pid edges
    for (int i = 1; i < N; i++) {
      auto u = vertex(i, g);
      auto v = vertex(i - 1, g);

      put(excess, u, i * 10);
      put(excess, v, (i - 1) * 10);

      graph_traits<Graph>::edge_descriptor e;
      bool ok;
      boost::tie(e, ok) = add_edge(u, v, e_static(eid++), g).commit();
      if (ok) {
        put(capacity, e, (i - 1)); // remote-safe
      }
    }
    // could connect N-1 with 0 to complete ring
  }
  std::cout << "Done building\n";

  synchronize(pg); // wait for master to create ring

  // print local vertices + their outgoing edges
  std::cout << pid << "] TOTAL VERTICES " << num_vertices(g) << '\n';
  std::cout << pid << "] TOTAL EDGES " << num_edges(g) << '\n';

  graph_traits<Graph>::vertex_iterator vi, vi_end;
  for (boost::tie(vi, vi_end) = vertices(g); vi != vi_end; ++vi) {
    auto u = *vi;

    auto u_gidx = get(v_index, u);

    long ex = get(excess, u);
    std::cout << pid << "]      v" << u_gidx << "; excess=" << ex << "\n";

    graph_traits<Graph>::out_edge_iterator ei, ei_end;
    for (boost::tie(ei, ei_end) = out_edges(u, g); ei != ei_end; ++ei) {
      auto e = *ei;
      auto e_gidx = get(e_index, e);
      long cap = get(capacity, e);
      auto tgt = target(e, g);
      auto t_gidx = get(v_index, tgt);
      std::cout << pid << "]        v" << u_gidx << " -e" << e_gidx << "-> v"
                << t_gidx << " cap=" << cap << "\n";
    }
  }

  return 0;
}