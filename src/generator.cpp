#include "generator.hpp"
#include <ostream>
#include <list>
#include <functional>
#include <cassert>

using namespace std;

std::ostream & operator << (std::ostream & out, StrLine const & line) {
    for (auto const & e : line)
        if (!e.is_var) {
            out << e.s;
        } else {
            out << '{' << e.s;
            if (!e.tag.empty())
                out << ", " << e.tag;
            out << '}';
        }
    return out;
}

std::ostream & operator << (std::ostream & out, Rule const & rule) {
    out << rule.name << ": ";
    for (auto const & dep : rule.dependencies)
        out << dep << " ";
    out << '\n';
    for (auto const & cmd : rule.content)
        out << '\t' << cmd << '\n';
    return out;
}

std::ostream & operator << (std::ostream & out, Var const & var) {
    out << var.first << " = ";
    for (auto const & value : var.second)
        out << '"' << value << "\" ";
    return out;
}

namespace {

inline void Pop(string & s, size_t size) {
    s.resize(s.size() - size);
}

class RuleGenerator {
    using TagState = unordered_map<string, size_t>;
    Vars const & vars;
    Rule const & rule;
    string & phony;
    string current_str;
    TagState tag_state;
public:
    list<string> output;

    RuleGenerator(Vars const & vars, Rule const & rule, string & phony)
        : vars(vars)
        , rule(rule)
        , phony(phony)
    {
        CheckAllTagConsistency();
    }

    void Gen() {
        GenNames();
    }

private:
    inline void RecursiveCall(Node const & node, size_t pos, function<void(size_t)> f) {
        if (!node.is_var) {
            current_str += node.s;
            f(pos + 1);
            Pop(current_str, node.s.size());
            return;
        }
        auto const & values = vars.find(node.s)->second;
        if (node.tag.empty()) {
            for (auto const & s : values) {
                current_str += s;
                f(pos + 1);
                Pop(current_str, s.size());
            }
            return;
        }

        auto tag = tag_state.find(node.tag);
        if (tag != tag_state.end()) {
            current_str += values[tag->second];
            f(pos + 1);
            Pop(current_str, values[tag->second].size());
            return;
        }
        auto new_tag = tag_state.emplace_hint(tag, node.tag, 0);
        for (auto const & s : values) {
            current_str += s;
            f(pos + 1);
            Pop(current_str, s.size());
            ++new_tag->second;
        }
        tag_state.erase(new_tag);
    }

    void GenNames(size_t pos = 0) {
        if (pos == rule.name.size()) {
            phony += ' ';
            phony += current_str;
            current_str += ':';
            GenDeps();
            current_str.pop_back();
            return;
        }
        RecursiveCall(rule.name[pos], pos, [this](size_t pos) {GenNames(pos);});
    }

    void GenDeps(size_t dep_pos = 0, size_t pos = 0) {
        if (dep_pos == rule.dependencies.size()) {
            current_str.push_back('\n');
            GenContent();
            current_str.pop_back();
            return;
        }
        if (pos == rule.dependencies[dep_pos].size())
            return GenDeps(dep_pos + 1);
        if (pos == 0)
            current_str += ' ';

        auto & dep = rule.dependencies[dep_pos][pos];
        RecursiveCall(dep, pos, [this, dep_pos](size_t pos) {GenDeps(dep_pos, pos);});

        if (pos == 0)
            current_str.pop_back();
    }

    void GenContent(size_t line_pos = 0, size_t pos = 0) {
        if (line_pos == rule.content.size())
            return output.push_back(current_str);
        if (pos == rule.content[line_pos].size())
            return GenContent(line_pos + 1);
        if (pos == 0)
            current_str += '\t';

        auto & line = rule.content[line_pos][pos];
        RecursiveCall(line, pos, [this, line_pos](size_t pos) {GenContent(line_pos, pos);});

        if (pos == 0)
            current_str.pop_back();
    }

    void CheckAllVarAreDefined() const {
        for (auto const & s : rule.name)
            if (s.is_var)
                CheckVarIsDefined(s.s);
        for (auto const & dep : rule.dependencies)
            for (auto const & s : dep)
                if (s.is_var)
                    CheckVarIsDefined(s.s);
        for (auto const & content : rule.content)
            for (auto const & s : content)
                if (s.is_var)
                    CheckVarIsDefined(s.s);
    }

    void CheckVarIsDefined(VarName const & var) const {
        if (vars.find(var) == vars.end())
            throw '\'' + var + "' is not defined!";
    }

    void CheckAllTagConsistency() const {
        CheckAllVarAreDefined();
        TagState state;
        for (auto const & s : rule.name)
            if (s.is_var && !s.tag.empty())
                CheckTagConsistency(s, state);
        for (auto const & dep : rule.dependencies)
            for (auto const & s : dep)
                if (s.is_var && !s.tag.empty())
                    CheckTagConsistency(s, state);
        for (auto const & content : rule.content)
            for (auto const & s : content)
                if (s.is_var && !s.tag.empty())
                    CheckTagConsistency(s, state);
    }

    void CheckTagConsistency(Node const & node, TagState & state) const {
        auto const & values = vars.find(node.s)->second;
        auto [it, ok] = state.emplace(node.tag, values.size());
        if (!ok && it->second != values.size())
            throw "Amount of var values for tag '" + node.tag + "' is not equally!";
    }

};

} // namespace

list<string> Generate(Vars const & vars, Rules const & rules) {
    list<string> res;
    string phony = ".PHONY:";
    // TODO: 1) gen phony; 2) output to a stream instead of list<string>
    for (auto const & rule : rules) {
        RuleGenerator rg(vars, rule, phony);
        rg.Gen();
        res.splice(res.end(), rg.output);
        // res.push_back("\n\n==================================\n\n");
    }
    res.push_front(move(phony));
    return res;
}