#include "noguess.h"

#include <algorithm>
#include <chrono>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

namespace noguess
{
Limits::Limits():repairsPerRound(12),maxMillis(3000)
{
}

Stats::Stats():attempts(0),repairs(0),suffixResamples(0),fullShuffles(0),work(0)
{
}

namespace
{
typedef std::vector<int> Cells;
typedef std::vector<Cells> Neighbors;
typedef std::pair<Cells,int> Constraint;

struct Stopped
{
	GenerationStatus status;
	explicit Stopped(GenerationStatus value):status(value) {}
};

class Budget
{
	const Limits &limits;
	Stats &stats;
	const std::function<bool()> &keepRunning;
	std::chrono::steady_clock::time_point started;
	std::size_t untilCheck;
public:
	Budget(const Limits &settings,Stats &counters,const std::function<bool()> &callback):
		limits(settings),stats(counters),keepRunning(callback),
		started(std::chrono::steady_clock::now()),untilCheck(0)
	{
	}
	void check()
	{
		if(keepRunning&&!keepRunning())throw Stopped(CANCELLED);
		if(std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now()-started).count()>=limits.maxMillis)
			throw Stopped(EXHAUSTED);
		untilCheck=256;
	}
	void tick(std::size_t amount=1)
	{
		stats.work+=amount;
		if(amount>=untilCheck)check();
		else untilCheck-=amount;
	}
};

int areaFor(int rows,int columns)
{
	if(rows<=0||columns<=0
		||static_cast<long long>(rows)*columns>std::numeric_limits<int>::max())return 0;
	return rows*columns;
}

Neighbors neighborsFor(int rows,int columns,Budget &budget)
{
	budget.tick(static_cast<std::size_t>(rows)*columns);
	Neighbors neighbors;
	for(int row=0;row<rows;row++)
		for(int column=0;column<columns;column++)
		{
			budget.tick(10);
			neighbors.push_back(Cells());
			Cells &adjacent=neighbors.back();
			for(int r=std::max(0,row-1);r<=std::min(rows-1,row+1);r++)
				for(int c=std::max(0,column-1);c<=std::min(columns-1,column+1);c++)
					if(r!=row||c!=column)adjacent.push_back(r*columns+c);
		}
	return neighbors;
}

void normalize(Cells &cells)
{
	std::sort(cells.begin(),cells.end());
	cells.erase(std::unique(cells.begin(),cells.end()),cells.end());
}

void addConstraint(const Cells &cells,int count,std::map<Cells,int> &known,
	std::vector<Constraint> &pending,Budget &budget)
{
	budget.tick(cells.size()+1);
	if(count<0||count>static_cast<int>(cells.size()))
		throw std::invalid_argument("inconsistent public clues");
	if(cells.empty())return;
	std::pair<std::map<Cells,int>::iterator,bool> inserted=
		known.insert(std::make_pair(cells,count));
	if(!inserted.second)
	{
		if(inserted.first->second!=count)
			throw std::invalid_argument("inconsistent public clues");
		return;
	}
	pending.push_back(std::make_pair(cells,count));
}

Deductions deduceWith(const Neighbors &neighbors,int mineCount,
	const Cells &visible,Budget &budget)
{
	Deductions result;
	Cells hidden;
	int marked=0;
	std::map<Cells,int> constraints;
	std::vector<Constraint> pending;
	for(std::size_t cell=0;cell<visible.size();cell++)
	{
		budget.tick();
		if(visible[cell]==-1)hidden.push_back(static_cast<int>(cell));
		else if(visible[cell]==-2)marked++;
		else
		{
			if(visible[cell]<0||visible[cell]>static_cast<int>(neighbors[cell].size()))
				throw std::invalid_argument("invalid visible clue");
			Cells unknown;
			int remaining=visible[cell];
			for(int next:neighbors[cell])
			{
				budget.tick();
				if(visible[next]==-1)unknown.push_back(next);
				else if(visible[next]==-2)remaining--;
			}
			addConstraint(unknown,remaining,constraints,pending,budget);
		}
	}
	if(mineCount<marked||mineCount-marked>static_cast<int>(hidden.size()))
		throw std::invalid_argument("inconsistent remaining mine count");
	if(mineCount==marked)result.safe=hidden;
	else if(mineCount-marked==static_cast<int>(hidden.size()))result.mines=hidden;
	if(!result.safe.empty()||!result.mines.empty())return result;
	for(const Constraint &constraint:pending)
	{
		budget.tick(constraint.first.size()+1);
		if(constraint.second==0)
			result.safe.insert(result.safe.end(),constraint.first.begin(),constraint.first.end());
		else if(constraint.second==static_cast<int>(constraint.first.size()))
			result.mines.insert(result.mines.end(),constraint.first.begin(),constraint.first.end());
	}
	if(!result.safe.empty()||!result.mines.empty())
	{
		normalize(result.safe);
		normalize(result.mines);
		return result;
	}
	// All derived sets stay within an original eight-cell neighborhood. The
	// global mine count is used above, not expanded into a huge subset system.
	std::vector<Cells> touching(visible.size());
	for(std::size_t index=0;index<pending.size();index++)
	{
		budget.tick();
		// Appending derived constraints may reallocate pending.
		const Cells cells=pending[index].first;
		const int count=pending[index].second;
		std::set<int> related;
		for(int cell:cells)
			for(int other:touching[cell])
			{
				budget.tick();
				related.insert(other);
			}
		for(int other:related)
		{
			const Cells otherCells=pending[other].first;
			const bool currentIsSmall=cells.size()<otherCells.size();
			const Cells &small=currentIsSmall?cells:otherCells;
			const Cells &large=currentIsSmall?otherCells:cells;
			budget.tick(small.size()+large.size()+1);
			if(small.size()==large.size()
				||!std::includes(large.begin(),large.end(),small.begin(),small.end()))continue;
			Cells difference;
			std::set_difference(large.begin(),large.end(),small.begin(),small.end(),
				std::back_inserter(difference));
			const int differenceCount=currentIsSmall?pending[other].second-count:
				count-pending[other].second;
			addConstraint(difference,differenceCount,constraints,pending,budget);
			if(differenceCount==0)
			{
				result.safe=difference;
				return result;
			}
			if(differenceCount==static_cast<int>(difference.size()))
			{
				result.mines=difference;
				return result;
			}
		}
		for(int cell:cells)
		{
			budget.tick();
			touching[cell].push_back(static_cast<int>(index));
		}
	}
	return result;
}

struct Solution
{
	bool won;
	Cells visible;
	Solution(int area):won(false),visible(area,-1) {}
};

void reveal(const Cells &safe,const Neighbors &neighbors,const std::vector<bool> &mines,
	Cells &visible,int &opened,Budget &budget)
{
	Cells queue=safe;
	for(std::size_t index=0;index<queue.size();index++)
	{
		budget.tick();
		const int cell=queue[index];
		if(visible[cell]>=0)continue;
		if(visible[cell]==-2||mines[cell])
			throw std::logic_error("invalid safe deduction");
		int clue=0;
		for(int next:neighbors[cell])
		{
			budget.tick();
			clue+=mines[next];
		}
		visible[cell]=clue;
		opened++;
		if(clue==0)
			for(int next:neighbors[cell])
			{
				budget.tick();
				if(visible[next]==-1)queue.push_back(next);
			}
	}
}

Solution solveBoard(const Neighbors &neighbors,const std::vector<bool> &mines,
	int mineCount,int firstCell,Budget &budget)
{
	budget.tick(mines.size());
	Solution result(static_cast<int>(mines.size()));
	int opened=0;
	reveal(Cells(1,firstCell),neighbors,mines,result.visible,opened,budget);
	while(opened<static_cast<int>(mines.size())-mineCount)
	{
		budget.check();
		Deductions next=deduceWith(neighbors,mineCount,result.visible,budget);
		if(next.safe.empty()&&next.mines.empty())return result;
		for(int cell:next.mines)
		{
			budget.tick();
			if(!mines[cell]||result.visible[cell]!=-1)
				throw std::logic_error("invalid mine deduction");
			result.visible[cell]=-2;
		}
		reveal(next.safe,neighbors,mines,result.visible,opened,budget);
	}
	result.won=true;
	return result;
}

// The first mineCount entries always equal the current board's mine set,
// including after repairs. This invariant is required by suffix resampling.
void swapPool(int a,int b,int mineCount,Cells &pool,Cells &position,
	std::vector<bool> &mines,Budget &budget)
{
	budget.tick();
	const int cellA=pool[a],cellB=pool[b];
	std::swap(pool[a],pool[b]);
	position[cellA]=b;
	position[cellB]=a;
	mines[cellA]=b<mineCount;
	mines[cellB]=a<mineCount;
}

void redraw(int start,int mineCount,Cells &pool,Cells &position,
	std::vector<bool> &mines,std::mt19937 &rng,Budget &budget)
{
	budget.check();
	for(int index=start;index<mineCount;index++)
	{
		budget.tick();
		std::uniform_int_distribution<int> pick(index,static_cast<int>(pool.size())-1);
		swapPool(index,pick(rng),mineCount,pool,position,mines,budget);
	}
}

bool repair(const Neighbors &neighbors,const Solution &solution,int firstCell,
	int mineCount,Cells &pool,Cells &position,std::vector<bool> &mines,
	std::mt19937 &rng,Budget &budget)
{
	Cells unknownSafe,unknownMines;
	for(std::size_t cell=0;cell<mines.size();cell++)
	{
		budget.tick();
		if(solution.visible[cell]!=-1||position[cell]<0)continue;
		(mines[cell]?unknownMines:unknownSafe).push_back(static_cast<int>(cell));
	}
	Cells selectedScope,selectedChanged;
	bool emptySelected=true;
	int bestMoves=std::numeric_limits<int>::max();
	std::size_t ties=0;
	std::set<Cells> seen;
	for(std::size_t cell=0;cell<mines.size();cell++)
	{
		budget.tick();
		if(solution.visible[cell]<0)continue;
		Cells scope,scopeMines,scopeSafe;
		for(int next:neighbors[cell])
		{
			budget.tick();
			if(solution.visible[next]!=-1||position[next]<0)continue;
			scope.push_back(next);
			(mines[next]?scopeMines:scopeSafe).push_back(next);
		}
		if(scopeMines.empty()||scopeSafe.empty()||!seen.insert(scope).second)continue;
		for(int direction=0;direction<2;direction++)
		{
			budget.tick();
			const Cells &changed=direction==0?scopeMines:scopeSafe;
			const std::size_t outside=direction==0?unknownSafe.size()-scopeSafe.size():
				unknownMines.size()-scopeMines.size();
			if(outside<changed.size()||static_cast<int>(changed.size())>bestMoves)continue;
			if(static_cast<int>(changed.size())<bestMoves)
			{
				bestMoves=static_cast<int>(changed.size());
				ties=0;
			}
			std::uniform_int_distribution<std::size_t> pick(0,ties++);
			if(pick(rng)==0)
			{
				selectedScope=scope;
				selectedChanged=changed;
				emptySelected=direction==0;
			}
		}
	}
	if(selectedChanged.empty())return false;
	Cells outside;
	const Cells &candidates=emptySelected?unknownSafe:unknownMines;
	for(int cell:candidates)
	{
		budget.tick();
		if(!std::binary_search(selectedScope.begin(),selectedScope.end(),cell))
			outside.push_back(cell);
	}
	for(int index=0;index<bestMoves;index++)
	{
		budget.tick();
		std::uniform_int_distribution<int> pick(index,static_cast<int>(outside.size())-1);
		std::swap(outside[index],outside[pick(rng)]);
		swapPool(position[selectedChanged[index]],position[outside[index]],mineCount,
			pool,position,mines,budget);
	}
	(void)firstCell; // Protected cells have position -1 and cannot enter a repair.
	return true;
}
}

Deductions deduce(int rows,int columns,int mineCount,const std::vector<int> &visible)
{
	const int area=areaFor(rows,columns);
	if(!area||static_cast<std::size_t>(area)!=visible.size()||mineCount<0||mineCount>area)
		throw std::invalid_argument("invalid deduction dimensions or mine count");
	const Limits limits;
	Stats stats;
	const std::function<bool()> keepRunning;
	Budget budget(limits,stats,keepRunning);
	try
	{
		budget.check();
		return deduceWith(neighborsFor(rows,columns,budget),mineCount,visible,budget);
	}
	catch(const Stopped &)
	{
		return Deductions();
	}
}

Generation generate(int rows,int columns,int mineCount,int firstCell,
	std::mt19937 &rng,const std::function<bool()> &keepRunning,const Limits &limits,
	bool noGuess)
{
	Generation result;
	result.status=UNSUPPORTED;
	const int area=areaFor(rows,columns);
	if(!area||mineCount<0||firstCell<0||firstCell>=area)return result;
	const int firstRow=firstCell/columns,firstColumn=firstCell%columns;
	const int protectedCount=(std::min(rows-1,firstRow+1)-std::max(0,firstRow-1)+1)
		*(std::min(columns-1,firstColumn+1)-std::max(0,firstColumn-1)+1);
	if(mineCount>area-protectedCount)return result;
	Budget budget(limits,result.stats,keepRunning);
	try
	{
		budget.check();
		// Grow storage while polling the deadline instead of initializing a
		// potentially huge board before the next time/cancellation check.
		Cells pool,position;
		std::vector<bool> mines;
		for(int cell=0;cell<area;cell++)
		{
			budget.tick();
			position.push_back(-1);
			mines.push_back(false);
			const int row=cell/columns,column=cell%columns;
			if(row>=firstRow-1&&row<=firstRow+1
				&&column>=firstColumn-1&&column<=firstColumn+1)continue;
			position[cell]=static_cast<int>(pool.size());
			pool.push_back(cell);
			mines[cell]=position[cell]<mineCount;
		}
		redraw(0,mineCount,pool,position,mines,rng,budget);
		result.stats.fullShuffles++;
		if(!noGuess)
		{
			budget.check();
			result.status=GENERATED;
			result.mines.swap(mines);
			return result;
		}
		Neighbors neighbors=neighborsFor(rows,columns,budget);
		unsigned repairsThisRound=0;
		int suffixDepth=1;
		while(true)
		{
			budget.check();
			result.stats.attempts++;
			Solution solution=solveBoard(neighbors,mines,mineCount,firstCell,budget);
			if(solution.won)
			{
				budget.check();
				result.status=GENERATED;
				result.mines.swap(mines);
				return result;
			}
			if(repairsThisRound<limits.repairsPerRound
				&&repair(neighbors,solution,firstCell,mineCount,pool,position,mines,rng,budget))
			{
				repairsThisRound++;
				result.stats.repairs++;
				continue;
			}
			const int start=std::max(0,mineCount-suffixDepth);
			redraw(start,mineCount,pool,position,mines,rng,budget);
			if(start==0)
			{
				result.stats.fullShuffles++;
				suffixDepth=1;
			}
			else
			{
				result.stats.suffixResamples++;
				suffixDepth++;
			}
			repairsThisRound=0;
		}
	}
	catch(const Stopped &stopped)
	{
		result.status=stopped.status;
	}
	return result;
}
}
