#include "parser.hpp"
#include <istream>
#include <string_view>
#include <cassert>
#include <functional>

using namespace std;

namespace {

bool IsSpace(char c) {
    return isspace(c);
}

size_t Skip(string_view s, function<bool(char)> is_delim, size_t pos = 0) {
    while (pos < s.size() && is_delim(s[pos]))
        ++pos;
    return pos;
}

size_t SkipSpaces(string_view s, size_t pos = 0) noexcept {
    return Skip(s, IsSpace, pos);
}

bool WhitespaceLine(string_view s) noexcept {
    return SkipSpaces(s) == s.size();
}

bool IsBrace(char c) noexcept {
    return c == '{' || c == '}';
}

bool IsLetter(char c) noexcept {
    return ('a' <= c && c <= 'z') ||
           ('A' <= c && c <= 'Z') ||
           ('0' <= c && c <= '9') ||
           c == '_';
}

std::pair<string, size_t> GetTag(string_view s) {
    auto start_pos = SkipSpaces(s);
    auto end_pos = Skip(s, IsLetter, start_pos);
    auto close_brace_pos = SkipSpaces(s, end_pos);
    if (close_brace_pos == s.size())
        throw "Expected '}' at the end of the line: '" + string(s) + '\'';
    if (s[close_brace_pos] == '{')
        throw "Nested templates are not supported; line: '" + string(s) + "' pos: " + to_string(close_brace_pos);
    if (s[close_brace_pos] != '}')
        throw "Only one tag allowed in template braces; line: '" + string(s) + "' pos: " + to_string(close_brace_pos);
    if (start_pos == end_pos)
        throw "Expected tag name; line: '" + string(s) + "' pos: " + to_string(close_brace_pos);
    return {string(s.substr(start_pos, end_pos - start_pos)), close_brace_pos};
}

std::pair<Node, size_t> GetVarNode(string_view s) {
    Node res;
    res.is_var = true;
    size_t len = Skip(s, IsLetter);
    auto pos = SkipSpaces(s, len);
    if (pos == s.size())
        throw "Expected '}' at the end of the line: '" + string(s) + '\'';
    if (s[pos] == '{')
        throw "Nested templates are not supported; line: '" + string(s) + "' pos: " + to_string(pos);
    if (s[pos] != '}' && s[pos] != ',')
        throw "Only one variable name allowed in template braces; line: '" + string(s) + "' pos: " + to_string(pos);
    if (len == 0)
        throw "Template braces must contain a variable name; line: '" + string(s) + "' pos: " + to_string(pos);
    res.s = string(s.substr(0, len));
    if (s[pos] == ',') {
        auto [tag, shift] = GetTag(s.substr(++pos));
        res.tag = move(tag);
        pos += shift;
    }
    return {move(res), pos + 1};
}

string GetStr(string_view s, function<bool(char)> is_delim) {
    size_t len = 0;
    while (len < s.size() && !IsBrace(s[len]) && !is_delim(s[len]))
        ++len;
    assert(len != 0);
    return string(s.substr(0, len));
}

StrLine ParseLine(string_view s, size_t & pos, function<bool(char)> is_delim) {
    StrLine res;
    pos = Skip(s, is_delim, pos);
    while (pos < s.size() && !is_delim(s[pos])) {
        if (s[pos] == '{') {
            ++pos;
            auto [var_node, shift] = GetVarNode(s.substr(pos));
            res.push_back(var_node);
            pos += shift;
            continue;
        }
        if (s[pos] == '}')
            throw "There is no '{' for '}' at pos: " + to_string(pos) + "; line: '" + string(s) + "'";
        auto str = GetStr(s.substr(pos), is_delim);
        pos += str.size();
        res.emplace_back(move(str), false);
    }
    return res;
}

Rule ParseRuleLine(string_view s) {
    Rule rule;
    size_t pos = 0;
    rule.name = ParseLine(s, pos, [](char c) { return IsSpace(c) || c == ':'; });
    pos = SkipSpaces(s, pos);
    if (pos == s.size() || s[pos] != ':')
        throw "Fail rule matching; ':' not found; line: '" + string(s) + '\'';
    ++pos;
    while (true) {
        auto line = ParseLine(s, pos, IsSpace);
        if (line.empty())
            break;
        rule.dependencies.push_back(line);
    };

    return rule;
}

pair<string_view, size_t> GetSingleValue(string_view s) {
    auto start_pos = SkipSpaces(s);
    if (start_pos == s.size())
        return {{}, 0};
    if (s[start_pos] != '"')
        return {s.substr(start_pos), s.size()};
    ++start_pos;
    auto end_pos = Skip(s, [](char c) { return c != '"'; }, start_pos);
    if (end_pos == s.size())
        throw "There is no '\"' pair for '\"' at pos " + to_string(start_pos) + "; line: '" + string(s) + '\'';
    return {s.substr(start_pos, end_pos - start_pos), end_pos + 1};
}

bool try_to_read_variable(string_view s, Vars & vars) {
    auto start_name_pos = SkipSpaces(s);
    auto end_name_pos = Skip(s, IsLetter, start_name_pos);
    auto equality_pos = SkipSpaces(s, end_name_pos);
    if (equality_pos == s.size() || s[equality_pos] != '=')
        return false;
    auto var_name = s.substr(start_name_pos, end_name_pos - start_name_pos);
    auto [it, ok] = vars.emplace(VarName(var_name), VarValues{});
    if (!ok)
        throw "Variable redefinition: '" + string(var_name) + '\'';

    auto content_pos = equality_pos + 1;
    while (content_pos < s.size()) {
        auto [value, shift] = GetSingleValue(s.substr(content_pos));
        if (shift == 0)
            break;
        content_pos += shift;
        it->second.emplace_back(value);
    }
    return true;
}

} // namespace


FileInfo Parse(std::istream & inp) {
    FileInfo info;
    string s;
    Vars & vars = info.first;
    Rules & rules = info.second;
    bool rule_time = false; 
    while (getline(inp, s)) {
        string_view sv(s);
        if (sv.empty() || WhitespaceLine(sv))
            continue;
        if (!rule_time) {
            if (try_to_read_variable(s, vars))
                continue;
            rule_time = true;
        }

        if (IsSpace(sv.front())) {
            if (rules.empty())
                throw "The command is not in a rule scope: '" + s + "'";
            auto pos = SkipSpaces(sv);
            rules.back().content.push_back(ParseLine(sv, pos, [](char){return false;}));
            continue;
        }
        rules.push_back(ParseRuleLine(s));
        
    }

    return info;
}
