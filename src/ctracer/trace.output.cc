#include "trace.hh"

#include <algorithm>
#include <iostream>
#include <map>
#include <unordered_map>
#include <vector>

#include <fstream>

namespace ct
{
static std::string beautify_function_name(std::string const &name)
{
    auto p = name.rfind(')');
    if (p == std::string::npos) // no (..)
    {
        p = name.rfind(' ');
        if (p == std::string::npos) // no space
            return name;
        return name.substr(p + 1); // "void foo" -> "foo"
    }

    int i = int(p);
    int db = 0;
    int da = 0;
    int cc = 0;
    while (i >= 0)
    {
        if (name[i] == ')')
            ++db;
        if (name[i] == '>')
            ++da;
        if (name[i] == '<')
            --da;
        if (name[i] == '(')
            --db;

        if (name[i] == ':' && da == 0 && db == 0)
        {
            ++cc;
            if (cc > 2)
                break;
        }

        if (name[i] == ' ' && da == 0 && db == 0)
            break;

        --i;
    }

    if (i < 0)
        return name;

    return name.substr(i + 1);
}

void write_speedscope_json(std::string const &filename)
{
    std::ofstream out(filename);
    if (!out.good())
        return;

    struct event
    {
        char type;
        int frame;
        uint64_t at;
    };
    struct stack_entry
    {
        int frame;
    };

    struct visitor : ct::visitor
    {
        uint64_t min_cycles = std::numeric_limits<uint64_t>::max();
        uint64_t max_cycles = 0;
        uint32_t last_cpu = 0;
        std::unordered_map<location *, int> frames;
        std::vector<location *> locations;
        std::vector<stack_entry> stack;
        std::vector<event> events;
        std::thread::id thread;

        int frame_of(location *loc)
        {
            auto it = frames.find(loc);
            if (it != frames.end())
                return it->second;

            auto f = int(frames.size());
            frames[loc] = f;
            locations.push_back(loc);
            return f;
        }

        virtual void on_thread(std::thread::id thread) override
        {
            close_pending_actions();

            this->thread = thread;
            stack.clear();
        }
        virtual void on_trace_start(ct::location *loc, uint64_t cycles, uint32_t cpu) override
        {
            last_cpu = cpu;
            min_cycles = std::min(min_cycles, cycles);
            max_cycles = std::max(max_cycles, cycles);

            auto f = frame_of(loc);
            events.push_back({'O', f, cycles});

            stack.push_back({f});
        }
        virtual void on_trace_end(uint64_t cycles, uint32_t cpu) override
        {
            last_cpu = cpu;
            min_cycles = std::min(min_cycles, cycles);
            max_cycles = std::max(max_cycles, cycles);

            auto se = stack.back();
            stack.pop_back();

            events.push_back({'C', se.frame, cycles});
        }

        void close_pending_actions()
        {
            while (!stack.empty())
                on_trace_end(max_cycles, last_cpu);
        }
    };
    visitor v;
    ct::visit_thread(v);
    v.close_pending_actions();

    out << "{";
    out << "\"version\":\"0.0.1\",";
    out << "\"$schema\": \"https://www.speedscope.app/file-format-schema.json\",";
    out << "\"shared\":{";
    out << "\"frames\":[";
    for (auto i = 0u; i < v.locations.size(); ++i)
    {
        auto loc = v.locations[i];
        if (i > 0)
            out << ",";
        out << "{";
        out << "\"name\":\"" << (std::string(loc->name).empty() ? beautify_function_name(loc->function) : loc->name) << "\",";
        std::string escapedFilePath = loc->file;
        std::replace(escapedFilePath.begin(), escapedFilePath.end(), '\\', '/');
        out << "\"file\":\"" << escapedFilePath << "\",";
        out << "\"line\":" << loc->line << "";
        out << "}";
    }
    out << "]";
    out << "},";
    out << "\"profiles\":[{";
    out << "\"type\":\"evented\",";
    out << "\"name\":\"Aion Trace\",";
    out << "\"unit\":\"none\",";
    out << "\"startValue\":0,";
    out << "\"endValue\":" << v.max_cycles - v.min_cycles << ",";
    out << "\"events\":[";
    auto first = true;
    for (auto const &e : v.events)
    {
        if (!first)
            out << ",";
        first = false;
        out << "{";
        out << "\"type\":\"" << e.type << "\",";
        out << "\"frame\":" << e.frame << ",";
        out << "\"at\":" << e.at - v.min_cycles;
        out << "}";
    }
    out << "]";
    out << "}]";
    out << "}";
}

void write_summary_csv(const std::string &filename)
{
    std::ofstream out(filename);
    if (!out.good())
        return;

    struct entry
    {
        int count = 0;
        uint64_t cycles_total = 0;
        uint64_t cycles_children = 0;
        uint64_t cycles_min = std::numeric_limits<uint64_t>::max();
        uint64_t cycles_max = 0;
    };

    struct stack_entry
    {
        location *loc;
        uint64_t cycles;
        uint64_t cycles_children;
    };

    struct visitor : ct::visitor
    {
        std::map<location *, entry> entries;
        std::vector<stack_entry> stack;
        int depth = 1;
        std::thread::id thread;
        virtual void on_thread(std::thread::id thread) override
        {
            this->thread = thread;
            stack.clear();
            // TODO: close pending actions
        }
        virtual void on_trace_start(ct::location *loc, uint64_t cycles, uint32_t cpu) override { stack.push_back({loc, cycles, 0}); }
        virtual void on_trace_end(uint64_t cycles, uint32_t cpu) override
        {
            auto se = stack.back();
            stack.pop_back();
            auto dt = cycles - se.cycles;

            auto &e = entries[se.loc];
            e.count++;
            e.cycles_total += dt;
            e.cycles_children += se.cycles_children;
            e.cycles_min = std::min(e.cycles_min, dt);
            e.cycles_max = std::max(e.cycles_max, dt);

            if (!stack.empty())
                stack.back().cycles_children += dt;
        }
    };
    visitor v;
    ct::visit_thread(v);

    out << "name,file,function,count,total,avg,min,max,total_body,avg_body\n";
    for (auto const &kvp : v.entries)
    {
        auto l = kvp.first;
        auto e = kvp.second;
        out << '"' << l->name << '"' << ",";
        out << '"' << l->file << ":" << l->line << '"' << ",";
        out << '"' << l->function << '"' << ",";
        out << e.count << ",";
        out << e.cycles_total << ",";
        out << e.cycles_total / e.count << ",";
        out << e.cycles_min << ",";
        out << e.cycles_max << ",";
        out << e.cycles_total - e.cycles_children << ",";
        out << (e.cycles_total - e.cycles_children) / e.count;
        out << "\n";
    }
}
} // namespace ct
