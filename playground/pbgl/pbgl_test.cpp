#include <boost/graph/use_mpi.hpp> // MUST be first before any other BGL headers

#include <boost/mpi/communicator.hpp>
#include <boost/mpi/environment.hpp>

#include <boost/graph/distributed/adjacency_list.hpp>
#include <boost/graph/distributed/mpi_process_group.hpp>
#include <boost/graph/iteration_macros.hpp>
#include <boost/graph/properties.hpp> // vertex_owner / vertex_local tags

#include <cstdlib>
#include <iostream>

using boost::graph::distributed::mpi_process_group;

using Graph =
    boost::adjacency_list<boost::vecS,
                          boost::distributedS<mpi_process_group, boost::vecS>,
                          boost::undirectedS, boost::no_property,
                          boost::no_property, boost::no_property>;

static inline void print_vid(const Graph &g, Graph::vertex_descriptor v) {
  // PBGL canonical identity = (owner_rank, local_index)
  auto owner = get(boost::vertex_owner, g, v);
  auto lidx =
      get(boost::vertex_local, g, v); // for vecS this is an integer index
  std::cout << lidx << "@" << owner;
}

int main(int argc, char **argv) {
  boost::mpi::environment env(argc, argv);
  boost::mpi::communicator world;

  const std::size_t N = (argc > 1) ? std::strtoull(argv[1], nullptr, 10) : 12;
  Graph g(N); // distributed graph with N global vertices

  const int rank = process_id(process_group(g));
  const int size = num_processes(process_group(g));

  // Add a few edges (only rank 0 to avoid duplicates)
  if (rank == 0) {
    add_edge(vertex(0, g), vertex(1, g), g);
    add_edge(vertex(0, g), vertex(2, g), g);
    add_edge(vertex(1, g), vertex(2, g), g);
    if (N > 4)
      add_edge(vertex(2, g), vertex(N - 1, g), g); // cross-partition edge
  }

  synchronize(process_group(g)); // finalize construction

  // Show which vertices THIS rank owns, using local_index@owner_rank
  std::cout << "[rank " << rank << "/" << size << "] owns vertices:";
  bool first = true;
  BGL_FORALL_VERTICES_T(v, g, Graph) {
    std::cout << (first ? " " : ", ");
    print_vid(g, v);
    first = false;
  }
  std::cout << "\n";

  // List local out-edges, labeling endpoints with global coords (li@owner)
  BGL_FORALL_VERTICES_T(u, g, Graph) {
    BGL_FORALL_OUTEDGES_T(u, e, g, Graph) {
      auto su = source(e, g);
      auto tv = target(e, g);
      std::cout << "[rank " << rank << "] edge (";
      print_vid(g, su);
      std::cout << " -> ";
      print_vid(g, tv);
      std::cout << ")\n";
    }
  }

  synchronize(process_group(g)); // flush comms before exit
  return 0;
}
