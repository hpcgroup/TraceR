#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <math.h>

/* We want to wrap entries around, and because mod operator % 
 * sometimes misbehaves on negative values. -1 maps to the highest value.*/
#define DO_COMM 1
#define wrap_x(a)	(((a)+num_blocks_x)%num_blocks_x)
#define wrap_y(a)	(((a)+num_blocks_y)%num_blocks_y)
#define calc_pe(a,b)	((a)*num_blocks_y+(b))

#define MAX_ITER        1
#define LEFT            1
#define RIGHT           2
#define TOP             3
#define BOTTOM          4


int main(int argc, char **argv) {
  int myRank, numPes;

  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &numPes);
  MPI_Comm_rank(MPI_COMM_WORLD, &myRank);
  MPI_Request req[4];
  MPI_Status status[4];

  int blockDimX, blockDimY, arrayDimX, arrayDimY;
  int noBarrier = 0;

  if (argc != 4 && argc != 6) {
    printf("%s [array_size] [block_size] +[no]barrier\n", argv[0]);
    printf("%s [array_size_X] [array_size_Y] [block_size_X] [block_size_Y] +[no]barrier\n", argv[0]);
    MPI_Abort(MPI_COMM_WORLD, -1);
  }

  if(argc == 4) {
    arrayDimY = arrayDimX = atoi(argv[1]);
    blockDimY = blockDimX = atoi(argv[2]);
    if(strcasecmp(argv[3], "+nobarrier") == 0)
      noBarrier = 1;
    else
      noBarrier = 0;
    if(noBarrier && myRank==0) printf("\nSTENCIL COMPUTATION WITH NO BARRIERS\n");
  }
  else {
    arrayDimX = atoi(argv[1]);
    arrayDimY = atoi(argv[2]);
    blockDimX = atoi(argv[3]);
    blockDimY = atoi(argv[4]);
    if(strcasecmp(argv[5], "+nobarrier") == 0)
      noBarrier = 1;
    else
      noBarrier = 0;
    if(noBarrier && myRank==0) printf("\nSTENCIL COMPUTATION WITH NO BARRIERS\n");
  }

  if (arrayDimX < blockDimX || arrayDimX % blockDimX != 0) {
    printf("array_size_X mod block_size_X != 0!\n");
    MPI_Abort(MPI_COMM_WORLD, -1);
  }
  if (arrayDimY < blockDimY || arrayDimY % blockDimY != 0) {
    printf("array_size_Y mod block_size_Y != 0!\n");
    MPI_Abort(MPI_COMM_WORLD, -1);
  }

  int num_blocks_x = arrayDimX / blockDimX;
  int num_blocks_y = arrayDimY / blockDimY;

  int iterations = 0, i, j;
  double error = 1.0, max_error = 0.0;

  if(myRank == 0) {
    printf("Running Jacobi on %d processors with (%d, %d) elements\n", numPes, num_blocks_x, num_blocks_y);
    printf("Array Dimensions: %d %d\n", arrayDimX, arrayDimY);
    printf("Block Dimensions: %d %d\n", blockDimX, blockDimY);
  }

  double *dataX = new double[blockDimX];
  double *dataY = new double[blockDimY];

  int myRow = myRank / num_blocks_y;
  int myCol = myRank % num_blocks_y;

  AMPI_Set_startevent(MPI_COMM_WORLD);
  for(; iterations < 5; iterations++) {
#if DO_COMM
    /* Receive my right, left, bottom and top edge */
    MPI_Irecv(dataX, blockDimX, MPI_DOUBLE, calc_pe(myRow, wrap_y(myCol+1)), RIGHT, MPI_COMM_WORLD, &req[RIGHT-1]);
    MPI_Irecv(dataX, blockDimX, MPI_DOUBLE, calc_pe(myRow, wrap_y(myCol-1)), LEFT, MPI_COMM_WORLD, &req[LEFT-1]);
    MPI_Irecv(dataY, blockDimY, MPI_DOUBLE, calc_pe(wrap_x(myRow+1), myCol), BOTTOM, MPI_COMM_WORLD, &req[BOTTOM-1]);
    MPI_Irecv(dataY, blockDimY, MPI_DOUBLE, calc_pe(wrap_x(myRow-1), myCol), TOP, MPI_COMM_WORLD, &req[TOP-1]);


    /* Send my left, right, top and bottom edge */
    MPI_Send(dataX, blockDimX, MPI_DOUBLE, calc_pe(myRow, wrap_y(myCol-1)), RIGHT, MPI_COMM_WORLD);
    MPI_Send(dataX, blockDimX, MPI_DOUBLE, calc_pe(myRow, wrap_y(myCol+1)), LEFT, MPI_COMM_WORLD);
    MPI_Send(dataY, blockDimY, MPI_DOUBLE, calc_pe(wrap_x(myRow-1), myCol), BOTTOM, MPI_COMM_WORLD);
    MPI_Send(dataY, blockDimY, MPI_DOUBLE, calc_pe(wrap_x(myRow+1), myCol), TOP, MPI_COMM_WORLD);

    MPI_Waitall(4, req, status);
#endif

#if !DO_COMM
    printf("%d %d %zd\n", myRank, calc_pe(myRow, wrap_y(myCol-1)), sizeof(double)*blockDimX);
    printf("%d %d %zd\n", myRank, calc_pe(myRow, wrap_y(myCol+1)), sizeof(double)*blockDimX);
    printf("%d %d %zd\n", myRank, calc_pe(wrap_x(myRow-1), myCol), sizeof(double)*blockDimY);
    printf("%d %d %zd\n", myRank, calc_pe(wrap_x(myRow+1), myCol), sizeof(double)*blockDimY);
#endif
  } /* end of while loop */

  if(myRank == 0) {
    printf("Completed %d iterations\n", iterations);
  }

  MPI_Finalize();
  return 0;
} /* end function main */

