
#include <fstream>
#include <sstream>

#include "sikfck.h"
#include "sikfckCompiler.h"
#include "sikfckLoopOptimizations.h"

int main(int argc, char** argv) {

	using namespace sikfck;
	namespace loopOpt = sikfck::LoopOptimizations;

	bool finalListing = true;
	bool verboseOptimisation = true;


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
	compiler.UseLoopOptimization<loopOpt::SetToZero<int, int>>();

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