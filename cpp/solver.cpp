// C++ version of py_linux_weakly_pal_finder_revised.py
// CaDiCaL must be installed and accessible through your PATH folder.
// To run, compile then run this command in bash:
// g++ -std=c++17 -O2 -o weakly_pal_finder /home/jon/cpp_linux_weakly_pal_finder.cpp && ./weakly_pal_finder

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace fs = std::filesystem;

// === Set problem size: N and r colors =======================================

static const int N = 120;
static const int r = 5;
// Toggle to show CaDiCaL real-time output
static const bool show_output = false;

// === Variable bookkeeping ====================================================

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

// v(n, c) shorthand used throughout, matching the Python closure.
int v(int n, int c) { return get_var("v", n, c); }

// === Reflection map ==========================================================
// 1) Each unique variable uses a representative variable from the first half
//    of the list.
// 2) For the special case N % 3 == 2, the two "exception" indices are left
//    as independent representative variables instead of being folded in.
// 3) Kept as its own function so it can be reused by separate constraints.

std::vector<int> build_reflect_map(int N) {
    std::vector<int> mapping(N + 1, 0); // 1-indexed, mapping[0] unused
    int half = (N + 1) / 2;
    std::set<int> exc_vars;
    if (N % 3 == 2) {
        int exc_index = (N + 1) / 3;
        exc_vars = {exc_index, N - exc_index + 1};
    }
    for (int n = 1; n <= N; ++n) {
        if (exc_vars.count(n)) {
            mapping[n] = n;
        } else if (n <= half) {
            mapping[n] = n;
        } else {
            mapping[n] = N - n + 1;
        }
    }
    return mapping;
}

// === Constraints =============================================================
// Adds:
// 1) Each number in [N] must be assigned one color.
// 2) Each number in [N] must be assigned at most one color.
// 3) No mst's can be monochromatic.
// 4) Fixes symmetry.

using Clause = std::vector<int>;

// Build constraints 1-2 with representative variables only.
void build_exactly_one_color_pal(std::vector<Clause>& clauses, int N, int r,
                                  const std::vector<int>& reflect_map) {
    std::set<int> reps(reflect_map.begin() + 1, reflect_map.end());
    for (int n : reps) {
        Clause clause;
        for (int c = 1; c <= r; ++c) clause.push_back(v(n, c));
        clauses.push_back(clause);
    }
    for (int n : reps) {
        for (int c1 = 1; c1 <= r; ++c1) {
            for (int c2 = c1 + 1; c2 <= r; ++c2) {
                clauses.push_back({-v(n, c1), -v(n, c2)});
            }
        }
    }
}

// Build constraint 3 with mapped representative variables.
void build_no_mst_pal(std::vector<Clause>& clauses, int N, int r,
                       const std::vector<int>& reflect_map) {
    for (int x = 1; x <= N; ++x) {
        for (int y = x; y <= N; ++y) {
            int z = x + y;
            if (z > N) continue;
            for (int c = 1; c <= r; ++c) {
                clauses.push_back({
                    -v(reflect_map[x], c),
                    -v(reflect_map[y], c),
                    -v(reflect_map[z], c)
                });
            }
        }
    }
}

// Build constraint 4 with representative variables.
void fix_symmetry(std::vector<Clause>& clauses) {
    clauses.push_back({v(1, 1)});
    clauses.push_back({v(2, 2)});
    clauses.push_back({v(3, 3), v(4, 3), v(5, 3)});
}

// === Decode SAT model for readability ========================================

std::map<int, int> decode_coloring(const std::map<int, int>& model, int N, int r,
                                    const std::vector<int>& reflect_map) {
    std::map<int, int> coloring;
    for (int n = 1; n <= N; ++n) {
        int rep = reflect_map[n];
        for (int c = 1; c <= r; ++c) {
            auto it = model.find(get_var("v", rep, c));
            if (it != model.end() && it->second > 0) {
                coloring[n] = c;
                break;
            }
        }
    }
    return coloring;
}

// === Small helpers ============================================================

// Equivalent of shutil.which("cadical").
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

// Prints a map<int,int> the way Python prints a dict, e.g. {1: 1, 2: 2}.
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

// === Main =====================================================================

int main() {
    // Build reflection map
    std::vector<int> reflect_map = build_reflect_map(N);

    // Build CNF file with clauses
    std::vector<Clause> clauses;
    build_exactly_one_color_pal(clauses, N, r, reflect_map);
    build_no_mst_pal(clauses, N, r, reflect_map);
    fix_symmetry(clauses);

    // Write CNF file
    int num_vars = next_var - 1;
    std::string cnf_path = "weakly_pal.cnf";
    {
        std::ofstream f(cnf_path);
        f << "p cnf " << num_vars << " " << clauses.size() << "\n";
        for (auto& clause : clauses) {
            for (int lit : clause) f << lit << " ";
            f << "0\n";
        }
    }

    std::cout << "\nWrote DIMACS CNF onto " << cnf_path << "\n";

    // Run CaDiCaL
    // 1) You need to have CaDiCaL installed and accessible through your PATH.
    // 2) I personally ran it through WSL (Ubuntu) on Windows but should work
    //    on any OS.
    std::string cadical = which("cadical");
    if (!cadical.empty()) {
        std::string sol_path = "result.sol";
        std::string cmd = cadical + " " + cnf_path + " -w " + sol_path + " 2>&1";

        auto start = std::chrono::steady_clock::now();

        std::vector<std::string> output_lines;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        if (!pipe) {
            std::cerr << "Failed to launch cadical.\n";
            return 1;
        }
        std::array<char, 1024> buf{};
        std::string line_acc;
        while (fgets(buf.data(), buf.size(), pipe.get())) {
            std::string chunk(buf.data());
            if (show_output) std::cout << chunk << std::flush;
            output_lines.push_back(chunk);
        }
        int status = pclose(pipe.release());
        int returncode = WEXITSTATUS(status);

        auto elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - start).count();
        std::cout << "\nCaDiCaL finished in " << elapsed << " seconds.\n";

        if (!show_output) {
            int start_idx = std::max(0, (int)output_lines.size() - 20);
            for (int i = start_idx; i < (int)output_lines.size(); ++i)
                std::cout << output_lines[i];
        }

        if (returncode == 10) {
            std::map<int, int> model;
            if (fs::exists(sol_path)) {
                std::ifstream g(sol_path);
                std::string line;
                while (std::getline(g, line)) {
                    if (line.empty() || line[0] == 'c' || line[0] == 'p' || line[0] == 's')
                        continue;
                    if (line[0] == 'v') {
                        std::istringstream iss(line.substr(1));
                        int lit;
                        while (iss >> lit) {
                            if (lit == 0) break;
                            model[std::abs(lit)] = lit > 0 ? 1 : 0;
                        }
                    }
                }
            }
            auto coloring = decode_coloring(model, N, r, reflect_map);
            std::cout << "\nColoring found:\n" << format_coloring(coloring) << "\n";
        } else if (returncode == 20) {
            std::cout << "\nUnsatisfiable! No valid coloring exists for N = " << N
                      << " and r = " << r << ".\n";
        } else {
            std::cout << "\nCaDiCaL reported an error: " << returncode << "\n";
            for (auto& l : output_lines) std::cout << l;
        }
    } else {
        std::cout << "\nCaDiCaL not found on PATH. Can run manually with:\n"
                   << "cadical " << cnf_path << " result.sol\n";
    }

    return 0;
}