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
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <assert.h>
#include "SColor/SColor.h"
#include "args/args.hxx"

#define MAXY 100
#define MAXX 100

using namespace std;

const string version="v1.4";

struct termios org_opts;

const int easyV[3]={9,9,10};
const int normalV[3]={16,16,30};
const int hardV[3]={20,20,60};
int difficultyV[3];
const int *difficulty;

int mMap[MAXX][MAXY];
bool mMine[MAXX][MAXY];
bool mSight[MAXX][MAXY];
bool mFlag[MAXX][MAXY];
bool mHighlight[MAXX][MAXY];

int &maxy=difficultyV[0];
int &maxx=difficultyV[1];
int &mineNum=difficultyV[2];

int nowy, nowx;

int theRestOfMine=mineNum;
int theRestOfSquare=maxy*maxx;

int cInput;
bool firstMove;
SColor defaultColor;
int lastLine;
int gameResult;
volatile sig_atomic_t terminalResized;
bool colorEnabled;
bool unicodeEnabled;

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

CellView mRendered[MAXX][MAXY];
int renderedRestOfMine;
int renderedRestOfSquare;
bool renderedStatusValid;

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

void quit()
{
	SColor::echoCursor();
	endColor();
	SColor::setCursor(lastLine,1);
	cout<<endl;
	//------  restore old settings ---------
	int res;
	res=tcsetattr(STDIN_FILENO, TCSANOW, &org_opts);assert(res==0);
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
	SColor::setCursor(x+2,y*3+2);
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
		SColor::setCursor(17,3*maxy+3);
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
		SColor::setCursor(18,3*maxy+3);
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
	for(int i=0;i<maxx;i++)
		for(int j=0;j<maxy;j++)
		{
			CellView view=getCellView(i,j,finished);
			if(!sameCellView(mRendered[i][j],view))
			{
				renderCell(i,j,view);
				mRendered[i][j]=view;
			}
			if(view.highlighted)mHighlight[i][j]=false;
		}
	refreshStatus();
	cout.flush();
}

void drawControl(int row,const string &label,const string &key)
{
	SColor::setCursor(row,3*maxy+3);
	cout<<"   ";
	beginColor(labelColor);
	cout<<label;
	for(size_t i=label.size();i<7;i++)cout<<' ';
	cout<<':';
	beginColor(keyColor);
	cout<<key;
	endColor();
}

void drawLayout()
{
	const char *horizontal=unicodeEnabled?"─":"-";
	const char *vertical=unicodeEnabled?"│":"|";
	const char *topLeft=unicodeEnabled?"╭":"+";
	const char *topRight=unicodeEnabled?"╮":"+";
	const char *bottomLeft=unicodeEnabled?"╰":"+";
	const char *bottomRight=unicodeEnabled?"╯":"+";
	beginColor(borderColor);
	SColor::setCursor(1,1);
	cout<<topLeft;
	for(int i=0;i<maxy*3;i++)cout<<horizontal;
	cout<<topRight;
	for(int i=0;i<maxx;i++)
	{
		SColor::setCursor(i+2,1);
		cout<<vertical;
		SColor::setCursor(i+2,3*maxy+2);
		cout<<vertical;
	}
	SColor::setCursor(maxx+2,1);
	cout<<bottomLeft;
	for(int i=0;i<maxy*3;i++)cout<<horizontal;
	cout<<bottomRight;
	endColor();
	SColor::setCursor(2,3*maxy+3);
	cout<<"   ";
	beginColor(borderColor);
	cout<<(unicodeEnabled?"╭─────────────╮":"+-------------+");
	endColor();
	SColor::setCursor(3,3*maxy+3);
	cout<<"   ";
	beginColor(borderColor);
	cout<<(unicodeEnabled?"│":"|");
	beginColor(titleColor);
	cout<<" minesweeper ";
	beginColor(borderColor);
	cout<<(unicodeEnabled?"│":"|");
	endColor();
	SColor::setCursor(4,3*maxy+3);
	cout<<"   ";
	beginColor(borderColor);
	cout<<(unicodeEnabled?"│":"|");
	beginColor(titleColor);
	cout<<"     "<<version<<"    ";
	beginColor(borderColor);
	cout<<(unicodeEnabled?"│":"|");
	endColor();
	SColor::setCursor(5,3*maxy+3);
	cout<<"   ";
	beginColor(borderColor);
	cout<<(unicodeEnabled?"╰─────────────╯":"+-------------+");
	endColor();
	drawControl(7,"up","w");
	drawControl(8,"down","s");
	drawControl(9,"left","a");
	drawControl(10,"right","d");
	drawControl(11,"flag","j");
	drawControl(12,"sweep","space");
	drawControl(13,"restart","r");
	drawControl(14,"quit","q");
	cout.flush();
}

void clearMessages()
{
	string blank(3*maxy+2,' ');
	SColor::setCursor(maxx+3,1);
	cout<<blank;
	SColor::setCursor(maxx+4,1);
	cout<<blank;
	cout.flush();
}

void invalidateRenderedState()
{
	memset(mRendered,0,sizeof(mRendered));
	renderedStatusValid=false;
}

void showGameResult(int finished)
{
	SColor::setCursor(maxx+3,1);
	beginColor(finished==1?successColor:failureColor);
	cout<<(finished==1?"you win! ":"you lose!");
	endColor();
	cout.flush();
}

void showNewGamePrompt()
{
	SColor::setCursor(maxx+4,1);
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

void redrawScreen(int finished=0,bool showPrompt=false)
{
	terminalResized=0;
	SColor::clean();
	invalidateRenderedState();
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
	else if(mMine[x][y])
	{
		mSight[x][y]=true;
		return false;
	}
	bool flag=true;
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
						mHighlight[i][j]=true;
			flag=false;
		}
	}
	else
	{
		theRestOfSquare--;
		mSight[x][y]=true;
		if(mMap[x][y])flag=false;
	}
	if(flag)
	{
		for(int i=x-1;i<x+2;i++)
			for(int j=y-1;j<y+2;j++)
				if(i>=0&&j>=0&&i<maxx&&j<maxy&&!mSight[i][j])
					if(!sweepMine(i,j))
						flag=false;
		return flag;
	}
	return true;
}

void init()
{
	lastLine=max(maxx+5,19);
	gameResult=0;
	firstMove=true;
	memset(mMine,false,sizeof(mMine));
	memset(mSight,false,sizeof(mSight));
	memset(mHighlight,false,sizeof(mHighlight));
	//memset(mSight,true,sizeof(mSight));
	memset(mMap,0,sizeof(mMap));
	memset(mFlag,false,sizeof(mFlag));
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
	theRestOfSquare=maxy*maxx;
}

bool getInput()
{
	while(1)
	{
		if(terminalResized)
		{
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
		case 'j':
			if(mSight[nowx][nowy])break;
			if(mFlag[nowx][nowy]){mFlag[nowx][nowy]=false;theRestOfMine++;theRestOfSquare++;}
			else {mFlag[nowx][nowy]=true;theRestOfMine--;theRestOfSquare--;}
			return true;
		case ' ':
			if(mFlag[nowx][nowy])break;
			return sweepMine(nowx,nowy);
		case 'r':
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
	bool wingame=true;
	for(int i=0;i<maxx&&wingame;i++)
		for(int j=0;j<maxy&&wingame;j++)
			if(!(mMine[i][j]||mSight[i][j]))wingame=false;
	return wingame;
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
	args::Positional<int> height(parser,"height","set height");
	args::Positional<int> weight(parser,"weight","set weight");
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
		maxx=max(9,min(100,args::get(height)));
		if(weight)maxy=max(9,min(100,args::get(weight)));
		else maxy=9;
		if(acountOfMine)mineNum=max(1,min(maxx*maxy,args::get(acountOfMine)));
		else mineNum=sqrt(maxx*maxy);
		difficulty=difficultyV;
	}
	else difficulty=normalV;
	if(difficultyV!=difficulty)memcpy(difficultyV,difficulty,sizeof(int)*3);
}

int main(int argc,char** argv)
{
	argsParse(argc,argv);
	realInit();
	init();
	redrawScreen();
	do{
		gameStart();
		if(!newGameStart())break;
		clearMessages();
		init();
	}while(1);
	quit();
	return 0;
}
