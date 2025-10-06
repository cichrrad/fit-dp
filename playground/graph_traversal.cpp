#include <boost/graph/adjacency_list.hpp>
#include <iostream>

using namespace boost;

using graphType =
    adjacency_list<vecS, vecS, undirectedS, no_property, no_property>;

// access
/* pulls traits from GraphType, then accesses edge/vertex descriptor*/
using edgeDesc = graph_traits<graphType>::edge_descriptor;
using edgeIter = graph_traits<graphType>::edge_iterator;
using vertDesc = graph_traits<graphType>::vertex_descriptor;

int main() {

  // create graph (undirected)
  //       [1]
  //      /  \ 
  //   [0]    [3]
  //      \  /
  //       [2]
  //
  graphType g(4);
  add_edge(0, 1, g);
  add_edge(0, 2, g);
  add_edge(3, 2, g);
  add_edge(3, 1, g);

  // traverse
  for (vertDesc v : make_iterator_range(vertices(g))) {

    for (edgeDesc e : make_iterator_range(out_edges(v, g))) {

      vertDesc u, v;
      u = source(e, g);
      v = target(e, g);
      std::cout << "[" << u << " == " << v << "]\n";
    }
  }
  return 0;
}