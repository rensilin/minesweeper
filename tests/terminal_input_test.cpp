#include "terminal_input.h"
#include <cassert>
#include <iostream>
#include <vector>
using namespace terminalInput;
std::vector<Event> decode(const std::string &bytes)
{
	Decoder decoder;
	std::vector<Event> events;
	for(unsigned char byte:bytes)
	{
		auto event=decoder.feed(byte);
		if(event.type!=NONE)events.push_back(event);
	}
	return events;
}
int main()
{
	// feed() receives individual bytes, including packet boundaries.
	auto events=decode("\033[<0;301;42M\033[<0;301;42m\033[<2;8;9M");
	assert(events.size()==2);
	assert(events[0].type==MOUSE&&events[0].button==0&&events[0].column==301&&events[0].row==42);
	assert(events[1].button==2&&events[1].column==8&&events[1].row==9);
	for(auto packet:std::vector<std::string>{"\033[<0;0;1M","\033[<0;1M","\033[<;2;2M",
		"\033[<0;2;2;3M","\033[<32;2;2M","\033[<64;2;2M","\033[<1;2;2M",
		"\033[<0;999999999999999;2M","\033[<0;"+std::string(100,'1')+";2M",
		"\033[<0;2;2m","\033[Mqrf"})
	{
		auto ignored=decode(packet+"q");
		assert(ignored.size()==1&&ignored[0].type==KEY&&ignored[0].key=='q');
	}
	events=decode("\033[A\033OB\033[1;5C\033[Dfr ");
	assert(events.size()==7);
	std::string keys;
	for(auto event:events){assert(event.type==KEY);keys+=event.key;}
	assert(keys=="wsdafr ");
	std::cout<<"Mouse packet parsing, ignored reports and keyboard decoding passed\n";
}
