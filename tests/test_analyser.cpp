#include "catch2/catch.hpp"

#include "instruction/instruction.h"
#include "tokenizer/tokenizer.h"
#include "analyser/analyser.h"
#include "simple_vm.hpp"

#include <sstream>
/*
	不要忘记写测试用例喔。
*/
TEST_CASE(){
	
	std::string input =
		"//asfas\n"
		"/*dafd*/\n"
		"const int b = 1;\n"
		"int x = 1;\n"
		"int y = 0;\n"
		"int fun(int x) {\n"
		"x = x+1;\n"
		"print(x);\n"
		"return  x;/*56\n"
		"*/}\n"
		"void main() {\n"
		"int i= 1;\n"
		"int max,y;\n"
		"scan(max);\n"
		"while(i < max){//yvjkhvhjvjhv\n"
		"scan(y);\n"
		"if(i < y)\n"
		"i = i+1;\n"
		"else print(i);}/*bhjhj*/\n"
		"return;\n"
		"}//";

	/*
		"int x;\n"
		"int fun(int num) {\n"
		"int rtv = num/2+1-1;\n"
		"return rtv+(1*2);\n"
		"}\n"
		"void main() {\n"
		"x = 1;\n"
		"if(x < 2)\n"
		"if(x > 0)\n"
		"x = x +1;"
		"else x = 1;\n"
		"print(x);\n"
		"print(1,fun(x),3);\n"
		"return;\n"
		"}";
		*/
	std::stringstream ss;
	ss.str(input);
	miniplc0::Tokenizer tkz(ss);
	auto tks = tkz.AllTokens();
	if (tks.second.has_value()) {
		FAIL();
	}
	miniplc0::Analyser analyser(tks.first);
	auto p = analyser.Analyse();

	if (p.second.has_value()) {
		FAIL();
	}
	//auto instructions = p.first;
	//miniplc0::VM vm(instructions);
	//std::vector<int32_t> res = vm.Run(), output = {2};
	//REQUIRE(res == output);
	
}