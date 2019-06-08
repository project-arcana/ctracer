#pragma once

#include <ctracer/ChunkAllocator.hh>
#include <ctracer/trace-container.hh>
#include <ctracer/trace.hh>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ct
{
/// visitor base class, call order is:
///   -> nested on_trace_start .. on_trace_end
/// traces might not have _end if they are still running
struct visitor
{
    virtual void on_trace_start(location const& /* loc */, uint64_t /* cycles */, uint32_t /* cpu */) {}
    virtual void on_trace_end(uint64_t /* cycles */, uint32_t /* cpu */) {}

    virtual ~visitor() = default;
};

/// sets the default chunk allocator for new threads (nullptr resets to builtin alloc)
void set_default_allocator(std::shared_ptr<ChunkAllocator> const& allocator);
/// sets the chunk allocator of the current thread (nullptr resets to builtin alloc)
void set_thread_allocator(std::shared_ptr<ChunkAllocator> const& allocator);
/// user-defined name for this thread
void set_thread_name(std::string name);

/// returns a trace object for the current thread
trace get_current_thread_trace();
/// returns a trace objects for all finished threads
std::vector<trace> get_finished_thread_traces();
/// frees memory of finished threads
void clear_finished_thread_traces();

/// calls visitor callbacks for each event in the trace
void visit(trace const& t, visitor& v);

/// writes a csv where all trace points are summarized per-location
void write_summary_csv(std::string const& filename);
/// Json file for use with https://github.com/jlfwong/speedscope
/// see https://github.com/jlfwong/speedscope/wiki/Importing-from-custom-sources
void write_speedscope_json(std::string const& filename = "speedscope.json");
} // namespace ct
