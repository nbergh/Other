//============================================================================
// Name        : TCgen
// Author      : Niklas Bergh
//============================================================================

#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <sys/time.h>

/* This program generates a text file detailing a general equation system. The equation system is not guaranteed to:
 * a) be solvable (e.g it can have 0, 1 or an infinite number of solutions)
 * b) have a solution where each variable is positive
 * c) have a solution where each variable is an integer
 */

static std::string getVar(int index) {
	std::string var = "";
	char ch;

	if (index==0) {return var+'a';}

	while (index>0) {
		ch = index%26 + 'a';
		var +=ch;
		index/=26;
	}

	return var;
}

int main(int argc, char** argv) {
	if (argc<2) {std::cerr << "No equation file provided in command line argument" << std::endl; return -1;}

	timeval now;
	gettimeofday(&now,NULL);
	srand(now.tv_sec*now.tv_usec);

	int nrVars=rand()%1000;
	float sparsity = (double)rand()/RAND_MAX; // Indicates how many variables and values will be after each equal sign
	std::ofstream out;
	out.open(argv[1]);

	for (int i=0;i<nrVars;i++) {
		out << getVar(i) << " = ";
		for (int j=0;j<nrVars;j++) {
			if (i==j) {continue;}
			if ((double)rand()/RAND_MAX < sparsity) {
				out << getVar(j) << " + ";
			}
			if ((double)rand()/RAND_MAX < sparsity) {
				out << rand()%2 << " + "; // random values from 0 to 999
			}
		}
		out << "0" << std::endl;
	}

	out.close();
}
