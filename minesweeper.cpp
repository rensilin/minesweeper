#include <cstdio>                                      /********************/
#include <iostream>                                    /*   made by kkke   */
#include <time.h>                                      /********************/
#include <cstring>
#include <cstdlib>
#include <cmath>
#include "getch.h"
#include <stdlib.h>
#include "SColor/SColor.h"

#define MAXY 100
#define MAXX 100

using namespace std;

const string version="v1.4";

int mMap[MAXX][MAXY];
bool mMine[MAXX][MAXY];
bool mSight[MAXX][MAXY];
bool mFlag[MAXX][MAXY];
bool mHighlight[MAXX][MAXY];

int maxy=20;
int maxx=20;
int mineNum=60;

int nowy, nowx;

int theRestOfMine=mineNum;
int theRestOfSquare=maxy*maxx;

char cInput;
bool firstMove;
SColor defaultColor;

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
	exit(0);
}

void printMap(int finished=0)
{
	SColor::setCursor(0,0);
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
		switch (i)
		{
		case 0:		cout<<"    ***************";break;
		case 1:		cout<<"    * minesweeper *";break;
		case 2:		cout<<"    *     "<<version<<"    *";break;
		case 3:		cout<<"    ***************";break;
		case 5:		cout<<"    up     :w";break;
		case 6:		cout<<"    down   :s";break;
		case 7:		cout<<"    left   :a";break;
		case 8:		cout<<"    right  :d";break;
		case 9:		cout<<"    flag   :j";break;
		case 10:	cout<<"    sweep  :space";break;
		case 11:	cout<<"    restart:r";break;
		case 12:	cout<<"    quit   :q";break;
		case 15:	cout<<"  rest square:"<<theRestOfSquare;break;
		case 16:	cout<<"  rest mine  :"<<theRestOfMine;break;
		}
		SColor::cleanLine();
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

int main(int argc,char* argv[])
{
	realInit();
	do{
		SColor::clean();
		init();
		gameStart();
	}while(newGameStart());
	quit();
	return 0;
}
