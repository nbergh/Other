//============================================================================
// Name        : TCcheckPos.cpp
// Author      : Niklas Bergh
//============================================================================

#include <fstream>
#include <iostream>
#include <vector>
#include <algorithm>

// This program tests the given solution by comparing the strings provided with the correct strings

int main(int argc, char** argv) {
	if (argc<2) {std::cerr << "No equation file provided in command line argument" << std::endl; return -1;}

	std::ifstream inFile;
	std::vector<std::string> correctLines,inLines;
	std::string line;
	int nrEquationsCorrect=0,nrEquationsIn=0;

	inFile.open(argv[1]);
	// Read equations file
	while (getline(inFile,line)) {
		correctLines.push_back(line);
		nrEquationsCorrect++;
	}
	inFile.close();

	sort( correctLines.begin(), correctLines.end());

	while (getline(std::cin,line)) {
		inLines.push_back(line);
		nrEquationsIn++;
	}

	if (nrEquationsCorrect!=nrEquationsIn) {
		std::cerr << "Incorrect nr of equations provided" << std::endl;
		return -1;
	}


	for (int i=0;i<nrEquationsCorrect;i++) {
		if (correctLines[i]!=inLines[i]) {
			std::cerr << "Line incorrect: " << inLines[i] << " expected: " << correctLines[i] << std::endl;
			return -1;
		}
	}
}
