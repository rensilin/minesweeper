# No-guess generation

`--mode=no-guess` generates a complete board for the actual first opening.
The first cell and all its neighbors are safe. The requested mine count is
preserved throughout generation. Ordinary random boards remain the default.

## Solve, repair, and retry

The initial candidate uses a partial Fisher–Yates shuffle of all cells outside
the protected opening. The first `mineCount` entries are the ordered mine list.

The solver starts with only the first clue. It repeatedly opens proven safe
cells and records proven mines. Its deductions use visible clue equations,
subset differences and the remaining mine count. Derived constraints may be
combined further: there is no limit on how many original clues contributed to
a deduction. The total mine count is used only when every remaining hidden cell
must be safe or every remaining hidden cell must be a mine; it does not enter
the subset-difference closure. The solver is sound but is not required to find
every possible deduction. Hidden mine positions are used only to reveal a proven safe cell's
clue and to recognize completion, never as a deduction premise.

When the solver stops, the generator chooses an unresolved neighborhood of a
visible clue. It tries to empty or fill that neighborhood, moving the same
number of mines to or from other unresolved cells. The opening, revealed safe
cells and proven mine cells keep their true status. Clue values can change,
so all old deductions are discarded and the modified board is solved afresh.
The fewest-mine feasible repair is preferred, with random tie breaking.

After at most 12 repairs, or when none is possible, the generator resamples a
suffix of the mine list. The suffix grows from one mine to the entire list;
each resampling allows another bounded group of repairs. Moving mines also
updates their positions in the ordered pool, so subsequent suffix resampling
always operates on the current board. A full resampling starts a new cycle.

The accepted board is the final fixed candidate that the solver has completely
solved from the first click. Failed candidates are never returned as playable
no-guess boards. User flags placed before the first click are not premises in
this validation; incorrect user flags can still obstruct play afterwards.

## Limits and cancellation

One generation call defaults to 512 candidate checks, 3000 ms and 20,000,000
work units. Work and elapsed time cover the entire call, including deductions,
repairs and reshuffles. These are practical limits, not a polynomial-time or
guaranteed-success claim. There is no uniform-sampling guarantee or additional
filter for trivial layouts.

The attempt and time limits can be set with `--max-attempts` and
`--generation-timeout`. A cancelled, exhausted or unsupported generation
returns no mine layout. Interactive generation processes quit, cancellation,
termination signals and terminal resizing through its cancellation callback.

## Related implementation

The repair strategy is inspired by Simon Tatham's Mines generator: modify an
unresolved constraint region, then eventually accept only a board that solves
from the original first click without any modification during that pass.
The implementation here uses its own solver and an explicit bounded suffix
resampling policy. Tatham's solver has different capabilities.

- [Tatham's repair dispatch](https://git.tartarus.org/?p=simon/puzzles.git;a=blob;f=mines.c;hb=38e7ea3212748cebb17e277b98b7fad3a2dd3b3e#l2520)
- [Tatham's final acceptance loop](https://git.tartarus.org/?p=simon/puzzles.git;a=blob;f=mines.c;hb=38e7ea3212748cebb17e277b98b7fad3a2dd3b3e#l3263)

`make test` exercises deduction soundness against independently enumerated
small boards, generation invariants, seeded output, bounded failure and TUI
interaction. Research measurements from earlier NG3-only prototypes do not
measure this solver or this combined generation policy.
