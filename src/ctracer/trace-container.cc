#include "trace-container.hh"

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

trace::trace(std::string name, std::vector<uint32_t> data, trace::time_point time_start, trace::time_point time_end, uint64_t cycles_start, uint64_t cycles_end)
  : _name(move(name)), //
    _data(move(data)),
    _time_start(time_start),
    _time_end(time_end),
    _cycles_start(cycles_start),
    _cycles_end(cycles_end)
{
}
