FROM nvidia/cuda:12.3.1-devel-ubuntu22.04

RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends \
    libomp-dev \
    git \
    build-essential \
    cmake \
    wget \
    && apt-get clean -y && rm -rf /var/lib/apt/lists/*

RUN wget https://raw.githubusercontent.com/kokkos/kokkos/master/bin/nvcc_wrapper -O /usr/local/bin/nvcc_wrapper \
    && chmod +x /usr/local/bin/nvcc_wrapper
