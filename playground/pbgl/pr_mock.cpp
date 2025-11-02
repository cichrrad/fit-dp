
#include <boost/graph/use_mpi.hpp>

#include <boost/graph/distributed/adjacency_list.hpp>
#include <boost/graph/distributed/mpi_process_group.hpp>
#include <boost/property_map/parallel/parallel_property_maps.hpp>
#include <boost/property_map/parallel/vector_property_map.hpp>

using namespace boost;
using boost::graph::distributed::mpi_process_group;

/*
https://www.boost.org/doc/libs/latest/libs/graph_parallel/doc/html/distributed_property_map.html#consistency-models
*/
using boost::parallel::cm_backward;
using boost::parallel::cm_bidirectional;
using boost::parallel::cm_forward;

using e_static = property<edge_index_t, std::size_t>;

using Graph = adjacency_list<vecS, distributedS<mpi_process_group, vecS>,
                             directedS, no_property, e_static>;

int main(int argc, char **argv) {

  //===================================
  // [0] MPI & PROPERTIES SETUP
  //===================================

  mpi::environment env(argc, argv);
  mpi_process_group pg;
  auto pid = process_id(pg);
  auto np = num_processes(pg);

  const std::size_t N = 6;
  // TODO REMOVE THIS CRUX !!!!!
  if (N != 6 || np != 2) {
    return -1;
  }

  // split between pg processes
  Graph g(N, pg);

  // properties setup

  // vertex and edge indexing
  auto vId = get(vertex_index, g);
  auto eId = get(edge_index, g);

  // map indexing onto properties
  // VERTEX PROPERTIES
  vector_property_map<unsigned long, decltype(vId)> height(vId);
  vector_property_map<unsigned long, decltype(vId)> new_height(vId);
  vector_property_map<unsigned long, decltype(vId)> excess(vId);
  vector_property_map<unsigned long, decltype(vId)> added_excess(vId);
  vector_property_map<unsigned long, decltype(vId)> work(vId);
  vector_property_map<unsigned char, decltype(vId)> discovered(vId);
  // EDGE PROPERTIES
  vector_property_map<unsigned long, decltype(eId)> capacity(eId);
  vector_property_map<unsigned long, decltype(eId)> residual(eId);
  // global idx of the reverse edges
  vector_property_map<std::size_t, decltype(eId)> rev_edge(eId);

  // CONSISTENCY MODELS
  // V
  height.set_consistency_model(cm_bidirectional);
  new_height.set_consistency_model(cm_forward);
  excess.set_consistency_model(cm_backward);
  added_excess.set_consistency_model(cm_forward);
  work.set_consistency_model(cm_backward);
  discovered.set_consistency_model(cm_forward);
  // E
  capacity.set_consistency_model(cm_backward);
  residual.set_consistency_model(cm_bidirectional);
  rev_edge.set_consistency_model(cm_backward);

  //===================================
  // [1] INITIALIZATION
  //===================================

  // dummy input
  //                      <u   , v   , cap >
  using eConf = std::tuple<long, long, long>;
  std::vector<eConf> edgeV;

  // each pid wires up their vertices
  // (DEMO ASSUME -np 2 and 6 vertices in graph!)
  // s = 0; t = 5;
  long sid = 0;
  long tid = 5;

  if (pid == 0) {
    edgeV.push_back({0L, 1L, 10L});
    edgeV.push_back({0L, 2L, 7L});
    edgeV.push_back({1L, 2L, 4L});
    // Cross edges
    edgeV.push_back({1L, 3L, 8L});
    edgeV.push_back({2L, 4L, 4L});
  }
  if (pid == 1) {
    edgeV.push_back({3L, 5L, 2L});
    edgeV.push_back({4L, 5L, 2L});
  }

  // GRAPH BUILDING
  for (int i = 0; i < edgeV.size(); i++) {
    const auto &e = edgeV[i];
    const auto &uid = get<0>(e);
    const auto &vid = get<1>(e);
    const auto &cap = get<2>(e);
    // I should own source
    auto u = vertex(uid, g);
    if (u.owner != pid) {
      return 1;
    }
    // vertex setup
    height[u] = (uid == sid ? N : 0);
  }
}