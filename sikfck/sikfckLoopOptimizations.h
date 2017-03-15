#pragma once
#include "sikfckCompiler.h"

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

	}
}
