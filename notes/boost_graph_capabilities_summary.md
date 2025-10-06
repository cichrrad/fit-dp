# (AI-generated document)

---

# State of Boost for graphs (BGL & PBGL)

* **BGL = mature, header-only graph library** with generic concepts, graph types (`adjacency_list`, etc.), and a broad algorithm set. ([Boost][1])
* **PBGL = distributed/parallel extension** exposing distributed graphs + algorithms with BGL-like APIs; strong for BFS/SSSP/MST/CC/PageRank/Coloring, etc. ([Boost][2])

## What’s implemented (high-level)

* **Core searches & structure:** BFS/DFS; topological sort; components (connected/strongly/biconnected); planarity tools; orderings (Cuthill–McKee, etc.). ([Boost][1])
* **Shortest paths:** Dijkstra, Bellman–Ford, Johnson, Floyd–Warshall, DAG shortest paths, A\*. ([Boost][1])
* **Spanning trees:** Kruskal, Prim (+ utilities). ([Boost][1])
* **Cuts/centrality/layout:** Stoer–Wagner min-cut; betweenness/closeness/degree centrality; multiple layouts. ([Boost][3])
* **Network flow (sequential, in BGL):**

  * **Max-flow:** Edmonds–Karp; Push–Relabel; Boykov–Kolmogorov. ([Boost][4])
  * **Min-cost max-flow:** Successive shortest path (non-negative weights); Cycle-canceling; plus `find_flow_cost`. ([Boost][5])
* **PBGL distributed algorithms:** BFS; Dijkstra variants (eager, Crauser, delta-stepping); DFS; MST (Borůvka family); CC & SCC; PageRank; graph coloring; force-directed layout; s-t connectivity; betweenness centrality. ([Boost][2])

## Code presence (repos)

* **BGL headers in tree (develop/master):** algorithm headers under `include/boost/graph` (e.g., `push_relabel_max_flow.hpp`, `edmonds_karp_max_flow.hpp`, `stoer_wagner_min_cut.hpp`, min-cost flow headers). ([GitHub][6])
* **PBGL headers:** distributed algorithms under `graph_parallel/include/boost/graph/distributed`. ([GitHub][7])

## Exact list of algorithms in BGL

Taken from [TOC](https://www.boost.org/doc/libs/latest/libs/graph/doc/table_of_contents.html)

* **Algorithms**

  * **Named parameters** (used in many graph algorithms)
  * **Basic Operations**

    * `copy_graph`
    * `transpose_graph`
  * **Core Searches**

    * `breadth_first_search`
    * `breadth_first_visit`
    * `depth_first_search`
    * `depth_first_visit`
    * `undirected_dfs`
  * **Other Core Algorithms**

    * `topological_sort`
    * `transitive_closure`
    * `lengauer_tarjan_dominator_tree`
  * **Shortest Paths / Cost Minimization Algorithms**

    * `dijkstra_shortest_paths`
    * `dijkstra_shortest_paths_no_color_map`
    * `bellman_ford_shortest_paths`
    * `dag_shortest_paths`
    * `johnson_all_pairs_shortest_paths`
    * `floyd_warshall_all_pairs_shortest_paths`
    * `r_c_shortest_paths` — resource-constrained shortest paths
    * `astar_search` (A* search algorithm)
  * **Minimum Spanning Tree Algorithms**

    * `kruskal_minimum_spanning_tree`
    * `prim_minimum_spanning_tree`
  * **Random Spanning Tree Algorithm**

    * `random_spanning_tree`
  * **Algorithm for Common Spanning Trees of Two Graphs**

    * `two_graphs_common_spanning_trees`
  * **Connected Components Algorithms**

    * `connected_components`
    * `strong_components`
    * `biconnected_components`
    * `articulation_points`
    * **Incremental Connected Components**

      * `initialize_incremental_components`
      * `incremental_components`
      * `same_component`
      * `component_index`
  * **Maximum Flow and Matching Algorithms**

    * `edmonds_karp_max_flow`
    * `push_relabel_max_flow`
    * `boykov_kolmogorov_max_flow`
    * `edmonds_maximum_cardinality_matching`
    * `maximum_weighted_matching`
  * **Minimum Cost Maximum Flow Algorithms**

    * `cycle_canceling`
    * `successive_shortest_path_nonnegative_weights`
    * `find_flow_cost`
  * **Minimum Cut Algorithms**

    * `stoer_wagner_min_cut`
  * **Sparse Matrix Ordering Algorithms**

    * `cuthill_mckee_ordering`
    * `king_ordering`
    * `minimum_degree_ordering`
    * `sloan_ordering`
    * `sloan_start_end_vertices`
  * **Graph Metrics**

    * `ith_wavefront`, `max_wavefront`, `aver_wavefront`, `rms_wavefront`
    * `bandwidth`
    * `ith_bandwidth`
    * `brandes_betweenness_centrality`
    * `minimum_cycle_ratio` and `maximum_cycle_ratio`
  * **Graph Structure Comparisons**

    * `isomorphism`
    * `vf2_sub_graph_iso` (VF2 subgraph isomorphism algorithm)
    * `mcgregor_common_subgraphs`
  * **Layout Algorithms**

    * Topologies used as spaces for graph drawing
    * `random_graph_layout`
    * `circle_layout`
    * `kamada_kawai_spring_layout`
    * `fruchterman_reingold_force_directed_layout`
    * `gursoy_atun_layout`
  * **Clustering algorithms**

    * `betweenness_centrality_clustering`
  * **Planar Graph Algorithms**

    * `boyer_myrvold_planarity_test`
    * `planar_face_traversal`
    * `planar_canonical_ordering`
    * `chrobak_payne_straight_line_drawing`
    * `is_straight_line_drawing`
    * `is_kuratowski_subgraph`
    * `make_connected`
    * `make_biconnected_planar`
    * `make_maximal_planar`
  * **Miscellaneous Algorithms**

    * `metric_tsp_approx`
    * `sequential_vertex_coloring`
    * `edge_coloring`
    * `is_bipartite` (including two-coloring of bipartite graphs)
    * `find_odd_cycle`
    * `maximum_adjacency_search`
    * `hawick_circuits` (find all circuits of a directed graph)

## Exact list of algorithms in PBGL

Taken from [PBGL docs](https://www.boost.org/doc/libs/1_89_0/libs/graph_parallel/doc/html/index.html)

* **Distributed algorithms**

  * **Breadth-first search**
  * **Dijkstra’s single-source shortest paths**

    * *Eager Dijkstra shortest paths*
    * *Crauser et al. Dijkstra shortest paths*
    * *Delta-Stepping shortest paths*
  * **Depth-first search**
  * **Minimum spanning tree**

    * *Borůvka’s minimum spanning tree*
    * *Merging local minimum spanning forests*
    * *Borůvka-then-merge*
    * *Borůvka-mixed-merge*
  * **Connected components**

    * *Connected components*
    * *Connected components parallel search*
    * *Strongly-connected components*
  * **PageRank**
  * **Boman et al. graph coloring**
  * **Fruchterman–Reingold force-directed layout**
  * **s–t connectivity**
  * **Betweenness centrality**
  * **Non-distributed betweenness centrality**

## Notable gaps / under-represented areas

* **No distributed max-flow / min-cost flow in PBGL.** The PBGL algorithm list omits flow; existing flow is only in sequential BGL. (Opportunity: parallel push-relabel, Dinic, cost-scaling.) ([Boost][2])
* **Dinic’s algorithm** (sequential) is **not listed** among BGL flow routines—adding a high-quality Dinic with layered networks & blocking flows could fill a common baseline. (Confirm via BGL TOC & flow docs.) ([Boost][8])
* **Min-cost flow features:** limited support for **lower/upper bounds, node demands/supplies, negative costs via potentials,** capacity/cost-scaling variants, and **circulation** formulations. Extending current SSP/cycle-canceling APIs with named parameters & invariants would be valuable. ([Boost][5])
* **Cut utilities:** richer **min s-t cut extraction**, **Gomory–Hu tree** (all-pairs min-cut) helpers are not front-and-center—room for a consistent API layer around existing max-flow/min-cut. (BGL exposes Stoer–Wagner; s-t min-cut is implicit via max-flow.) ([Boost][3])
* **Docs & examples:** PBGL lacks narrative/examples for flow-like workloads; adding **DIMACS I/O** style examples and perf notes (partitioning, ghost vertices, comms) would help adoption. ([Boost][2])

## Links (docs & repos used)

* **BGL docs (index/TOC):** BGL overview & algorithms; table of contents lists min-cost flow pages. ([Boost][1])
* **PBGL docs (index):** Distributed algorithm list (no flow). ([Boost][2])
* **Specific BGL algorithm docs:**

  * Push–Relabel max-flow, Edmonds–Karp max-flow, SSP min-cost, Cycle-canceling, Stoer–Wagner min-cut. ([Boost][9])
* **GitHub trees:**

  * BGL (develop) and (master) `include/boost/graph/` roots; PBGL distributed headers. ([GitHub][6])

[1]: https://www.boost.org/doc/libs/latest/libs/graph/doc/index.html "The Boost Graph Library"
[2]: https://www.boost.org/doc/libs/latest/libs/graph_parallel/doc/html/index.html "Parallel BGL Parallel Boost Graph Library"
[3]: https://www.boost.org/doc/libs/1_83_0/libs/graph/doc/stoer_wagner_min_cut.html?utm_source=chatgpt.com "Stoer–Wagner Min-Cut - Boost Graph Library"
[4]: https://www.boost.org/libs/graph/doc/edmonds_karp_max_flow.html?utm_source=chatgpt.com "Boost Graph Library: Edmonds-Karp Maximum Flow"
[5]: https://www.boost.org/libs/graph/doc/successive_shortest_path_nonnegative_weights.html?utm_source=chatgpt.com "Successive Shortest Path for Min Cost Max Flow"
[6]: https://github.com/boostorg/graph/tree/develop/include/boost/graph "graph/include/boost/graph at develop · boostorg/graph · GitHub"
[7]: https://github.com/boostorg/graph_parallel/tree/develop/include/boost/graph/distributed "graph_parallel/include/boost/graph/distributed at develop · boostorg/graph_parallel · GitHub"
[8]: https://www.boost.org/libs/graph/doc/table_of_contents.html?utm_source=chatgpt.com "Table of Contents: Boost Graph Library"
[9]: https://www.boost.org/libs/graph/doc/push_relabel_max_flow.html?utm_source=chatgpt.com "Push-Relabel Maximum Flow - Boost Graph Library"
