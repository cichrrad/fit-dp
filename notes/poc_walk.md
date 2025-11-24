### Pseudocode Walkthrough: One Synchronous Iteration

**Context:**

  * **`g`**: An instance of your `Graph<DeviceType>` struct.
  * **`iter`**: Current iteration number (integer).
  * **`threshold`**: The limit for global relabeling (e.g., $12N + 2M$).

-----

#### 1\. The "Process" Kernel (Push, Relabel, & Scan)

Discharge flow, calculate work, and build the next queue.
**Policy:** `Kokkos::parallel_reduce` over `g.current_active`.

```cpp
// DEVICE (KERNEL) CODE 
// Input: index 'i' (from 0 to current_queue_size), Output: 'work_done' (for reduction)
    
    // [1. LOAD NODE & STATE]
    // Coalesced read if 'current_active' is sorted/dense
    int u = g.current_active(i);
    
    // Load state into local registers (fast access)
    long long e_u = g.excess(u);
    int d_u       = g.label(u);
    int min_d     = INFINITY;

    // [2. IDENTIFY WORK & CONTRIBUTE TO REDUCTION]
    // Row map allows random access to the start of the neighbor list
    int start = g.row_map(u);
    int end   = g.row_map(u + 1);
    
    // Accumulate work for the Global Relabeling heuristic (Free metric!)
    work_done += (end - start); 

    // [3. EDGE SCAN LOOP]
    // Sequential memory access on 'entries' and 'residual_capacity' (Coalesced!)
    for (int j = start; j < end; ++j) {
        
        int v = g.entries(j);
        long long cap_uv = g.residual_capacity(j);

        // Skip saturated edges
        if (cap_uv == 0) continue;

        // Random Access (Scatter/Gather) - The main latency cost
        int d_v = g.label(v);

        // Track minimum height for potential relabel (fuse loops)
        if (d_v + 1 < min_d) min_d = d_v + 1;

        // [ADMISSIBILITY CHECK]
        if (d_u == d_v + 1) {
            
            // [WIN CONDITION - CONFLICT AVOIDANCE]
            // Deterministic check. If true, WE own this edge exclusively.
            // (d_u < d_v - 1) || (d_u == d_v + 1) || (d_u == d_v && u < v);
            if (Win(u, v, d_u, d_v)) {
                
                // [PUSH OPERATION]
                long long delta = min(e_u, cap_uv);

                // UPDATE EDGE: Non-Atomic! Safe because of Win condition.
                g.residual_capacity(j) -= delta;
                
                // UPDATE REVERSE EDGE: O(1) lookup via 'reverse_edge' view
                int rev_idx = g.reverse_edge(j);
                g.residual_capacity(rev_idx) += delta;

                // UPDATE SELF: Register only
                e_u -= delta;

                // UPDATE NEIGHBOR FLOW: Atomic! (Multiple nodes might push to v)
                Kokkos::atomic_add(&g.added_excess(v), delta);

                // [ENQUEUE NEIGHBOR]
                // Generational Masking: Check if 'v' is already in next_active for this 'iter'
                // atomic_exchange returns the OLD value.
                int old_color = Kokkos::atomic_exchange(&g.active_iteration_mask(v), iter + 1);
                
                if (old_color != iter + 1) {
                    // We are the first thread to wake 'v' this round.
                    size_t idx = Kokkos::atomic_fetch_add(&g.next_queue_size, 1);
                    g.next_active(idx) = v;
                }
            }
        }
        
        // Early exit if node is fully discharged
        if (e_u == 0) break;
    }

    // [4. RELABEL & RE-ENQUEUE SELF]
    if (e_u > 0) {
        // Node is still active.
        
        // Write to NEW label buffer to preserve 'd_u' for neighbors currently scanning 'u'
        g.new_label(u) = min_d;

        // Re-enqueue 'u' into next_active
        int old_color = Kokkos::atomic_exchange(&g.active_iteration_mask(u), iter + 1);
        if (old_color != iter + 1) {
            size_t idx = Kokkos::atomic_fetch_add(&g.next_queue_size, 1);
            g.next_active(idx) = u;
        }
    }

    // [5. WRITE BACK EXCESS]
    // Update global memory with remaining flow
    g.excess(u) = e_u;
```

-----

#### 2\. Host Logic: Synchronization & Triggers

Manage iterations and heuristics.

```cpp
// HOST (CPU, OUTSIDE OF KERNEL) CODE
size_t total_work_since_relabel = 0;

while (true) {
    size_t step_work = 0;
    
    // Launch Process Kernel
    Kokkos::parallel_reduce("Process", active_count, /*Process lambda*/, step_work);
    Kokkos::fence(); // Wait for GPU

    // [GLOBAL RELABEL HEURISTIC]
    total_work_since_relabel += step_work;
    
    if (total_work_since_relabel > global_relabel_threshold) {
        run_global_relabel_bfs(g);
        total_work_since_relabel = 0;
    }

    // Check for termination
    size_t next_count = 0;
    Kokkos::deep_copy(next_count, g.next_queue_size);
    if (next_count == 0) break;

    // Launch Apply Kernel
    Kokkos::parallel_for("Apply", next_count, /*Apply lambda*/);
    Kokkos::fence();

    // [POINTER SWAP]
    // Logically swap queues for next iteration
    swap(g.current_active, g.next_active);
    iter++;
}
```

-----

#### 3\. The "Apply" Kernel (State Update)

Commit buffered changes to global state.
**Policy:** `Kokkos::parallel_for` over `g.next_active`.

```cpp
// DEVICE (KERNEL) CODE
// each thread gets index i
    
    int u = g.next_active(i);

    // [1. APPLY FLOW]
    // Read the atomic buffer
    long long incoming = g.added_excess(u);
    
    if (incoming > 0) {
        g.excess(u) += incoming;
        
        // Reset buffer for the next round (Critical!)
        g.added_excess(u) = 0; 
    }

    // [2. APPLY LABEL]
    // Check if a new label was proposed during the Process phase
    int d_new = g.new_label(u);
    int d_old = g.label(u);

    // Only update if the new label is valid and higher (monotonicity)
    if (d_new > d_old) {
        g.label(u) = d_new;
        // Note: No need to reset new_label(u), we just overwrite it next time 'u' is processed.
    }
```

### Notes

1.  **Parallel Reduce:** We use reduction in the main kernel to calculate `step_work` essentially for free, avoiding a separate pass to sum edge scans.
2.  **Win Condition:** The `Win` check enables **non-atomic updates** on `residual_capacity`, which is the most frequent memory write in the algorithm.
3.  **Generational Masking:** Using `active_iteration_mask` with `atomic_exchange` lets us manage the queue sparsely without ever running an $O(N)$ kernel to clear flags.
4.  **Memory Layout:** `row_map`, `entries`, and `residual_capacity` are accessed sequentially, ensuring perfect coalescing on GPUs.