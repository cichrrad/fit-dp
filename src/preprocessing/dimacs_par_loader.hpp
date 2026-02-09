#ifndef DIMACS_PAR_LOADER_HPP
#define DIMACS_PAR_LOADER_HPP

#include <vector>
#include <future>
#include <thread>
#include <algorithm>
#include <charconv>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <iostream>
#include <stdexcept>

#include <Kokkos_Core.hpp>
#include "graph_defs.hpp"

inline const char *align_to_next_line(const char *ptr, const char *end)
{
    while (ptr < end && *ptr != '\n') ptr++;
    if (ptr < end) ptr++;
    return ptr;
}

// PASS 1: Count Edges
inline size_t count_edges_in_chunk(const char *start, const char *end)
{
    size_t count = 0;
    const char *cur = start;
    while (cur < end)
    {
        // Skip leading whitespace
        while (cur < end && std::isspace(static_cast<unsigned char>(*cur))) cur++;
        if (cur >= end) break;

        if (*cur == 'a') {
            count++;
        }
        cur = align_to_next_line(cur, end);
    }
    return count;
}

// PASS 2: Fill View
inline void fill_edges_chunk(const char *start, const char *end, HostEdgeList view, size_t offset)
{
    const char *cur = start;
    size_t current_idx = offset;

    while (cur < end)
    {
        while (cur < end && std::isspace(static_cast<unsigned char>(*cur))) cur++;
        if (cur >= end) break;

        char type = *cur;

        if (type == 'a')
        {
            cur++; // Skip 'a'
            int u, v;
            long long cap;

            // [u]
            while (cur < end && std::isspace(static_cast<unsigned char>(*cur))) cur++;
            auto res1 = std::from_chars(cur, end, u);
            cur = res1.ptr;

            // [v]
            while (cur < end && std::isspace(static_cast<unsigned char>(*cur))) cur++;
            auto res2 = std::from_chars(cur, end, v);
            cur = res2.ptr;

            // [cap]
            while (cur < end && std::isspace(static_cast<unsigned char>(*cur))) cur++;
            auto res3 = std::from_chars(cur, end, cap);
            cur = res3.ptr;

            // Write directly to View (-1 to 0 based)
            view(current_idx) = { u - 1, v - 1, cap };
            current_idx++;
        }

        cur = align_to_next_line(cur, end);
    }
}

// Main Loader
inline HostEdgeList parallel_load_dimacs(
    const std::string &filename,
    int &num_nodes,
    int &source,
    int &sink,
    unsigned int tc = 0)
{
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd == -1) throw std::runtime_error("Error opening file: " + filename);

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        close(fd);
        throw std::runtime_error("Error getting file size");
    }
    size_t length = sb.st_size;

    if (length == 0) {
        close(fd);
        return HostEdgeList("empty", 0);
    }

    void *map_base = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map_base == MAP_FAILED) {
        close(fd);
        throw std::runtime_error("mmap failed");
    }

    const char *data_start = static_cast<const char *>(map_base);
    const char *data_end = data_start + length;
    const char *edges_start_ptr = data_start;

    // Preamble Scan (not parallel because its really tiny)
    {
        const char *cur = data_start;
        bool preamble_done = false;

        while (cur < data_end && !preamble_done)
        {
            while (cur < data_end && std::isspace(static_cast<unsigned char>(*cur))) cur++;
            if (cur >= data_end) break;

            char type = *cur;
            if (type == 'a') {
                edges_start_ptr = cur;
                preamble_done = true;
            }
            else if (type == 'p') {
                cur++;
                // skip "max" or "flow" text
                while (cur < data_end && std::isspace(static_cast<unsigned char>(*cur))) cur++;
                while (cur < data_end && !std::isspace(static_cast<unsigned char>(*cur))) cur++;
                while (cur < data_end && std::isspace(static_cast<unsigned char>(*cur))) cur++;
                std::from_chars(cur, data_end, num_nodes);
                cur = align_to_next_line(cur, data_end);
            }
            else if (type == 'n') {
                cur++;
                int id;
                while (cur < data_end && std::isspace(static_cast<unsigned char>(*cur))) cur++;
                auto res = std::from_chars(cur, data_end, id);
                cur = res.ptr;
                
                while (cur < data_end && std::isspace(static_cast<unsigned char>(*cur))) cur++;
                if (*cur == 's') source = id - 1;
                else if (*cur == 't') sink = id - 1;
                
                cur = align_to_next_line(cur, data_end);
            }
            else {
                cur = align_to_next_line(cur, data_end);
            }
        }
    }

    // Prepare Threads
    auto num_threads = std::thread::hardware_concurrency();
    if (tc) num_threads = std::min(tc,num_threads);
    if (num_threads == 0) num_threads = 2;
    std::cout << "Loader using " << num_threads << " cores\n"; 

    struct Chunk {
        const char *begin;
        const char *end;
        size_t count;
        size_t offset;
    };
    std::vector<Chunk> chunks(num_threads);

    const char *chunk_begin = edges_start_ptr;
    size_t remaining_size = data_end - edges_start_ptr;
    size_t chunk_size = (remaining_size > 0) ? (remaining_size / num_threads) : 0;

    // Define chunks
    for (unsigned int i = 0; i < num_threads; ++i) {
        const char *chunk_end = (i == num_threads - 1) ? data_end : chunk_begin + chunk_size;
        if (i < num_threads - 1) {
            chunk_end = align_to_next_line(chunk_end, data_end);
        }
        
        chunks[i] = {chunk_begin, chunk_end, 0, 0};
        chunk_begin = chunk_end;
    }

    // PASS 1: Parallel Count
    std::vector<std::future<size_t>> count_futures;
    for (unsigned int i = 0; i < num_threads; ++i) {
        if (chunks[i].begin < chunks[i].end) {
            count_futures.push_back(std::async(std::launch::async, 
                count_edges_in_chunk, chunks[i].begin, chunks[i].end));
        } else {
            count_futures.push_back(std::async(std::launch::deferred, [](){ return (size_t)0; }));
        }
    }

    size_t total_edges = 0;
    for (unsigned int i = 0; i < num_threads; ++i) {
        chunks[i].count = count_futures[i].get();
        chunks[i].offset = total_edges;
        total_edges += chunks[i].count;
    }

    // Allocate View with count results
    HostEdgeList host_edges(
        Kokkos::ViewAllocateWithoutInitializing("host_raw_edges"), 
        total_edges
    );

    // PASS 2: Parallel Fill
    std::vector<std::future<void>> fill_futures;
    for (unsigned int i = 0; i < num_threads; ++i) {
        if (chunks[i].count > 0) {
            fill_futures.push_back(std::async(std::launch::async,
                fill_edges_chunk, 
                chunks[i].begin, 
                chunks[i].end, 
                host_edges, 
                chunks[i].offset));
        }
    }

    for (auto &f : fill_futures) {
        f.get();
    }

    // Cleanup
    if (munmap(map_base, length) == -1) {
        std::cerr << "Warning: munmap failed\n";
    }
    close(fd);

    return host_edges;
}

#endif // DIMACS_PAR_LOADER_HPP