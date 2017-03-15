#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <ostream>
#include <memory>

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

std::ostream& operator<<(std::ostream& out, const InstructionType& i){
	switch (i)
	{
	case InstructionType::Nop: out << "NOP"; break;
	case InstructionType::Add: out << "ADD"; break;
	case InstructionType::AddPi: out << "ADDPI"; break;
	case InstructionType::AddPd: out << "ADDPD"; break;
	case InstructionType::PtrAdd: out << "PTR"; break;
	case InstructionType::In: out << "IN "; break;
	case InstructionType::Out: out << "OUT"; break;
	case InstructionType::Jz: out << "JZ "; break;
	case InstructionType::Jnz: out << "JNZ"; break;
	case InstructionType::Set: out << "SET"; break;
	}
	return out;
}

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
	void StripDebugInfo(){
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
		for (unsigned i=0; i<65536; i++) {
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
				while(instruction.value--) {
					currentValue = std::getchar();
				}
				zero = currentValue == 0;
				dirty = true;
				++programCounter;
				break;
			case InstructionType::Out:
				while(instruction.value--) {
					std::putchar(currentValue);
				}
				++programCounter;
				break;
			case InstructionType::Jz:
				if (zero) {
					programCounter += instruction.value;
				} else {
					++programCounter;
				}
				break;
			case InstructionType::Jnz:
				if (!zero) {
					programCounter += instruction.value;
				} else {
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
		InstructionDebug<TRegister> previous(InstructionType::Nop,0,0,0,0,0);
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
			switch(c) {
			case '<': 
				if (instruction.type != InstructionType::PtrAdd) {
					program.Append(instruction);
					instruction.type = InstructionType::PtrAdd;
					instruction.sourceBegin = codePos;
					instruction.sourceEnd = codePos + 1;
					instruction.sourceLine = codeLine;
					instruction.sourceColumn = codeColumn;
					instruction.value = -1;
				} else {
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
				} else {
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
			return Optimize(output, pass+1);
		}
		
	}

	bool verboseOptimisation = false;
};


template <typename TRegister, typename TProgramCounter> class SetToZeroLoopOptimization : public LoopOptimization<TRegister, TProgramCounter>
{
public:
	bool TryPerform(const Program<TRegister, TProgramCounter>& input, Program<TRegister, TProgramCounter>& output, TProgramCounter begin, TProgramCounter end) override
	{
		TProgramCounter outerInstructionCount = end - begin;
		
		if (outerInstructionCount == 3)
		{
			InstructionDebug<TRegister> a = input.ReadDebug(begin);
			InstructionDebug<TRegister> b = input.ReadDebug(begin + 1);
			InstructionDebug<TRegister> c = input.ReadDebug(begin + 2);
			if (a.type == InstructionType::Jz && c.type == InstructionType::Jnz)
			{
				if (b.type == InstructionType::Add && (b.value == +1 || b.value == -1))
				{
					InstructionDebug<TRegister> replacement(InstructionType::Set, 0, a.sourceBegin, c.sourceEnd, a.sourceLine, a.sourceColumn);
					output.Append(replacement);
					return true;
				}
				return false; 
			}
		}
		return false;
	}
};


int main(int argc, char** argv) {
	bool finalListing = false;
	bool verboseOptimisation = false;


	if (argc!=2)
	{
		printf("Usage: sikfck sourcefile.bf\n");
		return 1;
	}
	std::ifstream t(argv[1]);
	std::stringstream buffer;
	buffer << t.rdbuf();

	Compiler<int, int> compiler;
	compiler.verboseOptimisation = verboseOptimisation;
	compiler.UseLoopOptimization<SetToZeroLoopOptimization<int, int>>();

	auto program = compiler.Compile(buffer.str());
	auto optimised = compiler.Optimize(program);
	Cpu<int, int, int> core;
	Memory<int, int> memory;
	core.Run(optimised, memory);

	if (finalListing)
	{
		std::cerr << "\n\n======= Final Bytecode Listing =======\n\n";
		std::cerr << optimised;
	}
	return 0;
}