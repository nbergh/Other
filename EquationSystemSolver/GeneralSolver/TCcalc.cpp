//============================================================================
// Name        : TCcalc
// Author      : Niklas Bergh
//============================================================================

#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <string> // std::stoi
#include <algorithm>
#include <math.h>

/* This program solves the system of linear equations on the form Ax=b by reading custom
 * variable names and equations from a file, solving them, and then prints their values. It LU decomposition
 * to accomplish it, described here: https://equilibriumofnothing.files.wordpress.com/2013/10/matrix_factorlup.png
 * and here: http://cseweb.ucsd.edu/~baden/classes/Exemplars/260_fa06/Ricketts_SR.pdf
 */

static inline bool isZero(double val) {
	/* In large equation systems rounding errors becomes a factor. When doing LU factorization, it is possible to encounter
	 * diagonal value that should be zero, but is instead very close to zero, due to rounding errors in the calculations.
	 * Example: 1 - 1/3 * 3 is zero in math world, but 0.0000000...1 in computer world, since 1/3 is represented as
	 * 0.333333333... Therefore this function decides whether a value is zero or not
	 */

	if (fabs(val)<0.000001) {return true;}
	return false;
}

static bool LUPfactorize(double** A, int* P, int matSize) {
	/* Factorizes the matrix A into a lower and upper triangular matrix and stores it in A. When
	 * the algorithm is complete. A will constitute of an upper and lower triangular matrix A = L+U
	 * The diagonal of A belongs to the upper matrix. The diagonal of the lower matrix consists of ones
	 */

	int swapRoxIndex,tempPval;
	double* tempRowPointer,maxValInCol;

	for (int i = 0; i < matSize; i++) {P[i] = i;} // Set the permutation matrix to identity

	for (int col=0; col<matSize-1; col++) {
		if (isZero(A[col][col])) {
			/* If the diagonal of A is zero, then we need to permutate the matrix to avoid dividing by zero. If all the entries in
			 * A[-][col] are zero then the matrix A is singular, and the equation system has no (or an infinite
			 * number of) solutions
			 */
			maxValInCol=0;
			swapRoxIndex = col;
			for (int row=col+1;row<matSize;row++) {
//				if (!isZero(A[row][col])) {
//					/* Normally you want to find the _largest_ absolute value in A[row=col+1 to row = matSize-1][col] here, which gives
//					 * higher precision in the answer; however i choose the first non-zero value, which takes less time, without sacrificing
//					 * much accuracy
//					 */
//					swapRoxIndex = row;
//					break;
//				}
				// Get the largest value in the column
				if (fabs(A[row][col])>fabs(maxValInCol)) {
					maxValInCol = A[row][col];
					swapRoxIndex = row;
				}
			}
			if (isZero(maxValInCol)) {
				std::cout << "Matrix is singular to working precision" << std::endl;
				return false;
			}
			// Pointer swap the rows in A
			tempRowPointer = A[col];
			A[col] = A[swapRoxIndex];
			A[swapRoxIndex] = tempRowPointer;

			tempPval = P[col];
			P[col]= P[swapRoxIndex];
			P[swapRoxIndex] = tempPval;
		}

		for (int row=col+1;row<matSize;row++) {
			/* This is the standard LU factorization algorithm, described here:
			 * https://equilibriumofnothing.files.wordpress.com/2013/10/matrix_factorlup.png or here:
			 * http://cseweb.ucsd.edu/~baden/classes/Exemplars/260_fa06/Ricketts_SR.pdf
			 */
			A[row][col] /= A[col][col];
			for (int col2=col+1;col2<matSize;col2++) {
				A[row][col2] = A[row][col2] - A[col][col2] * A[row][col];
			}
		}
	}

	/* At this stage, A[matSize-1][matSize-1] may be zero, since the outermost col-iterating loop doesnt
	 * go through the last column (by design). Therefore, it doesn't check if A[matSize-1][matSize-1] or try to permutate it.
	 * The check is instead done here. If this check is passed, all diagonal values in A (which is the same as the diagonal
	 * in the upper triangular matrix) are guaranteed to be non-zero
	 */
	if (isZero(A[matSize-1][matSize-1])) {
		std::cout << "Matrix is singular to working precision" << std::endl;
		return false;
	}
	return true;
}

int main(int argc, char** argv) {
	if (argc<2) {std::cerr << "No equation file provided in command line argument" << std::endl; return -1;}

	// Start by reading the input file
	std::unordered_map<std::string, int> variableMap;
	std::vector<std::string> variableList;
	std::ifstream inFile;
	std::istringstream ss;
	std::string line,token;

	double** A,* b,* x,* y;
	int* P;

	int lineIndex=0,matSize=0;
	bool firstToken=true;

	inFile.open(argv[1]);
	if (inFile.is_open()) {
		while (getline(inFile,line)) {
			ss.str(line);
			ss.clear();
			ss >> token;
			variableMap.insert({token,lineIndex});
			variableList.push_back(token);
			lineIndex++;
		}
		matSize=lineIndex;
		if (matSize==0) {std::cerr << "No equations in input file" << std::endl;return-1;}

		// Allocate the matrices, zero initialized with ()
		A = new double*[matSize];
		for (int i = 0; i < matSize; ++i) {A[i] = new double[matSize]();}
		b = new double[matSize]();
		x = new double[matSize]();
		y = new double[matSize]();
		P = new int[matSize](); // Permutation vector

		inFile.clear();
		inFile.seekg(std::ios::beg); // Reset file pointer
		lineIndex=0;

		while (getline(inFile,line)) {
			A[lineIndex][lineIndex]=1;
			ss.str(line);
			ss.clear();
			while (ss >> token) {
				if (firstToken) {firstToken=false;continue;}

				if (isalpha(token[0])) {
					A[lineIndex][variableMap.at(token)]--; // Subtract 1 from the matrix 'A'
				}
				else if (isdigit(token[0])) {
					b[lineIndex]+=std::stoi(token);
				}
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

//	// Print the matrix and the b vector to files:
//	std::ofstream matrixOut("matrixOut");
//	for (int i=0;i<matSize;i++) {
//		for (int j=0;j<matSize;j++) {
//			matrixOut << A[i][j] << " ";
//		}
//		matrixOut << std::endl;
//	}
//	matrixOut.close();
//
//	std::ofstream bOut("bOut");
//	for (int i=0;i<matSize;i++) {
//		bOut << b[i] << std::endl;
//	}
//	bOut.close();

	if(!LUPfactorize(A,P,matSize)) {return -1;}

	// Now x is given by the equations: L*y=b and U*x=y
	for (int i=0;i<matSize;i++) {
		y[P[i]] = b[P[i]];
		for (int j=0;j<i;j++) {
			y[P[i]]-=A[i][j]*y[P[j]];
		}
		y[P[i]]=y[P[i]]/1; // The diagonal of the lower triangular matrix is 1
	}
	for (int i=matSize-1;i>=0;i--) {
		x[P[i]] = y[P[i]];
		for (int j=i+1;j<matSize;j++) {
			x[P[i]]-=A[i][j]*x[P[j]];
		}
		x[P[i]]=x[P[i]]/A[i][i];
	}

	// Associate each variable string with its value:
	std::vector<std::pair<std::string,double>> varPairs;
	varPairs.reserve(matSize);
	for (int i=0;i<matSize;i++) {
		varPairs.push_back(std::make_pair(variableList[i],x[P[i]]));
	}

	// Sort the strings:
	sort( varPairs.begin(), varPairs.end());

	// Print the result:
	for (int i=0;i<matSize;i++) {
		std::cout << varPairs[i].first << " = " << varPairs[i].second << std::endl;
	}

	for (int i = 0; i < matSize; ++i) {
	    delete[] A[i];
	}
	delete[] A;
	delete[] b;
	delete[] x;
	delete[] y;
	delete[] P;
}


