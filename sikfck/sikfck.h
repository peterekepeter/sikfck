#pragma once
#include <iostream>
#include <vector>

namespace sikfck {

	enum class InstructionType {
		Nop,
		Add,
		AddPi,
		AddPd,
		PtrAdd,
		In,
		Out,
		Jz,
		Jnz,
		Set
	};

	std::ostream& operator<<(std::ostream& out, const InstructionType& i);

	template <typename TRegister> class Instruction {
	public:
		InstructionType type;

		Instruction(InstructionType type, const TRegister& value)
			: type(type),
			value(value) {}

		TRegister value;
	};

	template <typename TRegister> class InstructionDebug : public Instruction<TRegister>
	{
	public:
		size_t sourceBegin, sourceEnd, sourceLine, sourceColumn;

		InstructionDebug(InstructionType type, const TRegister& value, size_t source_begin, size_t source_end, size_t source_line, size_t source_column)
			: Instruction(type, value),
			sourceBegin(source_begin),
			sourceEnd(source_end),
			sourceLine(source_line),
			sourceColumn(source_column)
		{
		}
	};

	template <typename TRegister, typename TProgramCounter> class Program {
	public:

		std::vector<InstructionType> itype;
		std::vector<TRegister> ivalue;

		//for debug
		std::vector<size_t> sourceBegin;
		std::vector<size_t> sourceEnd;
		std::vector<size_t> sourceLine;
		std::vector<size_t> sourceColumn;
		std::string source;
		bool debug;

		Instruction<TRegister> Read(TProgramCounter index) const {
			return Instruction<TRegister>(itype[index], ivalue[index]);
		}

		InstructionDebug<TRegister> ReadDebug(TProgramCounter index) const
		{
			return InstructionDebug<TRegister>(itype[index], ivalue[index], sourceBegin[index], sourceEnd[index], sourceLine[index], sourceColumn[index]);
		}

		void Append(const InstructionDebug<TRegister>& instruction) {
			auto index = itype.size();
			itype.push_back(instruction.type);
			ivalue.push_back(instruction.value);
			if (debug)
			{
				sourceBegin.push_back(instruction.sourceBegin);
				sourceEnd.push_back(instruction.sourceEnd);
				sourceLine.push_back(instruction.sourceLine);
				sourceColumn.push_back(instruction.sourceColumn);
			}
		}

		void Append(const Instruction<TRegister>& instruction) {
			itype.push_back(instruction.type);
			ivalue.push_back(instruction.value);
		}

		void Replace(TProgramCounter index, const Instruction<TRegister>& instruction) {
			itype[index] = instruction.type;
			ivalue[index] = instruction.value;
		}

		void Replace(TProgramCounter index, const InstructionDebug<TRegister>& instruction) {
			itype[index] = instruction.type;
			ivalue[index] = instruction.value;
			if (debug)
			{
				sourceBegin[index] = instruction.sourceBegin;
				sourceEnd[index] = instruction.sourceEnd;
				sourceLine[index] = instruction.sourceLine;
				sourceColumn[index] = instruction.sourceColumn;
			}
		}

		inline TProgramCounter GetSize() const {
			return static_cast<TProgramCounter>(itype.size());
		}

		// removes debug information from program
		void StripDebugInfo() {
			sourceBegin.clear();
			sourceEnd.clear();
			sourceLine.clear();
			sourceColumn.clear();
			source.clear();
			debug = false;
		}

		// output operator
		friend std::ostream& operator<<(std::ostream& out, const Program& program)
		{
			size_t srcLastLine = -1;
			size_t sourceIndex = 0, sourceIndexLine = 0;
			TProgramCounter absolutePos = 0;
			bool lastPosLabel = false;

			for (int i = 0; i < program.itype.size(); i++)
			{
				// list source lines
				if (i < program.sourceLine.size())
				{
					size_t line = program.sourceLine[i];
					size_t sourcePrintBegin = sourceIndex;
					size_t sourcePrintLength = 0;
					if (line != srcLastLine)
					{
						srcLastLine = line;
						while (sourceIndexLine <= line && sourceIndex < program.source.length())
						{
							if (program.source[sourceIndex] == '\n')
							{
								sourceIndexLine++;
								out << ";;;;;;; " << program.source.substr(sourcePrintBegin, sourcePrintLength) << "\n";
								sourcePrintBegin = sourceIndex + 1;
								sourcePrintLength = -1;
							}
							sourceIndex++;
							sourcePrintLength++;
						}
					}
				}

				// list instruction
				out << "\t" << program.itype[i] << " " << std::showpos << static_cast<int>(program.ivalue[i]);

				if (program.itype[i] == InstructionType::Jz || program.itype[i] == InstructionType::Jnz)
				{
					out << "; L_" << std::noshowpos << (absolutePos + program.ivalue[i]) << "\n";
					out << "L_" << std::noshowpos << absolutePos << ":";
				}


				// source -> instruction breakdown
				/*if (i < program.sourceBegin.size() && i < program.sourceEnd.size())
				{
				out << '\t\t; ' << program.source.substr(program.sourceBegin[i], program.sourceEnd[i]);
				}*/
				++absolutePos;
				out << "\n";
			}
			return out;
		}
	};

	template <typename TRegister, typename TPointer> class Memory {

		TRegister raw[65536];
	public:
		Memory() {
			for (unsigned i = 0; i<65536; i++) {
				raw[i] = 0;
			}
		}

		void Write(TPointer pointer, TRegister current_value) {
			raw[pointer & 0xffff] = current_value;
		}

		TRegister Read(TPointer pointer) {
			return raw[pointer & 0xffff];
		}
	};

	template <typename TRegister, typename TProgramCounter, typename TPointer> class Cpu {
		TProgramCounter programCounter;
		TPointer pointer;
		TRegister currentValue;
		bool dirty;
		bool zero;

	public:

		Cpu() {
			programCounter = 0;
			pointer = 0;
			currentValue = 0;
			dirty = false;
			zero = false;
		}

		void Run(const Program<TRegister, TProgramCounter>& program, Memory<TRegister, TPointer>& memory) {
			while (programCounter < program.GetSize()) {
				auto instruction = program.Read(programCounter);
				switch (instruction.type) {
				case InstructionType::Nop:
					++programCounter;
					break;
				case InstructionType::Add:
					currentValue += instruction.value;
					zero = currentValue == 0;
					dirty = true;
					++programCounter;
					break;
				case InstructionType::AddPi:
					memory.Write(pointer, currentValue + instruction.value);
					dirty = false;
					++pointer;
					currentValue = memory.Read(pointer);
					zero = currentValue == 0;
					++programCounter;
					break;
				case InstructionType::AddPd:
					memory.Write(pointer, currentValue + instruction.value);
					dirty = false;
					--pointer;
					currentValue = memory.Read(pointer);
					zero = currentValue == 0;
					++programCounter;
					break;
				case InstructionType::PtrAdd:
					if (dirty) {
						memory.Write(pointer, currentValue);
						dirty = false;
					}
					pointer += instruction.value;
					currentValue = memory.Read(pointer);
					zero = currentValue == 0;
					++programCounter;
					break;
				case InstructionType::In:
					while (instruction.value--) {
						currentValue = std::getchar();
					}
					zero = currentValue == 0;
					dirty = true;
					++programCounter;
					break;
				case InstructionType::Out:
					while (instruction.value--) {
						std::putchar(currentValue);
					}
					++programCounter;
					break;
				case InstructionType::Jz:
					if (zero) {
						programCounter += instruction.value;
					}
					else {
						++programCounter;
					}
					break;
				case InstructionType::Jnz:
					if (!zero) {
						programCounter += instruction.value;
					}
					else {
						++programCounter;
					}
					break;
				case InstructionType::Set:
					currentValue = instruction.value;
					zero = currentValue == 0;
					dirty = true;
					++programCounter;
					break;
				default:
					throw std::invalid_argument("Illegal instruction.");
				}
			}
		}
	};

}
