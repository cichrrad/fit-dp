#include <boost/graph/adjacency_list.hpp>
#include <iostream>

using graph =
    boost::adjacency_list<boost::vecS, boost::vecS, boost::undirectedS,
                          boost::property<boost::vertex_degree_t, int>,
                          boost::no_property>;

using traits = boost::graph_traits<graph>;

// used for edges / vertices iterators
using vertexDesc = traits::vertex_descriptor;
using edgeDesc = traits::edge_descriptor;

void update_degrees(graph &g) {
  // prepare map storing boost::vertex_degree_t for each vertex in g
  auto degmap = get(boost::vertex_degree, g);

  // iterate all vertices and write t
  for (vertexDesc v : boost::make_iterator_range(vertices(g))) {
    int deg = static_cast<int>(boost::degree(v, g));
    put(degmap, v, deg);
  }
}

int main() {
  graph g(10);

  // make a star
  for (int v = 1; v < 10; ++v)
    add_edge(0, v, g);

  update_degrees(g);

  // create map for degrees (property of vertex -- stored value in each vertex )
  auto degmap = get(boost::vertex_degree, g);
  // read out degrees
  for (vertexDesc v : boost::make_iterator_range(vertices(g))) {
    std::cout << v << " : degree=" << get(degmap, v) << ")\n";
  }
}