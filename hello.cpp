#include <iostream>
#include <vector>
#include "scoped_resource.h"
#include <unistd.h>

void s1(const std::string &strMessage)
{
	auto res = make_scoped_resource([strMessage]
	{
		std::cout << "strMessage:" << std::endl;
		std::cout << strMessage << std::endl;
	});
	std::cout << "Should be first..." << std::endl;
}

void cs2(int iFd, const std::string &strFinal)
{
	write(iFd, strFinal.c_str(), strFinal.length());
	close(iFd);
}

void s2()
{
	std::string const strFinalMsg { "Final Message\n" };
	// cs2 simulates a system call taking 2 arguments
	auto file = make_scoped_resource(cs2, dup(1), strFinalMsg);
	write(file, "s2 begin\n", 9);
	std::cout << "leaving s2 with file == " << file << std::endl;
}

int main(int argc, const char *argv[])
{
	int iRC = 0;
	auto doneHere = make_scoped_resource([&iRC]()->void
	{
		std::cout << "Done with tests, last run: " << iRC << std::endl;
	});

	s1("Should be 2nd");
	++iRC;

	s2();
	++iRC;

	std::vector<std::string> vs = { { "Test" }, { "Test 2" }, { "Test 3" } };
	for(const std::string &str : vs)
	{
		std::cout << str.c_str() << std::endl;
	}
	++iRC;
	return 0;
}

