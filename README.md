# Assignment of Master's Thesis

| Field | Details |
| :--- | :--- |
| **Title** | **Portable High-Performance Implementation of the Maximum Flow Problem using Kokkos** |
| **Student** | Bc. Radek Cichra |
| **Supervisor** | doc. RNDr. Dušan Knop, Ph.D. |
| **Study Program** | Informatics |
| **Branch / Specialization** | System Programming |
| **Department** | Department of Theoretical Computer Science |
| **Validity** | Until the end of summer semester 2026/2027 |

## Instructions

Graph algorithms are fundamental to solving complex problems in diverse domains, yet their practical implementation on modern heterogeneous hardware presents significant challenges due to irregular memory access patterns and data-dependent control flow.

The goal of this thesis is to investigate the feasibility and performance characteristics of implementing such irregular algorithms using modern **Performance Portability frameworks**. The work will focus specifically on the **Maximum Flow problem** and the **Push-Relabel algorithm family**. Unlike traditional approaches that require maintaining separate codebases for CPUs and GPUs, this thesis aims to utilize the **Kokkos C++ library** to create a single, device-agnostic implementation.

The student will implement a synchronous, bulk-parallel variant of the Push-Relabel algorithm (PRSN), tailored to map efficiently to the throughput-oriented architecture of GPUs while remaining executable on multi-core CPUs. The work will conclude with a comparative study analyzing the trade-offs between high-level abstraction and raw hardware performance.

## Objectives

1.  **Theoretical Analysis:** Review state-of-the-art parallel approaches for the Maximum Flow problem, with a specific focus on synchronous bulk-parallel variants suitable for GPU execution (e.g., *Baumstark et al., 2015*).
2.  **Implementation:** Implement the Synchronous Parallel Push-Relabel (PRSN) algorithm using the **Kokkos ecosystem**. The implementation must be designed to compile for multiple backends (e.g., CPU via OpenMP/Threads, GPU via CUDA/HIP) from a single source codebase.
3.  **Performance Evaluation:** Benchmark the implementation on a representative set of large-scale graphs (e.g., real-world sparse matrices, small-world networks) to evaluate scalability and throughput.
4.  **Comparative Study (optional):** Analyze the "abstraction penalty" (portability tax) and performance portability by comparing execution times and memory bandwidth utilization across different hardware architectures and, where feasible, against available reference implementations.

---

# Repo structure

```
.
├── .devcontainer/              # Environment for dev + running, assumes CUDA GPU 
├── benchmarking/               # Benchmarking scripts, logs, graph instances...
├── helpers/                    # Ruby pre-processing scripts + convertors...
├── input/                      # Input data (not present due to size) + mock
├── profile_tools/              # .so profiling hooks
├── reference_implementations/  # source codes for all reference implementations
├── src/                        # KNFS source files
├── CMakeLists.txt              
├── CMakePresets.json           
├── Gemfile                     # 'package.json', but for Ruby
├── Gemfile.lock                # 'package-lock.json', but for Ruby
├── Makefile                    # (look here for neat build macros)
├── README.md                   
├── knfs_cpu                    # CPU (AMD ZEN 2) backend compiled solver binary 
├── knfs_gpu                    # GPU (NVIDIA AMPERE80) backend compiled solver binary
└── main.cpp                    
```
# How to use

> Note that this assumes CUDA GPU, if you have other vendor, then you must source their software so that you can run code on their hardware and configure `CMakePresets.json` to compile Kokkos for that vendor backend.

If you have Nvidia GPU, then the easiest way to do this is to simply open the workspace in vscode, make sure you have the dev containers extension, then just press `CTRL+SHIFT+P` and select *reopen in container*. It might take a hot minute the first time around, as you will be downloading cuda ubuntu image to run in docker, among other things. Once done, you can use `Makefile` macros to build GPU/CPU variant of the solver with `make gpu` or `make cpu`. First time around this will also take quite some time, because you need to download Kokkos and such. If you dont do `make clean`, most thigns will remain cached in your `build` directory for both cpu and gpu, so following recompilations should take way less time. 

Running the make commands places respective binaries to the root of the directory -- `knfs_cpu` and `knfs_gpu`. These can be then used as any other binary. For quick and dirty demo, just type 
```
make generate_graph ; make to_dimacs ; make run_cpu # or make run_gpu
```
This will generate new mock graph and run the solver on it. To run on some graph, make sure it is standard DIMACS format and just provide it as an argument to the binary like
```
knfs_gpu my_graph.dimacs
```

# Results

To see the logs which were processed as part of the thesis, look into `benchmarking` directory. In `benchmarking/logs/FINAL/`, you can find raw log files for all final benchmark runs (no kernel hooks, kernel timer, hwm, memory usage). For a neat HTML summary page generated by a ruby script to present the main data in a pretty fashion, see `benchmarking/benchmark_dashboard.html`.
