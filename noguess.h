#ifndef MINESWEEPER_NOGUESS_H
#define MINESWEEPER_NOGUESS_H

#include <cstddef>
#include <functional>
#include <random>
#include <vector>

namespace noguess
{
enum GenerationStatus { GENERATED, CANCELLED, EXHAUSTED, UNSUPPORTED };

struct Limits
{
	unsigned maxAttempts;
	unsigned repairsPerRound;
	unsigned maxMillis;
	std::size_t maxWork;
	Limits();
};

struct Stats
{
	unsigned attempts;
	unsigned repairs;
	unsigned suffixResamples; // Partial redraws; full redraws are counted below.
	unsigned fullShuffles;   // Includes the initial complete shuffle.
	std::size_t work;
	Stats();
};

struct Deductions
{
	std::vector<int> safe;
	std::vector<int> mines;
};

struct Generation
{
	GenerationStatus status;
	std::vector<bool> mines;
	Stats stats;
};

// Row-major public information: -1 hidden, -2 proven mine, 0..8 visible clue.
// Uses only these observations and the total mine count. Returned indices are
// hidden cells. Deduction is sound but incomplete, including when its bounded
// subset-difference search stops. Invalid dimensions or detected inconsistent
// observations throw std::invalid_argument.
Deductions deduce(int rows,int columns,int mineCount,
	const std::vector<int> &visible);

// Only GENERATED carries a board. It has exactly mineCount mines, a zero first
// click, and a complete solution verified anew using public information.
// The other statuses carry an empty board. UNSUPPORTED means invalid parameters
// or insufficient space outside the first-click neighborhood. Zero attempt,
// time or work budgets immediately exhaust the search. An empty callback keeps
// running; otherwise false requests cancellation. This is a heuristic search,
// with no uniformity or completeness guarantee.
Generation generate(int rows,int columns,int mineCount,int firstCell,
	std::mt19937 &rng,const std::function<bool()> &keepRunning,
	const Limits &limits=Limits());
}

#endif
