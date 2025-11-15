#include <iostream>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <cmath>
#include <cstring>
#include <algorithm>

const int MAX_NUM = 10000;
const int NUM_PROCESSES = 10;
const int INTERVAL = MAX_NUM / NUM_PROCESSES;

bool is_prime(int n) {
    if (n <= 1) return false;
    if (n <= 3) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;
    for (int i = 5; i * i <= n; i = i + 6) {
        if (n % i == 0 || n % (i + 2) == 0) return false;
    }
    return true;
}

void child_process(int read_pipe_fd, int write_pipe_fd) {
    int start, end;
    if (read(read_pipe_fd, &start, sizeof(int)) <= 0 ||
        read(read_pipe_fd, &end, sizeof(int)) <= 0) {
        exit(1);
    }
    close(read_pipe_fd);

    std::vector<int> primes;
    for (int i = start; i < end; ++i) {
        if (is_prime(i)) {
            primes.push_back(i);
        }
    }
    int count = primes.size();
    write(write_pipe_fd, &count, sizeof(int));
    if (count > 0) {
        write(write_pipe_fd, primes.data(), count * sizeof(int));
    }
    close(write_pipe_fd);
    exit(0);
}

int main() {
    std::cout << "Porneste cautarea de numere prime pana la " << MAX_NUM << " folosind " << NUM_PROCESSES << " procese.\n";
    int parent_to_child_pipes[NUM_PROCESSES][2];
    int child_to_parent_pipes[NUM_PROCESSES][2];
    pid_t pids[NUM_PROCESSES];
    std::vector<int> all_primes;
    for (int i = 0; i < NUM_PROCESSES; ++i) {
        if (pipe(parent_to_child_pipes[i]) == -1 || pipe(child_to_parent_pipes[i]) == -1) {
            std::cerr << "Eroare la crearea pipe-urilor.\n";
            return 1;
        }
        pids[i] = fork();
        if (pids[i] == -1) {
            std::cerr << "Eroare la fork().\n";
            return 1;
        } else if (pids[i] == 0) {
            // PROCES COPIL
            close(parent_to_child_pipes[i][1]); // Nu scrie catre copil
            close(child_to_parent_pipes[i][0]); // Nu citeste de la copil
            child_process(parent_to_child_pipes[i][0], child_to_parent_pipes[i][1]);
        } else {
            // PROCES PARINTE
            close(parent_to_child_pipes[i][0]); // Nu citeste de la parinte
            close(child_to_parent_pipes[i][1]); // Nu scrie catre parinte
            int start = i * INTERVAL + 1;
            int end = (i + 1) * INTERVAL + 1;

            write(parent_to_child_pipes[i][1], &start, sizeof(int));
            write(parent_to_child_pipes[i][1], &end, sizeof(int));

            close(parent_to_child_pipes[i][1]);
        }
    }
    for (int i = 0; i < NUM_PROCESSES; ++i) {
        if (pids[i] > 0) {
            waitpid(pids[i], nullptr, 0);
            int count = 0;
            if (read(child_to_parent_pipes[i][0], &count, sizeof(int)) > 0) {
                if (count > 0) {
                    std::vector<int> primes(count);
                    read(child_to_parent_pipes[i][0], primes.data(), count * sizeof(int));
                    all_primes.insert(all_primes.end(), primes.begin(), primes.end());
                }
            }
            close(child_to_parent_pipes[i][0]);
        }
    }

    std::sort(all_primes.begin(), all_primes.end());
    std::cout << "\nNumerele prime gasite:\n";
    for (int prime : all_primes) {
        std::cout << prime << " ";
    }

    std::cout << "\n\nTotal numere prime gasite: " << all_primes.size() << "\n";
    return 0;
}
