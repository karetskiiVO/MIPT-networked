#pragma once

#include <iostream>

template <typename T>
void print (const T& value) {
    std::cout << value;
}

template <typename T, typename... Args>
void print (const T& first, const Args&... args) {
    print(first);
    std::cout << " ";
    print(args...);
}

template <typename... Args>
void println (const Args&... args) {
    print(args...);
    std::cout << "\n";
}
