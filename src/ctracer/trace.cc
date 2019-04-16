#include <ctracer/trace-config.hh>

#include <chrono>
#include <mutex>
#include <vector>

namespace
{
thread_local size_t profile_chunk_size = 1000 * CTRACER_TRACE_SIZE;
thread_local size_t profile_chunk_size_max = 10 * (1 << 20); // 10 MB
thread_local float profile_growth_factor = 1.6f;

struct chunk
{
    uint32_t const* data_begin;
    uint32_t const* data_end;
    chunk* next_chunk;

    // TODO: for time sync
    // std::chrono::steady_clock::time_point creation_time;
    // uint64_t creation_cycles;
    // uint32_t creation_cpu;
};

struct thread_chunks
{
    std::thread::id thread;
    chunk* chunks_start = nullptr;
    chunk* chunks_end = nullptr;
};

std::mutex profile_chunks_mutex;
std::vector<thread_chunks*> profile_threads;

thread_local thread_chunks* profile_local_thread = nullptr;
thread_local std::vector<chunk*> profile_local_chunks;
thread_local std::string profile_thread_name;
} // namespace

namespace ct
{
void set_thread_chunk_size(size_t size, float growth_factor, size_t max_size)
{
    profile_chunk_size = size;
    profile_growth_factor = growth_factor;
    profile_chunk_size_max = max_size;
}

void set_thread_name(const std::string& name) { profile_thread_name = name; }

uint32_t* detail::alloc_chunk()
{
    // register thread
    if (profile_local_thread == nullptr)
    {
        profile_local_thread = new thread_chunks;
        profile_local_thread->thread = std::this_thread::get_id();

        // LOCKED: add to list of threads
        profile_chunks_mutex.lock();
        profile_threads.push_back(profile_local_thread);
        profile_chunks_mutex.unlock();
    }

    auto data_end = detail::tdata().curr;

    auto d = new uint32_t[profile_chunk_size + CTRACER_TRACE_SIZE](); // zero-init
    detail::tdata().curr = d;
    detail::tdata().end = d + profile_chunk_size;

    auto c = new chunk;
    c->data_begin = d;
    c->data_end = d + profile_chunk_size + CTRACER_TRACE_SIZE;
    c->next_chunk = nullptr;
    profile_local_chunks.push_back(c);

    // growth
    profile_chunk_size = size_t(profile_chunk_size * profile_growth_factor);
    if (profile_chunk_size > profile_chunk_size_max)
        profile_chunk_size = profile_chunk_size_max;

    // LOCKED: add chunk
    {
        profile_chunks_mutex.lock();

        // add list of chunks
        if (profile_local_thread->chunks_end == nullptr)
        {
            profile_local_thread->chunks_start = c;
            profile_local_thread->chunks_end = c;
        }
        else
        {
            profile_local_thread->chunks_end->data_end = data_end;
            profile_local_thread->chunks_end->next_chunk = c;
            profile_local_thread->chunks_end = c;
        }

        profile_chunks_mutex.unlock();
    }

    return d;
}

void visit_thread(visitor& v)
{
    if (profile_local_chunks.empty())
        return; // DEBUG

    v.on_thread(std::this_thread::get_id());

    auto curr_chunk = 0u;
    uint32_t const* curr_datum = profile_local_chunks[0]->data_begin;

    // get next word
    auto get = [&]() -> uint32_t {
        if (curr_datum == profile_local_chunks[curr_chunk]->data_end)
        {
            ++curr_chunk;
            curr_datum = profile_local_chunks[curr_chunk]->data_begin;
        }
        auto val = *curr_datum;
        ++curr_datum;
        return val;
    };

    while (true)
    {
        auto v0 = get();
        if (v0 == 0x0)
            return; // rest is not done

        if (v0 != CTRACER_END_VALUE)
        {
            auto v1 = get();
            auto loc = (location*)(((uint64_t)v1 << 32uLL) | v0);
            auto lo = get();
            auto hi = get();
            auto cpu = get();
            auto cycles = ((uint64_t)hi << 32) | lo;
            v.on_trace_start(loc, cycles, cpu);
        }
        else
        {
            auto lo = get();
            auto hi = get();
            auto cpu = get();
            auto cycles = ((uint64_t)hi << 32) | lo;
            v.on_trace_end(cycles, cpu);
        }
    }
}
} // namespace ct
