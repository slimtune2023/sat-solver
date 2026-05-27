# SAT Solver

This repository is the optimization target for the SAT solver agent system.

The baseline solver is `code/fast.c`. It is intentionally small and correct
rather than feature-complete: a DPLL solver with watched-literal Boolean
constraint propagation, chronological backtracking, and a first-unassigned
branching heuristic.

Input format is DIMACS CNF:

```sh
make -C code
code/fast test_inputs/smoke/sat_unit.cnf
```

Output is:

- `SAT` followed by a total assignment ending in `0`; or
- `UNSAT`.

The optimizer/orchestrator repository owns detailed experiment records,
benchmark policy, and generated run documentation. This solver repository keeps
only source code, a compact README, and solver test inputs.
