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
    string current_rule_header;
    string current_content_line;
    TagState tag_state;
public:
    list<string> output;

    RuleGenerator(Vars const & vars, Rule const & rule)
        : vars(vars)
        , rule(rule)
    {
        CheckAllTagConsistency();
    }

    void AppendPhony(string & phony) {
        GenNamesForPhony(phony);
    }

    void Gen() {
        assert(current_rule_header.empty());
        current_rule_header += '\n';
        GenNames();
        current_rule_header.pop_back();
        assert(current_rule_header.empty());
    }

private:
    void RecursiveCall(string & buf, Node const & node, size_t pos, function<void(size_t)> f) {
        if (!node.is_var) {
            buf += node.s;
            f(pos + 1);
            Pop(buf, node.s.size());
            return;
        }
        auto const & values = vars.find(node.s)->second;
        if (node.tag.empty()) {
            for (auto const & s : values) {
                buf += s;
                f(pos + 1);
                Pop(buf, s.size());
            }
            return;
        }

        auto tag = tag_state.find(node.tag);
        if (tag != tag_state.end()) {
            buf += values[tag->second];
            f(pos + 1);
            Pop(buf, values[tag->second].size());
            return;
        }
        auto new_tag = tag_state.emplace_hint(tag, node.tag, 0);
        for (auto const & s : values) {
            buf += s;
            f(pos + 1);
            Pop(buf, s.size());
            ++new_tag->second;
        }
        tag_state.erase(new_tag);
    }

    void GenNamesForPhony(string & phony, size_t pos = 0) {
        if (pos == rule.name.size()) {
            phony += ' ';
            phony += current_rule_header;
            return;
        }
        RecursiveCall(current_rule_header, rule.name[pos], pos, [this, &phony](size_t pos) {GenNamesForPhony(phony, pos);});
    }

    void GenNames(size_t pos = 0) {
        if (pos == rule.name.size()) {
            current_rule_header += ':';
            GenDeps();
            current_rule_header.pop_back();
            return;
        }
        RecursiveCall(current_rule_header, rule.name[pos], pos, [this](size_t pos) {GenNames(pos);});
    }

    void GenDeps() {
        auto rule_header_size_without_deps = current_rule_header.size();
        current_content_line += ' ';
        for (size_t i = 0; i < rule.dependencies.size(); ++i)
            GenSingleDep(i);
        current_content_line.pop_back();
        output.push_back(current_rule_header);
        GenContent();
        current_rule_header.resize(rule_header_size_without_deps);
    }

    void GenSingleDep(size_t dep_pos, size_t pos = 0) {
        if (pos == rule.dependencies[dep_pos].size()) {
            current_rule_header += current_content_line;
            return;
        }
        auto & dep = rule.dependencies[dep_pos][pos];
        RecursiveCall(current_content_line, dep, pos, [this, dep_pos](size_t pos) {GenSingleDep(dep_pos, pos);});
    }

    void GenContent() {
        current_content_line += '\t';
        for (size_t i = 0; i < rule.content.size(); ++i)
            GenSingleCmd(i);
        current_content_line.pop_back();
    }

    void GenSingleCmd(size_t line_pos, size_t pos = 0) {
        if (pos == rule.content[line_pos].size())
            return output.push_back(current_content_line);
        auto & line = rule.content[line_pos][pos];
        RecursiveCall(current_content_line, line, pos, [this, line_pos](size_t pos) {GenSingleCmd(line_pos, pos);});
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
    // TODO: 1) output to a stream instead of list<string>
    for (auto const & rule : rules) {
        RuleGenerator rg(vars, rule);
        rg.AppendPhony(phony);
    }
    res.push_back(move(phony));

    for (auto const & rule : rules) {
        RuleGenerator rg(vars, rule);
        rg.Gen();
        res.splice(res.end(), rg.output);
    }
    return res;
}