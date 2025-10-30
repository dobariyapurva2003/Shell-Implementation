#include "pipe.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cstring>

class Pipe_class {
public:
    int executePipeline(const std::string &input);
};

std::vector<std::string> split(const std::string &s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream ss(s);
    while (std::getline(ss, token, delimiter)) {
        // trim
        size_t start = token.find_first_not_of(" \t");
        size_t end = token.find_last_not_of(" \t");
        if (start != std::string::npos)
            tokens.push_back(token.substr(start, end - start + 1));
    }
    return tokens;
}

int Pipe_class::executePipeline(const std::string &input) {
    std::vector<std::string> commands = split(input, '|');
    int numCommands = commands.size();
    int pipefds[2 * (numCommands - 1)];

    // Create pipes
    for (int i = 0; i < numCommands - 1; i++) {
        if (pipe(pipefds + i * 2) < 0) {
            perror("pipe");
            exit(1);
        }
    }

    for (int i = 0; i < numCommands; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            // Child process

            // --- Handle input redirection for first command ---
            if (i == 0 && commands[i].find('<') != std::string::npos) {
                size_t pos = commands[i].find('<');
                std::string file = commands[i].substr(pos + 1);
                file.erase(0, file.find_first_not_of(" \t"));
                commands[i] = commands[i].substr(0, pos);
                int fd = open(file.c_str(), O_RDONLY);
                if (fd < 0) { perror("open input"); exit(1); }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }

            // --- Handle output redirection for last command ---
            if (i == numCommands - 1 && 
               (commands[i].find('>') != std::string::npos)) {

                bool append = (commands[i].find(">>") != std::string::npos);
                size_t pos = commands[i].find('>');
                std::string file = commands[i].substr(pos + (append ? 2 : 1));
                file.erase(0, file.find_first_not_of(" \t"));
                commands[i] = commands[i].substr(0, pos);

                int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
                int fd = open(file.c_str(), flags, 0644);
                if (fd < 0) { perror("open output"); exit(1); }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            // --- Set up input pipe (not first command) ---
            if (i != 0) {
                if (dup2(pipefds[(i - 1) * 2], STDIN_FILENO) == -1) {
                    perror("dup2 input pipe");
                    exit(1);
                }
            }

            // --- Set up output pipe (not last command) ---
            if (i != numCommands - 1) {
                if (dup2(pipefds[i * 2 + 1], STDOUT_FILENO) == -1) {
                    perror("dup2 output pipe");
                    exit(1);
                }
            }

            // Close all pipe ends
            for (int j = 0; j < 2 * (numCommands - 1); j++) {
                close(pipefds[j]);
            }

            // Tokenize command
            std::istringstream iss(commands[i]);
            std::vector<std::string> args;
            std::string arg;
            while (iss >> arg) args.push_back(arg);
            std::vector<char*> argv;
            for (auto &a : args) argv.push_back(&a[0]);
            argv.push_back(nullptr);

            execvp(argv[0], argv.data());
            perror("execvp");
            exit(1);
        }
    }

    // Parent closes all pipe ends
    for (int i = 0; i < 2 * (numCommands - 1); i++) {
        close(pipefds[i]);
    }

    // Wait for all children
    for (int i = 0; i < numCommands; i++) {
        wait(nullptr);
    }

    return 0;
}
