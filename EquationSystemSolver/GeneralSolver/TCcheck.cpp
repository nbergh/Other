//============================================================================
// Name        : TCcheck.cpp
// Author      : Niklas Bergh
//============================================================================

#include <fstream>
#include <sstream>
#include <string> // stof
#include <unordered_map>
#include <math.h> //fmod
#include <iostream>

/* This program tests the given solution by checking:
 * a) that the first char in the varName string isalpha,
 * b) (indirectly) that every varname in the equation system is defined in the given solution (unordered_map throws an exception
 * if a variable in the equation system isn't mentioned/defined in the given solution)
 * c) that, if all the varName strings in the given equation system are substituded for their respective values,
 * that the equations left hand and right hand side are the same
 *
 * It does not test:
 * a) whether new variables (not defined in the equation system) have been introduced in the solution or not
 * b) if the variables are presented in alphabetical order
 * c) if the variables are negative - negative variables are permitted
 * d) if the variables have decimals - decimals are permitted
 */

int main(int argc, char** argv) {
	if (argc<2) {std::cerr << "No equation file provided in command line argument" << std::endl; return -1;}

	std::unordered_map<std::string, double> variableMap;
	std::stringstream ss;
	std::string line, varName, token;
	int nrEquations=0;
	double varValue;
	char eqsign;

	// Read infile
	while (getline(std::cin,line)) {
		if (line=="Matrix is singular to working precision") {return 0;} // If equation system doesn't have a solution, return

		ss.str(line);
		ss.clear();
		ss >> varName >> eqsign >> varValue;
		if (!isalpha(varName[0]) /*|| fmod(varValue,1)!=0*/) {return -1;}
		variableMap.insert({varName,varValue});
		nrEquations++;
	}

	ss.clear();
	std::ifstream eqFile (argv[1]);
	double lvalue,rvalue,maxError=0;

	// Read eqFile
	while (getline(eqFile,line)) {
		rvalue=0,lvalue=0;
		ss.str(line);
		ss.clear();
		ss >> token;
		lvalue = variableMap.at(token); // Store the lvalue here, throws exception if token is not in the map

		while (ss >> token) {
			if (isalpha(token[0])) {
				rvalue += variableMap.at(token); // Throws exception if token is not in the map
			}
			else if (isdigit(token[0])) {
				rvalue += std::stof(token);
			}
		}
		if (fabs(lvalue-rvalue)>fabs(maxError)) {maxError = lvalue-rvalue;}
	}
	eqFile.close();

	/* There will always be some errors in the solution, due to rounding errors in the double datatype.
	 * Therefore, the maximal error will be shown, which should be "limited". The error will increase if the number
	 * of equations and the sparsity of the system is increased. It will also depend on the size of the variables,
	 * the larger the size, the less decimals in the double data type, which decreases precision. Running on 100
	 * equations with 5 % sparsity the max error is about 15, and the mean error is about 0.1-0.2. The answers
	 * are controlled against matlab on the same system, to make sure they are corrects within the rounding error
	 */
	if (maxError > nrEquations*0.2) {
		std::cout << "Max error in solution: " << maxError << std::endl;
		return -1;
	}
}
