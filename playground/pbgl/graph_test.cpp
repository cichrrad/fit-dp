// Copyright (C) 2004-2008 The Trustees of Indiana University.

// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//  Authors: Douglas Gregor
//           Andrew Lumsdaine

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

#ifdef BOOST_NO_EXCEPTIONS
void boost::throw_exception(std::exception const &ex) {
  std::cout << ex.what() << std::endl;
  abort();
}
#endif

using namespace boost;
using boost::graph::distributed::mpi_process_group;

template <typename Graph> struct never {
  typedef typename graph_traits<Graph>::edge_descriptor argument_type;
  typedef bool result_type;

  result_type operator()(argument_type) { return false; }
};

int main(int argc, char **argv) {
  boost::mpi::environment env(argc, argv);

  mpi_process_group pg;
  parallel::block dist(pg, 20);

  typedef adjacency_list<listS, distributedS<mpi_process_group, vecS>,
                         directedS>
      Graph;

  if (num_processes(pg) > 20)
    return -1;

  if (process_id(pg) == 0)
    std::cout << "Graph 2------------------\n";

  {
    Graph g(20);

    // wire between processes
    auto numofp = num_processes(pg);
    if (numofp > 1) {
      int nextpid = process_id(pg) + 1 % numofp;

      graph_traits<Graph>::vertex_iterator vi, vi_end;
      boost::tie(vi, vi_end) = vertices(g);
      // are there vertices?
      if (vi != vi_end) {
        // ~ vi_end--;
        auto u_last = *boost::prior(vi_end);
        std::cout << "I am process " << process_id(pg)
                  << " and my last vertex has ID "
                  // LOCAL ONLY GET
                  << get(vertex_index, g, u_last) << '\n';

        // get first of nextpid % numofp
        // might break if nextpid does not
        // own vertices, but that only
        // happens if numofp > number of
        // elements split between pg
        // (we check for that on line 49-50)
        auto n_global = g.distribution().global(nextpid, 0);
        auto v_first_next = vertex(n_global, g);
        add_edge(u_last, v_first_next, g);
      }
    }

    // should happen to propagate cross-process
    // edges (although we dont work with them)
    synchronize(g);

    graph_traits<Graph>::vertex_iterator v, v_end;
    int counter = 0;
    for (boost::tie(v, v_end) = vertices(g); v != v_end; ++v) {
      std::cout << "[" << counter << ";" << process_id(pg) << "]:\n";

      graph_traits<Graph>::vertex_descriptor u = *v;

      // Add an edge to the "next" local vertex if it exists
      auto vn = v;
      ++vn;
      if (vn != v_end)
        add_edge(u, *vn, g);

      // Iterate outgoing edges of u
      graph_traits<Graph>::out_edge_iterator e, e_end;
      // WE NEED THIS FOR CROSS PROCESS VERTICES
      auto vid = get(vertex_index, g); // distributed vertex index map
      for (tie(e, e_end) = out_edges(u, g); e != e_end; ++e) {
        auto s = source(*e, g);
        auto t = target(*e, g);
        std::cout << "  > edge from " << get(vid, s) << "[" << s.owner
                  << "] --> " << get(vid, t) << "[" << t.owner << "]\n";
      }
    }

    if (num_vertices(g) >= 2) {
      std::cout << "Processor #" << process_id(pg) << ": " << num_edges(g)
                << " edges and " << num_vertices(g) << " vertices.\n";
    }
  }

  return boost::report_errors();
}
