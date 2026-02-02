#ifndef RAW_GRAPH_DATA_HPP
#define RAW_GRAPH_DATA_HPP

#include <vector>
#include <cstddef>

// SoA-ish (like the device representation)
struct RawGraphData
{
    std::vector<int> u;
    std::vector<int> v;
    std::vector<long long> capacity;

    size_t size() const
    {
        return u.size();
    }

    void reserve(size_t n)
    {
        u.reserve(n);
        v.reserve(n);
        capacity.reserve(n);
    }

    void clear()
    {
        u.clear();
        v.clear();
        capacity.clear();
    }
};

#endif // RAW_GRAPH_DATA_HPP