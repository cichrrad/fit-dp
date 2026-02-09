# Used in Baumstark Et al. 2015

## Sequential

[X] [`hpf`](https://riot.ieor.berkeley.edu/Applications/Pseudoflow/maxflow.html)
>takes .dimacs stdin (piping), has 4 modes

[X] [`hi_pr` (no direct link, paper hunted down copy in another project)](https://code.google.com/archive/p/pmaxflow/source/default/source)
>takes .dimacs stdin (piping)

## Parallel

[X] [Whitepaper implementation -- CPU parallel native](https://github.com/niklasb/pbbs-maxflow/tree/master)
> takes PBBS format??? HAS convertor from .dimacs, `syncPar`(prefer) and `goldbergPar`

# Other

* [Handful of solvers AND worst-case generators for MF algorithms](https://github.com/leonard-weininger/worst-case-max-flow/tree/main/algorithms)

* [13th DIMACS challenge page (links back to 1st challenge and also links all generators and reference solvers)](https://coral.ise.lehigh.edu/flow-challenge-2-0/max-flow-instances-and-generators/)

* [`hipr4` should be improved version of hi_pr above (linked on DIMACS 13th challenge))](https://coral.ise.lehigh.edu/flow-challenge-2-0/max-flow-reference-solvers/)

* [Collection of many maxflow_algorithms](https://github.com/patmjen/maxflow_algorithms)

* [`ECL-MaxFlow` -- modern, CUDA native (tied with `ipccc25a.pdf` whitepaper)](https://github.com/burtscher/ECL-MaxFlow/)

* [Gunrock (v1) Max Flow (`app/mf`)](https://github.com/gunrock/gunrock/tree/master)
    * built with  `cmake .. -DCMAKE_CUDA_ARCHITECTURES=80`
    * also need to install boost `sudo apt-get install libboost-all-dev`