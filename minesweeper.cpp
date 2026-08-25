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

const int minimumPanelWidth=18;
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
bool firstMove;
SColor defaultColor;
int lastLine;
int gameResult;
volatile sig_atomic_t terminalResized;
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
	int sizeWidth=static_cast<int>(sizeValue.size())+11;
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
	SColor::echoCursor();
	endColor();
	int exitRow=terminalTooSmall?max(1,terminalRows):max(1,lastLine);
	SColor::setCursor(exitRow,1);
	cout<<endl;
	//------  restore old settings ---------
	int res;
	res=tcsetattr(STDIN_FILENO, TCSANOW, &org_opts);assert(res==0);
}

void quit()
{
	restoreTerminal();
	exit(0);
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

void refreshStatus()
{
	if(!renderedStatusValid||renderedRestOfSquare!=theRestOfSquare)
	{
		SColor::setCursor(10,3*visibleMaxy+3);
		beginColor(labelColor);
		cout<<" rest square:";
		beginColor(keyColor);
		cout<<theRestOfSquare;
		endColor();
		SColor::cleanLine();
		renderedRestOfSquare=theRestOfSquare;
	}
	if(!renderedStatusValid||renderedRestOfMine!=theRestOfMine)
	{
		SColor::setCursor(11,3*visibleMaxy+3);
		beginColor(labelColor);
		cout<<" rest mine  :";
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
	SColor::setCursor(row,3*visibleMaxy+3);
	cout<<"   ";
	beginColor(labelColor);
	cout<<label;
	for(size_t i=label.size();i<7;i++)cout<<' ';
	cout<<':';
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
	SColor::setCursor(8,3*visibleMaxy+3);
	cout<<"   ";
	beginColor(labelColor);
	cout<<"view   :";
	beginColor(keyColor);
	cout<<visibleMaxx<<'x'<<visibleMaxy;
	endColor();
	SColor::setCursor(9,3*visibleMaxy+3);
	cout<<"   ";
	beginColor(labelColor);
	cout<<"size   :";
	beginColor(keyColor);
	cout<<maxx<<'x'<<maxy;
	endColor();
	drawMovementControls();
	drawControl(15,"flag","j/f");
	drawControl(16,"sweep","space");
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

bool sweepMine(int x,int y)
{
	if(mFlag[x][y])return true;
	if(firstMove)
	{
		firstMove=false;
		if(mMine[x][y])
		{
			for(int i=x-1;i<x+2;i++)
				for(int j=y-1;j<y+2;j++)
					if(j>=0&&i>=0&&j<maxy&&i<maxx)
						mMap[i][j]--;
			int mineY,mineX;
			do{
				mineX=rand()%maxx;
				mineY=rand()%maxy;
			}while(mMine[mineX][mineY]);
			mMine[x][y]=false;
			mMine[mineX][mineY]=true;
			for(int i=mineX-1;i<mineX+2;i++)
				for(int j=mineY-1;j<mineY+2;j++)
					if(j>=0&&i>=0&&j<maxy&&i<maxx)
						mMap[i][j]++;
		}
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
	int mineY,mineX;
	for(int k=0;k<mineNum;k++)
	{
		mineY=rand()%maxy;
		mineX=rand()%maxx;
		if(mMine[mineX][mineY])k--;
		else
		{
			mMine[mineX][mineY]=true;
			for(int i=mineX-1;i<mineX+2;i++)
				for(int j=mineY-1;j<mineY+2;j++)
					if(j>=0&&i>=0&&j<maxy&&i<maxx)
						mMap[i][j]++;
		}
	}
	theRestOfMine=mineNum;
	theRestOfSquare=static_cast<long long>(maxy)*maxx;
	hiddenSafeSquares=theRestOfSquare-mineNum;
}

bool getInput()
{
	int escapeState=0;
	while(1)
	{
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
		if(terminalTooSmall)
		{
			if(cInput=='q')quit();
			continue;
		}
		if(escapeState==1)
		{
			if(cInput=='['||cInput=='O')
			{
				escapeState=2;
				continue;
			}
			escapeState=0;
		}
		else if(escapeState==2)
		{
			escapeState=0;
			switch(cInput)
			{
			case 'A':
				if(nowx>0)nowx--;
				return true;
			case 'B':
				if(nowx<maxx-1)nowx++;
				return true;
			case 'C':
				if(nowy<maxy-1)nowy++;
				return true;
			case 'D':
				if(nowy>0)nowy--;
				return true;
			}
		}
		switch (cInput)
		{
		case '\033':
			escapeState=1;
			break;
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
			if(mSight[nowx][nowy])break;
			if(mFlag[nowx][nowy]){mFlag[nowx][nowy]=false;theRestOfMine++;theRestOfSquare++;}
			else {mFlag[nowx][nowy]=true;theRestOfMine--;theRestOfSquare--;}
			return true;
		case ' ':
			if(mFlag[nowx][nowy])break;
			return sweepMine(nowx,nowy);
		case 'r':
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
	//-----  store old settings -----------
	res=tcgetattr(STDIN_FILENO, &org_opts);
	assert(res==0);
	struct sigaction resizeAction;
	memset(&resizeAction,0,sizeof(resizeAction));
	resizeAction.sa_handler=handleTerminalResize;
	sigemptyset(&resizeAction.sa_mask);
	res=sigaction(SIGWINCH,&resizeAction,NULL);assert(res==0);
	//---- set new terminal parms --------
	memcpy(&new_opts, &org_opts, sizeof(new_opts));
	new_opts.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ECHOPRT | ECHOKE | ICRNL);
	tcsetattr(STDIN_FILENO, TCSANOW, &new_opts);

	srand(time(NULL));
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
		int maxMineNum=boardArea>numeric_limits<int>::max()
			?numeric_limits<int>::max():static_cast<int>(boardArea-1);
		if(acountOfMine)mineNum=max(1,min(maxMineNum,args::get(acountOfMine)));
		else mineNum=static_cast<int>(sqrt(static_cast<double>(boardArea)));
		difficulty=difficultyV;
	}
	else difficulty=normalV;
	if(difficultyV!=difficulty)memcpy(difficultyV,difficulty,sizeof(int)*3);
}

int main(int argc,char** argv)
{
	argsParse(argc,argv);
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
