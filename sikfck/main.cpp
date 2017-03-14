#include <vector>

enum class InstructionType {
	Nop = 0,
	Add = 2,
	PtrAdd = 4,
	In = 8,
	Out = 9,
	Jz = 10,
	Jnz = 11
};

template <typename TRegister> class Instruction {
public:
	InstructionType Type;

	Instruction(InstructionType type, const TRegister& value)
		: Type(type),
		  Value(value) {}

	TRegister Value;
};

template <typename TRegister, typename TProgramCounter> class Program {
	std::vector<InstructionType> itype;
	std::vector<TRegister> ivalue;
public:

	Instruction<TRegister> Read(TProgramCounter index) const {
		return Instruction<TRegister>(itype[index], ivalue[index]);
	}

	void Append(const Instruction<TRegister>& instruction) {
		itype.push_back(instruction.Type);
		ivalue.push_back(instruction.Value);
	}

	void Replace(TProgramCounter index, const Instruction<TRegister>& instruction) {
		itype[index] = instruction.Type;
		ivalue[index] = instruction.Value;
	}

	inline TProgramCounter GetSize() const {
		return itype.size();
	}
};

class Memory {
	
};

template <typename TRegister, typename TProgramCounter, typename TPointer> class Cpu {
	TProgramCounter programCoutner;
	TPointer pointer;

	void Run(const Program<TRegister, TProgramCounter>& program, Memory& memory) {
		
	}
};

template <typename TRegister, typename TProgramCounter> class Compiler {
public:
	Program<TRegister, TProgramCounter> Compile(const std::string& code) {
		Program<TRegister, TProgramCounter> program;
		std::vector<TProgramCounter> returnStack;
		Instruction<TRegister> instruction(InstructionType::Nop, 0);

		for (auto c : code) {
			switch(c) {
			case '<': 
				if (instruction.Type != InstructionType::PtrAdd) {
					program.Append(instruction);
					instruction.Type = InstructionType::PtrAdd;
					instruction.Value = -1;
				} else {
					--instruction.Value;
				}
				break;

			case '>':
				if (instruction.Type != InstructionType::PtrAdd) {
					program.Append(instruction);
					instruction.Type = InstructionType::PtrAdd;
					instruction.Value = +1;
				} else {
					++instruction.Value;
				}
				break;

			case '-':
				if (instruction.Type != InstructionType::Add) {
					program.Append(instruction);
					instruction.Type = InstructionType::Add;
					instruction.Value = -1;
				}
				else {
					--instruction.Value;
				}
				break;

			case '+':
				if (instruction.Type != InstructionType::Add) {
					program.Append(instruction);
					instruction.Type = InstructionType::Add;
					instruction.Value = +1;
				}
				else {
					++instruction.Value;
				}
				break;

			case '.':
				if (instruction.Type != InstructionType::Out) {
					program.Append(instruction);
					instruction.Type = InstructionType::Out;
					instruction.Value = +1;
				}
				else {
					++instruction.Value;
				}

			case ',':
				if (instruction.Type != InstructionType::In) {
					program.Append(instruction);
					instruction.Type = InstructionType::In;
					instruction.Value = +1;
				}
				else {
					++instruction.Value;
				}
				break;

			case '[': 
				if (instruction.Type != InstructionType::Nop) {
					program.Append(instruction);
				}
				instruction.Type = InstructionType::Jz;
				instruction.Value = 0; // we don't know yet
				returnStack.push_back(program.GetSize());
				break;

			case ']':
				if (instruction.Type != InstructionType::Nop) {
					program.Append(instruction);
				}
				instruction.Type = InstructionType::Jnz;
				{
					if (returnStack.size() <= 0) {
						throw std::invalid_argument("Unexpected ] found while parsing. Make sure there are no unbalanced brackets.");
					}
					auto currentIndex = program.GetSize();
					auto matchingIndex = returnStack.back();
					returnStack.pop_back();
					auto matchingInstruction = program.Read(matchingIndex);
					matchingInstruction.Value = currentIndex - matchingIndex + 1;
					instruction.Value = matchingIndex - currentIndex + 1;
					program.Replace(matchingIndex, matchingInstruction);
				}
				break;
			}
		}

		if (instruction.Type != InstructionType::Nop) {
			program.Append(instruction);
			instruction.Type = InstructionType::Nop;
			instruction.Value = 0;
		}

		if (returnStack.size() > 0) {
			throw std::invalid_argument("Reached the end of the code with one or more missing brackets. Make sure there are no unbalanced brackets.");
		}

		return program;
	}

};

int main() {
	Compiler<int, int> compiler;
	compiler.Compile("++++++++[>+<-]");
}