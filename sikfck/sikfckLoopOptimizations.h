#pragma once
#include "sikfckCompiler.h"
#include <map>

namespace sikfck {

	namespace LoopOptimizations {

		template <typename TRegister, typename TProgramCounter> class SetToZero : public LoopOptimization<TRegister, TProgramCounter>
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


		template <typename TRegister, typename TProgramCounter> class LinearArithmetic : public LoopOptimization<TRegister, TProgramCounter>
		{

		public:
			bool TryPerform(const Program<TRegister, TProgramCounter>& input, Program<TRegister, TProgramCounter>& output, TProgramCounter begin, TProgramCounter end) override
			{
				TProgramCounter innerBegin = begin + 1;
				TProgramCounter innerEnd = end - 1;
				
				std::map<int, int> operators;
				int offset = 0;
				for (TProgramCounter i= innerBegin; i<innerEnd; ++i) {
					InstructionDebug<TRegister> instruction = input.ReadDebug(i);
					switch (instruction.type) {
						case InstructionType::Add:
							operators[offset] += instruction.value;
							break;
						case InstructionType::AddPd:
							operators[offset] += instruction.value;
							offset -= 1;
							break;
						case InstructionType::AddPi:
							operators[offset] += instruction.value;
							offset += 1;
							break;
						case InstructionType::PtrAdd:
							offset += instruction.value;
							break;
						default:
							// cannot optimised this
							return false;
							break;
					}
				}

				// loop must terminate on same offset
				if (offset != 0) {
					return false;
				}

				// the control variable must be in decrement mode
				if (operators[0] != -1) {
					return false;
				}

				// success, can be reduced
				InstructionDebug<TRegister> loopBegin = input.ReadDebug(begin);
				InstructionDebug<TRegister> loopEnd = input.ReadDebug(begin + 2);
				InstructionDebug<TRegister> temp(InstructionType::Nop, 0, loopBegin.sourceBegin, loopEnd.sourceEnd, loopBegin.sourceLine, loopBegin.sourceColumn);

				int power = 1;
				bool done = false;
				while (!done) {
					done = true;
					for (auto& op : operators) {
						if (op.first == 0) {
							continue;
						}
						if (op.second == 0) {
							// no need to do anyting here
						}
						else {
							if (op.second > 0 && ((power&op.second)>0)) {
								temp.type = InstructionType::AddM;
								temp.value = op.first; // ofset
								output.Append(temp);
								op.second -= power; // clear
								if (op.second != 0) {
									done = false;
								}
							}
							else if (op.second<0 && ((power&(-op.second))>0)) {
								temp.type = InstructionType::SubM;
								temp.value = op.first; // ofset
								output.Append(temp);
								op.second += power;
								if (op.second != 0) {
									done = false;
								}
							}
							else {
								done = false;
							}
						}
					}
					// double self
					if (!done) {
						power = power * 2;
						temp.type = InstructionType::AddM;
						temp.value = 0;
						output.Append(temp);
					}
				}
				

				temp.type = InstructionType::Set;
				temp.value = 0;
				operators[0] = 0;
				output.Append(temp);

				return true;
			}
		};

	}
}
