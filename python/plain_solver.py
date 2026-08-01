# Weakly palindromic coloring CNF generator solved with CaDiCaL through Linux.
# CaDiCaL must be installed and accessible through your PATH folder.
# I ran it through WSL (Ubuntu) on Windows but should work on any OS.

# === Imports ================================================================

# Used to create constraint 2
from itertools import combinations
# Used for CaDiCaL to operate outside this script
import os, shutil, subprocess, time


# === Set problem size: [N] and r colors =====================================

N = 120
r = 5
# Toggle to show CaDiCaL real-time output
show_output = False

# === Build CNF constraints for SAT solver ===================================

id_map = {}
next_var = 1

# Define get_var() to create unique variables for each {number: color} pair
def get_var(type, a = None, b = None):
    global next_var
    key = (type, a, b)
    if key not in id_map:
        id_map[key] = next_var
        next_var += 1
    return id_map[key]

# Create reflection map
"""
1) Each unique variable uses a representative variable from the first half of list.
2) For special cases of N = 155 and 158, their "exceptions" are left as independent
  representative variables instead.
3) It is defined as its own function so it can be used by separate constraints""
"""
def build_reflect_map(N):
    mapping = {}
    half = (N + 1) // 2
    exc_vars = set()
    if N % 3 == 2:
        exc_index = (N + 1) // 3
        exc_vars = {exc_index, N - exc_index + 1}
    for n in range(1, N + 1):
        if n in exc_vars:
            mapping[n] = n
        elif n <= half:
            mapping[n] = n
        else:
            mapping[n] = N - n + 1
    return mapping

# Create constraints
"""
Adds:
#1) Each number in [N] must be assigned one color.
#2) Each number in [N] must be assigned at most one color.
#3) No mst's can be monochromatic.
#4) Fixes symmetry
"""
# Build constraints 1-2 with representive variables only
def build_exactly_one_color_pal(clauses, N, r, reflect_map):
    def v(n, c):
        return get_var("v", n, c)
    # reps = representative variables
    reps = sorted(set(reflect_map.values()))
    for n in reps:
        clauses.append([v(n, c) for c in range(1, r + 1)])
    for n in reps:
        for c1, c2 in combinations(range(1, r + 1), 2):
            clauses.append([-v(n, c1), -v(n, c2)])
    return v

# Build constraint 3 with mapped representative variables
def build_no_mst_pal(clauses, v, N, r, reflect_map):
    for x in range(1, N + 1):
        for y in range(x, N + 1):
            z = x + y
            if z > N:
                continue
            for c in range(1, r + 1):
                clauses.append([
                    -v(reflect_map[x], c),
                    -v(reflect_map[y], c),
                    -v(reflect_map[z], c)
                ])

# Build constraint 4 with representative variables
def fix_symmetry(clauses, v):
    clauses.append([v(1, 1)])
    clauses.append([v(2, 2)])
    clauses.append([v(3, 3), v(4, 3), v(5, 3)])

# === Decode SAT model for readability =======================================

# Produce a readable coloring if one exists
def decode_coloring(model, N, r, reflect_map):
    coloring = {}
    for n in range(1, N + 1):
        rep = reflect_map[n]
        for c in range(1, r + 1):
            if model.get(get_var("v", rep, c), 0) > 0:
                coloring[n] = c
                break
    return coloring

# === Main ===================================================================

# Build reflection map
reflect_map = build_reflect_map(N)

# Build CNF file with clauses
clauses = []
v = build_exactly_one_color_pal(clauses, N, r, reflect_map)
build_no_mst_pal(clauses, v, N, r, reflect_map)
fix_symmetry(clauses, v)

# Write CNF file
num_vars = next_var - 1
cnf_path = "weakly_pal.cnf"
with open(cnf_path, "w", encoding = "utf-8") as f:
    f.write(f"p cnf {num_vars} {len(clauses)}\n")
    for clause in clauses:
        f.write(" ".join(map(str, clause)) + " 0\n")

print(f"\nWrote DIMACS CNF onto {cnf_path}")

# Function to run CaDiCaL
cadical = shutil.which("cadical")
if cadical:
    sol_path = "result.sol"

    start = time.perf_counter()

    process = subprocess.Popen(
        [cadical, cnf_path, "-w", sol_path],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1
    )

    stderr_lines = []
    for line in process.stdout:
        if show_output:
            print(line, end="", flush=True)
        stderr_lines.append(line)

    process.wait()
    returncode = process.returncode

    elapsed = time.perf_counter() - start
    print(f"\nCaDiCaL finished in {elapsed:.2f} seconds.")

    if not show_output:
        print("".join(stderr_lines[-20:]))
    if returncode == 10:
        model = {}
        if os.path.exists(sol_path):
            with open(sol_path, "r", encoding="utf-8") as g:
                for line in g:
                    line = line.strip()
                    if not line or line.startswith("c") or line.startswith("p") or line.startswith("s"):
                        continue
                    if line.startswith("v"):
                        tokens = line.split()[1:]
                        for token in tokens:
                            if token == "0":
                                break
                            lit = int(token)
                            model[abs(lit)] = 1 if lit > 0 else 0
        coloring = decode_coloring(model, N, r, reflect_map)
        print(f"\nColoring found:\n{coloring}")
    elif returncode == 20:
        print(f"\nUnsatisfiable! No valid coloring exists for N = {N} and r = {r}.")
    else:
        print("\nCaDiCaL reported an error:", returncode)
        print("".join(stderr_lines))
else:
    print(f"\nCaDiCaL not found on PATH. Can run manually with:\ncadical {cnf_path} result.sol")