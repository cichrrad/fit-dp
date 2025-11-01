
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

  const std::size_t N = 12;
  if (N < 2 || pg.size > N) {
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
  vector_property_map<bool, decltype(vId)> discovered(vId);
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

  // TODO everything else ;)
}