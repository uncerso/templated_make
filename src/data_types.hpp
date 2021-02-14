#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <utility>

using VarName = std::string;
using VarValues = std::vector<std::string>;

using Var = std::pair<VarName, VarValues>;
using Vars = std::unordered_map<VarName, VarValues>;

struct Node {
    std::string s;
    bool is_var = false;
    std::string tag;
};

using StrLine = std::vector<Node>;

struct Rule {
    StrLine name;
    std::vector<StrLine> dependencies;
    std::vector<StrLine> content;
};

using Rules = std::vector<Rule>;
using FileInfo = std::pair<Vars, Rules>;
