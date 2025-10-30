#include <boost/graph/use_mpi.hpp>

#include <boost/config.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/graph/distributed/adjacency_list.hpp>
#include <boost/graph/distributed/local_subgraph.hpp>
#include <boost/graph/distributed/mpi_process_group.hpp>
#include <boost/graph/parallel/distribution.hpp>
#include <boost/throw_exception.hpp>
#include <cassert>
#include <iostream>

using namespace boost;
using boost::graph::distributed::mpi_process_group;

// graph type
using Graph =
    adjacency_list<vecS, distributedS<mpi_process_group, boost::vecS>,
                   directedS, no_property,
                   property<edge_index_t, std::size_t,
                            property<edge_residual_capacity_t, long,
                                     property<edge_capacity_t, long>>>,
                   property<vertex_distance_t, long>, vecS>;

using GT = boost::graph_traits<Graph>;
using V = GT::vertex_descriptor;
using E = GT::edge_descriptor;

int main(int argc, char **argv) {
  boost::mpi::environment env(argc, argv);
  mpi_process_group pg;

  // split between pg
  Graph g(10, pg);

  // master branch
  if (process_id(pg) == 0) {
  }
}