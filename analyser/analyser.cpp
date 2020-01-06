#include "analyser.h"

#include <climits>
#include <sstream>
#include <cstdio>

namespace miniplc0 {
	std::pair<std::vector<Instruction>, std::optional<CompilationError>> Analyser::Analyse() {
		auto err = analyseProgram();
		if (err.has_value())
			return std::make_pair(std::vector<Instruction>(), err);
		else {
			return std::make_pair(_instructions, std::optional<CompilationError>());
		}
	}


	// <主过程> ::= <变量声明><函数声明>
	// 需要补全
	std::optional<CompilationError> Analyser::analyseProgram() {

		// <变量声明>
		auto var = analyseVariableDeclaration();
		if (var.has_value())
		{
			return var;
		}
		// <函数序列>
		auto seq = analyseFunctionDeclaration();
		if (seq.has_value())
		{
			return seq;
		}
		return {};
	}
	// <变量声明> ::= {<变量声明语句>}
// <变量声明语句> ::= 'int'|'char' <标识符>['='<表达式>]';'
// 需要补全
	std::optional<CompilationError> Analyser::analyseVariableDeclaration() {
		// 变量声明语句可能有一个或者多个
		while (true)
		{
			// 预读？
			auto next = nextToken();
			if (!next.has_value())
				return {};
			// 'const'
			if (next.value().GetType() ==TokenType::CONST)
			{
				next = nextToken();
				if(!next.has_value() || next.value().GetType() != INT&& next.value().GetType() != CHAR)
					return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidVariableDeclaration);
				auto type = next.value().GetType();
				auto err = analyseInitDeclaratorList(true, type);
				if (err.has_value())
				{
					return err;
				}
				else {
					next = nextToken();
					if (!next.has_value() || next.value().GetType() != SEMICOLON)
						return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidVariableDeclaration);
					continue;
				}
			}
			// 'int'
			if (next.value().GetType() != TokenType::INT&& next.value().GetType() != TokenType::CHAR)
			{
				unreadToken();
				return {};
			}
			auto type = next.value().GetType();
			// init-declarator-list
			auto next1 = nextToken();
			//尝试读取 标识符
			if (!next1.has_value() || next1.value().GetType() != TokenType::IDENTIFIER)
			{
				return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNeedIdentifier);
			}	
			// 尝试读取 ； （
			auto next2 = nextToken();
			if (!next2.has_value())
			{
				return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoSemicolon);
			}
			// 发现是函数声明 int identifer ( 
			if (next2.value().GetType()==TokenType::LEFT_BRACKET)
			{
				unreadToken();
				unreadToken();
				unreadToken();
				return {};
			}
			// 否则init-declarator-list
			else {
				unreadToken();
				unreadToken();
			}
			auto err = Analyser::analyseInitDeclaratorList(false, next.value().GetType());
			if (err.has_value())
			{
				return err;
			}
			// 获取 ;
			next = nextToken();
			if (!next.has_value()||next.value().GetType() != TokenType::SEMICOLON)
				return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoSemicolon);
			
		}
		return {};
	}

	// 声明序列 :: =
	//	<init - declarator>{ ',' < init - declarator > }
	std::optional<CompilationError> Analyser::analyseInitDeclaratorList(bool isConst,TokenType type) {
		do {
			auto err = analyseInitDeclarator(isConst,type);
			if (err.has_value())
			{
				return err;
			}
			auto next = nextToken();
			if (!next.has_value())
			{
				return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidVariableDeclaration);
			}
			if (next.value().GetType() != TokenType::COMMA)
			{
				unreadToken();
				return {};
			}
		} while (true);
	}

	//<identifier>[<initializer>]
	//<initializer> :: =
	//	'=' < expression >
	std::optional<CompilationError> Analyser::analyseInitDeclarator(bool isConst,TokenType type) {
		auto next = nextToken();
		// 试探为 identifier
		if (!next.has_value()||next.value().GetType() != IDENTIFIER)
		{
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNeedIdentifier);
		}
		auto tk = next.value();
		next = nextToken();
		if (!next.has_value())
		{
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidVariableDeclaration);
		}
		// 试探 initializer 的 '='
		if (next.value().GetType() == TokenType::ASSIGN)
		{
			TokenType exprtype;
			auto err = analyseExpression(exprtype);
			if (err.has_value())
			{
				return err;
			}
			//如果是char 则截断
			if (type == TokenType::CHAR && exprtype == TokenType::INT)
			{
				crtInstructions.emplace_back("i2c");
			}
		}
		// 否则回退
		else
		{
			if (isConst)
			{
				return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrConstantNeedValue);
			}
			crtInstructions.emplace_back("ipush 0");
			unreadToken();
		}
		// 防止重复声明
		if (isDeclaredLocal(tk.GetValueString()))
		{
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrDuplicateDeclaration);
		}
		addVariable(tk, isConst,type);
		return {};
	}

	// 函数声明
	//<function-definition> ::= 
	//<type - specifier><identifier><parameter - clause><compound - statement>
	
	std::optional<CompilationError> Analyser::analyseFunctionDeclaration() {
		// 获取 函数类型
		while (true)
		{
			if (level == 0)start = crtInstructions;
			auto next = nextToken();
			if (!next.has_value())
			{
				return {};
			}
			if (!next.has_value()||next.value().GetType()!=TokenType::VOID && next.value().GetType() != TokenType::INT && next.value().GetType() != TokenType::CHAR)
			{
				return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
			}
			auto type = next.value().GetType();

			// 获取 标识符
			next = nextToken();
			if (!next.has_value() || next.value().GetType() != TokenType::IDENTIFIER)
			{
				return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNeedIdentifier);
			}
			if (funcExist(next.value().GetValueString()))
			{
				return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrDuplicateDeclaration);
			}

			std::vector<std::string> instru;
			std::string name = next.value().GetValueString();
			std::vector<bool>isConstant;
			crtFuntion = name;
			int paraSize = 0;
			std::vector<TokenType> paraType;
			if (level == 0)
			{
				level++;
			}
			addConstant(name);
			int nameIndex = _constants.size() - 1;
			function fun = function{ nameIndex,name,type,paraType,isConstant,paraSize,level, instru };
			auto err = analyseParameterClause(fun);
			if (err.has_value())
			{
				return err;
			}
			err = analyseCompoundStatement();
			if (err.has_value())
			{
				return err;
			}
			auto ins = crtInstructions.at(crtInstructions.size() - 1);
			std::string ret = "ret",iret="iret";
			if (ins != ret && ins != iret)
			{
				if (functions.at(functions.size() - 1).type == VOID)
				{
					crtInstructions.push_back(ret);
				}
				else
				{
					crtInstructions.push_back("ipush 0");
					crtInstructions.push_back(iret);
				}
			}
			functions.at(functions.size() - 1).instructions = crtInstructions;
		}
		return {};
	}
	// <函数体>
	std::optional<CompilationError> Analyser::analyseCompoundStatement() {
		auto next = nextToken();
		if (!next.has_value() || next.value().GetType() != LEFTBRACE)
		{
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
		}
		auto err = analyseVariableDeclaration();
		if (err.has_value())
		{
			return err;
		}
		err = analyseStatementSequence();
		if (err.has_value())
		{
			return err;
		}
		next = nextToken();
		if (!next.has_value() || next.value().GetType() != RIGHTBRACE)
		{
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
		}
		return {};
	}
	// <函数参数>
	//<parameter - clause> :: =
	//	'('[<parameter - declaration - list>] ')'
	// done
	std::optional<CompilationError> Analyser::analyseParameterClause(function &fun) {
		auto next = nextToken();
		bool isConst = false;
		// 读取小括号
		if (!next.has_value() || next.value().GetType() != TokenType::LEFT_BRACKET)
		{
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
		}
		while (true)
		{
			next = nextToken();
			// 判断是否为 const 或 int
			if (!next.has_value() || next.value().GetType() != TokenType::CONST
				&& next.value().GetType() != TokenType::INT && next.value().GetType() != TokenType::CHAR)
			{
				unreadToken();
				break;
			}
			// 是const
			if (next.value().GetType()==TokenType::CONST)
			{
				isConst = true;
				next = nextToken();
			}
			// 读取 type-specifier 类型
			if (!next.has_value() || next.value().GetType() != INT && next.value().GetType() != CHAR)
			{
				return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
			}
			auto typeSpecifier = next.value().GetType();
			next = nextToken();
			// 读取 identifier
			if (!next.has_value() || next.value().GetType() != IDENTIFIER) {
				return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
			}
			// 添加参数个数
			// 保存参数类型
			// 加入符号表
			fun.paraSize++;
			if (isConst)
			{
				fun.paraType.push_back(typeSpecifier);
				fun.isConst.push_back(true);
				addVariable(next.value(),true, typeSpecifier);
			}
			else {
				fun.paraType.push_back(typeSpecifier);
				fun.isConst.push_back(false);
				addVariable(next.value(),false, typeSpecifier);
			}
			next = nextToken();
			if (!next.has_value() || next.value().GetType() != COMMA) {
				unreadToken();
				break;
			}
		}
		next = nextToken();
		if (!next.has_value() || next.value().GetType() != TokenType::RIGHT_BRACKET)
		{
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
		}
		functions.emplace_back(fun);
		crtInstructions = functions.at(functions.size() - 1).instructions;
		return {};
	}
	// <语句序列> ::= {<语句>}
	// <语句> :: = <赋值语句> | <输出语句> | <空语句>
	std::optional<CompilationError> Analyser::analyseStatementSequence() {
		while (true)
		{
			// 预读
			auto next = nextToken();
			if (!next.has_value())
				return {};
			auto token = next.value().GetType();
			unreadToken();
			//   { 条件 
			if (token != TokenType::LEFTBRACE &&    // {
				token != TokenType::IF &&			// if
				token != TokenType::WHILE &&		// while
				token != TokenType::RETURN &&		// return
				token != TokenType::PRINT &&			// print
				token != TokenType::SCAN &&			// scan
				token != TokenType::IDENTIFIER &&	// assign or function-call
				token != TokenType::SEMICOLON		// ;
				) {
				return {};
			}
			auto err = analyseStatement();
			if (err.has_value())
			{
				return err;
			}
		}
	}
	// <赋值语句> :: = <标识符>'='<表达式>';'
	// <输出语句> :: = 'print' '(' <表达式> ')' ';'
	// <空语句> :: = ';'
	// 需要补全
	std::optional<CompilationError> Analyser::analyseStatement() {
		// 预读
		auto next = nextToken();
		auto token = next.value().GetType();
		if (!next.has_value())
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
		unreadToken();
		//   { 条件 
		if (token != TokenType::LEFTBRACE &&    // {
			token != TokenType::IF &&			// if
			token != TokenType::WHILE &&		// while
			token != TokenType::RETURN &&		// return
			token != TokenType::PRINT&&			// print
			token != TokenType::SCAN&&			// scan
			token != TokenType::IDENTIFIER&&	// assign or function-call
			token != TokenType::SEMICOLON		// ;
			) {
			return  std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
		}
		std::optional<CompilationError> err;
		switch (next.value().GetType()) {
			// 这里需要你针对不同的预读结果来调用不同的子程序
			// 注意我们没有针对空语句单独声明一个函数，因此可以直接在这里返回
		case TokenType::IDENTIFIER: // assign or function-call
		{
			// 回到 identifier
			next = nextToken();

			next = nextToken();
			if (!next.has_value()|| next.value().GetType() != LEFT_BRACKET && next.value().GetType()!=ASSIGN)
			{
				return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
			}
			unreadToken();
			unreadToken();
			if (next.value().GetType() == ASSIGN)
			{
				err = analyseAssignmentStatement();
			}
			else
			{
				err = analyseFunctionCall();
			}
			if (err.has_value())
			{
				return err;
			}
			break;
		}
		case TokenType::LEFTBRACE: // {     done
		{ 
			next = nextToken();
			err = analyseStatementSequence();
			if (err.has_value())
			{
				return err;
			}
			next = nextToken();
			// 获取 }
			if (!next.has_value() || next.value().GetType() != RIGHTBRACE)
			{
				return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
			}
			break;
		}
		case TokenType::PRINT:		// print
		{
			err = analyseOutputStatement();
			if (err.has_value())
			{
				return err;
			}
			break;
		}
		case TokenType::SCAN:
		{
			err = analyseScanStatement();
			if (err.has_value())
			{
				return err;
			}
			break;
		}
		case TokenType::IF:
		{
			err = analyseConditionStatement();
			if (err.has_value())
			{
				return err;
			}
			break;
		}
		case TokenType::WHILE:
		{
			err = analyseLoopStatement();
			if (err.has_value())
			{
				return err;
			}
			break;
		}
		case TokenType::RETURN:
		{
			err = analyseJumpStatement();
			if (err.has_value())
			{
				return err;
			}
			break;
		}
		case TokenType::SEMICOLON:
		{
			next = nextToken();
			break;
		}
		default:
			return {};
		}
		return {};
	}
	// <跳转语句>
	std::optional<CompilationError> Analyser::analyseJumpStatement() {
		auto next = nextToken();
		TokenType type = functions.at(functions.size() - 1).type;
		// 读取return
		if (!next.has_value() || next.value().GetType() != RETURN )
		{
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
		}
		// void 类型无返回值
		function fun = getFunction(crtFuntion);
		if (fun.type == VOID)
		{
			next = nextToken();
			// 读取 ;
			if (!next.has_value()||next.value().GetType() != SEMICOLON)
			{
				return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
			}
			crtInstructions.emplace_back("ret");
			return {};
		}
		TokenType exprType;
		auto err = analyseExpression(exprType);
		if (err.has_value())
		{
			return err;
		}
		next = nextToken();
		if (!next.has_value() || next.value().GetType() != SEMICOLON)
		{
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
		}
		if (fun.type == CHAR && exprType == INT)
		{
			crtInstructions.emplace_back("i2c");
		}
		crtInstructions.emplace_back("iret");
		return {};
	}
	// <循环语句>
	std::optional<CompilationError> Analyser::analyseLoopStatement() {
		std::stringstream ss;
		auto next = nextToken();
		auto token = next.value().GetType();
		switch (token)
		{
		//'while' '(' <condition> ')' <statement>
		case WHILE: {
			// 保存第一次跳转的指令的地址
			int jmpBack = crtInstructions.size();;
			int jmpOut;

			// 读取 (
			next = nextToken();
			if(!next.has_value() || next.value().GetType() != TokenType::LEFT_BRACKET)
				return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
			// 读取 condition
			auto err = analyseCondition();
			if (err.has_value())
			{
				return err;
			}
			
			// 记录condition后的跳转指令的地址
			jmpOut = crtInstructions.size() - 1;
			
			// 读取 )
			next = nextToken();
			if (!next.has_value() || next.value().GetType() != TokenType::RIGHT_BRACKET)
				return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);

			// 读取statement
			err = analyseStatement();
			if (err.has_value())return err;

			// 跳转回到condition前面
			std::string tmp = "jmp ";
			ss << tmp << jmpBack;
			tmp = ss.str();
			crtInstructions.push_back(tmp);

			ss.str("");
			ss << crtInstructions[jmpOut] << crtInstructions.size();
			tmp = ss.str();
			crtInstructions[jmpOut] = tmp;
			break;
		}
		case DO: {

			break;
		}
		default:
			break;
		}
		return {};
	}

	// <表达式> ::= <项>{<加法型运算符><项>}
	std::optional<CompilationError> Analyser::analyseExpression(TokenType &myType) {
		// <项>
		TokenType tmp;
		auto err = analyseItem(myType);
		if (err.has_value())
			return err;

		// {<加法型运算符><项>}
		while (true) {
			// 预读
			auto next = nextToken();
			if (!next.has_value())
				return {};
			auto type = next.value().GetType();
			if (type != TokenType::PLUS_SIGN && type != TokenType::MINUS_SIGN) {
				unreadToken();
				return {};
			}

			// <项>
			err = analyseItem(tmp);
			if (err.has_value())
				return err;
			
			myType = INT;

			// 根据结果生成指令
			if (type == TokenType::PLUS_SIGN)
				crtInstructions.emplace_back("iadd");
				//_instructions.emplace_back(Operation::ADD, 0);
			else if (type == TokenType::MINUS_SIGN)
				crtInstructions.emplace_back("isub");
				//_instructions.emplace_back(Operation::SUB, 0);
			if (tmp == INT)
			{
				type = INT;
			}
		}
		return {};
	}

	// <赋值语句> ::= <标识符>'='<表达式>';'
	// 需要补全
	
	std::optional<CompilationError> Analyser::analyseAssignmentStatement() {
		// 这里除了语法分析以外还要留意
		// 需要生成指令吗？
		
		//读入标识符
		auto next = nextToken();
		auto token = next.value();
		// 标识符声明过吗？
		if (!isDeclared(token.GetValueString()))
		{
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNotDeclared);
		}
		// 标识符是常量吗？
		if (isConstant(token.GetValueString()))
		{
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrAssignToConstant);
		}
		// 读取该标识符的地址
		int index = getIndex(token.GetValueString());
		
		if (isDeclaredLocal(token.GetValueString()))
		{
			std::stringstream ss;
			std::string ins = "loada 0,";
			ss << ins << index;
			ins = ss.str();
			crtInstructions.emplace_back(ins);
		}
		else
		{
			std::stringstream ss;
			std::string ins = "loada 1,";
			ss << ins << index;
			ins = ss.str();
			crtInstructions.emplace_back(ins);
		}
		next = nextToken();
		// =
		if (!next.has_value() || next.value().GetType() != TokenType::ASSIGN)
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
		//表达式
		TokenType exprType;
		auto err = analyseExpression(exprType);
		if (err.has_value())
		{
			return err;
		}

		int tableIndex = -1;
		for (int i = symbols.size() - 1; i >= 0; i--)
		{
			if (token.GetValueString() == symbols[i].name && symbols[i].func == functions.at(functions.size() - 1).name) {
				tableIndex = i;
				break;
			}
		}
		if (tableIndex == -1)
		{
			for (int i = 0; i < symbols.size(); i++)
			{
				if (token.GetValueString() == symbols[i].name && symbols[i].func == "") {
					tableIndex = i;
					break;
				}
			}
		}
		TokenType tokentype = symbols[tableIndex].type;
		if (exprType == INT && tokentype == CHAR)
		{
			crtInstructions.emplace_back("i2c");
		}
		// 保存修改
		crtInstructions.emplace_back("istore");
		// _instructions.emplace_back(Operation::STO, getIndex(token.GetValueString()));
		//;
		next = nextToken();
		if (!next.has_value() || next.value().GetType() != TokenType::SEMICOLON)
		{
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoSemicolon);
		}
		return {};
	}
	
	// <输出语句> ::= 'print' '(' <表达式>{, <expression> } ')' ';'
	// done
	std::optional<CompilationError> Analyser::analyseOutputStatement() {
		// 如果之前 <语句序列> 的实现正确，这里第一个 next 一定是 TokenType::PRINT
		auto next = nextToken();

		// '('
		next = nextToken();
		if (!next.has_value() || next.value().GetType() != TokenType::LEFT_BRACKET)
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidPrint);

		//// <表达式>
		//auto err = analyseExpression();
		//if (err.has_value())
		//	return err;
		while (true)
		{
			next = nextToken();
			if (!next.has_value())
				return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidPrint);
			if (next.value().GetType() == TokenType::STRING_VALUE)
			{
				addConstant(next.value().GetValueString());
				std::string str;
				std::stringstream ss;
				ss << "loadc ";
				ss << (_constants.size() - 1);
				str = ss.str();
				crtInstructions.emplace_back(str);
				crtInstructions.emplace_back("sprint");
			}
			else{
				unreadToken();
				TokenType exprType;
				auto err = analyseExpression(exprType);
				if (err.has_value())
					return err;
				if (exprType == INT)
				{
					crtInstructions.emplace_back("iprint");
				}
				else if (exprType == CHAR)
				{
					crtInstructions.emplace_back("cprint");
				}
				else{
					return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidPrint);
				}
			}
			next = nextToken();
			if (!next.has_value())
				return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidPrint);
			if (next.value().GetType() == COMMA){
				crtInstructions.emplace_back("bipush 32");
				crtInstructions.emplace_back("cprint");
				continue;
			}
			else
			{
				unreadToken();
				break;
			}
		}

		// ')'
		next = nextToken();
		if (!next.has_value() || next.value().GetType() != TokenType::RIGHT_BRACKET)
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidPrint);

		// ';'
		next = nextToken();
		if (!next.has_value() || next.value().GetType() != TokenType::SEMICOLON)
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoSemicolon);

		crtInstructions.emplace_back("bipush 10");
		crtInstructions.emplace_back("cprint");
		// 生成相应的指令 WRT
		//_instructions.emplace_back(Operation::WRT, 0);
		return {};
	}
	// <输入语句>
	// 'scan' '(' <identifier> ')' ';'
	std::optional<CompilationError> Analyser::analyseScanStatement() {
		auto next = nextToken();
		// 读取 scan
		if (!next.has_value() || next.value().GetType() != SCAN)
		{
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidPrint);
		}
		next = nextToken();
		// 读取 （
		if (!next.has_value() || next.value().GetType() != LEFT_BRACKET)
		{
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidPrint);
		}
		next = nextToken();
		// 读取 indentifier
		if (!next.has_value() || next.value().GetType() != IDENTIFIER)
		{
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidPrint);
		}
		auto token = next.value();
		// 标识符声明过吗？
		if (!isDeclared(token.GetValueString()))
		{
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNotDeclared);
		}
		// 标识符是常量吗？
		if (isConstant(token.GetValueString()))
		{
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrAssignToConstant);
		}
		next = nextToken();
		// 读取 )
		if (!next.has_value() || next.value().GetType() != RIGHT_BRACKET)
		{
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidPrint);
		}
		next = nextToken();
		// 读取 ;
		if (!next.has_value() || next.value().GetType() != SEMICOLON)
		{
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidPrint);
		}
		int index = getIndex(token.GetValueString());
		if (isDeclaredLocal(token.GetValueString()))
		{
			std::stringstream ss;
			std::string ins = "loada 0,";
			ss << ins << index;
			ins = ss.str();
			crtInstructions.emplace_back(ins);
		}
		else
		{
			std::stringstream ss;
			std::string ins = "loada 1,";
			ss << ins << index;
			ins = ss.str();
			crtInstructions.emplace_back(ins);
		}
		crtInstructions.emplace_back("iscan");
		crtInstructions.emplace_back("istore");
		return {};
	}
	// <条件>
	//<condition> :: =
	//	<expression>[<relational - operator><expression>]
	std::optional<CompilationError> Analyser::analyseCondition() {
		// 分析 condition
		TokenType type;
		auto err = analyseExpression(type);
		// 试探 关系符
		auto next = nextToken();
		if (!next.has_value())
		{
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
		}
		// 只有一个表达式
		if (next.value().GetType() != GREATER && next.value().GetType() != SMAllER &&
			next.value().GetType() != NOT_GREATER && next.value().GetType() != NOT_SMALLER &&
			next.value().GetType() != EQUAL && next.value().GetType() != NOT_EQUAL) {
			unreadToken();
			crtInstructions.push_back("je ");
		}
		else
		{
			auto type = next.value().GetType();
			TokenType exprType;
			auto err = analyseExpression(exprType);
			if (err.has_value())
			{
				return err;
			}
			crtInstructions.push_back("isub");
			switch (type)
			{
				case GREATER: {
					crtInstructions.push_back("jle ");
					break;
				}
				case NOT_GREATER: {
					crtInstructions.push_back("jg ");
					break;
				}
				case NOT_EQUAL: {
					crtInstructions.push_back("je ");
					break;
				}
				case EQUAL: {
					crtInstructions.push_back("jne ");
					break;
				}
				case SMAllER: {
					crtInstructions.push_back("jge ");
					break;
				}
				case NOT_SMALLER: {
					crtInstructions.push_back("jl ");
					break;
				}
			}
		}
		return {};
	}
	// <条件语句>
	// 'if' '(' <condition> ')' <statement> ['else' <statement>]
	// to be continued
	std::optional<CompilationError> Analyser::analyseConditionStatement() {
		auto next = nextToken();
		std::stringstream ss;
		int ifJmp, endIfJmp;
		// 读入 IF
		if (!next.has_value() || next.value().GetType() != IF)
		{
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
		}
		next = nextToken();
		// 读入 （
		if (!next.has_value() || next.value().GetType() != LEFT_BRACKET)
		{
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
		}
		auto err = analyseCondition();
		if (err.has_value())
		{
			return err;
		}
		ifJmp = crtInstructions.size() - 1;
		next = nextToken();
		// 读入 )
		if (!next.has_value() || next.value().GetType() != RIGHT_BRACKET)
		{
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
		}
		// 读取 statement 
		err = analyseStatement();
		if (err.has_value())
		{
			return err;
		}
		endIfJmp = crtInstructions.size();
		crtInstructions.push_back("jmp ");
		
		auto tmp = crtInstructions.at(ifJmp);
		ss << tmp << crtInstructions.size();
		tmp = ss.str();
		crtInstructions[ifJmp] = tmp;
		// 尝试 else
		next = nextToken();
		if (!next.has_value() || next.value().GetType() != ELSE)
		{
			unreadToken();
			ss.str("");
			ss << crtInstructions[endIfJmp] << (endIfJmp + 1);
			tmp = ss.str();
			crtInstructions[endIfJmp] = tmp;
			return {};
		}
		// 读取 else 成功，读取statement
		err = analyseStatement();
		if (err.has_value())return err;

		ss.str("");
		ss << crtInstructions[endIfJmp] << crtInstructions.size();
		tmp = ss.str();
		crtInstructions[endIfJmp] = tmp;
		return {};
	}
	// <项> :: = <因子>{ <乘法型运算符><因子> }
	// 需要补全
	std::optional<CompilationError> Analyser::analyseItem(TokenType &mytype) {
		// 可以参考 <表达式> 实现
		auto err = analyseFactor(mytype);
		if (err.has_value())
			return err;
		TokenType tmpType;
		while (true)
		{
			auto next = nextToken();
			if (!next.has_value())
				return {};
			auto type = next.value().GetType();
			if (type != TokenType::MULTIPLICATION_SIGN && type != TokenType::DIVISION_SIGN)
			{
				unreadToken();
				return {};
			}

			//因子
			err = analyseFactor(tmpType);
			if (err.has_value())
				return err;
			mytype = INT;

			// 根据结果生成指令
			if (type == TokenType::MULTIPLICATION_SIGN)
				crtInstructions.emplace_back("imul");
				//_instructions.emplace_back(Operation::MUL, 0);
			else if (type == TokenType::DIVISION_SIGN)
				crtInstructions.emplace_back("idiv");
				//_instructions.emplace_back(Operation::DIV, 0);
		}
		return {};
	}

	// <因子> ::= [<符号>]( <标识符> | <无符号整数> | '('<表达式>')' | <函数调用>)
	// 需要补全
	std::optional<CompilationError> Analyser::analyseFactor(TokenType &type) {
		bool change = false;
		type = VOID;
		//预读 类型看是否需要转换
		while (true)
		{
			auto next = nextToken();
			if (!next.has_value() || next.value().GetType() != TokenType::LEFT_BRACKET) {
				unreadToken();
				break;
			}
			next = nextToken();
			if (!next.has_value() || next.value().GetType() != TokenType::INT
								  && next.value().GetType() != TokenType::CHAR
								  && next.value().GetType() != TokenType::VOID)
			{
				unreadToken();
				unreadToken();
				break;
			}
			if (next.value().GetType() == TokenType::CHAR)
			{
				change == true;
			}
			if (next.value().GetType() == TokenType::VOID)
			{
				return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidInput);
			}
			if (type == VOID)
			{
				type = next.value().GetType();
			}
			next = nextToken();
			if (!next.has_value() || next.value().GetType() != TokenType::RIGHT_BRACKET) {
				return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
			}
		}
		// [<符号>]
		auto next = nextToken();
		auto prefix = 1;
		if (!next.has_value())
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
		if (next.value().GetType() == TokenType::PLUS_SIGN)
			prefix = 1;
		else if (next.value().GetType() == TokenType::MINUS_SIGN) {
			prefix = -1;
			crtInstructions.emplace_back("ipush 0");
			//_instructions.emplace_back(Operation::LIT, 0);
		}
		else
			unreadToken();

		// 预读( <标识符> | <无符号整数> | '('<表达式>')' | <函数调用>)
		next = nextToken();
		if (!next.has_value())
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
		switch (next.value().GetType()) {
			// 这里和 <语句序列> 类似，需要根据预读结果调用不同的子程序
		case TokenType::IDENTIFIER: {
			auto token = next.value();
			auto next = nextToken();
			if (!next.has_value())
			{
				return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
			}
			// 函数调用,需要判断函数类型
			if (next.value().GetType() == TokenType::LEFT_BRACKET)
			{
				unreadToken();
				unreadToken();
				int index = getFunctionIndex(token.GetValueString());
				if (functions.at(index).type != INT && functions.at(index).type != CHAR)
				{
					return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidIdentifier);
				}
				if (type == VOID)
				{
					type = functions.at(index).type;
				}
				auto err = analyseFunctionCall();
				if (err.has_value())
				{
					return err;
				}
				break;
			}
			else
			{
				unreadToken();
			}
			if (!isDeclared(token.GetValueString()))
			{
				return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNotDeclared);
			}
			int index = getIndex(token.GetValueString());
			
			if (type == VOID)
			{
				int tableIndex = -1;
				for (int i = symbols.size() - 1; i >= 0; i--)
				{
					if (token.GetValueString() == symbols[i].name && symbols[i].func == functions.at(functions.size() - 1).name) {
						tableIndex = i;
						break;
					}
				}
				if (tableIndex == -1)
				{
					for (int i = 0; i < symbols.size(); i++)
					{
						if (token.GetValueString() == symbols[i].name && symbols[i].func == "") {
							tableIndex = i;
							break;
						}
					}
				}
				type = symbols[tableIndex].type;
			}
			if (isDeclaredLocal(token.GetValueString()))
			{
				std::stringstream ss;
				std::string ins = "loada 0,";
				ss << ins << index;
				ins = ss.str();
				crtInstructions.emplace_back(ins);
			}
			else
			{
				std::stringstream ss;
				std::string ins = "loada 1,";
				ss << ins << index;
				ins = ss.str();
				crtInstructions.emplace_back(ins);
			}
			crtInstructions.emplace_back("iload");
			//_instructions.emplace_back(Operation::LOD, getIndex(token.GetValueString()));
			break;
		}
		case TokenType::UNSIGNED_INTEGER: {
			int index = getIndex(next.value().GetValueString());
			if (type == VOID)
			{
				type = INT;
			}
			crtInstructions.emplace_back("ipush "+next.value().GetValueString());
			//_instructions.emplace_back(Operation::LIT, std::stoi(next.value().GetValueString()));
			break;
		}
		case TokenType::LEFT_BRACKET: {
			// <表达式>
			TokenType exprType;
			auto err = analyseExpression(exprType);
			if (err.has_value())
				return err;
			if (type == VOID)
			{
				type = exprType;
			}
			// ')'
			next = nextToken();
			if (!next.has_value() || next.value().GetType() != TokenType::RIGHT_BRACKET)
				return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidPrint);

			break;
		}
		case TokenType::CHAR_VALUE:{
			if (type == VOID)
			{
				type = CHAR;
			}
			std::string ins;
			std::stringstream ss;
			auto s = next.value().GetValueString();
			int tmp = s.at(0);
			ss << "bipush ";
			ss << tmp;
			ins = ss.str();
			crtInstructions.emplace_back(ins);
			break;
		}
			// 但是要注意 default 返回的是一个编译错误
		default:
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
		}

		// 取负
		if (prefix == -1)
			crtInstructions.emplace_back("isub");
			//_instructions.emplace_back(Operation::SUB, 0);
		if (change)
		{
			crtInstructions.emplace_back("i2c");
		}
		return {};
	}

	// 函数调用
	// <identifier> '(' [<expression-list>] ')'
	// 判断类型
	std::optional<CompilationError> Analyser::analyseFunctionCall() {
		auto next = nextToken();
		// 读取 标识符
		if (!next.has_value() || next.value().GetType() != IDENTIFIER)
		{
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNeedIdentifier);
		}
		auto token = next.value();
		// 函数存在吗？
		if (!funcExist(next.value().GetValueString()))
		{
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNotDeclared);
		}
		function fun = getFunction(token.GetValueString());
		// 读取 (
		next = nextToken();
		if (!next.has_value() || next.value().GetType() != LEFT_BRACKET)
		{
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
		}
		int paraSize = fun.paraSize,i = 0;
		auto types = fun.paraType;
		if (paraSize > 0)
		{
			while (paraSize--)
			{
				// 读取 表达式
				TokenType exprType;
				auto err = analyseExpression(exprType);
				if (err.has_value())
					return err;
				if (types[i]==CHAR && exprType == INT)
				{
					crtInstructions.emplace_back("i2c");
				}
				// 试探 ,
				next = nextToken();
				if (!next.has_value())
				{
					return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
				}
				if (next.value().GetType() != COMMA)
				{
					unreadToken();
					break;
				}
				i++;
			}
		}
		if (paraSize != 0)
		{
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
		}
		next = nextToken();
		// 读取 )
		if (!next.has_value() || next.value().GetType() != RIGHT_BRACKET)
		{
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
		}
		int index = getFunctionIndex(token.GetValueString());
		std::stringstream ss;
		std::string ins = "call ";
		ss << ins << index;
		ins = ss.str();
		crtInstructions.emplace_back(ins);
		return {};
	}

	std::optional<Token> Analyser::nextToken() {
		if (_offset == _tokens.size())
			return {};
		// 考虑到 _tokens[0..._offset-1] 已经被分析过了
		// 所以我们选择 _tokens[0..._offset-1] 的 EndPos 作为当前位置
		_current_pos = _tokens[_offset].GetEndPos();
		return _tokens[_offset++];
	}

	void Analyser::unreadToken() {
		if (_offset == 0)
			DieAndPrint("analyser unreads token from the begining.");
		_current_pos = _tokens[_offset - 1].GetEndPos();
		_offset--;
	}

	void Analyser::addVariable(const Token& tk,bool isConst,TokenType type) {
		symbol sb = symbol{ tk.GetValueString(),crtFuntion,isConst,type,level };
		symbols.push_back(sb);
	}
	void Analyser::addConstant(std::string s) {
		_constants.push_back(s);
	}
	bool Analyser::funcExist(std::string funcName) {
		for (auto i : functions)
		{
			if (i.name == funcName) {
				return true;
			}
		}
		return false;
	}
	

	// 获得函数
	Analyser::function Analyser::getFunction(std::string s){
		for (auto i : functions)
		{
			if (i.name == s)
				return i;
		}
	}
	// 先判断本地是否存在，是则返回局部变量index 否则返回全局index
	int32_t Analyser::getIndex(const std::string& s) {
		int32_t index = 0;
		if (isDeclaredLocal(s))
		{
			for (auto i : symbols)
			{
				if (i.func != crtFuntion )
				{
					continue;
				}
				else if (i.name != s)
				{
					index++;
				}
				else
				{
					break;
				}
			}
		}
		else
		{
			for (auto i : symbols)
			{
				if (i.func != "")
				{
					continue;
				}
				else if (i.name != s)
				{
					index++;
				}
				else
				{
					break;
				}
			}
		}
		return index;
	}
	// 获得函数index
	int Analyser::getFunctionIndex(std::string s) {
		for (int i  = 0;i < functions.size(); i++)
		{
			if (functions.at(i).name == s)
				return i;
		}
	}
	bool Analyser::isDeclaredLocal(const std::string& s) {
		for (auto i : symbols) {
			if (i.func == crtFuntion && s == i.name)
			{
				return true;
			}
		}
		return false;
	}
	
	bool Analyser::isDeclared(const std::string& s) {
		std::string str = "";
		for (auto i : symbols) {
			if ( s == i.name && (i.func == crtFuntion || i.func==str))
			{
				return true;
			}
		}
		return false;
	}


	bool Analyser::isConstant(const std::string&s) {
		std::string str = "";
		if (isDeclaredLocal(s))
		{
			for (auto i : symbols) {
				if (s == i.name && i.func == crtFuntion && i.isConst)
				{
					return true;
				}
			}
			return false;
		}
		for (auto i : symbols) {
			if (s == i.name &&  i.func == str && i.isConst)
			{
				return true;
			}
		}
		return false;
	}
}