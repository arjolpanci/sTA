#include "Core/game.hpp"
#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    try {
        Game game;
        return game.run(argc > 1 && std::string(argv[1]) == "--smoke-test", argc > 1 && std::string(argv[1]) == "--benchmark");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
