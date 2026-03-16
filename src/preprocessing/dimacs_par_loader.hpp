#ifndef DIMACS_PAR_LOADER_HPP
#define DIMACS_PAR_LOADER_HPP

#include <vector>
#include <algorithm>
#include <charconv>
#include <cstring> // For std::memchr
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <iostream>
#include <stdexcept>

#include <Kokkos_Core.hpp>
#include "graph_defs.hpp"

const char *align_to_next_line(const char *ptr, const char *end)
{
    if (ptr >= end)
        return end;
    // memchr is highly optimized and often uses SIMD instructions
    const void *nl = std::memchr(ptr, '\n', end - ptr);
    if (nl)
    {
        return static_cast<const char *>(nl) + 1;
    }
    return end;
};

// custom isspace to dodge locale checks
KOKKOS_INLINE_FUNCTION
bool fast_isspace(unsigned char c)
{

    return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v');
};

struct Chunk
{
    const char *begin;
    const char *end;
    size_t count;
    size_t offset;
};

// Main Loader
inline HostEdgeList parallel_load_dimacs(
    const std::string &filename,
    int &num_nodes,
    int &source,
    int &sink,
    unsigned int tc = 0)
{
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd == -1)
        throw std::runtime_error("Error opening file: " + filename);

    struct stat sb;
    if (fstat(fd, &sb) == -1)
    {
        close(fd);
        throw std::runtime_error("Error getting file size");
    }
    size_t length = sb.st_size;

    if (length == 0)
    {
        close(fd);
        return HostEdgeList("empty", 0);
    }

    // lazy mmap. Kokkos threads ? should ? fault the pages on their respective NUMA nodes.
    void *map_base = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map_base == MAP_FAILED)
    {
        close(fd);
        throw std::runtime_error("mmap failed");
    }

    const char *data_start = static_cast<const char *>(map_base);
    const char *data_end = data_start + length;
    const char *edges_start_ptr = data_start;

    // Preamble Scan (Sequential)
    {
        const char *cur = data_start;
        bool preamble_done = false;

        while (cur < data_end && !preamble_done)
        {
            while (cur < data_end && fast_isspace(static_cast<unsigned char>(*cur)))
                cur++;
            if (cur >= data_end)
                break;

            char type = *cur;
            if (type == 'a')
            {
                edges_start_ptr = cur;
                preamble_done = true;
            }
            else if (type == 'p')
            {
                cur++;
                while (cur < data_end && fast_isspace(static_cast<unsigned char>(*cur)))
                    cur++;
                while (cur < data_end && !fast_isspace(static_cast<unsigned char>(*cur)))
                    cur++;
                while (cur < data_end && fast_isspace(static_cast<unsigned char>(*cur)))
                    cur++;
                std::from_chars(cur, data_end, num_nodes);
                cur = align_to_next_line(cur, data_end);
            }
            else if (type == 'n')
            {
                cur++;
                int id;
                while (cur < data_end && fast_isspace(static_cast<unsigned char>(*cur)))
                    cur++;
                auto res = std::from_chars(cur, data_end, id);
                cur = res.ptr;

                while (cur < data_end && fast_isspace(static_cast<unsigned char>(*cur)))
                    cur++;
                if (*cur == 's')
                    source = id - 1;
                else if (*cur == 't')
                    sink = id - 1;

                cur = align_to_next_line(cur, data_end);
            }
            else
            {
                cur = align_to_next_line(cur, data_end);
            }
        }
    }

    unsigned int num_chunks = Kokkos::DefaultHostExecutionSpace().concurrency();
    if (tc > 0)
        num_chunks = std::min(tc, num_chunks);
    if (num_chunks == 0)
        num_chunks = 2;
    std::cout << "Loader using " << num_chunks << " Kokkos Host Threads\n";

    Kokkos::View<Chunk *, Kokkos::HostSpace> chunks("chunks", num_chunks);

    const char *chunk_begin = edges_start_ptr;
    size_t remaining_size = data_end - edges_start_ptr;
    size_t chunk_size = (remaining_size > 0) ? (remaining_size / num_chunks) : 0;

    // Define chunks sequentially (fast, at most #of threads on the CPU -- <= 128 on Epyc)
    for (unsigned int i = 0; i < num_chunks; ++i)
    {
        const char *chunk_end = (i == num_chunks - 1) ? data_end : chunk_begin + chunk_size;
        if (i < num_chunks - 1)
        {
            chunk_end = align_to_next_line(chunk_end, data_end);
        }

        chunks(i) = {chunk_begin, chunk_end, 0, 0};
        chunk_begin = chunk_end;
    }

    // PASS 1: Parallel Count
    Kokkos::parallel_for("Pass1_CountEdges",
                         Kokkos::RangePolicy<Kokkos::DefaultHostExecutionSpace>(0, num_chunks),
                         [=](const int i)
                         {
                             size_t count = 0;
                             const char *cur = chunks(i).begin;
                             const char *end = chunks(i).end;

                             while (cur < end)
                             {
                                 while (cur < end && fast_isspace(static_cast<unsigned char>(*cur)))
                                     cur++;
                                 if (cur >= end)
                                     break;
                                 if (*cur == 'a')
                                 {
                                     count++;
                                 }
                                 cur = align_to_next_line(cur, end);
                             }
                             chunks(i).count = count;
                         });
    Kokkos::fence();

    // Compute offsets sequentially 
    size_t total_edges = 0;
    for (unsigned int i = 0; i < num_chunks; ++i)
    {
        chunks(i).offset = total_edges;
        total_edges += chunks(i).count;
    }

    HostEdgeList host_edges(
        Kokkos::ViewAllocateWithoutInitializing("host_raw_edges"),
        total_edges);

    // PASS 2: Parallel Fill
    Kokkos::parallel_for("Pass2_FillEdges",
                         Kokkos::RangePolicy<Kokkos::DefaultHostExecutionSpace>(0, num_chunks),
                         [=](const int i)
                         {
                             const char *cur = chunks(i).begin;
                             const char *end = chunks(i).end;
                             size_t current_idx = chunks(i).offset;

                             while (cur < end)
                             {
                                 while (cur < end && fast_isspace(static_cast<unsigned char>(*cur)))
                                     cur++;
                                 if (cur >= end)
                                     break;

                                 if (*cur == 'a')
                                 {
                                     cur++; // Skip 'a'
                                     int u, v;
                                     long long cap;

                                     // [u]
                                     while (cur < end && fast_isspace(static_cast<unsigned char>(*cur)))
                                         cur++;
                                     auto res1 = std::from_chars(cur, end, u);
                                     cur = res1.ptr;

                                     // [v]
                                     while (cur < end && fast_isspace(static_cast<unsigned char>(*cur)))
                                         cur++;
                                     auto res2 = std::from_chars(cur, end, v);
                                     cur = res2.ptr;

                                     // [cap]
                                     while (cur < end && fast_isspace(static_cast<unsigned char>(*cur)))
                                         cur++;
                                     auto res3 = std::from_chars(cur, end, cap);
                                     cur = res3.ptr;

                                     host_edges(current_idx) = {u - 1, v - 1, cap};
                                     current_idx++;
                                 }
                                 cur = align_to_next_line(cur, end);
                             }
                         });
    Kokkos::fence();

    // Tidy
    if (munmap(map_base, length) == -1)
    {
        std::cerr << "Warning: munmap failed\n";
    }
    close(fd);

    return host_edges;
}

#endif // DIMACS_PAR_LOADER_HPP