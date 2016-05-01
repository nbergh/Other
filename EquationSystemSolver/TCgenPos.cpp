//============================================================================
// Name        : TCgenPos
// Author      : Niklas Bergh
//============================================================================

#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <string>
#include <sys/time.h>

/* This program generates a text file detailing an equation system. The equation system follows these rules:
 * a) the variable at the right hand side of the equal sign has the coefficient 1
 * b) the variable at the right hand side of the equal sign does not appear on the left hand side of the equal sign
 * c) all numbers on the left hand side are positive
 * d) all variables have positive integer solutions
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
	if (argc<3) {std::cerr << "No equation or correct answer file provided in command line argument" << std::endl; return -1;}

	timeval now;
	gettimeofday(&now,NULL);
	srand(now.tv_sec*now.tv_usec);

	int nrVars;

	if (argc>3) {nrVars = std::stoi(argv[3]);}
	else {nrVars=rand()%1000;}

	if (nrVars<0) {std::cerr << "Error: number of equations must be positive" << std::endl; return -1;}

	int lval,rval,lastval,lastvalIndex;
	int* varValues = new int[nrVars];

	for (int i=0;i<nrVars;i++) {varValues[i]=rand()%1000;}

	std::ofstream out;
	out.open(argv[1]);

	for (int i=0;i<nrVars;i++) {
		out << getVar(i) << " = ";
		lval = varValues[i];
		rval=0;
		lastval=0;

		for (int i=0;i<nrVars/5;i++) {
			lastvalIndex = rand()%nrVars; // Get a random var index
			if (lastval==i) {continue;}

			lastval=varValues[lastvalIndex];
			if (lval > rval+lastval) {
				out << getVar(lastvalIndex) << " + ";
				rval+=lastval;
			}
		}

		out << lval-rval << std::endl;
	}

	out.close();

	std::ofstream valOut(argv[2]);
	for (int i=0;i<nrVars;i++) {
		valOut << getVar(i) << " = " << varValues[i] << std::endl;
	}

	delete[] varValues;
}
