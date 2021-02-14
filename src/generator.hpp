#pragma once
#include "data_types.hpp"
#include <iosfwd>
#include <list>
#include <string>

std::ostream & operator << (std::ostream & out, StrLine const & line);

std::ostream & operator << (std::ostream & out, Rule const & rule);

std::ostream & operator << (std::ostream & out, Var const & var);

// void Generate(std::ostream & out, Vars const & vars, Rules const & rule);
std::list<std::string> Generate(Vars const & vars, Rules const & rules);
