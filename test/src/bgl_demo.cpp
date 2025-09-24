#include <boost/graph/adjacency_list.hpp>
#include <iostream>
int main() {
  using G = boost::adjacency_list<>;

  G g(3);
  add_edge(0, 1, g);
  add_edge(1, 2, g);
  std::cout << "BGL ok, vertices=" << num_vertices(g) << "\n";
}
