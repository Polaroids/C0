#include "argparse.hpp"
#include "fmt/core.h"

#include "src/vm.h"
#include "src/file.h"
#include "src/exception.h"
#include "src/util/print.hpp"

#include "tokenizer/tokenizer.h"
#include "analyser/analyser.h"
#include "fmts.hpp"
#include <stdlib.h>
#include <iostream>
#include <fstream>


#include <memory>
#include <string>
#include <exception>

std::vector<miniplc0::Token> _tokenize(std::istream& input) {
	miniplc0::Tokenizer tkz(input);
	auto p = tkz.AllTokens();
	if (p.second.has_value()) {
		fmt::print(stderr, "Tokenization error: {}\n", p.second.value());
		exit(2);
	}
	return p.first;
}

void Tokenize(std::istream& input, std::ostream& output) {
	auto v = _tokenize(input);
	for (auto& it : v)
		output << fmt::format("{}\n", it);
	return;
}

void Analyse(std::istream& input, std::ostream& output){
	auto tks = _tokenize(input);
	miniplc0::Analyser analyser(tks);
	auto p = analyser.Analyse();
	if (p.second.has_value()) {
		fmt::print(stderr, "Syntactic analysis error: {}\n", p.second.value());
		exit(2);
	}

	/*auto v = p.first;
	for (auto& it : v)
		output << fmt::format("{}\n", it);*/
	output << ".constants:\n";
	int i = 0;
	for (auto cons : analyser._constants) {
		output << i << " S " << '"' << cons << '"' << '\n';
		i++;
	}
	output << ".start:\n";
	auto f = analyser.start;
	for (int i = 0; i < f.size(); i++)
	{
		output << i << " " << f.at(i) << std::endl;
	}
	output << ".functions:\n";
	i = 0;
	for (auto fun : analyser.functions) {
		output << i << " " << fun.nameIndex << " " << fun.paraSize << " " << 1 << '\n';
		i++;
	}
	for (int i = 0; i < analyser.functions.size(); i++)
	{
		output << ".F" << i << ":\n";
		auto ins = analyser.functions.at(i).instructions;
		for (int j = 0; j < ins.size(); j++)
		{
			output << j << " " << ins.at(j) << std::endl;
		}
	}
	return;
}

void disassemble_binary(std::ifstream* in, std::ostream* out) {
	try {
		File f = File::parse_file_binary(*in);
		f.output_text(*out);
	}
	catch (const std::exception & e) {
		println(std::cerr, e.what());
	}
}

void assemble_text(std::ifstream* in, std::ofstream* out, bool run = false) {
	try {
		File f = File::parse_file_text(*in);
		// f.output_text(std::cout);
		f.output_binary(*out);
		if (run) {
			auto avm = std::move(vm::VM::make_vm(f));
			avm->start();
		}
	}
	catch (const std::exception & e) {
		println(std::cerr, e.what());
	}
}

void execute(std::ifstream* in, std::ostream* out) {
	try {
		File f = File::parse_file_binary(*in);
		auto avm = std::move(vm::VM::make_vm(f));
		avm->start();
	}
	catch (const std::exception & e) {
		println(std::cerr, e.what());
	}
}

int main(int argc, char** argv) {
	argparse::ArgumentParser program("cc0");
	program.add_argument("input")
		.help("speicify the file to be compiled.");
	program.add_argument("-s")
		.default_value(false)
		.implicit_value(true)
		.help("将输入的 c0 源代码翻译为文本汇编文件");
	program.add_argument("-c")
		.default_value(false)
		.implicit_value(true)
		.help("perform syntactic analysis for the input file.");
	program.add_argument("-o", "--output")
		.required()
		.default_value(std::string("out"))
		.help("输出到指定的文件 file");

	try {
		program.parse_args(argc, argv);
	}
	catch (const std::runtime_error& err) {
		fmt::print(stderr, "{}\n\n", err.what());
		program.print_help();
		exit(2);
	}

	auto input_file = program.get<std::string>("input");
	auto output_file = program.get<std::string>("--output");
	std::istream* input;
	std::ostream* output;
	std::ifstream inf;
	std::ofstream outf;
	if (input_file != "-") {
		inf.open(input_file, std::ios::in);
		if (!inf) {
			fmt::print(stderr, "Fail to open {} for reading.\n", input_file);
			exit(2);
		}
		input = &inf;
	}
	else
		input = &std::cin;
	if (output_file != "-") {
		std::stringstream ss;
		std::string tmp = output_file;
		ss << output_file << ".s0";
		if (program["-c"] == true)
		{
			input_file = ss.str();
			tmp = ss.str();
		}

		outf.open(tmp, std::ios::out | std::ios::trunc);
		if (!outf) {
			fmt::print(stderr, "Fail to open {} for writing.\n", output_file);
			exit(2);
		}
		output = &outf;
	}
	else
		output = &std::cout;
	if (program["-s"] == true && program["-c"] == true) {
		fmt::print(stderr, "You can only perform tokenization or syntactic analysis at one time.");
		exit(2);
	}
	else if (program["-s"] == true) {
		Analyse(*input, *output);
	}
	else if (program["-c"] == true)
	{
		Analyse(*input, *output);
			inf.close();
			outf.close();
		{
			std::ifstream* input;
			std::ostream* output;
			std::ifstream inf;
			std::ofstream outf;

			inf.open(input_file, std::ios::in);
			if (!inf) {
				exit(2);
			}
			input = &inf;
			outf.open(output_file, std::ios::out | std::ios::trunc|std::ios::binary);
			if (!outf) {
				inf.close();
				exit(2);
			}
			output = &outf;
			assemble_text(input, dynamic_cast<std::ofstream*>(output), false);
			inf.close();
			outf.close();
		}
		return 0;
	}
	else {
		fmt::print(stderr, "You must choose tokenization or syntactic analysis.");
		exit(2);
	}
	return 0;
}