#pragma once
#include <vector>
#include "sikfck.h"
#include <memory>

namespace sikfck {

	template <typename TRegister, typename TProgramCounter> class LoopOptimization
	{
	public:
		bool verbose;

		virtual bool TryPerform(const Program<TRegister, TProgramCounter>& input, Program<TRegister, TProgramCounter>& output, TProgramCounter begin, TProgramCounter end)
		{
			return false;
		}

		virtual ~LoopOptimization()
		{

		}
	};

	template <typename TRegister, typename TProgramCounter> class Compiler {
	private:

		std::vector<std::unique_ptr<LoopOptimization<typename TRegister, typename TProgramCounter>>> loopOptimizations;

		class OptimisationInfo
		{
		public:
			int instructionDelta = 0;
			int pointerDelta = 0;
		};

		void PrintOptimizedSection(const Program<TRegister, TProgramCounter>& input, const Program<TRegister, TProgramCounter>& output, TProgramCounter begin, TProgramCounter end, TProgramCounter outBegin, TProgramCounter outEnd)
		{
			TProgramCounter i = begin, j = outBegin;
			while (i<end || j<outEnd)
			{
				if (i < end) {
					std::cerr << "\t" << input.itype[i] << "\t" << std::showpos << static_cast<int>(input.ivalue[i]) << "\t";
				}
				else {
					std::cerr << "\t\t\t";
				}

				if (j < outEnd)
				{
					if (i < end)
					{
						std::cerr << "->";
					}
					std::cerr << "\t" << output.itype[j] << "\t" << std::showpos << static_cast<int>(output.ivalue[j]) << "\t";
				}
				else
				{
					std::cerr << "\t\t\t";
				}
				std::cerr << "\n";
				++i;
				++j;
			}
		}

		OptimisationInfo OptimizeFlat(const Program<TRegister, TProgramCounter>& input, Program<TRegister, TProgramCounter>& output, TProgramCounter begin, TProgramCounter end)
		{
			if (verboseOptimisation)
			{
				std::cerr << std::noshowpos << "Opt Flat Begin (" << begin << ", " << (end - 1) << ")\n";
			}
			OptimisationInfo info;
			TProgramCounter outBegin = output.GetSize();
			TProgramCounter startingOutputSize = output.GetSize();
			InstructionDebug<TRegister> previous(InstructionType::Nop, 0, 0, 0, 0, 0);
			for (TProgramCounter i = begin; i<end; ++i)
			{
				InstructionDebug<TRegister> instruction = input.ReadDebug(i);
				switch (instruction.type)
				{
				case InstructionType::Nop:
					// strip nop
					continue;
				case InstructionType::AddPi:
					info.pointerDelta += 1;
					output.Append(instruction);
					break;
				case InstructionType::AddPd:
					info.pointerDelta -= 1;
					output.Append(instruction);
					break;
				case InstructionType::PtrAdd:
					if (instruction.value == 0)
					{
						continue;
					}
					info.pointerDelta += instruction.value;
					if (previous.type == InstructionType::Add && instruction.value == -1)
					{
						instruction.type = InstructionType::AddPd;
						instruction.value = previous.value;
						instruction.sourceLine = previous.sourceLine;
						instruction.sourceColumn = previous.sourceColumn;
						instruction.sourceBegin = previous.sourceBegin;
						output.Replace(output.GetSize() - 1, instruction);
					}
					else if (previous.type == InstructionType::Add && instruction.value == +1)
					{
						instruction.type = InstructionType::AddPi;
						instruction.value = previous.value;
						instruction.sourceLine = previous.sourceLine;
						instruction.sourceColumn = previous.sourceColumn;
						instruction.sourceBegin = previous.sourceBegin;
						output.Replace(output.GetSize() - 1, instruction);
					}
					else
					{
						output.Append(instruction);
					}
					break;
				default:
					output.Append(instruction);
					break;
				}
				previous = instruction;
			}
			info.instructionDelta = (output.GetSize() - startingOutputSize) - (end - begin);
			TProgramCounter outEnd = output.GetSize();

			// print optimised section
			if (verboseOptimisation)
			{
				PrintOptimizedSection(input, output, begin, end, outBegin, outEnd);
				std::cerr << std::noshowpos << "Opt Flat End (" << begin << ", " << (end - 1) << ")\n";
				std::cerr << "i-delta: " << info.instructionDelta << " p-delta: " << info.pointerDelta << "\n";
			}
			return info;
		}

		OptimisationInfo OptimizeLoop(const Program<TRegister, TProgramCounter>& input, Program<TRegister, TProgramCounter>& output, TProgramCounter begin, TProgramCounter end)
		{
			if (verboseOptimisation)
			{
				std::cerr << std::noshowpos << "Opt Loop Begin (" << begin << ", " << (end - 1) << ")\n";
			}
			TProgramCounter innerBegin = begin + 1;
			TProgramCounter innerEnd = end - 1;

			auto startingOutputSize = output.GetSize();
			auto patternMatched = false;

			for (auto& optimizer : loopOptimizations)
			{
				bool success = optimizer->TryPerform(input, output, begin, end);
				if (success)
				{
					patternMatched = true;
					break;
				}
			}

			if (patternMatched)
			{
				//no need to continue
				OptimisationInfo info;
				info.instructionDelta = (output.GetSize() - startingOutputSize) - (end - begin);
				TProgramCounter outBegin = startingOutputSize;
				TProgramCounter outEnd = output.GetSize();
				for (TProgramCounter i = outBegin; i<outEnd; ++i)
				{
					Instruction<TRegister> instruction = output.Read(i);
					switch (instruction.type)
					{
					case InstructionType::PtrAdd:
						info.pointerDelta += instruction.value;
						break;
					case InstructionType::AddPi:
						info.pointerDelta += 1;
						break;
					case InstructionType::AddPd:
						info.pointerDelta -= 1;
						break;
					default:
						// no need to do anything
						break;
					}
				}
				if (verboseOptimisation)
				{
					PrintOptimizedSection(input, output, begin, end, outBegin, outEnd);
				}
				return info;
			}
			InstructionDebug<TRegister> loopBegin = input.ReadDebug(begin);
			InstructionDebug<TRegister> loopEnd = input.ReadDebug(end - 1);

			auto loopBeginIndex = output.GetSize();
			output.Append(loopBegin);
			if (verboseOptimisation)
			{
				std::cerr << "\t" << loopBegin.type << "\t" << std::showpos << static_cast<int>(loopBegin.value) << "???\n";
			}

			OptimisationInfo info;
			if (innerBegin < innerEnd)
			{
				info = OptimizeProgram(input, output, innerBegin, innerEnd);
			}
			else
			{
				if (innerBegin == innerEnd)
				{
					std::cerr << "Infinite loop detected.\n";
				}
				else
				{
					throw std::runtime_error("invalid loop");
				}
			}

			loopBegin.value += info.instructionDelta;
			loopEnd.value -= info.instructionDelta;

			output.Replace(loopBeginIndex, loopBegin);
			output.Append(loopEnd);
			if (verboseOptimisation)
			{
				std::cerr << "\t" << loopEnd.type << "\t" << std::showpos << static_cast<int>(loopEnd.value) << "\n";

				std::cerr << std::noshowpos << "Opt Loop End (" << begin << ", " << (end - 1) << ")\n";
				std::cerr << "i-delta: " << info.instructionDelta << " p-delta: " << info.pointerDelta << "\n";
			}
			return info;
		}

		OptimisationInfo OptimizeProgram(const Program<TRegister, TProgramCounter>& input, Program<TRegister, TProgramCounter>& output, TProgramCounter begin, TProgramCounter end)
		{
			if (verboseOptimisation)
			{
				std::cerr << std::noshowpos << "Opt Program Begin (" << begin << ", " << (end - 1) << ")\n";
			}
			OptimisationInfo info;
			TProgramCounter ldivIndex = begin;
			size_t depth = 0;
			for (TProgramCounter i = begin; i<end; ++i)
			{
				Instruction<TRegister> current = input.Read(i);
				if (current.type == InstructionType::Jz)
				{
					if (depth == 0)
					{
						TProgramCounter flatBegin = ldivIndex;
						TProgramCounter flatEnd = i;
						if (flatBegin < flatEnd)
						{
							OptimisationInfo subInfo = OptimizeFlat(input, output, ldivIndex, flatEnd);
							info.pointerDelta += subInfo.pointerDelta;
							info.instructionDelta += subInfo.instructionDelta;
						}
						else
						{
							if (flatBegin == flatEnd)
							{
								// ok, no instructions
							}
							else
							{
								throw std::runtime_error("invalid bytecode");
							}
						}
						ldivIndex = i;
					}
					depth++;
				}
				if (current.type == InstructionType::Jnz)
				{
					depth--;
					if (depth == 0)
					{
						TProgramCounter loopBegin = ldivIndex;
						TProgramCounter loopEnd = i + 1;
						if (loopBegin < loopEnd)
						{
							OptimisationInfo subInfo = OptimizeLoop(input, output, loopBegin, loopEnd);
							info.pointerDelta += subInfo.pointerDelta;
							info.instructionDelta += subInfo.instructionDelta;
						}
						else
						{
							throw std::runtime_error("invalid bytecode");
						}
						ldivIndex = i + 1;
					}
				}
			}
			OptimisationInfo subInfo = OptimizeFlat(input, output, ldivIndex, end);
			info.pointerDelta += subInfo.pointerDelta;
			info.instructionDelta += subInfo.instructionDelta;
			if (verboseOptimisation)
			{
				std::cerr << std::noshowpos << "Opt Program End (" << begin << ", " << (end - 1) << ")\n";
				std::cerr << "i-delta: " << info.instructionDelta << " p-delta: " << info.pointerDelta << "\n";
			}
			return info;
		}

	public:

		// compiles bf source code to bytecode representation, collpases consecutive instructions
		Program<TRegister, TProgramCounter> Compile(const std::string& code) {
			Program<TRegister, TProgramCounter> program;
			std::vector<TProgramCounter> returnStack;
			InstructionDebug<TRegister> instruction(InstructionType::Nop, 0, 0, 0, 0, 0);
			size_t codePos = 0;
			size_t codeLine = 0;
			size_t codeColumn = 0;
			program.source = code; // copy source
			program.debug = true;

			for (auto c : code) {
				switch (c) {
				case '<':
					if (instruction.type != InstructionType::PtrAdd) {
						program.Append(instruction);
						instruction.type = InstructionType::PtrAdd;
						instruction.sourceBegin = codePos;
						instruction.sourceEnd = codePos + 1;
						instruction.sourceLine = codeLine;
						instruction.sourceColumn = codeColumn;
						instruction.value = -1;
					}
					else {
						--instruction.value;
						instruction.sourceEnd = codePos + 1;
					}
					break;

				case '>':
					if (instruction.type != InstructionType::PtrAdd) {
						program.Append(instruction);
						instruction.type = InstructionType::PtrAdd;
						instruction.sourceBegin = codePos;
						instruction.sourceEnd = codePos + 1;
						instruction.sourceLine = codeLine;
						instruction.sourceColumn = codeColumn;
						instruction.value = +1;
					}
					else {
						++instruction.value;
						instruction.sourceEnd = codePos + 1;
					}
					break;

				case '-':
					if (instruction.type != InstructionType::Add) {
						program.Append(instruction);
						instruction.type = InstructionType::Add;
						instruction.sourceBegin = codePos;
						instruction.sourceEnd = codePos + 1;
						instruction.sourceLine = codeLine;
						instruction.sourceColumn = codeColumn;
						instruction.value = -1;
					}
					else {
						--instruction.value;
						instruction.sourceEnd = codePos + 1;
					}
					break;

				case '+':
					if (instruction.type != InstructionType::Add) {
						program.Append(instruction);
						instruction.type = InstructionType::Add;
						instruction.sourceBegin = codePos;
						instruction.sourceEnd = codePos + 1;
						instruction.sourceLine = codeLine;
						instruction.sourceColumn = codeColumn;
						instruction.value = +1;
					}
					else {
						++instruction.value;
						instruction.sourceEnd = codePos + 1;
					}
					break;

				case '.':
					if (instruction.type != InstructionType::Out) {
						program.Append(instruction);
						instruction.type = InstructionType::Out;
						instruction.sourceBegin = codePos;
						instruction.sourceEnd = codePos + 1;
						instruction.sourceLine = codeLine;
						instruction.sourceColumn = codeColumn;
						instruction.value = +1;
					}
					else {
						++instruction.value;
						instruction.sourceEnd = codePos + 1;
					}
					break;

				case ',':
					if (instruction.type != InstructionType::In) {
						program.Append(instruction);
						instruction.type = InstructionType::In;
						instruction.sourceBegin = codePos;
						instruction.sourceEnd = codePos + 1;
						instruction.sourceLine = codeLine;
						instruction.sourceColumn = codeColumn;
						instruction.value = +1;
					}
					else {
						++instruction.value;
						instruction.sourceEnd = codePos + 1;
					}
					break;

				case '[':
					if (instruction.type != InstructionType::Nop) {
						program.Append(instruction);
					}
					instruction.type = InstructionType::Jz;
					instruction.sourceBegin = codePos;
					instruction.sourceEnd = codePos + 1;
					instruction.sourceLine = codeLine;
					instruction.sourceColumn = codeColumn;
					instruction.value = 0; // we don't know yet
					returnStack.push_back(program.GetSize());
					break;

				case ']':
					if (instruction.type != InstructionType::Nop) {
						program.Append(instruction);
					}
					instruction.type = InstructionType::Jnz;
					instruction.sourceBegin = codePos;
					instruction.sourceEnd = codePos + 1;
					instruction.sourceLine = codeLine;
					instruction.sourceColumn = codeColumn;
					{
						if (returnStack.size() <= 0) {
							throw std::invalid_argument("Unexpected ] found while parsing. Make sure there are no unbalanced brackets.");
						}
						auto currentIndex = program.GetSize();
						auto matchingIndex = returnStack.back();
						returnStack.pop_back();
						auto matchingInstruction = program.Read(matchingIndex);
						matchingInstruction.value = currentIndex - matchingIndex;
						instruction.value = matchingIndex - currentIndex;
						program.Replace(matchingIndex, matchingInstruction);
					}
					break;
				case '\n':
					codeLine++;
					codeColumn = 0;
					break;
				default:
					// ignore char, do noting
					break;
				}
				codePos++;
				codeColumn++;
			}

			if (instruction.type != InstructionType::Nop) {
				program.Append(instruction);
				instruction.type = InstructionType::Nop;
				instruction.value = 0;
			}

			if (returnStack.size() > 0) {
				throw std::invalid_argument("Reached the end of the code with one or more missing brackets. Make sure there are no unbalanced brackets.");
			}

			return program;
		}

		template<typename TOpt> void UseLoopOptimization()
		{
			auto ptr = std::make_unique<TOpt>();
			ptr->verbose = this->verboseOptimisation;
			loopOptimizations.push_back(std::move(ptr));
		}

		Program<TRegister, TProgramCounter> Optimize(const Program<TRegister, TProgramCounter>& input, size_t pass = 0)
		{
			Program<TRegister, TProgramCounter> output;
			output.debug = input.debug;
			output.source = input.source;
			OptimisationInfo info;
			if (verboseOptimisation)
			{
				std::cerr << "\n\n======= Optimization pass " << std::noshowpos << pass << " =======\n\n";
			}
			info = OptimizeProgram(input, output, 0, input.GetSize());
			if (info.instructionDelta == 0)
			{
				return output;
			}
			else
			{
				return Optimize(output, pass + 1);
			}

		}

		bool verboseOptimisation = false;
	};


}
