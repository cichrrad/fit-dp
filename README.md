> For the main KNFS implementation, see [main branch](https://github.com/cichrrad/fit-dp). For the alternative multipar implementation, see [dev_multiparallelism branch](https://github.com/cichrrad/fit-dp/tree/dev_multiparallelism)
---

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
# DISCLAIMER

This software is licensed under GPLv3. It relies on the Kokkos Ecosystem, which is provided under the Apache License 2.0 with LLVM Exceptions.

# How to use

> Note that this assumes CUDA GPU, if you have other vendor, then you must source their software so that you can run code on their hardware and configure `CMakePresets.json` to compile Kokkos for that vendor backend.

## Open the devcontainer to match environment

### OPTION 1: I have NVIDIA GPU

Easiest way to do this is to simply open the workspace in vscode, make sure you have the dev containers extension, then just press `CTRL+SHIFT+P` and select *reopen in container*. It might take a hot minute to build, as you will be downloading cuda ubuntu image to run in docker and potentially compiling features for the container (python and Ruby). I should certainly have put those into post creation `apt-get`, but I worked with this exact container and at this point I am too scared to change it + to keep consistency. Any subsequent opening of the container will be basically instant unless you choose to rebuild it without cache.

If you cannot or refuse to use the attached devcontainer, you dont have to, but you'll need to download and source the dependencies for your GPU vendor etc. Matching the table in thesis **Chapter 5, Section 1** should do the job (modulo different vendor software of course, if thats the case).

### OPTION 2: I have another GPU vendor (AMD,...)

If that is the case, using the devcontainer as-is is not really a good idea, as it fetches specific cuda ubuntu image. Suppose you have AMD GPU -- you can swap the image in `Dockerfile` to be `rocm/dev-ubuntu-22.04` or similar (I am unfamiliar, but AI is good for this kind of stuff) or plain ubuntu and get all the dependencies for running code on that GPU inside the environment (or do it on your machine directly, thats up to you). Once you have this, you should look into `CMakePresets.json` and configure the `gpu` preset so that Kokkos compiles against your GPU. See backends [here](https://kokkos.org/kokkos-core-wiki/get-started/configuration-guide.html). For example, for HIP you would need to add `Kokkos_ENABLE_HIP` to the configuration variables. Just keep in mind only one device backend can be activated.

In addition to this, if you want to use the support scripts and to have full functionality, you need to also have Ruby (*latest* on ubuntu should do) installed as I use helper ruby scripts in some `make` macros and also to parse logs for the HTML report page.

### OPTION 3: I will only use CPU / I have no GPU

In that case, you can use plain ubuntu but then I am not sure if all dependencies are met. Alternative is to use the container, just comment out the parts which set up gpu passthrough in `devcontainer.json`, namely:
```json
"runArgs": [
  "--gpus", "all"
],
```
and
```json
"remoteEnv": {
  "PATH": "${containerEnv:PATH}:/usr/local/cuda/bin",
  "LD_LIBRARY_PATH": "${containerEnv:LD_LIBRARY_PATH}:/usr/local/cuda/lib64"
},
```
You won't be able to run GPU backend compiled variant of the solver (duh). I did this on my laptop without dedicated GPU and I was able to build the container and compile and run the CPU backend binary no problem.

## Building & running the solver binary

You can use `Makefile` macros to build GPU/CPU variant of the solver with `make gpu` or `make cpu`. First time around this will also take quite some time, because you need to download Kokkos. If you dont do `make clean`, most things will remain cached in your `build` directory for both cpu and gpu after first build, so following recompilations should take way less time.

Running the make commands places respective binaries --`knfs_cpu` and `knfs_gpu`-- to the root of the directory. These can be then used as any other binary. For quick and dirty demo, just type 

```
make generate_graph ; make to_dimacs ; make run_cpu # or make run_gpu
```
This will generate new mock graph and run the solver on it. To run on specific graph, make sure it is standard DIMACS format and just provide it as an argument to the binary like
```
knfs_gpu my_graph.dimacs
```

There is a small graph in `benchmarking/graph/dimacs/` you can test this on + so the benchmarking pipeline has some work.

# Results

To see the logs which were processed as part of the thesis, look into `benchmarking` directory. In `benchmarking/logs/FINAL/`, you can find raw log files for all final benchmark runs (no kernel hooks, kernel timer, hwm, memory usage). For a neat HTML summary page generated by a ruby script `master_parse.rb` to present the main data in a pretty fashion, see `benchmarking/benchmark_dashboard.html`.

# Run your own benchmarks

I hopefully set this up so that it is fairly easy to do this. In `benchmarking` directory, you have `graphs/dimacs` directory. This is where all the graph instances you want to run solvers on go. Just place the graph in standard dimacs format there and you can then run `./benchmark.sh` from the root of `benchmarking` dir. What happens is the script tries to match each graph in `dimacs` with its converted format in `graphs/ecl` and `graph/pbbs` for ECL and syncpar solvers. If they dont exist, thats fine, as it reaches into `binaries` directory and runs conversion binary (provided by the solvers, I just moved them here for ease of use). **DO NOT** rename graphs once you place them into the `dimacs` directory, as the matching is name based and you will create redundant copies of same graph. After this initial matching phase, benchmarking starts. Look at line 88 and onward in `benchmark.sh` and see, its a simple loop injecting headers for graph instances and then also solver runs and iterations, so that later parsing is much easier. ECL is commented out by default due to instability. Alternative KNFS solver is also commented out by default.

Running the benchmark will produce timestamped log in `logs`. You can then run `ruby master_parse.rb logs/my_log_file.log` to generate html dashboard summary.
