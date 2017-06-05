#include <cstdio>                                      /********************/
#include <iostream>                                    /*   made by kkke   */
#include <ctime>                                      /********************/
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <stdlib.h>
#include "getch.h"
#include "SColor/SColor.h"
#include "args/args.hxx"

#define MAXY 100
#define MAXX 100

using namespace std;

const string version="v1.4";

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

char cInput;
bool firstMove;
SColor defaultColor;
int lastLine;

SColor nColor[]={
	SColor(),
	SColor::BLUE,//1
	SColor::GREEN,//2
	SColor::RED,//3
	SColor(),//4
	SColor::YELLOW,//5
	SColor::CYAN,//6
	SColor::PURPLE,//7
	SColor(SColor::BLACK,SColor::YELLOW)//8
};

void quit()
{
	SColor::echoCursor();
	cout<<defaultColor;
	SColor::setCursor(lastLine,1);
	cout<<endl;
	exit(0);
}

void printMap(int finished=0)
{
	SColor::setCursor(17,3*maxy+2);	cout<<"  rest square:"<<theRestOfSquare;SColor::cleanLine();
	SColor::setCursor(18,3*maxy+2);	cout<<"  rest mine  :"<<theRestOfMine;SColor::cleanLine();
	SColor::setCursor(1,1);
	for(int i=-2;i<maxy*3;i++)cout<<'-';
	cout<<endl;
	for(int i=0;i<maxx;i++)
	{
		cout<<'|';
		for(int j=0;j<maxy;j++)
		{
			SColor color;
			if(mSight[i][j]&&!mMine[i][j])
				color=nColor[mMap[i][j]];
			if(finished==-1)
			{
				if(mFlag[i][j]&&!mMine[i][j])
					color.setFg(SColor::BLACK).setBg(SColor::RED);
				if(mMine[i][j]&&mSight[i][j])
				{
					color.setFg(SColor::BLACK).setBg(SColor::RED);
					color|=SColor::HIGHLIGHT;
				}
			}
			if(finished==0&&mFlag[i][j])
			{
				color.setFg(SColor::CYAN);
				color|=SColor::HIGHLIGHT|SColor::ITALIC;
			}
			if(!mSight[i][j]&&mHighlight[i][j])
			{
				mHighlight[i][j]=false;
				color|=SColor::INVERT;
			}
			cout<<color;
			if((i==nowx)&&(j==nowy))cout<<'[';
			else cout<<' ';
			if(mFlag[i][j])cout<<'@';
			else if(mSight[i][j])
			{
				if(mMine[i][j])cout<<'*';
				else if(mMap[i][j])cout<<mMap[i][j];
				else cout<<" ";
			}
			else cout<<'.';
			if((i==nowx)&&(j==nowy))cout<<']';
			else cout<<' ';
			cout<<defaultColor;
		}
		cout<<'|';
		cout<<endl;
	}
	for(int j=-2;j<maxy*3;j++)cout<<'-';
	cout<<endl;
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
	lastLine=max(maxx+2,18);
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
	cout<<"loading...";
	SColor::setCursor(2,3*maxy+2);		cout<<"    ***************";
	SColor::setCursor(3,3*maxy+2);		cout<<"    * minesweeper *";
	SColor::setCursor(4,3*maxy+2);		cout<<"    *     "<<version<<"    *";
	SColor::setCursor(5,3*maxy+2);		cout<<"    ***************";
	SColor::setCursor(7,3*maxy+2);		cout<<"    up     :w";
	SColor::setCursor(8,3*maxy+2);		cout<<"    down   :s";
	SColor::setCursor(9,3*maxy+2);		cout<<"    left   :a";
	SColor::setCursor(10,3*maxy+2);		cout<<"    right  :d";
	SColor::setCursor(11,3*maxy+2);		cout<<"    flag   :j";
	SColor::setCursor(12,3*maxy+2);		cout<<"    sweep  :space";
	SColor::setCursor(13,3*maxy+2);		cout<<"    restart:r";
	SColor::setCursor(14,3*maxy+2);		cout<<"    quit   :q";
}

bool getInput()
{
	while(1)
	{
		cInput=getch();
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
	do{
		if(winGame())
		{
			printMap(1);
			cout<<"you win!"<<endl;
			return;
		}
		printMap();
	}while(getInput());
	printMap(-1);
	cout<<"you lose!"<<endl;
}

bool newGameStart()
{
	cout<<"press y to start a new game"<<endl;
	lastLine=max(maxx+4,18);
	char c;
	while((c=getch())!='y'&&c!='q');
	if(c=='q')return false;
	return true;
}

void realInit()
{
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
	do{
		SColor::clean();
		init();
		gameStart();
	}while(newGameStart());
	quit();
	return 0;
}

