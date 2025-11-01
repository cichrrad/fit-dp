#include <boost/graph/use_mpi.hpp>

#include <boost/serialization/serialization.hpp>

#include <boost/config.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/graph/distributed/adjacency_list.hpp>
#include <boost/graph/distributed/local_subgraph.hpp>
#include <boost/graph/distributed/mpi_process_group.hpp>
#include <boost/graph/parallel/distribution.hpp>
#include <boost/property_map/parallel/distributed_property_map.hpp>
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

  Graph g(10, pg);

  auto dist_map = get(&vProperties::distance, g);
  // https://www.boost.org/doc/libs/1_87_0/libs/graph_parallel/doc/html/distributed_property_map.html
  dist_map.set_consistency_model(parallel::cm_forward | parallel::cm_backward);

  // everyone can use global vertex descriptors
  auto v = vertex(7, g); // vertex 7, wherever it lives
  if (process_id(pg) == 0) {
    std::cout << "owner of v is " << v.owner << '\n';
    put(dist_map, v, 42L); // if v is remote, this writes ghost + sends msg
  }

  synchronize(pg);
  synchronize(dist_map); // ALL ranks must call this
  v = vertex(7, g);
  long d = get(dist_map, v);
  std::cout << d << " from pid " << process_id(pg) << '\n';
}