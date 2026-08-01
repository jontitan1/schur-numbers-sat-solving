# Weakly Palindromic Monochromatic Schur Triple SAT Solving

(README in progress. Completion coming soon.)

Research project from the Kinnaird Institute Research Program at Millsaps College, supervised by Dr. Alex Rice and Dr. Priyadarshi Dey, testing a conjecture from Fredrickson-Sweet's 2000 paper that speculates certain 5-colored values of N <= 160 fail to produce "weakly palindromic" coloring.

## Background
A Schur triple is a group of three positive integers (x,y,z) such that x + y = z. They are prevalent in Ramsey theory, which studies the patterns of numbers when grouped/colored in finite sets. A monochromatic Schur triple (MST) is where all three integers (x + y = z) share the same color in a partitioned set. Schur's Theorem proves an MST is guaranteed to appear at least once if you color N = {1,2,...,N} up to a large enough number, S(r), using r amount of colors. For example, it was found in 2017 that for five colors, S(5) = 161, meaning all possible coloring of N = 161 is guaranteed to have at least one MST. This also proves that there exists at least one coloring for each 5-coloring of N < 161 where there are no MSTs.

Similarly, a modular monochromatic Schur triple (MMST) changes the condition of addition equation to a modular setting, satisfying x + y is congruent to z % N.
