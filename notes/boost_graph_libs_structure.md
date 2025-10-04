# (AI-generated document)

---

# How Boost thinks about graphs

* **Concepts-first.** Algorithms don’t care about your concrete type; they rely on *graph concepts* (e.g., `IncidenceGraph`, `VertexListGraph`, `EdgeListGraph`). You traverse with free functions like `vertices(g)`, `edges(g)`, `out_edges(u,g)`, and get endpoints via `source(e,g)`, `target(e,g)`. ([boost.org][1])
* **Descriptors & iterators.** A vertex is a `vertex_descriptor`; an edge is an `edge_descriptor`. You iterate using `*_iterator` pairs returned from `vertices(g)`, `edges(g)`, `out_edges(u,g)`, etc. (undirected graphs still use `out_edges`/`in_edges` APIs). ([boost.org][2])
* **Property maps everywhere.** Algorithm state and graph attributes are accessed via *property maps* (`get`, `put`, `operator[]`). They can be internal (stored on the graph) or external (e.g., an `iterator_property_map` over a `std::vector`). ([boost.org][3])

# Core types you’ll actually use (BGL)

* **`adjacency_list<...>`** — the flexible default. Choose containers for vertex/edge storage, directedness, and attach properties (either *tagged* internal props or *bundled* structs). ([boost.org][4])

  * *Bundled properties* let you define `struct Vertex { ... }; struct Edge { double capacity; ... };` and use `g[v].field` / `g[e].field`. ([boost.org][5])
* **`compressed_sparse_row_graph` (CSR)** — static, cache-friendly, fast for SSSP/MST/PR; attach properties via bundled/internal mechanisms. Prefer when topology is fixed. ([boost.org][6])

# Property maps (practical)

* Get a built-in map: `auto idx = get(vertex_index, g);` or `auto cap = get(edge_capacity, g);`. For your own bundled props, access via `g[e].capacity` or with `get(&Edge::capacity, g)`. ([boost.org][7])
* External scratch maps: `std::vector<int> dist(num_vertices(g)); auto dist_map = make_iterator_property_map(dist.begin(), idx);` (works with all algos). ([boost.org][3])

# Named parameters (how algos are called)

* BGL uses *named parameters* so calls read clearly and order doesn’t matter:
  `bellman_ford_shortest_paths(g, n, weight_map(w).distance_map(d).predecessor_map(p));` ([boost.org][8])

# Visitors (instrumentation & hooks)

* Most traversal/shortest-path algos accept a `*_visitor` to hook events (`discover_vertex`, `tree_edge`, …). This is how BGL exposes extensibility without subclassing graph nodes. ([Brown University Computer Science][9])

# Building / loading graphs

* **Programmatic build:**

  ```cpp
  using G = adjacency_list<vecS, vecS, directedS,
                           VertexProps, EdgeProps>;
  G g;
  auto u = add_vertex(g), v = add_vertex(g);
  auto [e,ok] = add_edge(u, v, EdgeProps{.capacity = 10}, g);
  ```

  (Bundled props filled inline on `add_edge`.) ([Stack Overflow][10])
* **From DOT (Graphviz):** `read_graphviz(in, g, dp, "node_id");` Handy for quick tests and examples. Mind directed vs. undirected matching the file. ([boost.org][11])
* **From edge lists / arrays:** CSR has constructors from edge ranges; there’s also an `edge_list` helper type for batch init. ([boost.org][12])

# Typical iteration patterns (cheats)

* Over vertices:

  ```cpp
  for (auto [vi, ve] = vertices(g); vi != ve; ++vi) { auto v = *vi; /* ... */ }
  ```
* Over out-edges of `u`:

  ```cpp
  for (auto [ei, ee] = out_edges(u, g); ei != ee; ++ei) {
    auto e = *ei; auto v = target(e, g);
  }
  ```

  (In undirected graphs `out_edges` == incident edges; `in_edges` requires `bidirectionalS`.) ([Brown University Computer Science][13])

# How existing flow algos are wired (mirror this)

* **Max-flow** (`edmonds_karp_max_flow`, `push_relabel_max_flow`, `boykov_kolmogorov_max_flow`) expect:

  * `capacity_map`, `residual_capacity_map`, **and** `reverse_edge_map`; plus `vertex_index_map`, optional color/queue maps.
  * You must create reverse edges for every edge you add. ([boost.org][4])
* **Min-cost flow** (`successive_shortest_path_nonnegative_weights`, `cycle_canceling`) expect:

  * `weight_map` (costs), the same capacity/residual/reverse maps, and you compute total cost via `find_flow_cost`. ([boost.org][5])

(Grab these function docs when in doubt and mirror their named-parameter style.)

# PBGL (distributed mindset)

* **Main type:** `distributed_adjacency_list` (vertices partitioned across processes; local + ghost vertices). API mirrors `adjacency_list` closely. ([boost.org][14])
* **Distributed property maps:** wrap a local map + MPI process group; `get/put` work transparently across ranks. ([boost.org][15])
* **Algorithms available:** distributed BFS/DFS, Dijkstra (incl. delta-stepping), Borůvka MST, connected/strongly-connected components, PageRank, coloring, FR layout, s–t connectivity, betweenness centrality. ([boost.org][14])
* **Data structures:** PBGL also describes a distributed CSR variant used by SSSP papers and examples. Pick based on workload and mutability. ([DIAg][16])

# Minimal checklists you’ll reuse

**A. “I want to run a BGL algorithm” checklist**

1. Choose graph type (`adjacency_list` or CSR). 2) Ensure you have `vertex_index` (auto for `vecS`). 3) Prepare required property maps (internal or external). 4) Call algo with named params (+ optional visitor). ([boost.org][4])

**B. “I need flow-like residuals” checklist**

* Add reverse edges when building the graph.
* Maintain `capacity`, `residual_capacity`, and `reverse_edge` maps.
* For min-cost, also provide `weight_map` and use `find_flow_cost` after. ([boost.org][4])

**C. “I need to scale out (PBGL)” checklist**

* Use `distributed_adjacency_list`.
* Create distributed property maps for state.
* Use PBGL algorithms; avoid fine-grained remote `put` in hot loops; batch when possible. ([boost.org][17])

# Handy links (docs & headers)

* **BGL index / quick tour / concepts:**

  * Index + overview; quick tour; graph concepts & traversal. ([boost.org][1])
* **Key structure pages:**

  * `adjacency_list`; CSR graph; bundled properties; property maps primer. ([boost.org][4])
* **Named parameters:** how to pass `weight_map(...)` etc. ([boost.org][8])
* **Graph I/O:** `read_graphviz` reference & notes. ([boost.org][11])
* **Visitors:** TOC for visitor concepts. ([Brown University Computer Science][9])
* **PBGL overview & distributed types/maps:** overview; distributed adjacency list; distributed property maps. ([boost.org][14])

---

[1]: https://www.boost.org/doc/libs/1_80_0/libs/graph/doc/index.html?utm_source=chatgpt.com "The Boost Graph Library"
[2]: https://www.boost.org/doc/libs/1_42_0/libs/graph/doc/graph_traits.html?utm_source=chatgpt.com "Graph Traits"
[3]: https://www.boost.org/libs/graph/doc/using_property_maps.html?utm_source=chatgpt.com "Using Property Maps - Boost Graph Library"
[4]: https://www.boost.org/doc/libs/1_89_0/libs/graph/doc/adjacency_list.html?utm_source=chatgpt.com "Boost Graph Library: Adjacency List"
[5]: https://www.boost.org/doc/libs/1_81_0/libs/graph/doc/bundles.html?utm_source=chatgpt.com "Bundled Properties"
[6]: https://www.boost.org/doc/libs/1_61_0/libs/graph/doc/compressed_sparse_row.html?utm_source=chatgpt.com "Compressed Sparse Row Graph"
[7]: https://www.boost.org/libs/graph/doc/quick_tour.html?utm_source=chatgpt.com "Quick Tour of Boost Graph Library"
[8]: https://www.boost.org/libs/graph/doc/bgl_named_params.html?utm_source=chatgpt.com "Boost Graph Library: Named Parameters"
[9]: https://cs.brown.edu/~jwicks/boost/libs/graph/doc/table_of_contents.html?utm_source=chatgpt.com "Table of Contents: Boost Graph Library"
[10]: https://stackoverflow.com/questions/37594052/adding-edges-to-a-graph-in-boost-graph?utm_source=chatgpt.com "Adding edges to a graph in Boost.Graph - c++"
[11]: https://www.boost.org/doc/libs/1_37_0/libs/graph/doc/read_graphviz.html?utm_source=chatgpt.com "Boost read_graphviz"
[12]: https://www.boost.org/doc/libs/1_65_0/libs/graph/doc/edge_list.html?utm_source=chatgpt.com "Boost Graph Library: Edge List Class"
[13]: https://cs.brown.edu/~jwicks/boost/libs/graph/doc/adjacency_list.html?utm_source=chatgpt.com "Boost Graph Library: Adjacency List"
[14]: https://www.boost.org/doc/libs/1_88_0/libs/graph_parallel/doc/html/overview.html?utm_source=chatgpt.com "An Overview of the Parallel Boost Graph Library"
[15]: https://www.boost.org/doc/libs/1_71_0/libs/graph_parallel/doc/html/distributed_property_map.html?utm_source=chatgpt.com "Parallel BGL Distributed Property Map"
[16]: https://www.diag.uniroma1.it/challenge9/papers/edmonds.pdf?utm_source=chatgpt.com "Single-Source Shortest Paths with the Parallel Boost Graph ..."
[17]: https://www.boost.org/doc/libs/1_65_0/libs/graph_parallel/doc/html/distributed_adjacency_list.html?utm_source=chatgpt.com "Parallel BGL Distributed Adjacency List"
