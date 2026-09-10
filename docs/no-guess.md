# Board generation

Both modes call `noguess::generate` for the actual first opening.
The first cell and all its neighbors are safe. The requested mine count is
preserved throughout generation. Its final `bool noGuess=true` parameter
controls whether the initial candidate is checked and repaired. Existing
callers keep no-guess generation by default; `--mode=random` passes `false`
and returns immediately after the shared initial shuffle, without building
solver constraints or running deductions. The CLI defaults to no-guess.

## Solve, repair, and retry

The initial candidate uses a partial Fisher–Yates shuffle of all cells outside
the protected opening. The first `mineCount` entries are the ordered mine list.

The solver starts with only the first clue. It repeatedly opens proven safe
cells and records proven mines. Its deductions use visible clue equations,
subset differences and the remaining mine count. Derived constraints may be
combined further: there is no limit on how many original clues contributed to
a deduction. When these rules stall, the solver starts a depth-first constraint
search using only the visible clue equations and remaining total mine count.
It branches on an unknown cell participating in the most constraints, tries
safe then mined, propagates each assumption, and backtracks on contradictions.
An assignment trail and explicit branch stack avoid copying full states at
each depth. Unknown cells outside all clues are interchangeable and represented
by their count rather than enumerating their individual placements.

The search first finds one satisfying model, then tests the opposite value of
each cell not already seen both safe and mined in valid models. An unsatisfiable
opposite assumption proves the cell's value. A satisfying counterexample is
also reused to establish possible values of other cells. Finding one model is
never sufficient to play its guessed moves. Once a forced cell is found, the
solver applies that deduction and resumes from the newly visible information.
If both values are possible for every hidden cell, this candidate is stuck.
The search shares the generation deadline; an interrupted proof never becomes
a deduction or a successful board. Hidden mine positions are used only to
reveal a proven safe cell's clue and to recognize completion, never as a premise.

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

One generation call has only a time budget, defaulting to 3000 ms. It continues
checking and repairing candidates until one succeeds, time runs out, or the
caller cancels. Attempt and work counts are diagnostics and never stop search.
Elapsed time covers the entire call, including the initial shuffle, deductions,
repairs and reshuffles. The deadline is checked throughout those operations.
With `noGuess=false`, the same deadline applies to the initial shuffle.
The per-round limit of 12 repairs still determines when to resample a suffix;
it does not terminate generation. There is no guaranteed-success or
uniform-sampling claim, or additional filter for trivial layouts.

Set the time limit with `--generation-timeout MILLISECONDS` (positive integer,
either mode). A cancelled, exhausted or unsupported generation
returns no mine layout. Interactive no-guess generation processes quit and
cancellation keys through its callback. Random generation leaves queued keys
for normal gameplay after the shuffle. Both modes check termination signals
and terminal resizing during generation.

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
