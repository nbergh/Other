//============================================================================
// Name        : TCcalcJacobi.cpp
// Author      : Niklas Bergh
//============================================================================

#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <string> // std::stoi
#include <algorithm>

#ifndef MAX_ITERATIONS
#define MAX_ITERATIONS 50
#endif

/* This solver uses the Jacobi/Gauss Seidel method, see https://www3.nd.edu/~zxu2/acms40390F12/Lec-7.3.pdf. It
 * converges for some systems, but not all
 */

static int jacobiIterate(int** C,int* b, int* x, int* xNew, int nrOfEquations) {
	// Calculate xNew:
	for (int i=0;i<nrOfEquations;i++) {
		xNew[i]=b[i];
		for (int j=0;j<nrOfEquations;j++) {
			if (i==j) {continue;}

#ifdef USE_GSEIDEL
			xNew[i]+=(C[i][j] * ((j<i) ? xNew[j] : x[j])); // Gauss-Seidel
#else
			xNew[i]+=(C[i][j] * x[j]); // Jacobi
#endif

		}
	}

	// Calculate the error and set x to xNew for the next iteration
	int error=0;
	for (int i=0;i<nrOfEquations;i++) {
		error+=abs(x[i]-xNew[i]);
		x[i]=xNew[i];
	}
	return error;
}

int main(int argc, char** argv) {
	if (argc<2) {std::cerr << "No equation file provided in command line argument" << std::endl; return -1;}

	int maxIterations = MAX_ITERATIONS;
	if (maxIterations<0 || maxIterations-MAX_ITERATIONS != 0) {
		std::cerr << "Illegal format of MAX_ITERATIONS" << std::endl;
		return -1;
	}

	// Start by reading the input file
	std::unordered_map<std::string, int> variableMap;
	std::vector<std::string> variableList;
	std::ifstream inFile;
	std::istringstream ss;
	std::string line,token;

	int** C,* b; // variable coefficients, equation constants
	int* x,* xNew; // variables, variables in the new iteration

	int lineIndex=0,nrOfEquations=0,iters=0;
	bool firstToken=true;

	inFile.open(argv[1]);
	if (inFile.is_open()) {
		while (getline(inFile,line)) {
			ss.str(line);
			ss.clear();
			ss >> token;
			variableMap.insert({token,nrOfEquations});
			variableList.push_back(token);
			nrOfEquations++;
		}
		if (nrOfEquations==0) {std::cerr << "No equations in input file" << std::endl;return-1;}

		// Allocate the matrices, zero initialized with ()
		C = new int*[nrOfEquations];
		for (int i = 0; i < nrOfEquations;i++) {C[i] = new int[nrOfEquations]();}
		b = new int[nrOfEquations]();
		x = new int[nrOfEquations]();
		xNew = new int[nrOfEquations]();

		inFile.clear();
		inFile.seekg(std::ios::beg); // Reset file pointer

		while (getline(inFile,line)) {
			C[lineIndex][lineIndex]=-1;
			ss.str(line);
			ss.clear();
			while (ss >> token) {
				if (firstToken) {firstToken=false;continue;}

				if (isalpha(token[0])) {
					C[lineIndex][variableMap.at(token)]++; // Add to coefficients
				}
				else if (isdigit(token[0])) {
					b[lineIndex]+=std::stoi(token); // Assuming no int overflow here
				}
			}
			if (C[lineIndex][lineIndex]!=-1) {
				/* C[lineIndex][lineIndex] cannot be zero. It is ok for it to be -1 or greater than 0. In the case it is not -1 or 0
				 * then all the other coefficients and b[lineIndex] needs to be divided by that -coefficient. I assume that it never happens
				 * here though, and only allow C[lineIndex][lineIndex] to be -1
				 */
				std::cerr << "Error, coefficient for diagonal variable is not 1" << std::endl;
				return -1;
			}
			firstToken=true;
			lineIndex++;
		}
	}
	else {
		std::cerr << "Unable to open file" << std::endl;
		return -1;
	}

	inFile.close();

	while (++iters<MAX_ITERATIONS && jacobiIterate(C,b,x,xNew,nrOfEquations) > 0); // Iterate until convergence

	if (iters==MAX_ITERATIONS) {std::cerr << "Jacobi method did not converge" << std::endl;return-1;}

	// Associate each variable string with its value:
	std::vector<std::pair<std::string,int>> varPairs;
	varPairs.reserve(nrOfEquations);
	for (int i=0;i<nrOfEquations;i++) {
		varPairs.push_back(std::make_pair(variableList[i],x[i]));
	}

	// Sort the strings:
	sort( varPairs.begin(), varPairs.end());

	// Print the result:
	for (int i=0;i<nrOfEquations;i++) {
		std::cout << varPairs[i].first << " = " << varPairs[i].second << std::endl;
	}

	for (int i = 0; i < nrOfEquations; ++i) {
		delete[] C[i];
	}
	delete[] C;
	delete[] b;
	delete[] x;
	delete[] xNew;
}
