#include <iostream>

#include "Engine.hpp"

int main() {
    try
    {
        Engine engine;
    }
    catch(const vk::SystemError& err)
    {
        std::cerr << err.what() << '\n';
        return EXIT_FAILURE;
    }
    catch(const std::exception& err)
    {
        std::cerr << err.what() << '\n';
        return EXIT_SUCCESS;
    }
    

    return EXIT_SUCCESS;
}