#include "tokenizer/tokenizer.h"

#include <cctype>
#include <sstream>
#include <string.h>

namespace miniplc0 {

	std::pair<std::optional<Token>, std::optional<CompilationError>> Tokenizer::NextToken() {
		if (!_initialized)
			readAll();
		if (_rdr.bad())
			return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(0, 0, ErrorCode::ErrStreamError));
		if (isEOF())
			return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(0, 0, ErrorCode::ErrEOF));
		auto p = nextToken();
		if (p.second.has_value())
			return std::make_pair(p.first, p.second);
		auto err = checkToken(p.first.value());
		if (err.has_value())
			return std::make_pair(p.first, err.value());
		return std::make_pair(p.first, std::optional<CompilationError>());
	}

	std::pair<std::vector<Token>, std::optional<CompilationError>> Tokenizer::AllTokens() {
		std::vector<Token> result;
		while (true) {
			auto p = NextToken();
			if (p.second.has_value()) {
				if (p.second.value().GetCode() == ErrorCode::ErrEOF)
					return std::make_pair(result, std::optional<CompilationError>());
				else
					return std::make_pair(std::vector<Token>(), p.second);
			}
			result.emplace_back(p.first.value());
		}
	}

	// 注意：这里的返回值中 Token 和 CompilationError 只能返回一个，不能同时返回。
	std::pair<std::optional<Token>, std::optional<CompilationError>> Tokenizer::nextToken() {
		// 用于存储已经读到的组成当前token字符
		std::stringstream ss;
		// 分析token的结果，作为此函数的返回值
		std::pair<std::optional<Token>, std::optional<CompilationError>> result;
		// <行号，列号>，表示当前token的第一个字符在源代码中的位置
		std::pair<int64_t, int64_t> pos;
		// 记录当前自动机的状态，进入此函数时是初始状态
		DFAState current_state = DFAState::INITIAL_STATE;
		// 这是一个死循环，除非主动跳出
		// 每一次执行while内的代码，都可能导致状态的变更
		while (true) {
			// 读一个字符，请注意auto推导得出的类型是std::optional<char>
			// 这里其实有两种写法
			// 1. 每次循环前立即读入一个 char
			// 2. 只有在可能会转移的状态读入一个 char
			// 因为我们实现了 unread，为了省事我们选择第一种
			auto current_char = nextChar();
			// 针对当前的状态进行不同的操作
			switch (current_state) {

				// 初始状态
				// 这个 case 我们给出了核心逻辑，但是后面的 case 不用照搬。
			case INITIAL_STATE: {
				// 已经读到了文件尾
				if (!current_char.has_value())
					// 返回一个空的token，和编译错误ErrEOF：遇到了文件尾
					return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(0, 0, ErrEOF));

				// 获取读到的字符的值，注意auto推导出的类型是char
				auto ch = current_char.value();
				// 标记是否读到了不合法的字符，初始化为否
				auto invalid = false;

				// 使用了自己封装的判断字符类型的函数，定义于 tokenizer/utils.hpp
				// see https://en.cppreference.com/w/cpp/string/byte/isblank
				if (miniplc0::isspace(ch)) // 读到的字符是空白字符（空格、换行、制表符等）
					current_state = DFAState::INITIAL_STATE; // 保留当前状态为初始状态，此处直接break也是可以的
				else if (!miniplc0::isvalid(ch)) // control codes and backspace
					invalid = true;
				else if (miniplc0::isdigit(ch)) // 读到的字符是数字
					current_state = DFAState::UNSIGNED_INTEGER_STATE; // 切换到无符号整数的状态
				else if (miniplc0::isalpha(ch)) // 读到的字符是英文字母
					current_state = DFAState::IDENTIFIER_STATE; // 切换到标识符的状态
				else {
					switch (ch) {
					case '(':
						current_state = DFAState::LEFTBRACKET_STATE;
						break;
					case ')':
						current_state = DFAState::RIGHTBRACKET_STATE;
						break;
					case '+':
						// 请填空：切换到加号的状态
						current_state = DFAState::PLUS_SIGN_STATE;
						break;
					case '-':
						// 请填空：切换到减号的状态
						current_state = DFAState::MINUS_SIGN_STATE;
						break;
					case '*':
						// 请填空：切换状态
						current_state = DFAState::MULTIPLICATION_SIGN_STATE;
						break;
					case '/':{
						// 判断是否为注释
						auto next = nextChar().value();
						if (next == '/')
						{
							current_state = DFAState::DIV_ANNOTATION_STATE;
						}
						else if (next == '*')
						{
							current_state = DFAState::STAR_ANNOTATION_STATE;
						}
						else
						{
							unreadLast();
							current_state = DFAState::DIVISION_SIGN_STATE;
						}
						break;
					}
					case '=': // 如果读到的字符是`=`，则切换到等于号的状态
					{
						auto nxt = nextChar().value();
						if (nxt == '=')
						{
							current_state = DFAState::EQUAL_STATE;
						}
						else {
							unreadLast();
							current_state = DFAState::ASSIGN_STATE;
						}
						break;
					}

					case '<':{
						auto nxt = nextChar().value();
						if (nxt == '=')
						{
							current_state = DFAState::NOT_GREATER_STATE;
						}
						else {
							unreadLast();
							current_state = DFAState::SMAllER_STATE;
						}
						break;
					}
					case '>': {
						auto nxt = nextChar().value();
						if (nxt == '=')
						{
							current_state = DFAState::NOT_SMALLER_STATE;
						}
						else {
							unreadLast();
							current_state = DFAState::GREATER_STATE;
						}
						break;
					}
					case '!': {
						auto nxt = nextChar().value();
						if (nxt == '=')
						{
							current_state = DFAState::NOT_EQUAL_STAET;
						}
						else{
							return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(currentPos(), ErrorCode::ErrInvalidInput));
						}
						break;
					}

					///// 请填空：
					///// 对于其他的可接受字符
					///// 切换到对应的状态
					case ';':
						current_state = DFAState::SEMICOLON_STATE;
						break;
					// 不接受的字符导致的不合法的状态
					case '{':
						current_state = DFAState::LEFTBRACE_STATE;
						break;
					case '}':
						current_state = DFAState::RIGHTBRACE_STATE;
						break;
					case '\'':
						current_state = DFAState::CHAR_STATE;
						break;
					case '"':
						current_state = DFAState::STRING_STATE;
						break;
						
					case ',':
						current_state = DFAState::COMMA_STATE;
						break;
					default:
						invalid = true;
						break;
					}
				}
				// 如果读到的字符导致了状态的转移，说明它是一个token的第一个字符
				if (current_state != DFAState::INITIAL_STATE)
					pos = previousPos(); // 记录该字符的的位置为token的开始位置
				// 读到了不合法的字符
				if (invalid) {
					// 回退这个字符
					unreadLast();
					// 返回编译错误：非法的输入
					return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos, ErrorCode::ErrInvalidInput));
				}
				// 如果读到的字符导致了状态的转移，说明它是一个token的第一个字符
				if (current_state != DFAState::INITIAL_STATE && current_state != DFAState::STRING_STATE) // ignore white spaces
					ss << ch; // 存储读到的字符
				break;
			}

								// 当前状态是无符号整数
			case UNSIGNED_INTEGER_STATE: {
				// 请填空：
				// 如果当前已经读到了文件尾，则解析已经读到的字符串为整数
				unsigned int val;
				if (!current_char.has_value()){
					std::string s = ss.str();
					//不允许前导0
					if (s.length() > 1 && s.at(0) == '0' && s.at(1) != 'x' && s.at(1) != 'X')
					{
						return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos, ErrorCode::ErrInvalidInput));
					}
					//16进制输出
					else if (s.length() > 1 && s.at(0) == '0')
					{
						std::sscanf(s.c_str(), "%x", &val);
					}
					//10进制输出
					else {
						ss >> val;
					}

					if (val > 2147483648)
					{
						return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos,  ErrorCode::ErrIntegerOverflow));
					}
					return std::make_pair(std::make_optional<Token>(TokenType::UNSIGNED_INTEGER, (int)val, pos, currentPos()), std::optional<CompilationError>());
				}

				// 解析成功则返回无符号整数类型的token，否则返回编译错误
				// 获取读到的字符的值，注意auto推导出的类型是char
				auto ch = current_char.value();
				// 标记是否读到了不合法的字符，初始化为否
				auto invalid = false;
				
				// 如果读到的字符是数字，则存储读到的字符
				if (miniplc0::isdigit(ch))
					ss << ch;
				// 如果读到的是字母，则判断是否为16进制
				else if (miniplc0::isalpha(ch)) {
					std::string tmp = ss.str();
					ss << ch;
					if (tmp.length()==1 && tmp.at(0) == '0' && (ch == 'x'||ch == 'X'))
					{
						current_state = DFAState::UNSIGNED_INTEGER_STATE;
					}
					else if(tmp.length() > 1 && tmp.at(0) == '0' && (tmp.at(1) == 'x'|| tmp.at(1) == 'X'))
					{
						if ('a'<= ch && ch <= 'f' || 'A'<=ch && ch <= 'F' )
						{
							current_state = DFAState::UNSIGNED_INTEGER_STATE;
						}
						else
							current_state = DFAState::IDENTIFIER_STATE;
					}
					else
					{
						current_state = DFAState::IDENTIFIER_STATE;
					}
				}
				// 如果读到的字符不是上述情况之一，则回退读到的字符，并解析已经读到的字符串为整数
				//     解析成功则返回无符号整数类型的token，否则返回编译错误
				else {
					unreadLast();
					ss >> val;
					std::string s = ss.str();
					//不允许前导0
					if (s.length() > 1 && s.at(0) == '0' && s.at(1) != 'x' && s.at(1) != 'X')
					{
						return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos, ErrorCode::ErrInvalidInput));
					}
					//16进制输出
					else if (s.length() > 1 && s.at(0) == '0')
					{
						std::sscanf(s.c_str(), "%x", &val);
					}
					//10进制输出
					else {
						ss >> val;
					}

					if (val > 2147483648)
					{
						return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos, ErrorCode::ErrIntegerOverflow));
					}
					return std::make_pair(std::make_optional<Token>(TokenType::UNSIGNED_INTEGER, (int)val, pos, currentPos()), std::optional<CompilationError>());
				}
				break;
			}
			case IDENTIFIER_STATE: {
				// 请填空：
				// 如果当前已经读到了文件尾，则解析已经读到的字符串
				std::string str, keys[] = { "const" ,"void","int","char","double",
											"struct","if","else","switch","case",
											"default","while","for","do","return",
											"break","continue","print","scan"};
				//     如果解析结果是关键字，那么返回对应关键字的token，否则返回标识符的token
				if (!current_char.has_value()) {
					str = ss.str();
					int i;
					for (i = 0;i < 19;i++)
					{
						if (str == keys[i])
							break;
					}
					if (i < 19)
					{
						return std::make_pair(std::make_optional<Token>(TokenType(i+2), str, pos, currentPos()), std::optional<CompilationError>());
					}
					else
						return std::make_pair(std::make_optional<Token>(TokenType::IDENTIFIER, str, pos, currentPos()), std::optional<CompilationError>());
				}
				auto ch = current_char.value();
				// 如果读到的是字符或字母，则存储读到的字符
				if (miniplc0::isalpha(ch)||miniplc0::isdigit(ch))
				{
					ss << ch;
				}
				// 如果读到的字符不是上述情况之一，则回退读到的字符，并解析已经读到的字符串
				//     如果解析结果是关键字，那么返回对应关键字的token，否则返回标识符的token
				else
				{
					unreadLast();
					str = ss.str();
					int i;
					for (i = 0; i < 19; i++)
					{
						if (str == keys[i])
							break;
					}
					if (i < 19)
					{
						return std::make_pair(std::make_optional<Token>(TokenType(i + 2), str, pos, currentPos()), std::optional<CompilationError>());
					}
					else
						return std::make_pair(std::make_optional<Token>(TokenType::IDENTIFIER, str, pos, currentPos()), std::optional<CompilationError>());
				}
				break;
			}

								   // 如果当前状态是加号
			case PLUS_SIGN_STATE: {
				// 请思考这里为什么要回退，在其他地方会不会需要
				unreadLast(); // Yes, we unread last char even if it's an EOF.
				return std::make_pair(std::make_optional<Token>(TokenType::PLUS_SIGN, '+', pos, currentPos()), std::optional<CompilationError>());
			}
								  // 当前状态为减号的状态
			case MINUS_SIGN_STATE: {
				// 请填空：回退，并返回减号token
				unreadLast(); // Yes, we unread last char even if it's an EOF.
				return std::make_pair(std::make_optional<Token>(TokenType::MINUS_SIGN, '-', pos, currentPos()), std::optional<CompilationError>());
			}
								 // 如果当前状态为乘号
			case MULTIPLICATION_SIGN_STATE: {
				unreadLast(); // Yes, we unread last char even if it's an EOF.
				return std::make_pair(std::make_optional<Token>(TokenType::MULTIPLICATION_SIGN, '*', pos, currentPos()), std::optional<CompilationError>());
			}					   
									// 如果当前状态为除号
			case DIVISION_SIGN_STATE: {
				unreadLast(); // Yes, we unread last char even if it's an EOF.
				return std::make_pair(std::make_optional<Token>(TokenType::DIVISION_SIGN, '/', pos, currentPos()), std::optional<CompilationError>());
			}					   
									// 如果当前状态为=
			case ASSIGN_STATE: {
				unreadLast(); // Yes, we unread last char even if it's an EOF.
				return std::make_pair(std::make_optional<Token>(TokenType::ASSIGN, '=', pos, currentPos()), std::optional<CompilationError>());
			}
									// 如果当前状态为==
			case EQUAL_STATE: {
				unreadLast();
				std::string str = "==";
				return std::make_pair(std::make_optional<Token>(TokenType::EQUAL, str, pos, currentPos()), std::optional<CompilationError>());
			}

			case NOT_EQUAL_STAET: {
				unreadLast();
				std::string str = "!=";
				return std::make_pair(std::make_optional<Token>(TokenType::NOT_EQUAL, str, pos, currentPos()), std::optional<CompilationError>());
			}
									// 如果当前状态为;
			case SEMICOLON_STATE: {
				unreadLast(); // Yes, we unread last char even if it's an EOF.
				return std::make_pair(std::make_optional<Token>(TokenType::SEMICOLON, ';', pos, currentPos()), std::optional<CompilationError>());
			}					  
									// 如果当前状态为)
			case LEFTBRACKET_STATE: {
				unreadLast(); // Yes, we unread last char even if it's an EOF.
				return std::make_pair(std::make_optional<Token>(TokenType::LEFT_BRACKET, '(', pos, currentPos()), std::optional<CompilationError>());
			}
									// 如果当前状态为(
			case RIGHTBRACKET_STATE: {
				unreadLast(); // Yes, we unread last char even if it's an EOF.
				return std::make_pair(std::make_optional<Token>(TokenType::RIGHT_BRACKET, ')', pos, currentPos()), std::optional<CompilationError>());
			}
								    // {
			case LEFTBRACE_STATE: {
				unreadLast(); // Yes, we unread last char even if it's an EOF.
				return std::make_pair(std::make_optional<Token>(TokenType::LEFTBRACE, '{', pos, currentPos()), std::optional<CompilationError>());
			}
									// }
			case RIGHTBRACE_STATE: {
				unreadLast(); // Yes, we unread last char even if it's an EOF.
				return std::make_pair(std::make_optional<Token>(TokenType::RIGHTBRACE, '}', pos, currentPos()), std::optional<CompilationError>());
			}
									// <
			case SMAllER_STATE: {
				unreadLast(); // Yes, we unread last char even if it's an EOF.
				return std::make_pair(std::make_optional<Token>(TokenType::SMAllER, '<', pos, currentPos()), std::optional<CompilationError>());
			}
			case NOT_SMALLER_STATE: {
				unreadLast(); // Yes, we unread last char even if it's an EOF.
				std::string str = ">=";
				return std::make_pair(std::make_optional<Token>(TokenType::NOT_SMALLER, str, pos, currentPos()), std::optional<CompilationError>());
			}
			case GREATER_STATE: {
				unreadLast(); // Yes, we unread last char even if it's an EOF.
				return std::make_pair(std::make_optional<Token>(TokenType::GREATER, '>', pos, currentPos()), std::optional<CompilationError>());
			}
			case NOT_GREATER_STATE: {
				unreadLast(); // Yes, we unread last char even if it's an EOF.
				std::string str = "<=";
				return std::make_pair(std::make_optional<Token>(TokenType::NOT_GREATER, str, pos, currentPos()), std::optional<CompilationError>());
			}
								  // ,
			case COMMA_STATE: {
				unreadLast(); // Yes, we unread last char even if it's an EOF.
				return std::make_pair(std::make_optional<Token>(TokenType::COMMA, ',', pos, currentPos()), std::optional<CompilationError>());
			}
			case DIV_ANNOTATION_STATE: {
				if (!current_char.has_value())
				{
					return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos, ErrEOF));
				}
				auto ch = current_char.value();
				while (ch != 0x0A && ch != 0x0D)
				{
					auto next = nextChar();
					if (!next.has_value())
					{
						return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos, ErrEOF));
					}
					ch = next.value();
				}
				ss.str("");
				current_state = INITIAL_STATE;
				break;
			}
			case STAR_ANNOTATION_STATE: {
				if (!current_char.has_value())
				{
					return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos, ErrIncompleteExpression));
				}
				auto ch = current_char.value();
				while (true)
				{
					if (ch == '*')
					{
						auto next = nextChar();
						if (!next.has_value())
						{
							return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos, ErrIncompleteExpression));
						}
						ch = next.value();
						if (ch == '/')
						{
							break;
						}
						else
						{
							unreadLast();
						}
					}
					auto next = nextChar();
					if (!next.has_value())
					{
						return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos, ErrIncompleteExpression));
					}
					ch = next.value();
				}
				ss.str("");
				current_state = INITIAL_STATE;
				break;
			}
			
			case CHAR_STATE: {
				if (!current_char.has_value())
				{
					return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos, ErrorCode::ErrInvalidInput));
				}
				auto ch = current_char.value();
				if (ch != '\\')// 可接受字符中除了 '
				{
					current_char = nextChar();
					if (!current_char.has_value()|| current_char.value()!= '\'')
					{
						return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos, ErrorCode::ErrInvalidInput));
					}
					else
					{
						if (acceptable(ch)||ch == '"')
						{
							return std::make_pair(std::make_optional<Token>(TokenType::CHAR_VALUE, ch, pos, currentPos()), std::optional<CompilationError>());
						}
						else
						{
							return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos, ErrorCode::ErrInvalidInput));
						}
					}
				}
				else
				{
					auto next = nextChar();
					if (!next.has_value())
					{
						return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos, ErrorCode::ErrInvalidInput));
					}
					ch = next.value();
					switch (ch)
					{
					case '\\':
					{
						ch = '\\';
						break;
					}
					case '\'': {
						ch = '\'';
						break;
					}
					case '"': {
						ch = '"';
						break;
					}
					case 'n':
					{
						ch = '\n';
						break;
					}
					case 'r': {
						ch = '\r';
						break;
					}
					case 't': {
						ch = '\t';
						break;
					}
					case 'x': {
						std::string s = "1234567890abcdefABCDEF";
						ss.str("");
						ss << "0x";
						for(int i = 0; i < 2;i++){
							auto next = nextChar();
							if (!next.has_value())
							{
								return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos, ErrorCode::ErrInvalidInput));
							}
							auto c = next.value();
							if (s.find(c) == -1)
							{
								return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos, ErrorCode::ErrInvalidInput));
							}
							std::string s = "1";
							s.at(0) = c;
							ss << c;
						}
						unsigned int tmp;
						s = ss.str();
						std::sscanf(s.c_str(), "%x", &tmp);
						ch = tmp;
						break;
					}
					default:
						return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos, ErrorCode::ErrInvalidInput));
					}
					next = nextChar();
					if (!next.has_value() || next.value() != '\'')
					{
						return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos, ErrorCode::ErrInvalidInput));
					}
					return std::make_pair(std::make_optional<Token>(TokenType::CHAR_VALUE, ch, pos, currentPos()), std::optional<CompilationError>());
				}
			}
			case STRING_STATE: {
				if (!current_char.has_value())
				{
					return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos, ErrorCode::ErrInvalidInput));
				}
				auto ch = current_char.value();
				if (ch == '"')
				{
					return std::make_pair(std::make_optional<Token>(TokenType::STRING_VALUE, ss.str(), pos, currentPos()), std::optional<CompilationError>());
				}
				else if (ch != '\\')
				{
					
					if (acceptable(ch) || ch == '\'')
					{
						ss << ch;
					}
					else
					{
						return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos, ErrorCode::ErrInvalidInput));
					}

				}
				else
				{
					auto next = nextChar();
					if (!next.has_value())
					{
						return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos, ErrorCode::ErrInvalidInput));
					}
					ch = next.value();
					switch (ch)
					{
					case '\\':
					{
						ch = '\\';
						break;
					}
					case '\'': {
						ch = '\'';
						break;
					}
					case '"': {
						ch = '"';
						break;
					}
					case 'n':
					{
						ch = '\n';
						break;
					}
					case 'r': {
						ch = '\r';
						break;
					}
					case 't': {
						ch = '\t';
						break;
					}
					case 'x': {
						std::string s = "1234567890abcdefABCDEF";
						std::stringstream tmpss;
						tmpss << "0x";
						for (int i = 0; i < 2; i++) {
							auto next = nextChar();
							if (!next.has_value())
							{
								return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos, ErrorCode::ErrInvalidInput));
							}
							auto c = next.value();
							if (s.find(c) == -1)
							{
								return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos, ErrorCode::ErrInvalidInput));
							}
							std::string s = "1";
							s.at(0) = c;
							tmpss << c;
						}
						unsigned int tmp;
						s = tmpss.str();
						std::sscanf(s.c_str(), "%x", &tmp);
						ch = tmp;
						break;
					}
					default:
						return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos, ErrorCode::ErrInvalidInput));
					}
					ss << "\\x";
					int tmp = ch / 16;
					if (tmp < 10)
						ss << tmp;
					else
						ss << (char)('a' + (tmp - 10));
					tmp = ch % 16;
					if (tmp < 10)
						ss << tmp;
					else
						ss << (char)('a' + (tmp - 10));
				}
				break; 
			}
			

								   // 请填空：
								   // 对于其他的合法状态，进行合适的操作
								   // 比如进行解析、返回token、返回编译错误
								   // 预料之外的状态，如果执行到了这里，说明程序异常
			default:
				DieAndPrint("unhandled state.");
				break;
			}
		}
		// 预料之外的状态，如果执行到了这里，说明程序异常
		return std::make_pair(std::optional<Token>(), std::optional<CompilationError>());
	}

	std::optional<CompilationError> Tokenizer::checkToken(const Token& t) {
		switch (t.GetType()) {
			case IDENTIFIER: {
				auto val = t.GetValueString();
				if (miniplc0::isdigit(val[0]))
					return std::make_optional<CompilationError>(t.GetStartPos().first, t.GetStartPos().second, ErrorCode::ErrInvalidIdentifier);
				break;
			}
		default:
			break;
		}
		return {};
	}

	void Tokenizer::readAll() {
		if (_initialized)
			return;
		for (std::string tp; std::getline(_rdr, tp);)
			_lines_buffer.emplace_back(std::move(tp + "\n"));
		_initialized = true;
		_ptr = std::make_pair<int64_t, int64_t>(0, 0);
		return;
	}

	// Note: We allow this function to return a postion which is out of bound according to the design like std::vector::end().
	std::pair<uint64_t, uint64_t> Tokenizer::nextPos() {
		if (_ptr.first >= _lines_buffer.size())
			DieAndPrint("advance after EOF");
		if (_ptr.second == _lines_buffer[_ptr.first].size() - 1)
			return std::make_pair(_ptr.first + 1, 0);
		else
			return std::make_pair(_ptr.first, _ptr.second + 1);
	}

	std::pair<uint64_t, uint64_t> Tokenizer::currentPos() {
		return _ptr;
	}

	std::pair<uint64_t, uint64_t> Tokenizer::previousPos() {
		if (_ptr.first == 0 && _ptr.second == 0)
			DieAndPrint("previous position from beginning");
		if (_ptr.second == 0)
			return std::make_pair(_ptr.first - 1, _lines_buffer[_ptr.first - 1].size() - 1);
		else
			return std::make_pair(_ptr.first, _ptr.second - 1);
	}

	std::optional<char> Tokenizer::nextChar() {
		if (isEOF())
			return {}; // EOF
		auto result = _lines_buffer[_ptr.first][_ptr.second];
		_ptr = nextPos();
		return result;
	}

	bool Tokenizer::acceptable(char ch) {
		if (isalnum(ch) || isalpha(ch))
			return true;
		
		char* chs = "_ ( ) [ ] { } < = > . , : ; ! ? + - * / % ^ & | ~ ` $ # @";
		
		for (int i = 0; i < strlen(chs); i++)
		{
			if (chs[i] == ch)
			{
				return true;
			}
		}
		return false;
	}

	bool Tokenizer::isEOF() {
		return _ptr.first >= _lines_buffer.size();
	}

	// Note: Is it evil to unread a buffer?
	void Tokenizer::unreadLast() {
		_ptr = previousPos();
	}
}