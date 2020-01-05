#pragma once

#include "error/error.h"
#include "instruction/instruction.h"
#include "tokenizer/token.h"

#include <stack>
#include <vector>
#include <optional>
#include <utility>
#include <map>
#include <cstdint>
#include <cstddef> // for std::size_t

namespace miniplc0 {

	class Analyser final {
	private:
		using uint64_t = std::uint64_t;
		using int64_t = std::int64_t;
		using uint32_t = std::uint32_t;
		using int32_t = std::int32_t;
		typedef struct {
			int nameIndex;
			std::string name;
			TokenType type;
			std::vector<TokenType> paraType;
			std::vector<bool> isConst;
			int paraSize;
			int level;
			std::vector<std::string> instructions;
		}function;
	public:

		Analyser(std::vector<Token> v)
			: _tokens(std::move(v)), _offset(0), _instructions({}), _current_pos(0, 0),
			 _nextTokenIndex(0) {}
		Analyser(Analyser&&) = delete;
		Analyser(const Analyser&) = delete;
		Analyser& operator=(Analyser) = delete;
		// 唯一接口
		std::pair<std::vector<Instruction>, std::optional<CompilationError>> Analyse();
	private:
		// 所有的递归子程序

		// <主过程>
		//std::optional<CompilationError> analyseMain();
		// <程序>
		std::optional<CompilationError> analyseProgram();
		// <变量声明>
		std::optional<CompilationError> analyseVariableDeclaration();
		// <一串变量声明>
		std::optional<CompilationError> analyseInitDeclaratorList(bool isConst,TokenType);
		// 变量初始化
		std::optional<CompilationError> analyseInitDeclarator(bool,TokenType);
		// <函数定义>
		std::optional<CompilationError> analyseFunctionDeclaration();
		// <函数参数>
		std::optional<CompilationError> analyseParameterClause(function&);
		// <函数体>
		std::optional<CompilationError> analyseCompoundStatement();
		// <函数调用>
		std::optional<CompilationError> analyseFunctionCall();
		// <语句序列>
		std::optional<CompilationError> analyseStatementSequence();
		// <单条语句>
		std::optional<CompilationError> analyseStatement();
		// <表达式>
		std::optional<CompilationError> analyseExpression(TokenType&);
		// <赋值语句>
		std::optional<CompilationError> analyseAssignmentStatement();
		// <输出语句>
		std::optional<CompilationError> analyseOutputStatement();
		// <输入语句>
		std::optional<CompilationError> analyseScanStatement();
		// <条件语句>
		std::optional<CompilationError> analyseConditionStatement();
		// <条件>
		std::optional<CompilationError> analyseCondition();
		// <循环语句>
		std::optional<CompilationError> analyseLoopStatement();
		// <跳转语句>
		std::optional<CompilationError> analyseJumpStatement();
		// <项>
		std::optional<CompilationError> analyseItem(TokenType&);
		// <因子>
		std::optional<CompilationError> analyseFactor(TokenType&);

		// 获得函数
		Analyser::function getFunction(std::string);		
		// 获得函数index
		int getFunctionIndex(std::string);
		// Token 缓冲区相关操作

		// 返回下一个 token
		std::optional<Token> nextToken();
		// 回退一个 token
		void unreadToken();

		// 下面是符号表相关操作
		bool funcExist(std::string);
		// 添加变量
		void addVariable(const Token&,bool,TokenType);
		void addConstant(std::string);
		// 是否是局部变量
		bool isDeclaredLocal(const std::string&);
		// 是否被声明过
		bool isDeclared(const std::string&);
		// 是否是常量
		bool isConstant(const std::string&);
		// 获得 {变量，常量} 在栈上的偏移
		int32_t getIndex(const std::string&);
	public:
		std::vector<std::string> start, crtInstructions = start;
		std::vector<function> functions;
		std::vector<std::string> _constants;
	private:
		std::vector<Token> _tokens;
		std::size_t _offset;
		std::vector<Instruction> _instructions;
		std::pair<uint64_t, uint64_t> _current_pos;

		// 为了简单处理，我们直接把符号表耦合在语法分析里
		// 变量                   示例
		// _uninitialized_vars    var a;
		// _vars                  var a=1;
		// _consts                const a=1;
		std::map<std::string, int32_t> _vars;
		std::map<std::string, int32_t> _consts;
		typedef struct {
			std::string name;
			std::string func;
			bool isConst = false;
			TokenType type;
			int level;
		}symbol;
		std::vector<symbol> symbols;
		std::string crtFuntion = "";
		// 下一个 token 在栈的偏移
		int32_t _nextTokenIndex;
		int32_t level = 0;
	};
}
