#!/bin/bash

# SETUP & CONFIGURATION
TIMEOUT_SEC=300
TIMEOUT_CMD="timeout ${TIMEOUT_SEC}s"

# Ensure output directory exists
mkdir -p logs

# Generate a timestamped log file
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
LOGFILE="logs/log_${TIMESTAMP}.log"

echo "Benchmarking started at $(date)" | tee "$LOGFILE"
echo "Log file: $LOGFILE" | tee -a "$LOGFILE"
echo "---------------------------------------------------" | tee -a "$LOGFILE"

# PHASE 1: GRAPH CONVERSION (Optional)
echo ">>> PHASE 1: Checking and converting graphs..." | tee -a "$LOGFILE"

for dimacs_file in graphs/dimacs/*.dimacs; do
    # Skip if directory is empty
    [ -e "$dimacs_file" ] || continue 
    
    base_name=$(basename "$dimacs_file" .dimacs)
    ecl_file="graphs/ecl/${base_name}.ecl"
    pbbs_file="graphs/pbbs/${base_name}.pbbs"

    # Convert to ECL if missing
    if [ ! -f "$ecl_file" ]; then
        echo "Converting $base_name to ECL..." | tee -a "$LOGFILE"
        ./binaries/dimacs2ecl "$dimacs_file" "$ecl_file" >> "$LOGFILE" 2>&1
    fi

    # Convert to PBBS if missing
    if [ ! -f "$pbbs_file" ]; then
        echo "Converting $base_name to PBBS..." | tee -a "$LOGFILE"
        ./binaries/dimacs2pbbs "$dimacs_file" "$pbbs_file" >> "$LOGFILE" 2>&1
    fi
done

# PHASE 2: RUNNING BENCHMARKS
echo "" | tee -a "$LOGFILE"
echo ">>> PHASE 2: Running benchmark iterations..." | tee -a "$LOGFILE"

run_benchmark() {
    local bin_name="$1"
    local cmd="$2"
    local iter="$3"
    local threads="$4"
    
    # Unified header for parsing
    local header="--- BINARY: ${bin_name} | ITERATION: ${iter} | THREADS: ${threads:-N/A} ---"
    echo "$header" >> "$LOGFILE"
    
    # Run the command, merge stderr to stdout, pipe to tail, and append to log
    eval "$TIMEOUT_CMD $cmd" 2>&1 | tail -n 50 >> "$LOGFILE"
    
    # Capture exit code 'eval'
    local exit_code=${PIPESTATUS[0]}
    
    if [ $exit_code -eq 124 ]; then
        echo "!!! RESULT: TIMEOUT (${TIMEOUT_SEC}s) !!!" >> "$LOGFILE"
    elif [ $exit_code -ne 0 ]; then
        echo "!!! RESULT: FAILED (Exit code: $exit_code) !!!" >> "$LOGFILE"
    fi
    echo "" >> "$LOGFILE" # Add blank line for readability
}

# Iterate through all DIMACS graphs
for dimacs_file in graphs/dimacs/*.dimacs; do
    [ -e "$dimacs_file" ] || continue 
    
    base_name=$(basename "$dimacs_file" .dimacs)
    ecl_file="graphs/ecl/${base_name}.ecl"
    pbbs_file="graphs/pbbs/${base_name}.pbbs"

    # Fetch data about the graph from .dimacs representation
    read nodes edges <<< $(awk '$1=="p" {print $3, $4; exit}' "$dimacs_file")
    source_id=$(awk '$1=="n" && $3=="s" {print $2; exit}' "$dimacs_file")
    sink_id=$(awk '$1=="n" && $3=="t" {print $2; exit}' "$dimacs_file")
    
    # make them 0-indexed
    source_0=$(( ${source_id:-1} - 1 ))
    sink_0=$(( ${sink_id:-2} - 1 ))

    fsize=$(stat -c%s "$dimacs_file" 2>/dev/null || stat -f%z "$dimacs_file")

    # Write Graph Header to Log
    echo "==================================================" >> "$LOGFILE"
    echo "GRAPH_INFO | NAME: $base_name | SIZE_BYTES: $fsize | NODES: $nodes | EDGES: $edges | SOURCE_0: $source_0 | SINK_0: $sink_0" >> "$LOGFILE"
    echo "==================================================" >> "$LOGFILE"
    
    echo "Benchmarking graph: $base_name (Logs appending...)"

    # Run binaries
    for iter in {1..10}; do
        
        # - hipr4
        run_benchmark "hipr4" "./binaries/hipr4 < \"$dimacs_file\"" "$iter" ""
        
        # - ECL_MaxFlow
        # this crashes GPU randomly for some reason, better to run it alone ???
        # might be cause we are running from devcontainer with GPU passthrough, though idk 
        # run_benchmark "ECL_MaxFlow" "./binaries/ECL_MaxFlow \"$ecl_file\" $source_0 $sink_0" "$iter" ""
        
        # - hpf_pseudo_fifo
        run_benchmark "hpf_pseudo_fifo" "./binaries/hpf_pseudo_fifo < \"$dimacs_file\"" "$iter" ""
        
        # - pbbs_syncpar
        run_benchmark "pbbs_syncpar" "./binaries/pbbs_syncpar \"$pbbs_file\"" "$iter" ""
        
        # - knfs_cpu & knfs_gpu (With thread options)
        for t in 0; do
            # run_benchmark "knfs_cpu_AE" "env OMP_PROC_BIND=spread OMP_PLACES=threads ./binaries/knfs_cpu_AE \"$dimacs_file\" $t" "$iter" "$t"
            # run_benchmark "knfs_gpu_AE" "env OMP_PROC_BIND=spread OMP_PLACES=threads ./binaries/knfs_gpu_AE \"$dimacs_file\" $t" "$iter" "$t"
            
            # run_benchmark "knfs_cpu_AP" "env OMP_PROC_BIND=spread OMP_PLACES=threads ./binaries/knfs_cpu_AP \"$dimacs_file\" $t" "$iter" "$t"
            # run_benchmark "knfs_gpu_AP" "env OMP_PROC_BIND=spread OMP_PLACES=threads ./binaries/knfs_gpu_AP \"$dimacs_file\" $t" "$iter" "$t"
              
            # run_benchmark "knfs_cpu_AP2" "env OMP_PROC_BIND=spread OMP_PLACES=threads ./binaries/knfs_cpu_AP2 \"$dimacs_file\" $t" "$iter" "$t"
            # run_benchmark "knfs_gpu_AP2" "env OMP_PROC_BIND=spread OMP_PLACES=threads ./binaries/knfs_gpu_AP2 \"$dimacs_file\" $t" "$iter" "$t"
            
            # run_benchmark "knfs_cpu_old" "env OMP_PROC_BIND=spread OMP_PLACES=threads ./binaries/knfs_cpu_old \"$dimacs_file\" $t" "$iter" "$t"
            # run_benchmark "knfs_gpu_old" "env OMP_PROC_BIND=spread OMP_PLACES=threads ./binaries/knfs_gpu_old \"$dimacs_file\" $t" "$iter" "$t"
            
            run_benchmark "knfs_cpu" "env OMP_PROC_BIND=spread OMP_PLACES=threads ./binaries/knfs_cpu \"$dimacs_file\" $t" "$iter" "$t"
            run_benchmark "knfs_gpu" "env OMP_PROC_BIND=spread OMP_PLACES=threads ./binaries/knfs_gpu \"$dimacs_file\" $t" "$iter" "$t"
            
            run_benchmark "knfs_cpu_multipar" "env OMP_PROC_BIND=spread OMP_PLACES=threads ./binaries/knfs_cpu_multipar \"$dimacs_file\" $t" "$iter" "$t"
            run_benchmark "knfs_gpu_multipar" "env OMP_PROC_BIND=spread OMP_PLACES=threads ./binaries/knfs_gpu_multipar \"$dimacs_file\" $t" "$iter" "$t"
            
        done

    done
done

echo "---------------------------------------------------" | tee -a "$LOGFILE"
echo "Benchmarking complete. Results written to $LOGFILE" | tee -a "$LOGFILE"