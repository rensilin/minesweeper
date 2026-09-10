#include "../noguess.h"

#include <algorithm>
#include <cassert>
#include <climits>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

#ifdef NDEBUG
#error "noguess_test requires assertions enabled"
#endif

namespace
{
unsigned population(unsigned bits)
{
	unsigned result=0;
	for(;bits;bits&=bits-1)result++;
	return result;
}

// This oracle enumerates complete layouts, independently of the production
// constraint and subset-difference implementation. It is deliberately small.
struct Oracle
{
	int area;
	std::vector<unsigned> adjacent;
	Oracle(int rows,int columns):area(rows*columns),adjacent(area,0)
	{
		assert(area<=16);
		for(int a=0;a<area;a++)
			for(int b=0;b<area;b++)
			{
				const int dr=a/columns-b/columns,dc=a%columns-b%columns;
				if(a!=b&&dr>=-1&&dr<=1&&dc>=-1&&dc<=1)
					adjacent[a]|=1u<<b;
			}
	}
	void forced(int total,const std::vector<int> &visible,
		unsigned &safe,unsigned &mines) const
	{
		const unsigned end=1u<<area;
		unsigned anyMine=0,allMine=end-1,models=0;
		for(unsigned layout=0;layout<end;layout++)
		{
			if(population(layout)!=static_cast<unsigned>(total))continue;
			bool matches=true;
			for(int cell=0;cell<area&&matches;cell++)
			{
				if(visible[cell]==-2)matches=(layout&(1u<<cell))!=0;
				else if(visible[cell]>=0)
					matches=!(layout&(1u<<cell))
						&&population(layout&adjacent[cell])==static_cast<unsigned>(visible[cell]);
			}
			if(matches)
			{
				models++;
				anyMine|=layout;
				allMine&=layout;
			}
		}
		assert(models>0);
		safe=(end-1)^anyMine;
		mines=allMine;
	}
};

unsigned soundDeductions(int rows,int columns,unsigned layout,unsigned observed)
{
	Oracle oracle(rows,columns);
	std::vector<int> visible(oracle.area,-1);
	for(int cell=0;cell<oracle.area;cell++)
		if(observed&(1u<<cell))
			visible[cell]=(layout&(1u<<cell))?-2:
				static_cast<int>(population(layout&oracle.adjacent[cell]));
	const std::vector<int> before=visible;
	const int total=static_cast<int>(population(layout));
	const noguess::Deductions found=noguess::deduce(rows,columns,total,visible);
	assert(visible==before);
	unsigned safe,mines,returned=0;
	oracle.forced(total,visible,safe,mines);
	for(int cell:found.safe)
	{
		assert(cell>=0&&cell<oracle.area&&visible[cell]==-1);
		assert((safe&(1u<<cell))&&!(returned&(1u<<cell)));
		returned|=1u<<cell;
	}
	for(int cell:found.mines)
	{
		assert(cell>=0&&cell<oracle.area&&visible[cell]==-1);
		assert((mines&(1u<<cell))&&!(returned&(1u<<cell)));
		returned|=1u<<cell;
	}
	return population(returned);
}

void testDeductionSoundness()
{
	unsigned deductions=0;
	for(unsigned layout=0;layout<64;layout++)
		for(unsigned observed=0;observed<64;observed++)
			deductions+=soundDeductions(2,3,layout,observed);
	assert(deductions>0);
	std::mt19937 fixtures(4294967295u);
	for(int sample=0;sample<256;sample++)
		soundDeductions(3,4,fixtures()&4095u,fixtures()&4095u);
	// Basic-rule regressions prevent an always-empty implementation from passing.
	// The total must subtract already proven mines, even with no open clues.
	const auto sameCells=[](std::vector<int> actual,const std::vector<int> &expected)
	{
		std::sort(actual.begin(),actual.end());
		assert(actual==expected);
	};
	sameCells(noguess::deduce(1,4,1,{-2,-1,-1,-1}).safe,{1,2,3});
	sameCells(noguess::deduce(1,4,4,{-2,-1,-1,-1}).mines,{1,2,3});
	sameCells(noguess::deduce(1,3,0,{-1,0,-1}).safe,{0,2});
	sameCells(noguess::deduce(1,3,2,{-1,2,-1}).mines,{0,2});
}

void testInvalidObservations()
{
	const auto invalid=[](int rows,int columns,int total,const std::vector<int> &visible)
	{
		bool threw=false;
		try { noguess::deduce(rows,columns,total,visible); }
		catch(const std::invalid_argument &) { threw=true; }
		assert(threw);
	};
	invalid(0,3,1,{});
	invalid(INT_MAX,INT_MAX,1,{});
	invalid(2,2,1,{-1});
	invalid(1,3,1,{-1,3,-1});
	invalid(1,3,0,{-2,-1,-1});
	invalid(1,3,1,{-1,-3,-1});
	invalid(1,3,1,{0,-2,-1});
}

bool independentlySolvable(int rows,int columns,int total,int first,
	const std::vector<bool> &board)
{
	Oracle oracle(rows,columns);
	assert(board.size()==static_cast<std::size_t>(oracle.area));
	unsigned layout=0;
	for(int cell=0;cell<oracle.area;cell++)
		if(board[cell])layout|=1u<<cell;
	assert(population(layout)==static_cast<unsigned>(total));
	assert(!(layout&((1u<<first)|oracle.adjacent[first])));
	// Start afresh from the returned layout: no construction-time clues or
	// production deductions are reused to establish the no-guess guarantee.
	std::vector<int> visible(oracle.area,-1);
	visible[first]=0;
	int opened=1;
	while(opened<oracle.area-total)
	{
		unsigned safe,mines;
		oracle.forced(total,visible,safe,mines);
		bool progress=false;
		for(int cell=0;cell<oracle.area;cell++)
		{
			if(visible[cell]!=-1)continue;
			if(safe&(1u<<cell))
			{
				assert(!board[cell]);
				visible[cell]=static_cast<int>(population(layout&oracle.adjacent[cell]));
				opened++;
				progress=true;
			}
			else if(mines&(1u<<cell))
			{
				assert(board[cell]);
				visible[cell]=-2;
				progress=true;
			}
		}
		if(!progress)return false;
	}
	return true;
}

void testGeneratedBoardsAndDeterminism()
{
	// These small families contain boards solvable from their protected zero.
	const int fixtures[][4]={{1,1,0,0},{3,3,1,0},{3,4,2,3},{4,4,3,0},
		{2,7,4,3},{1,5,2,0},{5,1,2,0},{3,3,5,0}};
	noguess::Limits limits;
	limits.maxMillis=30000;
	for(const auto &fixture:fixtures)
		for(unsigned seed:{0u,1u,37u,4294967295u})
		{
			std::mt19937 rng(seed),repeated(seed);
			const noguess::Generation a=noguess::generate(fixture[0],fixture[1],
				fixture[2],fixture[3],rng,{},limits);
			const noguess::Generation b=noguess::generate(fixture[0],fixture[1],
				fixture[2],fixture[3],repeated,{},limits);
			if(a.status!=noguess::GENERATED)
				std::cerr<<"fixture "<<fixture[0]<<'x'<<fixture[1]<<" mines="<<fixture[2]
					<<" first="<<fixture[3]<<" seed="<<seed<<" status="<<a.status<<'\n';
			assert(a.status==noguess::GENERATED);
			assert(a.status==b.status&&a.mines==b.mines);
			assert(a.stats.attempts==b.stats.attempts&&a.stats.repairs==b.stats.repairs);
			assert(a.stats.suffixResamples==b.stats.suffixResamples);
			assert(a.stats.fullShuffles==b.stats.fullShuffles&&a.stats.work==b.stats.work);
			assert(rng==repeated);
			assert(independentlySolvable(fixture[0],fixture[1],fixture[2],fixture[3],a.mines));
		}
	// A negative control ensures the independent verifier rejects a real 50/50.
	assert(!independentlySolvable(1,5,2,0,{false,false,true,true,false}));
	assert(independentlySolvable(1,5,2,0,{false,false,false,true,true}));
}

void testRepairAndRedrawBudgets()
{
	noguess::Limits limits;
	limits.maxMillis=30000;
	limits.repairsPerRound=0;
	limits.maxAttempts=100;
	std::mt19937 exhaustedRng(0);
	const noguess::Generation exhausted=noguess::generate(9,9,50,40,exhaustedRng,{},limits);
	assert(exhausted.status==noguess::EXHAUSTED&&exhausted.mines.empty());
	assert(exhausted.stats.attempts==limits.maxAttempts);
	assert(exhausted.stats.repairs==0);
	assert(exhausted.stats.suffixResamples>0&&exhausted.stats.fullShuffles>1);
	assert(exhausted.stats.work<=limits.maxWork);

	limits.repairsPerRound=12;
	limits.maxAttempts=512;
	std::mt19937 repairRng(0);
	const noguess::Generation repaired=noguess::generate(20,20,100,210,repairRng,{},limits);
	assert(repaired.status==noguess::GENERATED);
	assert(repaired.stats.repairs>0&&repaired.stats.suffixResamples>0);
	assert(repaired.stats.attempts<=limits.maxAttempts&&repaired.stats.work<=limits.maxWork);
	assert(repaired.mines.size()==400);
	assert(std::count(repaired.mines.begin(),repaired.mines.end(),true)==100);
	for(int row=9;row<=11;row++)
		for(int column=9;column<=11;column++)
			assert(!repaired.mines[row*20+column]);
}

void testFailureStatuses()
{
	const auto status=[](const noguess::Generation &result,noguess::GenerationStatus expected)
	{
		assert(result.status==expected);
		assert(result.mines.empty());
	};
	std::mt19937 rng(0);
	status(noguess::generate(9,9,10,0,rng,[] { return false; }),noguess::CANCELLED);
	int callbacks=0;
	status(noguess::generate(20,20,200,210,rng,[&callbacks] { return ++callbacks<4; }),
		noguess::CANCELLED);
	assert(callbacks==4);
	for(int budget=0;budget<4;budget++)
	{
		noguess::Limits limits;
		if(budget==0)limits.maxAttempts=0;
		if(budget==1)limits.maxMillis=0;
		if(budget==2)limits.maxWork=0;
		if(budget==3)limits.maxWork=1;
		status(noguess::generate(9,9,10,0,rng,{},limits),noguess::EXHAUSTED);
	}
	const int invalid[][4]={{0,9,1,0},{INT_MAX,INT_MAX,1,0},
		{3,3,-1,0},{3,3,9,0},{3,3,1,-1},{3,3,1,9},{3,3,1,4}};
	for(const auto &fixture:invalid)
		status(noguess::generate(fixture[0],fixture[1],fixture[2],fixture[3],rng,{}),
			noguess::UNSUPPORTED);
}
}

int main()
{
	testDeductionSoundness();
	testInvalidObservations();
	testGeneratedBoardsAndDeterminism();
	testRepairAndRedrawBudgets();
	testFailureStatuses();
	std::cout<<"no-guess oracle and generation tests passed\n";
}
