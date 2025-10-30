#include "io.h"
#include <iostream>
#include <vector>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cstring>

class Io_class {
public:
    int io(std::string& input);
};

std::vector<std::string> split(const std::string &s) {
    std::vector<std::string> tokens;
    std::istringstream iss(s);
    std::string token;
    while (iss >> token) tokens.push_back(token);
    return tokens;
}

int Io_class::io(std::string &input) {
    std::vector<std::string> tokens = split(input);
    std::vector<char*> args;
    std::string inputFile, outputFile;
    bool append = false;

    // Parse tokens
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i] == "<") {
            inputFile = tokens[i + 1];
            ++i;
        } else if (tokens[i] == ">") {
            outputFile = tokens[i + 1];
            ++i;
        } else if (tokens[i] == ">>") {
            outputFile = tokens[i + 1];
            append = true;
            ++i;
        } else {
            args.push_back(const_cast<char*>(tokens[i].c_str()));
        }
    }

    args.push_back(nullptr);

    pid_t pid = fork();
    if (pid == 0) {
        // Child Process
        if (!inputFile.empty()) {
            int inFd = open(inputFile.c_str(), O_RDONLY);
            if (inFd < 0) {
                perror("Error opening input file");
                exit(1);
            }
            dup2(inFd, STDIN_FILENO);
            close(inFd);
        }

        if (!outputFile.empty()) {
            int flags = O_WRONLY | O_CREAT;
            flags |= (append ? O_APPEND : O_TRUNC);
            int outFd = open(outputFile.c_str(), flags, 0644);
            if (outFd < 0) {
                perror("Error opening output file");
                exit(1);
            }
            dup2(outFd, STDOUT_FILENO);
            close(outFd);
        }

        execvp(args[0], args.data());
        perror("execvp failed");
        exit(1);
    } else if (pid > 0) {
        // Parent
        int status;
        waitpid(pid, &status, 0);
    } else {
        perror("fork failed");
    }

    return 0;
}

