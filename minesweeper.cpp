/**
 * Copyright (C) 2017 kkkeQAQ <kkke@nwsuaf.edu.cn>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include <cstdio>
#include <iostream>
#include <ctime>
#include <cctype>
#include <clocale>
#include <exception>
#include <limits>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <assert.h>
#include <utility>
#include <vector>
#include "SColor/SColor.h"
#include "args/args.hxx"
#include <sstream>
#include <iomanip>
#include <chrono>
#include <random>
#include <sys/select.h>
#include "terminal_input.h"
#include "noguess.h"

#define MAXY 100
#define MAXX 100

using namespace std;

const string version="v2.0";

struct termios org_opts;

const int easyV[3]={9,9,10};
const int normalV[3]={16,16,30};
const int hardV[3]={20,20,60};
int difficultyV[3];
const int *difficulty;

const int minimumPanelWidth=22;
const int reservedTerminalRows=5;
const int minTerminalRows=19;
const int minBoardSize=9;

vector<vector<int> > mMap;
vector<vector<bool> > mMine;
vector<vector<bool> > mSight;
vector<vector<bool> > mFlag;
vector<vector<bool> > mHighlight;

int &maxy=difficultyV[0];
int &maxx=difficultyV[1];
int &mineNum=difficultyV[2];

int nowy, nowx;

long long theRestOfMine=mineNum;
long long theRestOfSquare=static_cast<long long>(maxy)*maxx;
long long hiddenSafeSquares;
vector<pair<int,int> > highlightedCells;

int cInput;
terminalInput::Decoder inputDecoder;
bool mouseEnabled=false;
bool firstMove;
bool showBoard=false,seedSpecified=false;
unsigned int boardSeed=0;
int displayFirst=-1;
enum GameMode { RANDOM_MODE, NO_GUESS_MODE };
GameMode gameMode=NO_GUESS_MODE;
mt19937 boardRandom;
noguess::Limits generationLimits;
SColor defaultColor;
int lastLine;
int gameResult;
volatile sig_atomic_t terminalResized;
volatile sig_atomic_t terminationSignal;
bool colorEnabled;
bool unicodeEnabled;
bool boardSizeLimited;
bool boardSizeArgumentLimited;
bool boardClipped;
bool terminalTooSmall;
int terminalRows;
int terminalColumns;
int visibleMaxx;
int visibleMaxy;
int viewportX;
int viewportY;
int panelWidth;

enum CellStyle
{
	CELL_DEFAULT,
	CELL_NUMBER,
	CELL_FLAG,
	CELL_ERROR
};

struct CellView
{
	char value;
	int number;
	CellStyle style;
	bool selected;
	bool highlighted;
	bool valid;
};

vector<vector<CellView> > mRendered;
long long renderedRestOfMine;
long long renderedRestOfSquare;
bool renderedStatusValid;
bool renderedBordersValid;
bool renderedHiddenAbove;
bool renderedHiddenBelow;
bool renderedHiddenLeft;
bool renderedHiddenRight;

SColor nColor[]={
	SColor(),
	SColor::BLUE,//1
	SColor::GREEN,//2
	SColor::RED,//3
	SColor::PURPLE,//4
	SColor::YELLOW,//5
	SColor::CYAN,//6
	SColor::WHITE,//7
	SColor(SColor::BLACK,SColor::YELLOW)//8
};

SColor borderColor(SColor::CYAN,SColor::DEFAULT,SColor::HIGHLIGHT);
SColor titleColor(SColor::YELLOW,SColor::DEFAULT,SColor::HIGHLIGHT);
SColor labelColor(SColor::CYAN);
SColor keyColor(SColor::GREEN,SColor::DEFAULT,SColor::HIGHLIGHT);
SColor hiddenColor(SColor::WHITE,SColor::DEFAULT,SColor::DARK);
SColor successColor(SColor::GREEN,SColor::DEFAULT,SColor::HIGHLIGHT);
SColor failureColor(SColor::RED,SColor::DEFAULT,SColor::HIGHLIGHT);

void beginColor(const SColor &color)
{
	if(colorEnabled)cout<<color;
}

void endColor()
{
	if(colorEnabled)cout<<defaultColor;
}

void detectTerminalCapabilities()
{
	const char *localeName=setlocale(LC_CTYPE,"");
	string normalizedLocale=localeName?localeName:"";
	for(size_t i=0;i<normalizedLocale.size();i++)
		normalizedLocale[i]=tolower(static_cast<unsigned char>(normalizedLocale[i]));
	const char *term=getenv("TERM");
	bool capableTerm=!term||strcmp(term,"dumb")!=0;
	unicodeEnabled=capableTerm
		&&(normalizedLocale.find("utf-8")!=string::npos
			||normalizedLocale.find("utf8")!=string::npos);
	colorEnabled=capableTerm&&isatty(STDOUT_FILENO)&&getenv("NO_COLOR")==NULL;
}

bool readEnvironmentDimension(const char *name,int &value)
{
	const char *rawValue=getenv(name);
	if(!rawValue||!*rawValue)return false;
	errno=0;
	char *end=NULL;
	long parsedValue=strtol(rawValue,&end,10);
	if(errno||*end!='\0'||parsedValue<=0
		||parsedValue>numeric_limits<int>::max())return false;
	value=static_cast<int>(parsedValue);
	return true;
}

bool readTerminalSize(int descriptor,struct winsize &terminalSize)
{
	struct winsize candidate;
	memset(&candidate,0,sizeof(candidate));
	if(ioctl(descriptor,TIOCGWINSZ,&candidate)!=0
		||candidate.ws_row==0||candidate.ws_col==0)return false;
	terminalSize=candidate;
	return true;
}

bool keepCursorVisible()
{
	int previousViewportX=viewportX;
	int previousViewportY=viewportY;
	if(nowx<viewportX)viewportX=nowx;
	else if(nowx>=viewportX+visibleMaxx)viewportX=nowx-visibleMaxx+1;
	if(nowy<viewportY)viewportY=nowy;
	else if(nowy>=viewportY+visibleMaxy)viewportY=nowy-visibleMaxy+1;
	viewportX=max(0,min(viewportX,maxx-visibleMaxx));
	viewportY=max(0,min(viewportY,maxy-visibleMaxy));
	return previousViewportX!=viewportX||previousViewportY!=viewportY;
}

void updateTerminalViewport()
{
	string sizeValue=to_string(maxx)+"x"+to_string(maxy);
	long long boardArea=static_cast<long long>(maxx)*maxy;
	int sizeWidth=static_cast<int>(sizeValue.size())+13;
	int counterWidth=static_cast<int>(to_string(boardArea).size())+14;
	panelWidth=max(minimumPanelWidth,max(sizeWidth,counterWidth));
	struct winsize terminalSize;
	memset(&terminalSize,0,sizeof(terminalSize));
	bool sizeAvailable=readTerminalSize(STDOUT_FILENO,terminalSize)
		||readTerminalSize(STDIN_FILENO,terminalSize);
	if(!sizeAvailable)
	{
		int terminalDescriptor=open("/dev/tty",O_RDONLY);
		if(terminalDescriptor>=0)
		{
			sizeAvailable=readTerminalSize(terminalDescriptor,terminalSize);
			close(terminalDescriptor);
		}
	}
	if(sizeAvailable)
	{
		terminalRows=terminalSize.ws_row;
		terminalColumns=terminalSize.ws_col;
	}
	else
	{
		bool environmentSizeAvailable=readEnvironmentDimension("LINES",terminalRows)
			&&readEnvironmentDimension("COLUMNS",terminalColumns);
		sizeAvailable=environmentSizeAvailable;
		if(!sizeAvailable)terminalRows=terminalColumns=0;
	}
	int requiredColumns=minBoardSize*3+2+panelWidth;
	terminalTooSmall=!sizeAvailable
		||terminalRows<minTerminalRows||terminalColumns<requiredColumns;
	if(terminalTooSmall)return;

	visibleMaxx=maxx;
	visibleMaxy=maxy;
	if(sizeAvailable)
	{
		int supportedWidth=(terminalColumns-panelWidth-2)/3;
		int supportedHeight=terminalRows-reservedTerminalRows;
		visibleMaxy=min(maxy,supportedWidth);
		visibleMaxx=min(maxx,supportedHeight);
	}
	boardClipped=visibleMaxx<maxx||visibleMaxy<maxy;
	boardSizeLimited=boardSizeArgumentLimited
		||boardClipped;
	keepCursorVisible();
	lastLine=max(visibleMaxx+5,minTerminalRows);
}

void restoreTerminal()
{
	if(mouseEnabled)cout<<"\033[?1000l\033[?1006l";
	SColor::echoCursor();
	endColor();
	int exitRow=terminalTooSmall?max(1,terminalRows):max(1,lastLine);
	SColor::setCursor(exitRow,1);
	cout<<endl;
	//------  restore old settings ---------
	int res;
	res=tcsetattr(STDIN_FILENO, TCSANOW, &org_opts);assert(res==0);
}

void quit(int status=0)
{
	restoreTerminal();
	exit(status);
}

bool sameCellView(const CellView &left,const CellView &right)
{
	return left.valid
		&&left.value==right.value
		&&left.number==right.number
		&&left.style==right.style
		&&left.selected==right.selected
		&&left.highlighted==right.highlighted;
}

CellView getCellView(int x,int y,int finished)
{
	CellView view={'.',0,CELL_DEFAULT,nowx==x&&nowy==y,false,true};
	if(mFlag[x][y])view.value='@';
	else if(mSight[x][y])
	{
		if(mMine[x][y])view.value='*';
		else if(mMap[x][y])
		{
			view.value='0'+mMap[x][y];
			view.number=mMap[x][y];
			view.style=CELL_NUMBER;
		}
		else view.value=' ';
	}
	if(finished==-1&&mFlag[x][y]&&!mMine[x][y])view.style=CELL_ERROR;
	if(finished==-1&&mMine[x][y]&&mSight[x][y])view.style=CELL_ERROR;
	if(finished==0&&mFlag[x][y])view.style=CELL_FLAG;
	view.highlighted=!mSight[x][y]&&mHighlight[x][y];
	return view;
}

void renderCell(int x,int y,const CellView &view)
{
	SColor color;
	if(view.style==CELL_DEFAULT&&view.value=='.')color=hiddenColor;
	else if(view.style==CELL_NUMBER)color=nColor[view.number];
	else if(view.style==CELL_FLAG)
	{
		color.setFg(SColor::CYAN);
		color|=SColor::HIGHLIGHT|SColor::ITALIC;
	}
	else if(view.style==CELL_ERROR)
	{
		color.setFg(SColor::BLACK).setBg(SColor::RED);
		color|=SColor::HIGHLIGHT;
	}
	if(view.highlighted)color|=SColor::INVERT;
	if(view.selected)color|=SColor::HIGHLIGHT;
	SColor::setCursor(x-viewportX+2,(y-viewportY)*3+2);
	beginColor(color);
	cout<<(view.selected?'[':' ')
		<<view.value
		<<(view.selected?']':' ');
	endColor();
}

void drawPanelLabel(int row,const string &label)
{
	SColor::setCursor(row,3*visibleMaxy+3);
	beginColor(labelColor);
	cout<<' '<<label<<string(11-label.size(),' ')<<':';
}

void refreshStatus()
{
	if(!renderedStatusValid||renderedRestOfSquare!=theRestOfSquare)
	{
		drawPanelLabel(10,"rest square");
		beginColor(keyColor);
		cout<<theRestOfSquare;
		endColor();
		SColor::cleanLine();
		renderedRestOfSquare=theRestOfSquare;
	}
	if(!renderedStatusValid||renderedRestOfMine!=theRestOfMine)
	{
		drawPanelLabel(11,"rest mine");
		beginColor(keyColor);
		cout<<theRestOfMine;
		endColor();
		SColor::cleanLine();
		renderedRestOfMine=theRestOfMine;
	}
	renderedStatusValid=true;
}

void refreshMap(int finished=0)
{
	if(terminalTooSmall)return;
	for(int i=viewportX;i<viewportX+visibleMaxx;i++)
		for(int j=viewportY;j<viewportY+visibleMaxy;j++)
		{
			CellView view=getCellView(i,j,finished);
			CellView &rendered=mRendered[i-viewportX][j-viewportY];
			if(!sameCellView(rendered,view))
			{
				renderCell(i,j,view);
				rendered=view;
			}
		}
	for(size_t i=0;i<highlightedCells.size();i++)
		mHighlight[highlightedCells[i].first][highlightedCells[i].second]=false;
	highlightedCells.clear();
	refreshStatus();
	cout.flush();
}

void drawControl(int row,const string &label,const string &key)
{
	drawPanelLabel(row,label);
	beginColor(keyColor);
	cout<<key;
	endColor();
}

void drawMovementControls()
{
	int column=3*visibleMaxy+3;
	SColor::setCursor(13,column);
	cout<<"     ";
	beginColor(keyColor);
	cout<<(unicodeEnabled?"↑":"^")<<"       w";
	endColor();
	SColor::setCursor(14,column);
	cout<<"   ";
	beginColor(keyColor);
	cout<<(unicodeEnabled?"← ↓ →":"< v >");
	beginColor(labelColor);
	cout<<" / ";
	beginColor(keyColor);
	cout<<"a s d";
	endColor();
}

void drawViewportBorders()
{
	const char *horizontal=unicodeEnabled?"─":"-";
	const char *vertical=unicodeEnabled?"│":"|";
	const char *topLeft=unicodeEnabled?"╭":"+";
	const char *topRight=unicodeEnabled?"╮":"+";
	const char *bottomLeft=unicodeEnabled?"╰":"+";
	const char *bottomRight=unicodeEnabled?"╯":"+";
	bool hiddenAbove=viewportX>0;
	bool hiddenBelow=viewportX+visibleMaxx<maxx;
	bool hiddenLeft=viewportY>0;
	bool hiddenRight=viewportY+visibleMaxy<maxy;
	bool drawTop=!renderedBordersValid||hiddenAbove!=renderedHiddenAbove;
	bool drawBottom=!renderedBordersValid||hiddenBelow!=renderedHiddenBelow;
	bool drawLeft=!renderedBordersValid||hiddenLeft!=renderedHiddenLeft;
	bool drawRight=!renderedBordersValid||hiddenRight!=renderedHiddenRight;
	if(!(drawTop||drawBottom||drawLeft||drawRight))return;
	beginColor(borderColor);
	if(drawTop)
	{
		SColor::setCursor(1,1);
		cout<<topLeft;
		for(int i=0;i<visibleMaxy;i++)
		{
			cout<<horizontal;
			cout<<(hiddenAbove&&i%2==0?"^":horizontal);
			cout<<horizontal;
		}
		cout<<topRight;
	}
	if(drawLeft||drawRight)
	{
		for(int i=0;i<visibleMaxx;i++)
		{
			if(drawLeft)
			{
				SColor::setCursor(i+2,1);
				cout<<(hiddenLeft&&i%2==0?"<":vertical);
			}
			if(drawRight)
			{
				SColor::setCursor(i+2,3*visibleMaxy+2);
				cout<<(hiddenRight&&i%2==0?">":vertical);
			}
		}
	}
	if(drawBottom)
	{
		SColor::setCursor(visibleMaxx+2,1);
		cout<<bottomLeft;
		for(int i=0;i<visibleMaxy;i++)
		{
			cout<<horizontal;
			cout<<(hiddenBelow&&i%2==0?"v":horizontal);
			cout<<horizontal;
		}
		cout<<bottomRight;
	}
	endColor();
	renderedHiddenAbove=hiddenAbove;
	renderedHiddenBelow=hiddenBelow;
	renderedHiddenLeft=hiddenLeft;
	renderedHiddenRight=hiddenRight;
	renderedBordersValid=true;
}

void drawLayout()
{
	drawViewportBorders();
	SColor::setCursor(2,3*visibleMaxy+3);
	cout<<"   ";
	beginColor(borderColor);
	cout<<(unicodeEnabled?"╭─────────────╮":"+-------------+");
	endColor();
	SColor::setCursor(3,3*visibleMaxy+3);
	cout<<"   ";
	beginColor(borderColor);
	cout<<(unicodeEnabled?"│":"|");
	beginColor(titleColor);
	cout<<" minesweeper ";
	beginColor(borderColor);
	cout<<(unicodeEnabled?"│":"|");
	endColor();
	SColor::setCursor(4,3*visibleMaxy+3);
	cout<<"   ";
	beginColor(borderColor);
	cout<<(unicodeEnabled?"│":"|");
	beginColor(titleColor);
	cout<<"     "<<version<<"    ";
	beginColor(borderColor);
	cout<<(unicodeEnabled?"│":"|");
	endColor();
	SColor::setCursor(5,3*visibleMaxy+3);
	cout<<"   ";
	beginColor(borderColor);
	cout<<(unicodeEnabled?"╰─────────────╯":"+-------------+");
	endColor();
	if(boardSizeLimited)
	{
		SColor::setCursor(7,3*visibleMaxy+3);
		cout<<"   ";
		beginColor(failureColor);
		cout<<(boardClipped?"warning:clipped":"warning:limited");
		endColor();
	}
	drawPanelLabel(8,"view");
	beginColor(keyColor);
	cout<<visibleMaxx<<'x'<<visibleMaxy;
	endColor();
	drawPanelLabel(9,"size");
	beginColor(keyColor);
	cout<<maxx<<'x'<<maxy;
	endColor();
	if(gameMode==NO_GUESS_MODE)drawControl(6,"mode","no-guess");
	drawMovementControls();
	drawControl(12,"pan","LMB edge");
	drawControl(15,"flag","j/f,RMB");
	drawControl(16,"sweep","space,LMB");
	drawControl(17,"restart","r");
	drawControl(18,"quit","q");
	cout.flush();
}

void clearMessages()
{
	if(terminalTooSmall)return;
	string blank(3*visibleMaxy+2,' ');
	SColor::setCursor(visibleMaxx+3,1);
	cout<<blank;
	SColor::setCursor(visibleMaxx+4,1);
	cout<<blank;
	SColor::setCursor(max(visibleMaxx+3,minTerminalRows),1);
	SColor::cleanLine();
	cout.flush();
}

void invalidateRenderedState()
{
	CellView emptyView={'.',0,CELL_DEFAULT,false,false,false};
	if(terminalTooSmall)mRendered.clear();
	else mRendered.assign(visibleMaxx,vector<CellView>(visibleMaxy,emptyView));
	renderedStatusValid=false;
}

void showGameResult(int finished)
{
	if(terminalTooSmall)return;
	SColor::setCursor(visibleMaxx+3,1);
	beginColor(finished==1?successColor:failureColor);
	cout<<(finished==1?"you win! ":"you lose!");
	endColor();
	cout.flush();
}

void showNewGamePrompt()
{
	if(terminalTooSmall)return;
	SColor::setCursor(visibleMaxx+4,1);
	beginColor(labelColor);
	cout<<"new game ";
	beginColor(keyColor);
	cout<<"[y]";
	beginColor(labelColor);
	cout<<"  quit ";
	beginColor(keyColor);
	cout<<"[q]";
	endColor();
	cout.flush();
}

void drawTerminalWarning()
{
	string message;
	if(terminalRows<=0||terminalColumns<=0)
		message="warning: terminal size unavailable";
	else
	{
		int requiredColumns=minBoardSize*3+2+panelWidth;
		message="warning: terminal too small; need "
			+to_string(requiredColumns)+"x"+to_string(minTerminalRows);
	}
	if(terminalColumns>0&&message.size()>static_cast<size_t>(terminalColumns))
		message.resize(terminalColumns);
	SColor::setCursor(1,1);
	beginColor(failureColor);
	cout<<message;
	endColor();
	cout.flush();
}

void redrawScreen(int finished=0,bool showPrompt=false)
{
	terminalResized=0;
	renderedBordersValid=false;
	invalidateRenderedState();
	SColor::clean();
	if(terminalTooSmall)
	{
		drawTerminalWarning();
		return;
	}
	drawLayout();
	refreshMap(finished);
	if(finished)showGameResult(finished);
	if(showPrompt)showNewGamePrompt();
}

void handleTerminalResize(int)
{
	terminalResized=1;
}

void handleTerminationSignal(int signal)
{
	if(!terminationSignal)terminationSignal=signal;
}

int maximumZeroMines(int first)
{
	int row=first/maxy,column=first%maxy;
	int protectedRows=min(maxx,row+2)-max(0,row-1);
	int protectedColumns=min(maxy,column+2)-max(0,column-1);
	return maxx*maxy-protectedRows*protectedColumns;
}

void init();

void showGenerationMessage(const string &message)
{
	if(terminalTooSmall)return;
	SColor::setCursor(max(visibleMaxx+3,minTerminalRows),1);
	SColor::cleanLine();
	beginColor(labelColor);
	cout<<message.substr(0,static_cast<size_t>(terminalColumns));
	endColor();
	cout.flush();
}

bool generateFirstBoard(int row,int column)
{
	int first=row*maxy+column;
	if(mineNum>maximumZeroMines(first))
	{
		showGenerationMessage("First zero supports 1.."+to_string(maximumZeroMines(first))
			+" mines here; choose another cell");
		return false;
	}
	const bool noGuess=gameMode==NO_GUESS_MODE;
	const string message=noGuess?"Generating no-guess... q:quit r:cancel":"Generating board...";
	const string failurePrefix=noGuess?"No-guess":"Board generation";
	showGenerationMessage(message);
	bool cancelled=false;
	auto keepRunning=[&]()
	{
		if(terminationSignal)quit(128+terminationSignal);
		if(terminalResized)
		{
			updateTerminalViewport();
			redrawScreen();
			showGenerationMessage(message);
		}
		// A random board returns after one shuffle. Leave queued gameplay input
		// for getInput instead of consuming it during the first opening.
		if(!noGuess)return true;
		// Drain a bounded number of queued bytes; partial mouse packets stay
		// in the shared decoder and never become generation commands.
		for(int polled=0;polled<128;polled++)
		{
			fd_set input;
			FD_ZERO(&input);
			FD_SET(STDIN_FILENO,&input);
			struct timeval timeout={0,0};
			if(select(STDIN_FILENO+1,&input,NULL,NULL,&timeout)<=0)break;
			errno=0;
			int key=getchar();
			if(key==EOF)
			{
				if(errno==EINTR){clearerr(stdin);break;}
				quit();
			}
			auto event=inputDecoder.feed(key);
			if(event.type!=terminalInput::KEY)continue;
			if(event.key=='q')quit();
			if(event.key=='r')cancelled=true;
		}
		return !cancelled;
	};
	try
	{
		noguess::Generation generated=noguess::generate(maxx,maxy,mineNum,first,
			boardRandom,keepRunning,generationLimits,noGuess);
		clearMessages();
		if(generated.status==noguess::CANCELLED)
		{
			init();
			showGenerationMessage("Generation cancelled; open a cell to retry");
			return false;
		}
		if(generated.status!=noguess::GENERATED)
		{
			showGenerationMessage(failurePrefix+" timed out; Space:retry r:restart q:quit");
			return false;
		}
		for(int r=0;r<maxx;r++)
			for(int c=0;c<maxy;c++)
			{
				mMine[r][c]=generated.mines[r*maxy+c];
				mMap[r][c]=0;
			}
		for(int r=0;r<maxx;r++)
			for(int c=0;c<maxy;c++)
				if(mMine[r][c])
					for(int nr=max(0,r-1);nr<min(maxx,r+2);nr++)
						for(int nc=max(0,c-1);nc<min(maxy,c+2);nc++)
							mMap[nr][nc]++;
		return true;
	}
	catch(const exception &)
	{
		clearMessages();
		showGenerationMessage(failurePrefix+" failed; Space:retry r:restart q:quit");
		return false;
	}
}

bool sweepMine(int x,int y)
{
	if(mFlag[x][y])return true;
	if(firstMove)
	{
		if(!generateFirstBoard(x,y))return true;
		firstMove=false;
	}
	vector<pair<int,int> > pending;
	if(mSight[x][y])
	{
		int cnt=0;
		for(int i=x-1;i<x+2;i++)
			for(int j=y-1;j<y+2;j++)
				if(i>=0&&j>=0&&i<maxx&&j<maxy&&mFlag[i][j])
					cnt++;
		if(cnt!=mMap[x][y])
		{
			for(int i=x-1;i<x+2;i++)
				for(int j=y-1;j<y+2;j++)
					if(i>=0&&j>=0&&i<maxx&&j<maxy)
					{
						mHighlight[i][j]=true;
						highlightedCells.push_back(make_pair(i,j));
					}
			return true;
		}
		for(int i=x-1;i<x+2;i++)
			for(int j=y-1;j<y+2;j++)
				if(i>=0&&j>=0&&i<maxx&&j<maxy&&!mSight[i][j]&&!mFlag[i][j])
					pending.push_back(make_pair(i,j));
	}
	else pending.push_back(make_pair(x,y));

	bool safe=true;
	while(!pending.empty())
	{
		int currentX=pending.back().first;
		int currentY=pending.back().second;
		pending.pop_back();
		if(mSight[currentX][currentY]||mFlag[currentX][currentY])continue;
		if(mMine[currentX][currentY])
		{
			mSight[currentX][currentY]=true;
			safe=false;
			continue;
		}
		mSight[currentX][currentY]=true;
		theRestOfSquare--;
		hiddenSafeSquares--;
		if(mMap[currentX][currentY])continue;
		for(int i=currentX-1;i<currentX+2;i++)
			for(int j=currentY-1;j<currentY+2;j++)
				if(i>=0&&j>=0&&i<maxx&&j<maxy&&!mSight[i][j]&&!mFlag[i][j])
					pending.push_back(make_pair(i,j));
	}
	return safe;
}

void init()
{
	if(!terminalTooSmall)lastLine=max(visibleMaxx+5,minTerminalRows);
	gameResult=0;
	firstMove=true;
	highlightedCells.clear();
	try
	{
		mMine.assign(maxx,vector<bool>(maxy,false));
		mSight.assign(maxx,vector<bool>(maxy,false));
		mHighlight.assign(maxx,vector<bool>(maxy,false));
		mMap.assign(maxx,vector<int>(maxy,0));
		mFlag.assign(maxx,vector<bool>(maxy,false));
		invalidateRenderedState();
	}
	catch(const exception &)
	{
		restoreTerminal();
		cerr<<"error: unable to allocate the requested board"<<endl;
		exit(1);
	}
	theRestOfMine=mineNum;
	theRestOfSquare=static_cast<long long>(maxy)*maxx;
	hiddenSafeSquares=theRestOfSquare-mineNum;
}

// Border clicks pan one logical row/column; keep selection within the new
// viewport so the keyboard's cursor-following logic does not undo the pan.
bool panFromMouse(int column,int row)
{
	int right=3*visibleMaxy+2,bottom=visibleMaxx+2;
	if(column<1||column>right||row<1||row>bottom)return false;
	int oldX=viewportX,oldY=viewportY;
	if(row==1&&viewportX>0)viewportX--;
	if(row==bottom&&viewportX+visibleMaxx<maxx)viewportX++;
	if(column==1&&viewportY>0)viewportY--;
	if(column==right&&viewportY+visibleMaxy<maxy)viewportY++;
	if(oldX==viewportX&&oldY==viewportY)return false;
	nowx=max(viewportX,min(nowx,viewportX+visibleMaxx-1));
	nowy=max(viewportY,min(nowy,viewportY+visibleMaxy-1));
	invalidateRenderedState();
	drawViewportBorders();
	return true;
}

bool getInput()
{
	while(1)
	{
		if(terminationSignal)quit(128+terminationSignal);
		if(terminalResized)
		{
			updateTerminalViewport();
			redrawScreen();
			continue;
		}
		errno=0;
		cInput=getchar();
		if(cInput==EOF)
		{
			if(errno==EINTR)
			{
				clearerr(stdin);
				continue;
			}
			quit();
		}
		auto event=inputDecoder.feed(cInput);
		if(event.type==terminalInput::NONE)continue;
		if(terminalTooSmall)
		{
			if(event.type==terminalInput::KEY&&event.key=='q')quit();
			continue;
		}
		if(event.type==terminalInput::MOUSE)
		{
			if(!mouseEnabled)continue;
			if(event.button==0&&panFromMouse(event.column,event.row))return true;
			if(event.row<2||event.row>visibleMaxx+1
				||event.column<2||event.column>3*visibleMaxy+1)continue;
			nowx=viewportX+event.row-2;
			nowy=viewportY+(event.column-2)/3;
			cInput=event.button==0?' ':'f';
		}
		else cInput=event.key;
		switch (cInput)
		{
		case 'w':
			if(nowx>0)nowx--;
			return true;
		case 'a':
			if(nowy>0)nowy--;
			return true;
		case 's':
			if(nowx<maxx-1)nowx++;
			return true;
		case 'd':
			if(nowy<maxy-1)nowy++;
			return true;
		case 'f':
		case 'j':
			if(mSight[nowx][nowy])return true;
			if(mFlag[nowx][nowy]){mFlag[nowx][nowy]=false;theRestOfMine++;theRestOfSquare++;}
			else {mFlag[nowx][nowy]=true;theRestOfMine--;theRestOfSquare--;}
			return true;
		case ' ':
			if(mFlag[nowx][nowy])return true;
			return sweepMine(nowx,nowy);
		case 'r':
			clearMessages();
			updateTerminalViewport();
			init();
			return true;
		case 'q':
			quit();
		}
	}
	return false;//will never run
}

bool winGame()
{
	return hiddenSafeSquares==0;
}

void gameStart()
{
	while(1)
	{
		if(winGame())
		{
			gameResult=1;
			refreshMap(gameResult);
			showGameResult(gameResult);
			return;
		}
		if(!terminalTooSmall&&keepCursorVisible())
		{
			invalidateRenderedState();
			drawViewportBorders();
		}
		refreshMap();
		if(!getInput())
		{
			gameResult=-1;
			refreshMap(gameResult);
			showGameResult(gameResult);
			return;
		}
	}
}

bool newGameStart()
{
	showNewGamePrompt();
	while(1)
	{
		if(terminationSignal)quit(128+terminationSignal);
		if(terminalResized)
		{
			updateTerminalViewport();
			redrawScreen(gameResult,true);
			continue;
		}
		errno=0;
		int input=getchar();
		if(input==EOF)
		{
			if(errno==EINTR)
			{
				clearerr(stdin);
				continue;
			}
			return false;
		}
		auto event=inputDecoder.feed(input);
		if(event.type==terminalInput::MOUSE)
		{
			if(!mouseEnabled||terminalTooSmall||event.button!=0
				||event.row!=visibleMaxx+4)continue;
			// Hit boxes include the brackets in "new game [y]  quit [q]".
			if(event.column>=10&&event.column<=12)return true;
			if(event.column>=20&&event.column<=22)return false;
			continue;
		}
		if(event.type!=terminalInput::KEY)continue;
		input=event.key;
		if(terminalTooSmall)
		{
			if(input=='q')return false;
			continue;
		}
		if(input=='y')return true;
		if(input=='q')return false;
	}
}

void realInit()
{
	struct termios new_opts;
	int res=0;
	detectTerminalCapabilities();
	setvbuf(stdin,NULL,_IONBF,0);
	//-----  store old settings -----------
	res=tcgetattr(STDIN_FILENO, &org_opts);
	assert(res==0);
	struct sigaction resizeAction;
	memset(&resizeAction,0,sizeof(resizeAction));
	resizeAction.sa_handler=handleTerminalResize;
	sigemptyset(&resizeAction.sa_mask);
	res=sigaction(SIGWINCH,&resizeAction,NULL);assert(res==0);
	struct sigaction terminationAction;
	memset(&terminationAction,0,sizeof(terminationAction));
	terminationAction.sa_handler=handleTerminationSignal;
	sigemptyset(&terminationAction.sa_mask);
	res=sigaction(SIGINT,&terminationAction,NULL);assert(res==0);
	res=sigaction(SIGTERM,&terminationAction,NULL);assert(res==0);
	//---- set new terminal parms --------
	memcpy(&new_opts, &org_opts, sizeof(new_opts));
	new_opts.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ECHOPRT | ECHOKE | ICRNL);
	tcsetattr(STDIN_FILENO, TCSANOW, &new_opts);

	const char *term=getenv("TERM");
	mouseEnabled=isatty(STDIN_FILENO)&&isatty(STDOUT_FILENO)&&term&&strcmp(term,"dumb")!=0;
	if(mouseEnabled)cout<<"\033[?1000h\033[?1006h";
	SColor::hideCursor();
	for(auto &i:nColor)i|=SColor::HIGHLIGHT;
}

void argsParse(int argc,char **argv)
{
	args::ArgumentParser parser(string("minesweeper ")+version);
	args::HelpFlag help(parser,"help","Show this help menu.",{'h',"help"});
	args::Flag fVersion(parser,"version","Show version",{'v',"version"});
	args::Flag easy(parser,"easy",
					"Choose easy("
					+to_string(easyV[0])
					+"*"+to_string(easyV[1])
					+"("+to_string(easyV[2])+"))"
					,{'1','E',"easy"});
	args::Flag normal(parser,"normal",
					  "Choose normal("
					  +to_string(normalV[0])
					  +"*"+to_string(normalV[1])
					  +"("+to_string(normalV[2])+"))"
					  ,{'2','N',"normal"});
	args::Flag hard(parser,"hard",
					"Choose hard("
					+to_string(hardV[0])
					+"*"+to_string(hardV[1])
					+"("+to_string(hardV[2])+"))"
					,{'3','H',"hard"});
	args::Flag noMaxSize(parser,"no max size",
					"Allow board dimensions above 100.",{"no-max-size"});
	args::ValueFlag<string> mode(parser,"mode","Board generation: no-guess (default) or random.",{"mode"},"no-guess");
	args::ValueFlag<string> timeout(parser,"milliseconds","Maximum generation time in milliseconds (default: 3000).",{"generation-timeout"});
	args::Flag show(parser,"show","Print the complete board without terminal controls, then exit.",{"show"});
	args::ValueFlag<string> first(parser,"row,column","1-based first click for --show (default: center); initial cursor otherwise.",{"first"});
	args::ValueFlag<string> seed(parser,"seed","Unsigned 32-bit seed for reproducible random generation.",{"seed"});
	args::Positional<int> height(parser,"height","set height (minimum 9)");
	args::Positional<int> weight(parser,"width","set width (minimum 9)");
	args::Positional<int> acountOfMine(parser,"acount of mine","set acount of mine");
	try{
		parser.ParseCLI(argc,argv);
	}
	catch(args::Help e)
	{
		cout<<parser;
		exit(0);
	}
	catch(args::ParseError e)
	{
		cerr<<e.what()<<endl;
		cerr<<parser;
		exit(1);
	}
	catch(args::ValidationError e)
	{
		cerr<<e.what()<<endl;
		cerr<<parser;
		exit(1);
	}
	if(fVersion)
	{
		cout<<version<<endl;
		exit(0);
	}
	string modeName=args::get(mode);
	if(modeName=="no-guess")gameMode=NO_GUESS_MODE;
	else if(modeName=="random")gameMode=RANDOM_MODE;
	else
	{
		cerr<<"error: --mode must be random or no-guess"<<endl;
		exit(1);
	}
	if(timeout)
	{
		auto positiveLimit=[](const string &value,const string &option)
		{
			unsigned parsed=0,maximum=static_cast<unsigned>(numeric_limits<int>::max());
			bool valid=!value.empty();
			for(char c:value)
			{
				if(c<'0'||c>'9'||parsed>(maximum-static_cast<unsigned>(c-'0'))/10)
				{
					valid=false;
					break;
				}
				parsed=parsed*10+static_cast<unsigned>(c-'0');
			}
			if(!valid||!parsed)
			{
				cerr<<"error: "<<option<<" must be a positive integer in 1.."<<maximum<<endl;
				exit(1);
			}
			return parsed;
		};
		generationLimits.maxMillis=positiveLimit(args::get(timeout),"--generation-timeout");
	}
	if(easy)difficulty=easyV;
	else if(normal)difficulty=normalV;
	else if(hard)difficulty=hardV;
	else if(height)
	{
		int requestedHeight=args::get(height);
		int requestedWidth=weight?args::get(weight):minBoardSize;
		boardSizeArgumentLimited=requestedHeight<minBoardSize
			||requestedWidth<minBoardSize
			||(!noMaxSize&&(requestedHeight>MAXX||requestedWidth>MAXY));
		maxx=max(minBoardSize,noMaxSize?requestedHeight:min(MAXX,requestedHeight));
		maxy=max(minBoardSize,noMaxSize?requestedWidth:min(MAXY,requestedWidth));
		long long boardArea=static_cast<long long>(maxx)*maxy;
		if(acountOfMine)mineNum=args::get(acountOfMine);
		else mineNum=static_cast<int>(sqrt(static_cast<double>(boardArea)));
		difficulty=difficultyV;
	}
	else difficulty=normalV;
	if(difficultyV!=difficulty)memcpy(difficultyV,difficulty,sizeof(int)*3);
	showBoard=bool(show);
	if(1LL*maxx*maxy>numeric_limits<int>::max())
	{
		cerr<<"error: board generation supports at most INT_MAX board cells"<<endl;
		exit(1);
	}
	if(mineNum<1||mineNum>maxx*maxy-4)
	{
		cerr<<"error: board generation supports 1.."<<maxx*maxy-4
			<<" mines; the first zero needs at least four safe cells"<<endl;
		exit(1);
	}
	if(first)
	{
		string value=args::get(first);int row=0,column=0;char comma=0;
		istringstream input(value);
		if(!(input>>row>>comma>>column)||comma!=','||!input.eof()||row<1||row>maxx||column<1||column>maxy)
		{
			cerr<<"error: --first must be row,column within the configured board (1-based)"<<endl;
			exit(1);
		}
		displayFirst=(row-1)*maxy+column-1;nowx=row-1;nowy=column-1;
	}
	if(seed)
	{
		string value=args::get(seed);unsigned long long parsed=0;
		bool valid=!value.empty();
		for(char c:value)
		{
			if(c<'0'||c>'9'||parsed>429496729ULL){valid=false;break;}
			parsed=parsed*10+c-'0';
			if(parsed>4294967295ULL){valid=false;break;}
		}
		if(!valid){cerr<<"error: --seed must be in 0..4294967295"<<endl;exit(1);}
		boardSeed=static_cast<unsigned int>(parsed);seedSpecified=true;
	}
}

int printBoardAndExit()
{
	int first=displayFirst<0?(maxx/2)*maxy+maxy/2:displayFirst;
	auto started=chrono::steady_clock::now();
	try
	{
		if(mineNum>maximumZeroMines(first))
		{
			cerr<<"error: this first zero supports 1.."<<maximumZeroMines(first)<<" mines"<<endl;
			return 1;
		}
		auto generated=noguess::generate(maxx,maxy,mineNum,first,boardRandom,
			[]{return true;},generationLimits,gameMode==NO_GUESS_MODE);
		if(generated.status!=noguess::GENERATED)
		{
			cerr<<"error: "<<(gameMode==NO_GUESS_MODE?"no-guess ":"")
				<<"generation timed out after "<<generationLimits.maxMillis<<" ms ("
				<<generated.stats.attempts<<" checks); increase --generation-timeout to allow more time"<<endl;
			return 1;
		}
		const vector<bool> &mines=generated.mines;
		cout<<"rows="<<maxx<<" columns="<<maxy<<" mines="<<mineNum
			<<" first="<<first/maxy+1<<','<<first%maxy+1<<" seed="<<boardSeed;
		if(gameMode==NO_GUESS_MODE)cout<<" mode=no-guess attempts="<<generated.stats.attempts;
		cout<<'\n';
		cout<<"elapsed_ms="<<fixed<<setprecision(3)<<chrono::duration<double,milli>(chrono::steady_clock::now()-started).count()
			<<" (* mine, . zero, digits clues)\n";
		for(int r=0;r<maxx;r++)
		{
			for(int c=0;c<maxy;c++)
			{
				if(c)cout<<' ';
				int cell=r*maxy+c,n=0;
				for(int nr=max(0,r-1);nr<min(maxx,r+2);nr++)
					for(int nc=max(0,c-1);nc<min(maxy,c+2);nc++)n+=mines[nr*maxy+nc];
				cout<<(mines[cell]?'*':n?static_cast<char>('0'+n):'.');
			}
			cout<<'\n';
		}
		return 0;
	}
	catch(const exception &e){cerr<<"error: generation failed: "<<e.what()<<endl;return 1;}
}

int main(int argc,char** argv)
{
	argsParse(argc,argv);
	if(!seedSpecified)boardSeed=static_cast<unsigned int>(time(NULL));
	boardRandom.seed(boardSeed);
	if(showBoard)return printBoardAndExit();
	realInit();
	updateTerminalViewport();
	init();
	redrawScreen();
	do{
		gameStart();
		if(!newGameStart())break;
		clearMessages();
		updateTerminalViewport();
		init();
	}while(1);
	quit();
	return 0;
}
