```cpp
boost::mpi::environment env(argc, argv); // init mpi in main

// distribute 20 elements
// between pg processes
mpi_process_group pg;
parallel::block dist(pg, 20);

process_id(pg);

// returns vertex iter pair
// for LOCAL vertices of g
// ex boost::tie(v, v_end) = vertices(g);
graph_traits<Graph2>::vertex_iterator v, v_end;
vertices(g);

// return pair of iterators
// for out-edges from v (descriptor)
out_edges(*v, g);

// for getting source and target when iterating
// over edges
auto s = source(*e, g2);
auto t = target(*e, g2);

// get / put for distributed property maps
auto dpm = get(attribute,g);

synchronize(g);
process_group(g).synchronize();

```