#include "sikfck.h"

std::ostream& sikfck::operator<<(std::ostream& out, const InstructionType& i) {
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
		case InstructionType::AddM: out << "ADDM"; break;
		case InstructionType::SubM: out << "SUBM"; break;
		case InstructionType::MulM: out << "MULM"; break;
	}
	return out;
}