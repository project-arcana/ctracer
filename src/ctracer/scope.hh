#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "chunk.hh"
#include "detail.hh"

// technically not needed but including scope without trace is unusual
#include "trace-container.hh"
#include "trace.hh"

namespace ct
{
class ChunkAllocator;
struct trace;

/**
 * An arena for TRACE calls
 * All calls made when scope is valid are directed into its local trace and not into the global trace
 *
 * NOTE: scopes are NOT thread-safe. they must be used with C++ scopes and not concurrently accessed.
 *
 * Usage:
 *
 *   ct::scope s;
 *
 *   TRACE(...); // recorded into s
 *
 *   print(s.trace()); // show its trace
 */
struct scope
{
    using time_point = std::chrono::high_resolution_clock::time_point;

    /// creates a new scope and optionally specifies a custom allocator
    scope(std::string name = "", std::shared_ptr<ChunkAllocator> const& allocator = nullptr);
    ~scope();

    // raii type
    scope(scope&&) = delete;
    scope(scope const&) = delete;
    scope& operator=(scope&&) = delete;
    scope& operator=(scope const&) = delete;

    /// creates a trace object (NOTE: copies chunk data)
    ct::trace trace() const;

    /// returns the trace name (either thread name or scope name)
    std::string const& name() const { return _name; }

private:
    std::string _name;
    std::shared_ptr<ChunkAllocator> _allocator;
    std::vector<chunk> _chunks;

    time_point _time_start;
    uint64_t _cycles_start;

    bool _orphaned = false;

    // TODO: bool if orphaned scope

    friend uint32_t* detail::alloc_chunk();
    friend void detail::mark_as_orphaned(scope& s);
    friend void detail::pop_scope(scope&);
    friend void set_thread_name(std::string name);
    friend void set_thread_allocator(std::shared_ptr<ChunkAllocator> const& allocator);
};
}
