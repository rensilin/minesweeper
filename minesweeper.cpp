#include <cstdio>                                      /********************/
#include <iostream>                                    /*   made by kkke   */
#include <conio.h>                                     /********************/
#include <time.h>
#include <cstring>
#include <cstdlib>

#define MAXX 20
#define MAXY 20
#define MINE_NUM 40

using namespace std;

int mymap[MAXY][MAXX];
bool mymine[MAXY][MAXX];
bool mysight[MAXY][MAXX];
bool myflag[MAXY][MAXX];
int nowx, nowy;
int the_rest_of_mine=MINE_NUM;
int the_rest_of_square=MAXX*MAXY;
char c_input;

void print_map()
{
   for(int j=-2;j<MAXX*3;j++)cout<<'-';
   cout<<"    ********"<<endl;
   for(int i=0;i<MAXY;i++)
   {
    cout<<'|';
   	for(int j=0;j<MAXX;j++)
   	{
   		if((i==nowy)&&(j==nowx))cout<<'[';
   		else cout<<' ';
   		if(myflag[i][j])cout<<'@';
   		else if(mysight[i][j])
   		{
   			if(mymine[i][j])cout<<'*';
   			else if(mymap[i][j])cout<<mymap[i][j];
   			else cout<<" ";
   		}
   		else cout<<'.';
   		if((i==nowy)&&(j==nowx))cout<<']';
   		else cout<<' ';
   	}
   	cout<<'|';
   	switch (i)
   	{
   	    case 0: cout<<"    * made *";break;
   	    case 1: cout<<"    *  by  *";break;
   	    case 2: cout<<"    * kkke *";break;
   	    case 3: cout<<"    ********";break;
   	    case 7: cout<<"    up   :w";break;
   	    case 8: cout<<"    down :s";break;
   	    case 9: cout<<"    left :a";break;
       case 10: cout<<"    right:d";break;
       case 11: cout<<"    flag :j";break;
       case 12: cout<<"    sweep:space";break;
       case 15: cout<<"  rest square:"<<the_rest_of_square;break;
       case 16: cout<<"  rest mine  :"<<the_rest_of_mine;break;
   	}
   	cout<<endl;
   }
   for(int j=-2;j<MAXX*3;j++)cout<<'-';
   cout<<endl;
}

void sweep_mine(int y,int x)
{
	if(mysight[y][x])return;
	if(myflag[y][x])return;
	the_rest_of_square--;
	mysight[y][x]=true;
	if(mymap[y][x])return;
	for(int i=y-1;i<y+2;i++)
	    for(int j=x-1;j<x+2;j++)
	        if(i>=0&&j>=0&&i<MAXY&&j<MAXX)
	            sweep_mine(i,j);
}

bool get_input()
{
	while(1)
	{
		c_input=getch();
		switch (c_input)
		{
			case 'w':
			    if(nowy>0)nowy--;
			    return true;
			case 'a':
			    if(nowx>0)nowx--;
			    return true;
			case 's':
			    if(nowy<MAXY-1)nowy++;
			    return true;
			case 'd':
			    if(nowx<MAXX-1)nowx++;
			    return true;
			case 'j':
			    if(mysight[nowy][nowx])break;
			    if(myflag[nowy][nowx]){myflag[nowy][nowx]=false;the_rest_of_mine++;the_rest_of_square++;}
			    else {myflag[nowy][nowx]=true;the_rest_of_mine--;the_rest_of_square--;}
			    return true;
   		 case ' ':
   		     if(myflag[nowy][nowx])break;
		        sweep_mine(nowy,nowx);
			    if(mymine[nowy][nowx])return false;
			    return true;
		}
	}
}

bool win_game()
{
	bool wingame=true;
	for(int i=0;i<MAXY&&wingame;i++)
	    for(int j=0;j<MAXX&&wingame;j++)
	        if(!(mymine[i][j]||mysight[i][j]))wingame=false;
	return wingame;
}

void game_start()
{
	do{
		system("cls");
		print_map();
		if(win_game())
		{
			cout<<"you win!"<<endl;
			return;
		}
	}while(get_input());
	system("cls");
	print_map();
	cout<<"you lose!"<<endl;
}

void init()
{
	memset(mymine,false,sizeof(mymine));
	memset(mysight,false,sizeof(mysight));
//memset(mysight,true,sizeof(mysight));
	memset(mymap,0,sizeof(mymap));
	memset(myflag,false,sizeof(myflag));
	int mine_x,mine_y;
	for(int k=0;k<MINE_NUM;k++)
	{
		mine_x=rand()%MAXX;
		mine_y=rand()%MAXY;
		if(mymine[mine_y][mine_x])k--;
		else
		{
			mymine[mine_y][mine_x]=true;
			for(int j=mine_y-1;j<mine_y+2;j++)
			    for(int i=mine_x-1;i<mine_x+2;i++)
			        if(i>=0&&j>=0&&i<MAXX&&j<MAXY)
			            mymap[j][i]++;
		}
	}
}

bool new_game_start()
{
	cout<<"press y to start a new game"<<endl;
	while(getch()!='y');
	return true;
}

void real_init()
{
	srand(time(NULL));
}

int main()
{
	real_init();
	do{
		init();
		game_start();
	}while(new_game_start());
    return 0;
}
