#include <vector>

enum class InstructionType {
	Nop,
	Add,
	PtrAdd,
	In,
	Out,
	Jz,
	Jnz 
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
			switch (instruction.Type) {
			case InstructionType::Nop:
				++programCounter;
				break;
			case InstructionType::Add:
				currentValue += instruction.Value;
				zero = currentValue == 0;
				dirty = true;
				++programCounter;
				break;
			case InstructionType::PtrAdd:
				if (dirty) {
					memory.Write(pointer, currentValue);
					dirty = false;
				}
				pointer += instruction.Value;
				currentValue = memory.Read(pointer);
				++programCounter;
				break;
			case InstructionType::In:
				while(instruction.Value--) {
					currentValue = std::getchar();
				}
				zero = currentValue == 0;
				dirty = true;
				++programCounter;
				break;
			case InstructionType::Out:
				while(instruction.Value--) {
					std::putchar(currentValue);
				}
				++programCounter;
				break;
			case InstructionType::Jz:
				if (zero) {
					programCounter += instruction.Value;
				} else {
					++programCounter;
				}
				break;
			case InstructionType::Jnz:
				if (!zero) {
					programCounter += instruction.Value;
				} else {
					++programCounter;
				}
				break;
			}
		}
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
				break;

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
	auto program = compiler.Compile("++++++++++[>+++++++>++++++++++>+++>+<<<<-]>++.>+.+++++++..+++.>++.<<+++++++++++++++.>.+++.------.--------.>+.>.");
	Cpu<int, int, int> core;
	Memory<int, int> memory;
	core.Run(program, memory);
}