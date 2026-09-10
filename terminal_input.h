#ifndef MINESWEEPER_TERMINAL_INPUT_H
#define MINESWEEPER_TERMINAL_INPUT_H
#include <string>
namespace terminalInput
{
enum Type { NONE, KEY, MOUSE };
struct Event
{
	Type type;
	int key,button,column,row;
};
// Incremental parser shared by gameplay, generation and the end-game prompt.
// Partial escape packets never become keyboard commands.
class Decoder
{
	int state=0;
	int legacyLeft=0;
	bool overflow=false;
	std::string parameters;
public:
	Event feed(int ch)
	{
		Event empty={NONE,0,0,0,0};
		if(legacyLeft){legacyLeft--;return empty;}
		if(ch==27){state=1;parameters.clear();overflow=false;return empty;}
		if(state==1)
		{
			state=0;
			if(ch=='['){state=2;return empty;}
			if(ch=='O'){state=3;return empty;}
		}
		else if(state==2||state==3)
		{
			if(ch>=0x40&&ch<=0x7e)
			{
				state=0;
				if(overflow)return empty;
				if(parameters.empty()&&ch=='M'){legacyLeft=3;return empty;}
				if(!parameters.empty()&&parameters[0]=='<')
				{
					if(ch!='M')return empty; // Release, motion and wheel do not click.
					int fields[3]={0,0,0},field=0;
					bool digit=false;
					for(size_t i=1;i<parameters.size();i++)
					{
						char c=parameters[i];
						if(c==';'&&digit&&field<2){field++;digit=false;}
						else if(c>='0'&&c<='9'&&fields[field]<=1000000)
						{fields[field]=fields[field]*10+c-'0';digit=true;}
						else return empty;
					}
					int button=fields[0]&~28; // Shift / Alt / Ctrl modifier bits.
					if(field!=2||!digit||fields[1]<1||fields[2]<1||(button!=0&&button!=2))return empty;
					return Event{MOUSE,0,button,fields[1],fields[2]};
				}
				// Arrow-key CSI (including modifiers) and SS3 forms.
				for(char c:parameters)if((c<'0'||c>'9')&&c!=';')return empty;
				switch(ch)
				{
				case 'A':return Event{KEY,'w',0,0,0};
				case 'B':return Event{KEY,'s',0,0,0};
				case 'C':return Event{KEY,'d',0,0,0};
				case 'D':return Event{KEY,'a',0,0,0};
				}
				return empty;
			}
			if(parameters.size()<64)parameters.push_back(static_cast<char>(ch));
			else overflow=true;
			return empty;
		}
		return Event{KEY,ch,0,0,0};
	}
};
}
#endif
