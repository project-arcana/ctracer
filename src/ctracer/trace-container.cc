#include "trace-container.hh"

#include <unordered_map>

#include "trace-config.hh"

using namespace ct;

std::vector<event> trace::compute_events() const
{
    struct my_visitor : ct::visitor
    {
        std::vector<event> events;
        std::vector<location const*> loc_stack;

        void on_trace_start(ct::location const& loc, uint64_t cycles, uint32_t cpu) override
        {
            loc_stack.push_back(&loc);
            events.push_back({loc_stack.back(), cycles, cpu, true});
        }

        void on_trace_end(uint64_t cycles, uint32_t cpu) override
        {
            events.push_back({loc_stack.back(), cycles, cpu, false});
            loc_stack.pop_back();
        }
    };

    my_visitor v;
    visit(*this, v);

    return v.events;
}

std::vector<location_stats> trace::compute_location_stats() const
{
    struct my_visitor : ct::visitor
    {
        std::unordered_map<location const*, location_stats> stats;

        std::vector<location const*> loc_stack;
        std::vector<uint64_t> cycle_stack;

        void on_trace_start(ct::location const& loc, uint64_t cycles, uint32_t cpu) override
        {
            loc_stack.push_back(&loc);
            cycle_stack.push_back(cycles);
        }

        void on_trace_end(uint64_t cycles, uint32_t cpu) override
        {
            auto loc = loc_stack.back();
            auto& s = stats[loc];
            s.loc = loc;
            s.samples++;
            s.total_cycles += cycles - cycle_stack.back();

            cycle_stack.pop_back();
            loc_stack.pop_back();
        }
    };

    my_visitor v;
    visit(*this, v);

    std::vector<location_stats> res;
    for (auto const& kvp : v.stats)
        res.push_back(kvp.second);
    return res;
}

trace::trace(std::string name, std::vector<uint32_t> data, trace::time_point time_start, trace::time_point time_end, uint64_t cycles_start, uint64_t cycles_end)
  : _name(move(name)), //
    _data(move(data)),
    _time_start(time_start),
    _time_end(time_end),
    _cycles_start(cycles_start),
    _cycles_end(cycles_end)
{
}
