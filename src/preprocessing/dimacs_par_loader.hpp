#ifndef DIMACS_PAR_LOADER_HPP
#define DIMACS_PAR_LOADER_HPP

// Toggle for debug output
// #define LOADER_DEBUG 1

#include <vector>
#include <string_view>
#include <future>
#include <thread>
#include <algorithm>
#include <charconv>
#include <system_error>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <iostream>
#include <mutex>

#include "raw_graph_data.hpp"

static std::mutex debug_mutex;

inline const char *align_to_next_line(const char *ptr, const char *end)
{
    while (ptr < end && *ptr != '\n')
    {
        ptr++;
    }
    if (ptr < end)
        ptr++;
    return ptr;
}

inline RawGraphData parse_dimacs_chunk(const char *start, const char *end, int thread_id)
{

#ifdef LOADER_DEBUG
    {
        std::lock_guard<std::mutex> lock(debug_mutex);
        std::cout << "[Thread " << thread_id << "] Started. Processing "
                  << (end - start) << " bytes.\n";
    }
#endif

    RawGraphData local_data;

    // NOTE: -- / 21 here is an estimate of 'line size' to
    // prevent many reallocations -- 20 gives bytes per edge before
    // realloc
    size_t est_edges = (end - start) / 20;
    local_data.reserve(est_edges);

    const char *cur = start;
    while (cur < end)
    {
        while (cur < end && std::isspace(static_cast<unsigned char>(*cur)))
            cur++;
        if (cur >= end)
            break;

        char type = *cur;

        if (type == 'a')
        {
            cur++;
            int u, v;
            long long cap;

            //[u]
            while (cur < end && std::isspace(static_cast<unsigned char>(*cur)))
                cur++;
            auto res1 = std::from_chars(cur, end, u);
            cur = res1.ptr;

            //[v]
            while (cur < end && std::isspace(static_cast<unsigned char>(*cur)))
                cur++;
            auto res2 = std::from_chars(cur, end, v);
            cur = res2.ptr;

            //[cap]
            while (cur < end && std::isspace(static_cast<unsigned char>(*cur)))
                cur++;
            auto res3 = std::from_chars(cur, end, cap);
            cur = res3.ptr;

            // -1 because dimacs is 1-based
            local_data.u.push_back(u - 1);
            local_data.v.push_back(v - 1);
            local_data.capacity.push_back(cap);
        }

        cur = align_to_next_line(cur, end);
    }

#ifdef LOADER_DEBUG
    {
        std::lock_guard<std::mutex> lock(debug_mutex);
        std::cout << "[Thread " << thread_id << "] Finished. Found "
                  << local_data.size() << " edges.\n";
    }
#endif

    return local_data;
}

inline RawGraphData parallel_load_dimacs(
    const std::string &filename,
    int &num_nodes,
    int &source,
    int &sink)
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
        return {};
    }

    void *map_base = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map_base == MAP_FAILED)
    {
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
            while (cur < data_end && std::isspace(static_cast<unsigned char>(*cur)))
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
                while (cur < data_end && std::isspace(static_cast<unsigned char>(*cur)))
                    cur++;
                while (cur < data_end && !std::isspace(static_cast<unsigned char>(*cur)))
                    cur++;

                while (cur < data_end && std::isspace(static_cast<unsigned char>(*cur)))
                    cur++;
                std::from_chars(cur, data_end, num_nodes);

                cur = align_to_next_line(cur, data_end);
            }
            else if (type == 'n')
            {
                cur++;
                int id;
                char which;

                while (cur < data_end && std::isspace(static_cast<unsigned char>(*cur)))
                    cur++;
                auto res = std::from_chars(cur, data_end, id);
                cur = res.ptr;

                while (cur < data_end && std::isspace(static_cast<unsigned char>(*cur)))
                    cur++;
                which = *cur;

                if (which == 's')
                    source = id - 1;
                else if (which == 't')
                    sink = id - 1;

                cur = align_to_next_line(cur, data_end);
            }
            else
            {
                cur = align_to_next_line(cur, data_end);
            }
        }
    }

    // Parallel Launch
    unsigned int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0)
        num_threads = 2;

    std::vector<std::future<RawGraphData>> futures;
    const char *chunk_begin = edges_start_ptr;
    size_t remaining_size = data_end - edges_start_ptr;
    size_t chunk_size = (remaining_size > 0) ? (remaining_size / num_threads) : 0;

    if (chunk_size > 0)
    {
        for (unsigned int i = 0; i < num_threads; ++i)
        {
            const char *chunk_end;

            if (i == num_threads - 1)
            {
                chunk_end = data_end;
            }
            else
            {
                chunk_end = chunk_begin + chunk_size;
                chunk_end = align_to_next_line(chunk_end, data_end);
            }

            if (chunk_begin < chunk_end)
            {
                futures.push_back(std::async(
                    std::launch::async,
                    parse_dimacs_chunk,
                    chunk_begin,
                    chunk_end,
                    i));
            }
            chunk_begin = chunk_end;
        }
    }

    RawGraphData all_data;

    // NOTE: -- Could pre-sum sizes to reserve once?
    for (auto &f : futures)
    {
        RawGraphData part = f.get();
        all_data.u.insert(all_data.u.end(), part.u.begin(), part.u.end());
        all_data.v.insert(all_data.v.end(), part.v.begin(), part.v.end());
        all_data.capacity.insert(all_data.capacity.end(), part.capacity.begin(), part.capacity.end());
    }

    if (munmap(map_base, length) == -1)
    {
        std::cerr << "Warning: munmap failed\n";
    }
    close(fd);

    return all_data;
}

#endif // DIMACS_PAR_LOADER_HPP