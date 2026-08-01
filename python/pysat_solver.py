# Import libraries
# (Must install "python-sat" package)
from pysat.formula import IDPool, CNF
from pysat.solvers import Solver
from itertools import combinations
import time

# === Set problem size =======================================================

N = 158
r = 5

# === Build constaints =======================================================

"""
Adds:
1) Each number in [ceil(N/2)] is assigned one color
2) Each number in [ceil(N/2)] cannot have more than one color
3) No mst's can be the same color.
4) Limits redundancy from permuting colors, for efficiency
"""
def build_exactly_one_color_pal(cnf, idpool, N, r, v_name = "v"):
    # Variable naming: v_n_c means "number n has color c" and returns v(n, c)
    def v(n,c):
        return idpool.id(f"{v_name}_{n}_{c}")

    # Create constraint 1
    for n in range(1, (N + 1)//2+1):
        cnf.append([v(n, c) for c in range(1, r + 1)])

    # Create constraint 2
    """
    Logic: n cannot be two colors for every possible combination of
    colors (r choose 2), meaning n can only be one color.
    """
    for n in range(1, (N + 1) // 2 + 1):
        for c1, c2 in combinations(range(1, r + 1), 2):
            cnf.append([-v(n, c1), -v(n, c2)])

    # Additional 'free number' when N is 2 modulo 3
    if N % 3 == 2:
        m = int(2 * (N + 1) / 3)
        cnf.append([v(m, c) for c in range(1, r + 1)])
        for c1, c2 in combinations(range(1, r + 1), 2):
            cnf.append([-v(m, c1), -v(m, c2)])
    return v

# Create reflection
def pal_rep(x, N):
    C = (N + 1) // 2
    if x <= C or x == 2 * (N + 1) / 3:
        return x
    else:
        return N + 1 - x
  
# Create constraint 3
def build_no_mst_pal(cnf, idpool, v, N, r):
    count = 0
    for x in range(1, N + 1):
        for y in range(x, N + 1):
            z = x + y
            if z > N:
                continue
            for c in range(1, r + 1):
                cnf.append([-v(pal_rep(x ,N), c),
                            -v(pal_rep(y, N), c),
                            -v(pal_rep(z, N), c)])
                count += 1
    return count

# Create constraint 4
def fix_symmetry(cnf, v):
    cnf.append([v(1, 1)])
    cnf.append([v(2, 2)])
    cnf.append([v(3, 3), v(4, 3), v(5, 3)])

# === Decode SAT solver for readability ======================================

def decode_coloring_pal(model, idpool, N, r, v_name = "v"):
    true_int = set(val for val in model if val > 0)
    coloring = {}
    for n in range(1 , (N + 1) // 2 + 1):
        assigned = [c for c in range(1, r + 1)
                    if idpool.id(f"{v_name}_{n}_{c}") in true_int]
        coloring[n] = assigned[0] if len(assigned) == 1 else assigned
    if N % 3 == 2:
        m = int(2 * (N + 1) / 3)
        assigned =[c for c in range(1, r + 1)
                   if idpool.id(f"{v_name}_{m}_{c}") in true_int]
        coloring[m] = assigned[0] if len(assigned) == 1 else assigned
    return coloring

# === Main ===================================================================

# Initiate idpool and create empty CNF list
idpool = IDPool()
cnf = CNF()

# Put constraints into cnf
v = build_exactly_one_color_pal(cnf, idpool, N, r)
build_no_mst_pal(cnf, idpool, v, N, r)
fix_symmetry(cnf, v)

# Function for SAT solver
start = time.time()
print("SAT solver in progress...")
with Solver(name = "cadical153", bootstrap_with = cnf.clauses) as S:
    sat = S.solve()
    if sat:
        print("Satisfiable!")
        model = S.get_model()
        coloring = decode_coloring_pal(model, idpool, N, r)

        # Returns "invalid" if N or r are set incorrectly
        invalid = {n: c for n, c in coloring.items() if not isinstance(c, int)}
        if invalid:
            print("Invalid assignment(s) found:", invalid)
        else:
            print("There exists a weakly palindromic mst-free coloring.")
        
        # Prints out first valid coloring combination
        color_classes = {c: [] for c in range(1, r + 1)}
        for n, c in coloring.items():
            color_classes[c].append(n)
            if n != 2 * (N + 1) / 3 and n != (N + 1) / 3:
                color_classes[c].append(N + 1 - n)
        for c in range(1, r + 1):
            color_classes[c].sort()
            print(f"color {c}: {color_classes[c]}")
    else:
        print("Unsatisfiable! No weakly palindromic mst-free coloring exists.")
end = time.time()
print("Total time elapsed (s):", round(end - start, 4))