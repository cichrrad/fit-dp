#include <boost/graph/use_mpi.hpp>

#include <boost/serialization/serialization.hpp>

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

using Traits =
    adjacency_list_traits<vecS, distributedS<mpi_process_group, vecS>,
                          directedS>;
using ED = Traits::edge_descriptor;
using VD = Traits::vertex_descriptor;

// bundle properties

struct vProperties {
  long index = 0;
  long distance = 0;
  long excess = 0;

  template <class Archive>
  void serialize(Archive &ar, const unsigned int version) {
    ar & index;
    ar & distance;
    ar & excess;
  }
};

struct eProperties {
  long index = 0;
  long capacity = 0;
  long residual_capacity = 0;
  long reverse_edge_res = 0;
  long reverse_edge = 0; // index of reverse edge

  template <class Archive>
  void serialize(Archive &ar, const unsigned int version) {
    ar & index;
    ar & capacity;
    ar & residual_capacity;
    ar & reverse_edge_res;
    ar & reverse_edge;
  }
};

using Graph = adjacency_list<vecS, distributedS<mpi_process_group, vecS>,
                             directedS, vProperties, eProperties, vecS>;

int main(int argc, char **argv) {
  boost::mpi::environment env(argc, argv);
  mpi_process_group pg;

  // split between pg
  Graph g(pg);

  // master branch
  if (process_id(pg) == 0) {
  }
}