#pragma once

#include <ctracer/trace.hh>

#include <cstdint>
#include <string>
#include <thread>

namespace ct
{
/// sets the size of newly allocated chunks
/// is a per-thread setting
void set_thread_chunk_size(size_t size, float growth_factor = 1.6f, size_t max_size = 10 * (1 << 20));
/// user-defined name for this thread
void set_thread_name(std::string const& name);

/// visitor base class, call order is:
/// on_thread
///   -> nested on_trace_start .. on_trace_end
/// traces might not have _end if they are still running
struct visitor
{
    virtual void on_thread(std::thread::id /* thread */) {}
    virtual void on_trace_start(location* /* loc */, uint64_t /* cycles */, uint32_t /* cpu */) {}
    virtual void on_trace_end(uint64_t /* cycles */, uint32_t /* cpu */) {}
};

void visit_thread(visitor& v);

/// writes a csv where all trace points are summarized per-location
void write_summary_csv(std::string const& filename);
/// Json file for use with https://github.com/jlfwong/speedscope
/// see https://github.com/jlfwong/speedscope/wiki/Importing-from-custom-sources
void write_speedscope_json(std::string const& filename = "speedscope.json");
} // namespace ct
