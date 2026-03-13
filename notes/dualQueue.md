# IDEA

To introduce per edge parallelism AND allow for "pseudo current arc" optimization --> define 2 type of vertices:

* `lowNode` -- degree $\leq$ `warp_size` * C ($32 \times C$) -- idk what C is ok
* `highNode`-- else

Each type processed differently...

## `lowNode` processing

Same as till now -- range policy, vertex parallel (sequential loop inside) --> we do parallel for over all such nodes, and inside
they do sequential loops over edges

## `highNode` processing

new approach -- team policy, where we dispatch whole warp(s) on 1 vertex to allow edge parallelism. Because we will be working on edges in parallel, trying to decrement from the same excess at once --> **team-wide prefix sum scan** (see below)

### pushing in parallel

since multiple threads will try to push along their edge if its admissible, we must separate this so they first *claim* the amount to push --> with exclusive prefix we will end up with array showing *claimed* flow at current index --> for all threads with $claimed\_flow\_sum > excess$ in their slot, they wont push, while all with $claimed\_flow\_sum \leq excess$ will.

---

#### EXAMPLE

Assume vertex has an excess of **`e_u = 15`**. A team of 4 threads evaluates 4 edges simultaneously:

* **Thread 0** finds an eligible edge with cap = **10**.
* **Thread 1** finds an eligible edge with cap = **8**.
* **Thread 2** finds an eligible edge with cap = **5**.
* **Thread 3** finds an ineligible edge, cap =  **0**.

**Step 1: The Scan**
The Kokkos team scan runs and calculates the exclusive prefix sums:

* Thread 0: `prefix = 0`
* Thread 1: `prefix = 10`
* Thread 2: `prefix = 18`
* Thread 3: `prefix = 23`

**Step 2: The Push**
Each thread applies the formula `min(cap, max(0, e_u - prefix))`:

* **Thread 0:** `min(10, max(0, 15 - 0))` -> Pushes **10**.
* **Thread 1:** `min(8, max(0, 15 - 10))` -> Pushes **5**.
* **Thread 2:** `min(5, max(0, 15 - 18))` -> Pushes **0** (Excess ran out!).
* **Thread 3:** `min(0, max(0, 15 - 23))` -> Pushes **0**.

Total pushed: 10 + 5 + 0 + 0 = **15**.
The excess is perfectly distributed, no atomics were used on `e_u`, and no race conditions occurred!

---

### Chunking

What if vertex has 750 degrees ? We process them in warp chunks (sequential over *warps* of edges, parallel inside) -> `ceil(`$\frac{750}{32}=23.4375$`)`$ = 24$ warp chunks. We could make this even more layered and make this also parallel, BUT making it sequential has some benefits:

* If we dispose of all excess by the second chunk (i.e., within 64 edges), we can break early --> immediate break off once exhausted, no need for the remaining 22 chunks to do anything.

* We can use chunk indices as *pseudo current arc* optimization -- if we exhausted all edges in first four chunks, next (process) iteration can be done from 5th chunk on -- **WE MUST RESET ARC WHEN RELABELING THOUGH!** This is because with new label, old edges can become admissible.

## How to do?

Main change is splitting active queue into 2 queues -- low and high queues + adding current arc to the graph.

## DRAFT -- HIGH NODES PROCESS

```cpp
// 1. Define the Team Policy
// Leagues = number of high-degree nodes in the queue.
// Team Size = Hardware warp/wavefront size (e.g., 32).
Kokkos::TeamPolicy<Device> policy_high(h_current_q_size_high, 32);

Kokkos::parallel_for("process_high_kernel", policy_high, KOKKOS_LAMBDA(const Kokkos::TeamPolicy<Device>::member_type& team) {
    // League rank maps exactly to the active vertex index
    int u = g.current_active_high(team.league_rank());
    
    if (u == s || u == t) return;

    // e_u is kept in a local register for EVERY thread in the team.
    // We will update it uniformly so it stays synchronized.
    long long e_u = g.excess(u);
    const int d_u_start = g.label(u);
    int d_u_current = d_u_start;

    int row_start = g.row_map(u);
    int row_end = g.row_map(u + 1);
    int degree = row_end - row_start;

    // Load the current arc (Thread 0 does it, but we broadcast or just let all threads read it)
    int arc = g.current_arc(u);

    // Loop until discharged or blocked
    while (e_u > 0) {
        unsigned long local_min_d = N + 1;
        int skipped_admissible = 0; 

        // 2. CHUNKING LOGIC
        // We process edges in jumps of team_size()
        for (int chunk_start = arc; chunk_start < degree; chunk_start += team.team_size()) {
            
            // Each thread grabs an edge
            int edge_offset = chunk_start + team.team_rank();
            int idx = row_start + edge_offset;
            bool valid_edge = (edge_offset < degree);

            long long cap = 0;
            int v = -1, d_v = N + 1;
            bool admissible = false, wins = false;

            if (valid_edge) {
                v = g.entries(idx);
                cap = g.residual_capacity(idx);
                if (cap > 0) {
                    d_v = g.label(v);
                    if (d_u_current == d_v + 1) {
                        wins = (d_u_start < d_v - 1) || 
                               (d_u_start == d_v + 1) || 
                               (d_u_start == d_v && u < v);
                        admissible = true;
                    }
                }
            }

            // --- Team Reductions for state ---
            // Get the minimum neighbor distance across the chunk
            
            // NOTE: - ?? Could we merge this reduce with skipped_admissible to 
            // handle both in 1 reduce ??
            // TODO: - Merge the reductions
            unsigned long thread_min_d = (valid_edge && cap > 0) ? d_v : (N + 1);
            unsigned long team_min_d;
            Kokkos::Min<unsigned long> min_reducer(team_min_d);
            Kokkos::parallel_reduce(Kokkos::TeamThreadRange(team, team.team_size()), thread_min_d, min_reducer);
            if (team_min_d < local_min_d) local_min_d = team_min_d;

            // Did anyone skip an admissible edge?
            int thread_skipped = (valid_edge && admissible && !wins) ? 1 : 0;
            int team_skipped;
            Kokkos::parallel_reduce(Kokkos::TeamThreadRange(team, team.team_size()), thread_skipped, team_skipped);
            if (team_skipped > 0) skipped_admissible = 1;


            // 3. TEAM PREFIX SUM FOR PUSHING
            long long req_flow = (valid_edge && admissible && wins && cap > 0) ? cap : 0;
            long long exclusive_prefix = 0;
            long long total_chunk_req = 0;

            // Scan to distribute flow correctly
            Kokkos::parallel_scan(Kokkos::TeamThreadRange(team, team.team_size()), 
                [&](const int thread_rank, long long& update, const bool final_pass) {
                    if (final_pass) { exclusive_prefix = update; }
                    update += req_flow;
            });
            
            // Reduce to get total requested flow by the team
            Kokkos::parallel_reduce(Kokkos::TeamThreadRange(team, team.team_size()), req_flow, total_chunk_req);

            // Calculate actual push based on prefix sum and remaining excess
            long long actual_push = 0;
            if (req_flow > 0) {
                long long available = e_u - exclusive_prefix;
                if (available > 0) {
                    actual_push = (req_flow < available) ? req_flow : available;
                }
            }

            // Apply the flow
            if (actual_push > 0) {
                g.residual_capacity(idx) -= actual_push;
                g.residual_capacity(g.reverse_edge(idx)) += actual_push;
                Kokkos::atomic_add(&g.added_excess(v), actual_push);
            }

            // Uniformly update e_u for ALL threads in the team
            long long total_pushed = (total_chunk_req < e_u) ? total_chunk_req : e_u;
            e_u -= total_pushed;


            // 4. DYNAMIC ENQUEUE WITH BLOCK ATOMICS
            int needs_enqueue = 0;
            int is_high = 0, is_low = 0;

            if (actual_push > 0 && v != s && v != t) {
                int seen = Kokkos::atomic_exchange(&g.active_iteration_mask(v), next_iter_mask);
                if (seen != next_iter_mask) {
                    // NOTE: - This is never OOB access, though it looks
                    // like one (CSR format perk)
                    int n_degree = g.row_map(v + 1) - g.row_map(v);
                    if (n_degree > 32) is_high = 1;
                    else is_low = 1;
                }
            }

            // processing high nodes
            int high_offset = 0;
            Kokkos::parallel_scan(Kokkos::TeamThreadRange(team, team.team_size()), 
                [&](const int dummy, int& update, const bool final_pass) {
                    if (final_pass) high_offset = update;
                    update += is_high;
                });

            // last thread knows the total
            int my_total_h = high_offset + is_high;

            // Broadcast from the last thread (rank = team_size - 1) to the whole team
            int total_high_enq = team.team_broadcast(my_total_h, team.team_size() - 1);

            int high_base = 0;
            if (team.team_rank() == 0 && total_high_enq > 0) {
                // ONE atomic per chunk for the whole team!
                high_base = Kokkos::atomic_fetch_add(&g.next_queue_size_high(), total_high_enq);
            }
            high_base = team.team_broadcast(high_base, 0); // Share base index with the team
            if (is_high) {
                g.next_active_high(high_base + high_offset) = v;
            }

            // [Repeat Block Atomic logic for `is_low` and `next_queue_size_low`]
                        // processing high nodes
            int low_offset = 0;
            Kokkos::parallel_scan(Kokkos::TeamThreadRange(team, team.team_size()), 
                [&](const int dummy, int& update, const bool final_pass) {
                    if (final_pass) low_offset = update;
                    update += is_low;
                });

            // last thread knows the total
            int my_total_l = low_offset + is_low;

            // Broadcast from the last thread (rank = team_size - 1) to the whole team
            int total_low_enq = team.team_broadcast(my_total_l, team.team_size() - 1);

            int low_base = 0;
            if (team.team_rank() == 0 && total_low_enq > 0) {
                // ONE atomic per chunk for the whole team!
                low_base = Kokkos::atomic_fetch_add(&g.next_queue_size_low(), total_low_enq);
            }
            low_base = team.team_broadcast(low_base, 0); // Share base index with the team
            if (is_low) {
                g.next_active_low(low_base + low_offset) = v;
            }

            // 5. EARLY EXIT & CURRENT ARC
            if (e_u == 0) {
                // Thread 0 saves the chunk index to resume later
                if (team.team_rank() == 0) g.current_arc(u) = chunk_start;
                break; // Break the chunk loop
            }
        } // End of chunk loop

        if (e_u == 0 || skipped_admissible) break;

        // 6. RELABEL
        int new_d = local_min_d + 1;
        if (new_d < N + 1 && new_d > d_u_start) {
            d_u_current = new_d;
            if (team.team_rank() == 0) {
                g.new_label(u) = new_d;
                g.current_arc(u) = 0; // CRITICAL: Reset arc on relabel!
            }
            arc = 0; // Rescan from chunk 0
        } else {
            break; // Stuck
        }
    }

    // 7. WRITEBACK & SELF-ENQUEUE
    if (team.team_rank() == 0) {
        g.excess(u) = e_u;
        if (e_u > 0 || d_u_current > d_u_start) {
            int seen = Kokkos::atomic_exchange(&g.active_iteration_mask(u), next_iter_mask);
            if (seen != next_iter_mask) {
                // Self enqueue (since u is already high-degree, put it in high queue)
                size_t pos = Kokkos::atomic_fetch_add(&g.next_queue_size_high(), 1);
                g.next_active_high(pos) = u;
            }
        }
    }
});
```

## DRAFT -- LOW NODES PROCESS

```cpp
// 1. Define standard 1D RangePolicy
Kokkos::RangePolicy<Device> policy_low(0, h_current_q_size_low);

Kokkos::parallel_for("process_low_kernel", policy_low, KOKKOS_LAMBDA(const int& i) {
    int u = g.current_active_low(i);
    
    if (u == s || u == t) return;

    long long e_u = g.excess(u);
    const int d_u_start = g.label(u);
    int d_u_current = d_u_start;

    int row_start = g.row_map(u);
    int row_end = g.row_map(u + 1);
    
    // Load the saved arc (edge offset)
    int current_edge_idx = row_start + g.current_arc(u);

    while (e_u > 0) {
        unsigned long min_d_neighbor = N + 1;
        bool skipped_admissible_edge = false;

        // 2. Resume from current_edge_idx instead of row_start
        for (int idx = current_edge_idx; idx < row_end; ++idx) {
            int v = g.entries(idx);
            long long cap = g.residual_capacity(idx);

            if (cap > 0) {
                int d_v = g.label(v);
                if (d_v < min_d_neighbor) min_d_neighbor = d_v;

                if (d_u_current == d_v + 1) {
                    // Win condition to avoid locking on edges 
                    bool wins = (d_u_start < d_v - 1) || 
                                (d_u_start == d_v + 1) || 
                                (d_u_start == d_v && u < v);

                    if (wins) {
                        long long delta = (e_u < cap) ? e_u : cap;

                        g.residual_capacity(idx) -= delta;
                        g.residual_capacity(g.reverse_edge(idx)) += delta;
                        e_u -= delta;
                        
                        // Must be atomic because other threads might push to v simultaneously 
                        Kokkos::atomic_add(&g.added_excess(v), delta);

                        // 3. Dynamic Routing during Enqueue
                        if (v != s && v != t) {
                            int seen_mask = Kokkos::atomic_exchange(&g.active_iteration_mask(v), next_iter_mask);
                            // If we are the thread that activated v 
                            if (seen_mask != next_iter_mask) {
                                int degree_v = g.row_map(v + 1) - g.row_map(v);
                                
                                // Route to the correct queue!
                                if (degree_v <= 32) {
                                    size_t pos = Kokkos::atomic_fetch_add(&g.next_queue_size_low(), 1);
                                    g.next_active_low(pos) = v;
                                } else {
                                    size_t pos = Kokkos::atomic_fetch_add(&g.next_queue_size_high(), 1);
                                    g.next_active_high(pos) = v;
                                }
                            }
                        }

                        if (e_u == 0) {
                            // 4. Save the arc!
                            // We exhausted our excess. Save the current edge index so we don't 
                            // rescan [row_start, idx-1] next time this vertex is activated.
                            current_edge_idx = idx; 
                            break;
                        }
                    } else {
                        skipped_admissible_edge = true;
                        continue; 
                    }
                }
            }
        } // end for

        if (e_u == 0 || skipped_admissible_edge) break;

        // 5. Relabel
        int new_d = min_d_neighbor + 1;
        if (new_d < N + 1 && new_d > d_u_start) {
            d_u_current = new_d;
            g.new_label(u) = new_d;
            
            // CRITICAL: Reset the arc on relabel!
            current_edge_idx = row_start;
            g.current_arc(u) = 0; 
        } else {
            break;
        }
    }

    // Write back state
    g.excess(u) = e_u;
    
    // Update the global arc tracker if we exited the while loop without relabeling
    if (d_u_current == d_u_start) {
        g.current_arc(u) = current_edge_idx - row_start;
    }

    // 6. Self-Enqueue (u is a low-degree node, so it stays in the low queue)
    if (e_u > 0 || d_u_current > d_u_start) {
        int seen_mask = Kokkos::atomic_exchange(&g.active_iteration_mask(u), next_iter_mask);
        if (seen_mask != next_iter_mask) {
            size_t pos = Kokkos::atomic_fetch_add(&g.next_queue_size_low(), 1);
            g.next_active_low(pos) = u;
        }
    }
});
```