// C++ cube-and-conquer version of py_linux_weakly_pal_finder_revised.py
//
// Splits search space into "cubes" and solves each cube independently
// If any cube is SAT, the whole problem is SAT and remaining running cubes are killed.
// If every cube is UNSAT, the whole problem is UNSAT.
//
// CaDiCaL must be installed and accessible through your PATH folder.
// To run, compile then run this command in bash:
// g++ -std=c++17 -O3 -pthread -o finder cubeconquer_cpp_linux_weakly_pal_finder.cpp && ./finder

// === Imports ================================================================

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <vector>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;

// === (IMPORTANT!!!) Set problem size =======================================

static const int N = 120;
static const int r = 5;

// Set number of threads to use
// (Advised not to put more than amount of cores on your computer)
static const int NUM_THREADS = 8;

// How many of the highest-degree free variables to cube on. Total cubes =
// r^CUBE_DEPTH. With r=5:
//   depth 3 ->   125 cubes  (light splitting, low ove8rhead)
//   depth 4 ->   625 cubes  (good match for 14 cores, default)
//   depth 5 -> 3,125 cubes  (finer-grained, more scheduling overhead)
// Note: aim for cubes-per-core in the tens to low hundreds so slow
// cubes don't leave cores idle near the end, without going so fine that
// per-process startup overhead dominates.
static const int CUBE_DEPTH = 4;

static const std::string WORK_DIR = "cubes";

// === Build CNF constraints =================================================

static std::map<std::tuple<std::string, int, int>, int> id_map;
static int next_var = 1;

// Define get_var() to create unique variables for each {number: color} pair
int get_var(const std::string& type, int a = -1, int b = -1) {
    auto key = std::make_tuple(type, a, b);
    auto it = id_map.find(key);
    if (it != id_map.end()) return it->second;
    int id = next_var++;
    id_map[key] = id;
    return id;
}

int v(int n, int c) { return get_var("v", n, c); }

// Build reflection map
std::vector<int> build_reflect_map(int N) {
    std::vector<int> mapping(N + 1, 0);
    int half = (N + 1) / 2;
    std::set<int> exc_vars;
    if (N % 3 == 2) {
        int exc_index = (N + 1) / 3;
        exc_vars = {exc_index, N - exc_index + 1};
    }
    for (int n = 1; n <= N; ++n) {
        if (exc_vars.count(n)) mapping[n] = n;
        else if (n <= half) mapping[n] = n;
        else mapping[n] = N - n + 1;
    }
    return mapping;
}

using Clause = std::vector<int>;

// Build constraints 1-2 with representive variables only
void build_exactly_one_color_pal(std::vector<Clause>& clauses, int N, int r,
                                  const std::vector<int>& reflect_map) {
    std::set<int> reps(reflect_map.begin() + 1, reflect_map.end());
    for (int n : reps) {
        Clause clause;
        for (int c = 1; c <= r; ++c) clause.push_back(v(n, c));
        clauses.push_back(clause);
    }
    for (int n : reps) {
        for (int c1 = 1; c1 <= r; ++c1)
            for (int c2 = c1 + 1; c2 <= r; ++c2)
                clauses.push_back({-v(n, c1), -v(n, c2)});
    }
}

// Build constraint 3 with mapped representative variables
// Accumulates a degree count per representative variable; used to pick good cube variables
void build_no_mst_pal(std::vector<Clause>& clauses, int N, int r,
                       const std::vector<int>& reflect_map,
                       std::map<int, long long>& degree) {
    for (int x = 1; x <= N; ++x) {
        for (int y = x; y <= N; ++y) {
            int z = x + y;
            if (z > N) continue;
            int rx = reflect_map[x], ry = reflect_map[y], rz = reflect_map[z];
            degree[rx]++; degree[ry]++; degree[rz]++;
            for (int c = 1; c <= r; ++c) {
                clauses.push_back({-v(rx, c), -v(ry, c), -v(rz, c)});
            }
        }
    }
}

// Build constraint 4 with representative variables
void fix_symmetry(std::vector<Clause>& clauses) {
    clauses.push_back({v(1, 1)});
    clauses.push_back({v(2, 2)});
    clauses.push_back({v(3, 3), v(4, 3), v(5, 3)});
}

// Decode SAT model for readability
std::map<int, int> decode_coloring(const std::map<int, int>& model, int N, int r,
                                    const std::vector<int>& reflect_map) {
    std::map<int, int> coloring;
    for (int n = 1; n <= N; ++n) {
        int rep = reflect_map[n];
        for (int c = 1; c <= r; ++c) {
            auto it = model.find(get_var("v", rep, c));
            if (it != model.end() && it->second > 0) { coloring[n] = c; break; }
        }
    }
    return coloring;
}

std::string which(const std::string& name) {
    std::string cmd = "which " + name + " 2>/dev/null";
    std::array<char, 256> buf{};
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) return "";
    while (fgets(buf.data(), buf.size(), pipe.get())) result += buf.data();
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();
    return result;
}

std::string format_coloring(const std::map<int, int>& coloring) {
    std::ostringstream oss;
    oss << "{";
    bool first = true;
    for (auto& [k, val] : coloring) {
        if (!first) oss << ", ";
        oss << k << ": " << val;
        first = false;
    }
    oss << "}";
    return oss.str();
}

std::map<int, int> parse_sol(const std::string& sol_path) {
    std::map<int, int> model;
    std::ifstream g(sol_path);
    std::string line;
    while (std::getline(g, line)) {
        if (line.empty() || line[0] == 'c' || line[0] == 'p' || line[0] == 's') continue;
        if (line[0] == 'v') {
            std::istringstream iss(line.substr(1));
            int lit;
            while (iss >> lit) {
                if (lit == 0) break;
                model[std::abs(lit)] = lit > 0 ? 1 : 0;
            }
        }
    }
    return model;
}

// === Cube-and-conquer dispatch ===============================================

std::atomic<bool> g_found_sat{false};
std::atomic<long long> g_unsat_count{0};
std::atomic<long long> g_error_count{0};
std::mutex g_result_mutex;
std::string g_winning_sol_path;
long long g_winning_cube_index = -1;

// Runs `cadical <cnf> -w <sol>` as a child process we can poll and kill.
// Returns the process exit code (10 SAT / 20 UNSAT / other = error), or -1
// if we killed it early because another cube already found SAT.
int run_cadical(const std::string& cadical_bin, const std::string& cnf_path,
                 const std::string& sol_path, const std::string& log_path) {
    // Flush every open C stream (stdout/stderr, which std::cout/cerr are
    // synced to by default) before forking. Without this, any buffered but
    // not-yet-written output in the parent gets duplicated: the child
    // inherits a copy of the pending buffer and flushes it again later.
    fflush(nullptr);

    pid_t pid = fork();
    if (pid == 0) {
        // child
        FILE* log = freopen(log_path.c_str(), "w", stdout);
        (void)log;
        freopen(log_path.c_str(), "a", stderr);
        execlp(cadical_bin.c_str(), cadical_bin.c_str(), cnf_path.c_str(),
               "-w", sol_path.c_str(), (char*)nullptr);
        _exit(127); // exec failed
    }
    if (pid < 0) return -2; // fork failed

    int status;
    while (true) {
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid) break;
        if (g_found_sat.load(std::memory_order_relaxed)) {
            kill(pid, SIGTERM);
            // give it a moment, then force kill if still alive
            for (int i = 0; i < 20; ++i) {
                pid_t r2 = waitpid(pid, &status, WNOHANG);
                if (r2 == pid) return -1;
                usleep(10000);
            }
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            return -1;
        }
        usleep(20000); // 20ms poll
    }
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return -3; // killed by signal etc.
}

// === Main ================================================================

int main() {
    std::string cadical_bin = which("cadical");

    std::vector<int> reflect_map = build_reflect_map(N);

    std::vector<Clause> base_clauses;
    std::map<int, long long> degree;
    build_exactly_one_color_pal(base_clauses, N, r, reflect_map);
    build_no_mst_pal(base_clauses, N, r, reflect_map, degree);
    fix_symmetry(base_clauses);

    int num_base_vars = next_var - 1;

    if (cadical_bin.empty()) {
        // Fall back to writing a single CNF, same as the sequential version.
        std::string cnf_path = "weakly_pal.cnf";
        std::ofstream f(cnf_path);
        f << "p cnf " << num_base_vars << " " << base_clauses.size() << "\n";
        for (auto& c : base_clauses) {
            for (int lit : c) f << lit << " ";
            f << "0\n";
        }
        std::cout << "\nWrote DIMACS CNF onto " << cnf_path << "\n";
        std::cout << "\nCaDiCaL not found on PATH. Can run manually with:\n"
                   << "cadical " << cnf_path << " result.sol\n";
        return 0;
    }

    // Pick cube variables: highest-degree reps, excluding 1 and 2 (fixed symmetry)
    std::set<int> reps(reflect_map.begin() + 1, reflect_map.end());
    std::vector<int> candidates;
    for (int rep : reps) if (rep != 1 && rep != 2) candidates.push_back(rep);
    std::sort(candidates.begin(), candidates.end(), [&](int a, int b) {
        return degree[a] > degree[b];
    });
    int depth = std::min((int)candidates.size(), CUBE_DEPTH);
    std::vector<int> cube_vars(candidates.begin(), candidates.begin() + depth);

    long long total_cubes = 1;
    for (int i = 0; i < depth; ++i) total_cubes *= r;

    std::cout << "\nBase CNF: " << num_base_vars << " vars, "
              << base_clauses.size() << " clauses.\n";
    std::cout << "Cubing on " << depth << " variable(s) (highest constraint degree): ";
    for (int cv : cube_vars) std::cout << cv << " ";
    std::cout << "-> " << total_cubes << " cubes across " << NUM_THREADS
              << " worker threads.\n";

    fs::create_directory(WORK_DIR);

    // Pre-render the base clause lines once; every cube file is "header + k unit clauses + these lines"
    std::ostringstream base_body;
    for (auto& c : base_clauses) {
        for (int lit : c) base_body << lit << " ";
        base_body << "0\n";
    }
    std::string base_body_str = base_body.str();

    auto start = std::chrono::steady_clock::now();

    std::atomic<long long> next_cube{0};

    auto worker = [&]() {
        while (true) {
            long long idx = next_cube.fetch_add(1);
            if (idx >= total_cubes) return;
            if (g_found_sat.load(std::memory_order_relaxed)) return;

            // Decode idx into a color assignment for each cube variable
            // (base-r digits of idx).
            std::vector<int> colors(depth);
            long long t = idx;
            for (int i = 0; i < depth; ++i) {
                colors[i] = (t % r) + 1;
                t /= r;
            }

            std::string cnf_path = WORK_DIR + "/cube_" + std::to_string(idx) + ".cnf";
            std::string sol_path = WORK_DIR + "/cube_" + std::to_string(idx) + ".sol";
            std::string log_path = WORK_DIR + "/cube_" + std::to_string(idx) + ".log";

            {
                std::ofstream f(cnf_path);
                f << "p cnf " << num_base_vars << " "
                  << (base_clauses.size() + depth) << "\n";
                for (int i = 0; i < depth; ++i)
                    f << v(cube_vars[i], colors[i]) << " 0\n";
                f << base_body_str;
            }

            int code = run_cadical(cadical_bin, cnf_path, sol_path, log_path);

            if (code == 10) {
                std::lock_guard<std::mutex> lock(g_result_mutex);
                if (!g_found_sat.load()) {
                    g_found_sat.store(true);
                    g_winning_sol_path = sol_path;
                    g_winning_cube_index = idx;
                }
                // don't delete the winning files
                if (g_winning_cube_index != idx) {
                    std::error_code ec;
                    fs::remove(cnf_path, ec);
                    fs::remove(sol_path, ec);
                    fs::remove(log_path, ec);
                }
                return;
            } else if (code == 20) {
                g_unsat_count.fetch_add(1);
                std::error_code ec;
                fs::remove(cnf_path, ec);
                fs::remove(sol_path, ec);
                fs::remove(log_path, ec);
            } else if (code == -1) {
                // killed early because another cube already won; clean up
                std::error_code ec;
                fs::remove(cnf_path, ec);
                fs::remove(sol_path, ec);
                fs::remove(log_path, ec);
            } else {
                g_error_count.fetch_add(1);
                std::cerr << "cube " << idx << " exited with unexpected code "
                          << code << " (see " << log_path << ")\n";
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; ++i) threads.emplace_back(worker);
    for (auto& t : threads) t.join();

    double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();
    std::cout << "\nAll cubes finished in " << elapsed << " seconds.\n";

    if (g_found_sat.load()) {
        auto model = parse_sol(g_winning_sol_path);
        auto coloring = decode_coloring(model, N, r, reflect_map);
        std::cout << "SAT via cube " << g_winning_cube_index << ".\n";
        std::cout << "\nColoring found:\n" << format_coloring(coloring) << "\n";
    } else if (g_unsat_count.load() == total_cubes) {
        std::cout << "\nUnsatisfiable! No valid coloring exists for N = " << N
                  << " and r = " << r << ".\n";
    } else {
        std::cout << "\nNo SAT cube found, but " << g_error_count.load()
                  << " cube(s) errored out (of " << total_cubes
                  << "). Check logs under " << WORK_DIR << "/ for details.\n";
    }

    return 0;
}