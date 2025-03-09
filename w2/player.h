#pragma once

#include <string>

struct Player {
    std::string id;
    float x, y;
    unsigned ping;

    void UpdateFromString (const std::string& str) {
        sscanf(str.c_str(), "%g %g", &x, &y);
    }

    std::string String (const std::string& sep = " ") const {
        return id + sep + std::to_string(x) + sep + std::to_string(y) + sep + std::to_string(ping);
    }
};

#include <vector>
#include <string>
#include <sstream>

std::vector<std::string> split(const std::string& str, char delimiter = ' ') {
    std::vector<std::string> tokens; 
    if (str.length() == 0) {
        return tokens;
    }

    std::string token;               
    std::istringstream tokenStream(str); 

    while (std::getline(tokenStream, token, delimiter)) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }

    return tokens;
}
