// mpicxx -O2 ring.cpp -o out \
//   -lboost_mpi -lboost_serialization
// mpirun -np X ./out

#include <boost/graph/use_mpi.hpp>
#include <boost/mpi/communicator.hpp>
#include <boost/mpi/environment.hpp>
#include <iostream>
#include <vector>

#include <boost/graph/distributed/adjacency_list.hpp>
#include <boost/graph/distributed/mpi_process_group.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/parallel/process_group.hpp>
#include <boost/graph/properties.hpp>

int main(int argc, char **argv) {
  namespace bgd = boost::graph::distributed;
  boost::mpi::environment env(argc, argv);
  boost::mpi::communicator world;

  using Graph = boost::adjacency_list<
      boost::vecS, boost::distributedS<bgd::mpi_process_group, boost::vecS>,
      boost::directedS>;

  const std::size_t N = 12; // total global vertices
  Graph g(N);               // collective: partitions [0..N)

  // Build a simple ring (on rank 0 -- can be distributed as well)
  if (world.rank() == 0) {
    for (std::size_t i = 0; i < N; ++i) {
      auto u = vertex(i, g);
      auto v = vertex((i + 1) % N, g);
      add_edge(u, v, g);
    }
  }

  // Finalize construction across ranks.
  synchronize(g.process_group());

  // Helpful property maps.
  auto owner = get(boost::vertex_owner, g); // who owns a vertex
  auto lidx = get(boost::vertex_index, g);  // local index (0..local-1)

  // Print in rank order to avoid interleaving.
  for (int p = 0; p < world.size(); ++p) {
    world.barrier();
    if (world.rank() != p)
      continue;

    std::cout << "=== Process " << world.rank() + 1 << " / " << world.size()
              << " ===\n";

    // List local vertices by LOCAL index.
    std::vector<std::size_t> locals;
    for (auto [vi, vi_end] = vertices(g); vi != vi_end; ++vi) {
      locals.push_back(get(lidx, *vi));
    }
    std::cout << "Local vertices (count " << locals.size() << "): ";
    if (locals.empty())
      std::cout << "(none)\n";
    else {
      std::cout << "[";
      for (std::size_t i = 0; i < locals.size(); ++i) {
        if (i)
          std::cout << ", ";
        std::cout << locals[i];
      }
      std::cout << "]\n";
    }

    // For each local vertex, show out-edges and flag cross-process targets.
    for (auto [vi, vi_end] = vertices(g); vi != vi_end; ++vi) {
      auto u = *vi;
      auto u_lidx = get(lidx, u);

      std::cout << "  from local vertex " << u_lidx << ":\n";

      auto [ei, ei_end] = out_edges(u, g);
      if (ei == ei_end) {
        std::cout << "    (no outgoing edges)\n";
        continue;
      }

      for (; ei != ei_end; ++ei) {
        auto v = target(*ei, g);
        int v_owner = get(owner, v);

        if (v_owner == world.rank()) {
          // Target is local
          auto v_lidx = get(lidx, v);
          std::cout << "    -> local  vertex " << v_lidx << "\n";
        } else {
          // Target is remote.
          std::cout << "    -> REMOTE vertex on process " << v_owner + 1
                    << "\n";
        }
      }
    }

    std::cout.flush();
  }
  world.barrier();
  return 0;
}
