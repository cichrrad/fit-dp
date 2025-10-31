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

using Traits =
    adjacency_list_traits<vecS, distributedS<mpi_process_group, vecS>,
                          directedS>;
using ED = Traits::edge_descriptor;
using VD = Traits::vertex_descriptor;

using Graph =
    adjacency_list<vecS, distributedS<mpi_process_group, vecS>, directedS,
                   property<vertex_distance_t, long>,
                   property<edge_index_t, std::size_t,
                            property<edge_residual_capacity_t, long,
                                     property<edge_capacity_t, long,
                                              property<edge_reverse_t, ED>>>>,
                   vecS>;

int main(int argc, char **argv) {
  boost::mpi::environment env(argc, argv);
  mpi_process_group pg;

  // split between pg
  Graph g(pg);
  // each process adds locally
  auto u = add_vertex(g);
  auto v = add_vertex(g);

  // add edge (with reverse)
  add_edge(u, v, g);
  add_edge(v, u, g);
  auto e1 = out_edges(u, g).first;
  auto e2 = out_edges(v, g).first;

  // distributed maps for edge
  auto cap_map = get(edge_capacity, g);
  auto res_map = get(edge_residual_capacity, g);
  auto rev_map = get(edge_reverse, g);
  auto e_map = get(edge_index, g);
  rev_map[*e1] = *e2;
  rev_map[*e2] = *e1;
  // initialize values
  synchronize(g);
  graph_traits<Graph>::edge_iterator e, end;
  for (tie(e, end) = edges(g); e != end; e++) {

    put(cap_map, *e, 10 * (process_id(pg) + 1));
    std::cout << "   >" << cap_map[*e] << "\n";
    put(res_map, *e, 5 * (process_id(pg) + 1));
    std::cout << "   >" << res_map[*e] << "\n";

    auto t = target(rev_map[*e], g);
    std::cout << "   >" << t.local << "\n";
  }

  std::cout << "I am processor pid " << process_id(pg) << " and I got "
            << num_vertices(g) << " local vertices and " << num_edges(g)
            << " local edges \n";

  auto globalV = boost::parallel::all_reduce(pg, num_vertices(g), std::plus());
  auto globalE = boost::parallel::all_reduce(pg, num_edges(g), std::plus());

  synchronize(g);
  // master branch
  if (process_id(pg) == 0) {

    std::cout << "\n[Master process only]\n"
              << "#Vertices " << globalV << "\n#Edges " << globalE << "\n";
  }
}