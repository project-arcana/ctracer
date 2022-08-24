#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace ct
{
struct visitor;
struct location;

struct event
{
    location const* loc = nullptr;
    uint64_t cycles = 0;
    uint32_t cpu = 0;
    bool enter = false;
};

struct location_stats
{
    location const* loc = nullptr;
    int samples = 0;
    uint64_t total_cycles = 0;
};

/// An opaque value type representing a hierarchical call trace of TRACEs.
/// Not all TRACEs might be closed because traces can be queried in-between
struct trace
{
public:
    using time_point = std::chrono::high_resolution_clock::time_point;

    std::string const& name() const { return _name; }

    /// convenience function that visits this trace and converts it into event form
    std::vector<event> compute_events() const;
    /// convenience function that visits this trace and computes per-location stats
    std::vector<location_stats> compute_location_stats() const;

    time_point time_start() const { return _time_start; }
    time_point time_end() const { return _time_end; }
    uint64_t cycles_start() const { return _cycles_start; }
    uint64_t cycles_end() const { return _cycles_end; }
    float elapsed_seconds() const { return std::chrono::duration<float>(_time_end - _time_start).count(); }
    uint64_t elapsed_cycles() const { return _cycles_end - _cycles_start; }

    trace() = default;

private:
    explicit trace(std::string name, std::vector<uint32_t> data, time_point time_start, time_point time_end, uint64_t cycles_start, uint64_t cycles_end);

    std::string _name;
    std::vector<uint32_t> _data;

    time_point _time_start;
    time_point _time_end;
    uint64_t _cycles_start;
    uint64_t _cycles_end;

    friend struct scope;
    friend void visit(trace const& t, visitor& v);
};
}
