#pragma once

#include <cstdint>
#include <string>
#include <thread>

#ifdef _MSC_VER
#include <intrin.h>
#endif

/**
 * Main Macro: TRACE(...)
 *
 * See https://godbolt.org/z/qs33MN
 *
 * Usage:
 *   int foo() {
 *      TRACE();
 *
 *      return do_stuff();
 *   }
 *
 * Overhead: ~70-105 cycles
 *
 * TODO: low-overhead version without alloc_chunk check (~80 cycles)
 */
#define TRACE(...)                                                                                                                 \
    (void)__VA_ARGS__ " has to be a string literal";                                                                               \
    static ct::location CTRACER_MACRO_JOIN(_ct_trace_label, __LINE__) = {__FILE__, CTRACER_PRETTY_FUNC, "" __VA_ARGS__, __LINE__}; \
    ct::detail::raii_tracer CTRACER_MACRO_JOIN(_ct_trace_, __LINE__)(&CTRACER_MACRO_JOIN(_ct_trace_label, __LINE__))

// Implementation:

#define CTRACER_TRACE_SIZE 9
#define CTRACER_LABEL_MASK 0x80000000
#define CTRACER_END_VALUE 0xFFFFFFFF

#define CTRACER_MACRO_JOIN_IMPL(arg1, arg2) arg1##arg2
#define CTRACER_MACRO_JOIN(arg1, arg2) CTRACER_MACRO_JOIN_IMPL(arg1, arg2)

#ifndef _MSC_VER

#define CTRACER_FORCEINLINE __attribute__((always_inline))
#define CTRACER_NOINLINE __attribute__((noinline))
#define CTRACER_PRETTY_FUNC __PRETTY_FUNCTION__

#define CTRACER_LIKELY(x) __builtin_expect((x), 1)
#define CTRACER_UNLIKELY(x) __builtin_expect((x), 0)
#define CTRACER_COLD __attribute__((cold))

#else

#define CTRACER_FORCEINLINE __forceinline
#define CTRACER_NOINLINE __declspec(noinline)
#define CTRACER_PRETTY_FUNC __FUNCTION__

#define CTRACER_LIKELY(x) x
#define CTRACER_UNLIKELY(x) x
#define CTRACER_COLD

#endif

namespace ct
{
/// sets the size of newly allocated chunks
/// is a per-thread setting
void set_thread_chunk_size(size_t size, float growth_factor = 1.6f, size_t max_size = 10 * (1 << 20));
/// user-defined name for this thread
void set_thread_name(std::string const& name);

struct location
{
    char const* file;
    char const* function;
    char const* name;
    int line;
};

/// visitor base class, call order is:
/// on_thread
///   -> nested on_trace_start .. on_trace_end
/// traces might not have _end if they are still running
struct visitor
{
    virtual void on_thread(std::thread::id thread) {}
    virtual void on_trace_start(location* loc, uint64_t cycles, uint32_t cpu) {}
    virtual void on_trace_end(uint64_t cycles, uint32_t cpu) {}
};

void visit_thread(visitor& v);

#ifdef _WIN32
inline uint64_t current_cycles() { return __rdtsc(); }
#else //  Linux/GCC
inline uint64_t current_cycles()
{
    unsigned int lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}
#endif

/// writes a csv where all trace points are summarized per-location
void write_summary_csv(std::string const& filename);
/// Json file for use with https://github.com/jlfwong/speedscope
/// see https://github.com/jlfwong/speedscope/wiki/Importing-from-custom-sources
void write_speedscope_json(std::string const& filename = "speedscope.json");

namespace detail
{
struct thread_data
{
    uint32_t* curr;
    uint32_t* end; ///< not actually end, has a CTRACER_TRACE_SIZE buffer at the end
};

CTRACER_COLD CTRACER_NOINLINE uint32_t* alloc_chunk();

inline thread_data& tdata()
{
    static thread_local thread_data data = {nullptr, nullptr};
    return data;
}

struct raii_tracer
{
    raii_tracer(location* loc)
    {
        auto pd = tdata().curr;
        if (CTRACER_UNLIKELY(pd >= tdata().end)) // alloc new chunk
            pd = alloc_chunk();
        tdata().curr = pd + 5;

        *(location**)pd = loc;

        unsigned int core;
#ifdef _MSC_VER
        int64_t cc = __rdtscp(&core);
        *(int64_t*)(pd + 2) = cc;
#else
        unsigned int lo, hi;
        __asm__ __volatile__("rdtscp" : "=a"(lo), "=d"(hi), "=c"(core));
        pd[2] = lo;
        pd[3] = hi;
#endif
        pd[4] = core;
    }

    ~raii_tracer()
    {
        auto pd = tdata().curr;
        if (CTRACER_UNLIKELY(pd >= tdata().end)) // alloc new chunk
            pd = alloc_chunk();
        tdata().curr = pd + 4;

        unsigned int core;
#ifdef _MSC_VER
        int64_t cc = __rdtscp(&core);
        pd[0] = CTRACER_END_VALUE;
        *(int64_t*)(pd + 1) = cc;
#else
        unsigned int lo, hi;
        __asm__ __volatile__("rdtscp" : "=a"(lo), "=d"(hi), "=c"(core));
        pd[0] = CTRACER_END_VALUE;
        pd[1] = lo;
        pd[2] = hi;
#endif
        pd[3] = core;
    }
};
} // namespace detail
} // namespace ct

#undef CTRACER_COLD
#undef CTRACER_LIKELY
#undef CTRACER_NOINLINE
#undef CTRACER_UNLIKELY
#undef CTRACER_LABEL_MASK
#undef CTRACER_FORCEINLINE
