#include "parser.hpp"
#include "generator.hpp"

#include <iostream>
#include <fstream>

void SetEmptyValueInsteadOfNothing(Vars & vars) {
    for (auto & var : vars)
        if (var.second.empty())
            var.second.emplace_back("");
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char const *argv[]) {
    std::ifstream inp("TMakefile");
    if (!inp.is_open()) {
        std::cerr << "'TMakefile' does not exist!\n";
        return 1;
    }
    
    try {
        auto file = Parse(inp);
        SetEmptyValueInsteadOfNothing(file.first);
        // for (auto const & t : file.first)
        //     std::cout << t << '\n';
        // for (auto const & t : file.second)
        //     std::cout << t << '\n';

        // std::cout << "\n==============================\n\n";

        // Generate(std::cout, file.first, file.second);
        auto generated_list = Generate(file.first, file.second);
        for (auto const & s : generated_list)
            std::cout << s << '\n';
    } catch (std::string & s) {
        std::cerr << s << '\n';
        return 1;
    } catch (std::exception & e) {
        std::cerr << e.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "Caught unknown exception :(\n";
        return 1;
    }
    return 0;
}
