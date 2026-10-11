# Weakly Palindromic Monochromatic Schur Triple SAT Solving

Research project from the Kinnaird Institute Research Program at Millsaps College, supervised by Dr. Alex Rice and Dr. Priyadarshi Dey, testing a conjecture from Fredricksen and Sweet's 2000 paper that speculates certain 5-colored values of N <= 160 fail to produce "weakly palindromic" coloring.

## Background

A Schur triple is a group of three positive integers (x,y,z) such that x + y = z. They are prevalent in Ramsey theory, which studies the patterns of numbers when grouped/colored in finite sets. A monochromatic Schur triple (MST) is where all three integers (x + y = z) share the same color in a partitioned set. Schur's Theorem proves an MST is guaranteed to appear at least once if you color N = {1, 2, ..., N} up to a large enough number, S(r), using r amount of colors. For example, it was found in 2017 that for five colors, S(5) = 161, meaning all possible coloring of N = 161 is guaranteed to have at least one MST. This also proves that there exists at least one coloring for each 5-coloring of N < 161 where there are no MSTs.

Similarly, a modular monochromatic Schur triple (MMST) changes the condition of addition equation to a modular setting, satisfying x + y is congruent to z % N.

## Results

The conjecture was tested with a computation of about two days on the Magnolia supercluster at the Mississippi Center for Supercomputing Research (MCSR).

- There does not exist a palindromic 5-coloring of [158] with no monochromatic solutions to x + y = z. This confirms the conjecture of Fredricksen and Sweet (2000) for N = 158.
- The analogous conjecture for [155] is false.
- Together with the case above, palindromic MST-free 5-colorings of [N] are impossible for N > 160 and possible for all N <= 160 except N = 158.
- This is the first known example of M, N, r with M < N where a palindromic MST-free r-coloring exists for [N] but not for [M] (M = 158, N = 160, r = 5).

Solver benchmarks across N = 40 to 158 show a sharp increase in difficulty around N = 133 to 140.

## Method

1. Each integer in {1, ..., N} is assigned one of r colors, with one Boolean variable per integer-color pair.
2. Palindromic symmetry is imposed by mapping position n and position N - n + 1 to shared variables, which shrinks the search space. The weakly palindromic variant allows exceptions at (N + 1) // 3 and its mirror. This symmetry-reduction conjecture was proposed by Dr. Alex Rice.
3. For every Schur triple x + y = z and every color, a clause forbids all three integers from sharing that color. The modular variant (MMST) applies the same constraint with z taken modulo N.
4. The resulting CNF formula is solved with the CaDiCaL SAT solver, which uses conflict-driven clause learning. A satisfying assignment is a valid coloring, and an unsatisfiable result shows that none exists.

CNF output was checked for equivalence between the two solver scripts.

## Repository Structure

```
python/
  pysat_solver.py   Solver built on PySAT
  plain_solver.py   Standalone solver that writes a CNF file and calls CaDiCaL directly
```

## Requirements

- Python 3
- [PySAT](https://pysathq.github.io/): `pip install python-sat`
- [CaDiCaL](https://github.com/arminbiere/cadical), built from source (needed for `plain_solver.py`)

## Usage

```
python python/pysat_solver.py <N> <r>
python python/plain_solver.py <N> <r>
```

## Reference

H. Fredricksen and M. M. Sweet, "Symmetric Sum-Free Partitions and Lower Bounds for Schur Numbers," Electronic Journal of Combinatorics 7(1), #R32, 2000. https://doi.org/10.37236/1510

## Acknowledgments

Supervised by Dr. Alex Rice and Dr. Priyadarshi Dey. Kinnaird Institute Research Program, Millsaps College. Computation performed on the Magnolia supercluster at the Mississippi Center for Supercomputing Research (MCSR).
