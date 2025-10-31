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

// has to be in boost,
// else I get error
namespace boost {
// excess property
struct vertex_excess_t {
  using kind = boost::vertex_property_tag;
};
BOOST_INSTALL_PROPERTY(vertex, excess);
} // namespace boost

using Traits =
    adjacency_list_traits<vecS, distributedS<mpi_process_group, vecS>,
                          directedS>;

using Graph = adjacency_list<
    vecS, distributedS<mpi_process_group, vecS>, directedS, no_property,
    property<
        edge_index_t, std::size_t,
        property<edge_residual_capacity_t, long,
                 property<edge_capacity_t, long,
                          property<edge_reverse_t, Traits::edge_descriptor>>>>,
    property<vertex_distance_t, long, property<vertex_excess_t, long>>, vecS>;

int main(int argc, char **argv) {
  boost::mpi::environment env(argc, argv);
  mpi_process_group pg;

  // split between pg
  Graph g(pg);

  // each process adds locally
  auto u = add_vertex(g);
  auto v = add_vertex(g);

  // distributed maps
  auto cap_map = get(edge_capacity, g);
  auto res_map = get(edge_residual_capacity, g);
  auto rev_map = get(edge_reverse, g);

  // add edge

  std::cout << "I am processor pid " << process_id(pg) << " and I got "
            << num_vertices(g) << " local vertices\n";

  // master branch
  if (process_id(pg) == 0) {
  }
}