//============================================================================
// Name        : TCcalcJacobiParallel.cpp
// Author      : Niklas Bergh
//============================================================================

#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <string> // std::stoi
#include <algorithm>
#include <assert.h> // cudaCheckReturn

#ifndef BLOCK_SIZE
#define BLOCK_SIZE 16
#endif

#ifndef MAX_ITERATIONS
#define MAX_ITERATIONS 50
#endif

/* This solver uses the Jacobi/Gauss Seidel method, see https://www3.nd.edu/~zxu2/acms40390F12/Lec-7.3.pdf. It
 * converges for some systems, but not all
 */

__device__ int errorOnGPU;

__device__ inline int getValFromMatrix(int* matrix, int row, int col,int matSize,int pitchLength) {
	if (row<matSize && col<matSize) {return matrix[row*pitchLength + col];}
	return 0;
}

__device__ inline int getValFromVector(int* vector, int row, int matSize) {
	if (row<matSize) {return vector[row];}
	return 0;
}

__global__ void jacobiIterateKernel(int* cOnGPU,int* bOnGPU, int* xOnGPU, int* deltaXonGPU, int matSize, int pitchLength) {
	__shared__ int cShared[BLOCK_SIZE][BLOCK_SIZE+1]; // Make the columns BLOCK_SIZE + 1, so that there is less chance of a shared memory bank conflict
	__shared__ int xShared[BLOCK_SIZE];

	int myRow = blockIdx.x * blockDim.x + threadIdx.x;
	int myRowInBlock = threadIdx.x, myColInBlock = threadIdx.y;
	int rowSum=getValFromVector(bOnGPU,myRow,matSize);

	for (int m = 0; m < (matSize + BLOCK_SIZE - 1) / BLOCK_SIZE; m++) {
		cShared[myRowInBlock][myColInBlock] = getValFromMatrix(cOnGPU,myRow,m*BLOCK_SIZE+myColInBlock,matSize,pitchLength);
		if (myColInBlock==0) {xShared[myRowInBlock] = getValFromVector(xOnGPU,m*BLOCK_SIZE+myRowInBlock,matSize);}

		__syncthreads(); // Sync threads to make sure all fields have been written by all threads in the block to cShared and xShared

		if (myColInBlock==0) {
			for (int k=0;k<BLOCK_SIZE;k++) {
				// Only the Jacobi (not Gauss Seidel) iteration works in parallell:
				if (m*BLOCK_SIZE+k==myRow) {continue;}
				rowSum += cShared[myRowInBlock][k] * xShared[k];
			}
		}

		__syncthreads(); // Sync here so that no threads start changing the shared arrays (in the next iteration of m) before rowSum has been updated
	}

	if (myColInBlock==0 && myRow<matSize) {
		deltaXonGPU[myRow] = abs(rowSum - xOnGPU[myRow]);
		xOnGPU[myRow] = rowSum; // Update x
	}
}

__global__ void calculateErrorKernel(int* deltaXonGPU, int matSize) {
	__shared__ int deltaXshared[BLOCK_SIZE];
	__shared__ int sharedError;

	int myIndexInBlock = threadIdx.x;
	sharedError=0;

	for (int m = 0; m < (matSize + BLOCK_SIZE - 1) / BLOCK_SIZE;m++) {
		deltaXshared[myIndexInBlock] = getValFromVector(deltaXonGPU,m*BLOCK_SIZE+myIndexInBlock,matSize);

		__syncthreads();

		if (myIndexInBlock==0) {
			for (int k=0;k<BLOCK_SIZE;k++) {
				sharedError+=deltaXshared[k];
			}
		}

		__syncthreads();
	}
	if (myIndexInBlock==0) {
		errorOnGPU = sharedError;
	}
}

static inline void cudaCheckReturn(cudaError_t result) {
	if (result != cudaSuccess) {
		std::cerr <<"CUDA Runtime Error: " << cudaGetErrorString(result) << std::endl;
		assert(result == cudaSuccess);
	}
}

static int jacobiIterate(int* cOnGPU,int* bOnGPU, int* xOnGPU, int* deltaXonGPU, int matSize, int pitchLength) {
	int nrOfBlocks = (matSize + BLOCK_SIZE -1) / BLOCK_SIZE;
	dim3 threadSize(BLOCK_SIZE,BLOCK_SIZE);

	jacobiIterateKernel<<<nrOfBlocks,threadSize>>>(cOnGPU,bOnGPU,xOnGPU,deltaXonGPU,matSize,pitchLength);
	calculateErrorKernel<<<1,BLOCK_SIZE>>>(deltaXonGPU,matSize); // Alternatively deltaXonGPU can be copied to an array defined here, and iterate through it in CPU to get the error

	int error=0;
	// This call synchronizes the device with the host, so there is no application code running on the device past this point
	cudaCheckReturn(cudaMemcpyFromSymbol(&error,errorOnGPU,sizeof(int),0,cudaMemcpyDeviceToHost));

	return error;
}

int main(int argc, char** argv) {
	if (argc<2) {std::cerr << "No equation file provided in command line argument" << std::endl; return -1;}

	// Check BLOCK_SIZE
	int blockSizeIn = BLOCK_SIZE;
	if (blockSizeIn<0 || blockSizeIn-BLOCK_SIZE!=0 || (blockSizeIn*blockSizeIn)%32!=0) {
		std::cerr << "Block size must be > 0, an integer and its square must be a multiple of 32" << std::endl;
		return -1;
	}

	// Check MAX_ITERATIONS
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
	int* x; // variables, variables in the new iteration

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
		C[0] = new int[nrOfEquations*nrOfEquations];
		for (int i = 1; i < nrOfEquations;i++) {C[i] = &C[0][i*nrOfEquations];}
		b = new int[nrOfEquations]();
		x = new int[nrOfEquations]();

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

	// Allocate data on the GPU:
	int* cOnGPU,* bOnGPU;
	int* xOnGPU,* deltaXonGPU;

	int rowSizeOnGPU = nrOfEquations * sizeof(int),pitchLength;
	size_t cPitch;

	cudaCheckReturn(cudaMallocPitch(&cOnGPU,&cPitch,rowSizeOnGPU,nrOfEquations));
	cudaCheckReturn(cudaMalloc(&bOnGPU,rowSizeOnGPU));
	cudaCheckReturn(cudaMalloc(&xOnGPU,rowSizeOnGPU));
	cudaCheckReturn(cudaMalloc(&deltaXonGPU,rowSizeOnGPU));

	pitchLength = cPitch/sizeof(int);

	cudaCheckReturn(cudaMemcpy2D(cOnGPU,cPitch,C[0],rowSizeOnGPU,rowSizeOnGPU,nrOfEquations,cudaMemcpyHostToDevice));
	cudaCheckReturn(cudaMemcpy(bOnGPU,b,rowSizeOnGPU,cudaMemcpyHostToDevice));

	cudaCheckReturn(cudaMemset(xOnGPU,0,rowSizeOnGPU));

	while (++iters<MAX_ITERATIONS && jacobiIterate(cOnGPU,bOnGPU,xOnGPU,deltaXonGPU,nrOfEquations,pitchLength) > 0); // Iterate until convergence

	if (iters==MAX_ITERATIONS) {std::cerr << "Jacobi method did not converge" << std::endl;return -1;}

	cudaCheckReturn(cudaMemcpy(x,xOnGPU,rowSizeOnGPU,cudaMemcpyDeviceToHost));

	cudaCheckReturn(cudaFree(cOnGPU));
	cudaCheckReturn(cudaFree(bOnGPU));
	cudaCheckReturn(cudaFree(xOnGPU));
	cudaCheckReturn(cudaFree(deltaXonGPU));

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

	delete[] C[0];
	delete[] C;
	delete[] b;
	delete[] x;
}
