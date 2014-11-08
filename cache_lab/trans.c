/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 *
 * @author Wei-Lin Tsai - weilints@andrew.cmu.edu
 * @bug No known bugs.
 */ 
#include <stdio.h>
#include "cachelab.h"
#include "contracts.h"

/** Functions declarations */
int is_transpose(int M, int N, int A[N][M], int B[M][N]);
void trans(int M, int N, int A[N][M], int B[M][N]);
/** Customize for size 32 * 32 */
void transpose_32_32(int M, int N, int A[N][M], int B[M][N]);
/** Customize for size 64 * 64 */
void transpose_64_64(int M, int N, int A[N][M], int B[M][N]);
/** Customize for size 61 * 67 */
void transpose_61_67(int M, int N, int A[N][M], int B[M][N]);
/** Use Z shape to fill 8*8 matrix, used for 64*64 case */
void transpose_z_8_8(int M, int N, int A[N][M], int B[M][N],\
                     int ib, int jb);
/** Use speical method to fill 8*8 matrix when ib == jb,
 * used for 64*64 case */
void transpose_diago_8_8(int M, int N, int A[N][M], int B[M][N],\
                         int ib, int jb);

/** Constant Define */
#define STEP 8      ///< 1 block == 32 bytes == 8 int
#define HALF_STEP 4
#define TWO_STEP 16 ///< for 61 * 67 use

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. The REQUIRES and ENSURES from 15-122 are included
 *     for your convenience. They can be removed if you like.
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    REQUIRES(M > 0);
    REQUIRES(N > 0);

    // call specific function according to its size 
    if ((M == 32) && (N==32)){
        transpose_32_32(M, N, A, B);
    } else if ((M == 64) && (N==64)){
        transpose_64_64(M, N, A, B);
    } else if ((M == 61) && (N==67)){
        transpose_61_67(M, N, A, B);   
    } else {
        // otherwise, call normal one
        trans(M, N, A, B);
    }

    ENSURES(is_transpose(M, N, A, B));
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */

/**
 * @brief transpose 32*32 matrix
 *
 * Do the transpose by each 8*8 matrix
 *
 * @param M int, A's column size, need to be 32
 * @param N int, A's row size, need to be 32
 * @param A input array
 * @param B transposed array of A
 */
void transpose_32_32(int M, int N, int A[N][M], int B[M][N]){
    int ib, jb;   ///< i block, j block
    int i, j;
    int tmpVal = 0;
    int isDiagonal = 0;

    REQUIRES(M == 32);
    REQUIRES(N == 32);

    for (ib = 0; ib < N; ib += STEP){
        for (jb = 0; jb < M; jb += STEP){
            /** Now do the transpose with in a 8*8 block */
            for ( i = ib; i < ib + STEP; i++){
                for ( j = jb; j < jb + STEP; j++){
                    // please note if we do not handle diagonal 
                    // it will introdue extra conflict miss
                    if ( i == j ) {    // diagonal
                        tmpVal = A[i][j];
                        isDiagonal = 1; 
                    }
                    else {
                        B[j][i] = A[i][j];
                    }
                }
                if ( isDiagonal ) {
                    B[i][i] = tmpVal;
                    isDiagonal = 0;
                }
            }
        }
    }
}

/**
 * @brief transpose 64*64 matrix
 *
 * Do the transpose by each 8*8 matrix
 * For each 8*8, do the specail handling.
 *   diagonal: call transpose_z_8_8
 *   normal:   call transpose_diago_8_8
 *
 * @param M int, A's column size, need to be 64
 * @param N int, A's row size, need to be 64
 * @param A input array
 * @param B transposed array of A
 */
void transpose_64_64(int M, int N, int A[N][M], int B[M][N]){
    int ib, jb;   ///< i block, j block

    REQUIRES(M == 64);
    REQUIRES(N == 64);

    for (ib = 0; ib < N; ib += STEP){
        for (jb = 0; jb < M; jb += STEP){
            if ( ib == jb ){
                transpose_diago_8_8(M,N,A,B,ib,jb);
            } else {
                transpose_z_8_8(M,N,A,B,ib,jb);
            }
        }
    }
}

/**
 * @brief transpose 61*67 matrix
 *
 * Do the transpose by each 16*16 matrix
 *
 * @param M int, A's column size, need to be 61
 * @param N int, A's row size, need to be 67
 * @param A input array
 * @param B transpose array of A
 */
void transpose_61_67(int M, int N, int A[N][M], int B[M][N]){
    int ib, jb;   ///< i block, j block
    int i, j;
    int tmpVal = 0;
    int isDiagonal = 0;

    REQUIRES(M == 61);
    REQUIRES(N == 67);

    for (ib = 0; ib < N; ib += TWO_STEP){
        for (jb = 0; jb < M; jb += TWO_STEP){
            /** Now do the transpose with in a 8*8 block */
            for ( i = ib; (i < ib + TWO_STEP) && ( i < N); i++){
                for ( j = jb; (j < jb + TWO_STEP) && (j < M); j++){
                    // please note if we do not handle diagonal 
                    // it will introdue extra conflict miss
                    if ( i == j ) {    // diagonal
                        tmpVal = A[i][j];
                        isDiagonal = 1; 
                    }
                    else {
                        B[j][i] = A[i][j];
                    }
                }
                if ( isDiagonal ) {
                    B[i][i] = tmpVal;
                    isDiagonal = 0;
                }
            }
        }
    }
}

/**
 * @brief transpose 8*8 matrix using 4 4*4 sub array
 *
 * Do transpose one 8*8 sub-matrix of 64*64 matrix
 * When the sub-matrix is not diagonal
 * See comments inside for details
 *
 * @param M int, A's column size, need to be 64
 * @param N int, A's row size, need to be 64
 * @param A input array
 * @param B transposed array of A
 * @param ib int, the i-index of sub-matrix, need to be 0 ~ 56 
 * @param ib int, the j-index of sub-matrix, need to be 0 ~ 56
 */
void transpose_z_8_8(int M, int N, int A[N][M], int B[M][N],\
                     int ib, int jb) {
    /* 8*8 can be four 4*4, denoted as Z1 ~ Z4
     * Z1 Z2
     * Z3 Z4, the trans order is Z1->Z2->Z4->Z3
     * */
    int i, j;

    REQUIRES(M == 64);
    REQUIRES(N == 64);
    REQUIRES((ib >= 0) && (ib <= 56));
    REQUIRES((jb >= 0) && (jb <= 56));

    // Z1 
    for ( i = ib; i < ib + HALF_STEP; i++){
        for ( j = jb; j < jb + HALF_STEP; j++){
            B[j][i] = A[i][j];
        }
    }
    
    // Z2
    for ( i = ib; i < ib + HALF_STEP; i++){
        for ( j = jb + HALF_STEP; j < jb + STEP; j++){
            B[j][i] = A[i][j];
        }
    }
    
    // Z4
    for ( i = ib + HALF_STEP; i < ib + STEP; i++){
        for ( j = jb + HALF_STEP; j < jb + STEP; j++){
            B[j][i] = A[i][j];
        }
    }
  
    // Z3 
    for ( i = ib + HALF_STEP; i < ib + STEP; i++){
        for ( j = jb; j < jb + HALF_STEP; j++){
            B[j][i] = A[i][j];
        }
    } 
}

/**
 * @brief transpose 8*8 matrix using 4 4*4 sub array
 *        with hand coded order 
 *
 * Do transpose one 8*8 sub-matrix of 64*64 matrix when
 * the sub-matrix is diagonal
 * 
 * It's hand coded, see comments inside for details
 *
 * @param M int, A's column size, need to be 64
 * @param N int, A's row size, need to be 64
 * @param A input array
 * @param B transposed array of A
 * @param ib int, the i-index of sub-matrix, need to be 0 ~ 56
 * @param ib int, the j-index of sub-matrix, need to be 0 ~ 56
 *
 * @attention ib need to be equivalent to jb
 */
void transpose_diago_8_8(int M, int N, int A[N][M], int B[M][N],\
                         int ib, int jb) {
    /* 8*8 can be four 4*4, denoted as Z1 ~ Z4
     * Z1 Z2
     * Z3 Z4, the trans order is Z1->Z3->Z2->Z4
     * but with special handling
     *
     * For easy memory, let array be 
     *  1  2  3  4 
     *  5  6  7  8
     *  9 10 11 12
     * 13 14 15 16 
     * */
    int tmpVal = 0;
    int tmp1 = 0;
    int tmp2 = 0;
    int tmp3 = 0;
    int tmp4 = 0;
    int tmp5 = 0;
    int tmp6 = 0;
    int tmp7 = 0;
    int tmp8 = 0;

    REQUIRES(M == 64);
    REQUIRES(N == 64);
    REQUIRES((ib >= 0) && (ib <= 56));
    REQUIRES((jb >= 0) && (jb <= 56));
    REQUIRES((jb == ib));

    // Z1
    // load 1 2 3 4 (A Row 0)
    tmp1 = A[ib][jb  ];  // 1
    tmp2 = A[ib][jb+1];  // 2
    tmp3 = A[ib][jb+2];  // 3
    tmp4 = A[ib][jb+3];  // 4
    // load 5 9 13 (B Row 0) 
    tmp5 = A[ib+1][jb];  // 5
    tmp6 = A[ib+2][jb];  // 9
    tmp7 = A[ib+3][jb];  // 13
    // write 1 5 9 13 (B Row 0)
    B[ib][jb  ] = tmp1;
    B[ib][jb+1] = tmp5;
    B[ib][jb+2] = tmp6;
    B[ib][jb+3] = tmp7;
    // load 6 7 8 (A Row 1)
    tmp1 = A[ib+1][jb+1];  // 6
    tmp5 = A[ib+1][jb+2];  // 7
    tmp6 = A[ib+1][jb+3];  // 8
    // load 10 14 (B Row 1)
    tmp7 = A[ib+2][jb+1];  // 10 
    tmp8 = A[ib+3][jb+1];  // 14
    // write 2 6 10 14 (B Row 1)
    B[ib+1][jb  ] = tmp2;  // 2
    B[ib+1][jb+1] = tmp1;  // 6
    B[ib+1][jb+2] = tmp7;  // 10
    B[ib+1][jb+3] = tmp8;  // 14
    // load 11 12 (A Row 2)  
    tmp1 = A[ib+2][jb+2];  // 11
    tmp2 = A[ib+2][jb+3];  // 12
    // load 15 (B Row 2)
    tmp7 = A[ib+3][jb+2];  // 15
    // write 3 7 11 15 (B Row 2)
    B[ib+2][jb  ] = tmp3;  // 3
    B[ib+2][jb+1] = tmp5;  // 7
    B[ib+2][jb+2] = tmp1;  // 11
    B[ib+2][jb+3] = tmp7;  // 15
    // load 16
    tmp1 = A[ib+3][jb+3];  // 16
    // write 4 8 12 16 (B Row 3)
    B[ib+3][jb  ] = tmp4;  // 4
    B[ib+3][jb+1] = tmp6;  // 8
    B[ib+3][jb+2] = tmp2;  // 12
    B[ib+3][jb+3] = tmp1;  // 16
    
    // Z3
    // deal row 0 of Z3
    tmpVal        = A[ib+4][jb  ];
    B[jb+1][ib+4] = A[ib+4][jb+1];
    B[jb+2][ib+4] = A[ib+4][jb+2];
    B[jb+3][ib+4] = A[ib+4][jb+3];
    B[jb  ][ib+4] = tmpVal;
    // deal row 1 of Z3
    B[jb  ][ib+5] = A[ib+5][jb  ];
    tmpVal        = A[ib+5][jb+1];
    B[jb+2][ib+5] = A[ib+5][jb+2];
    B[jb+3][ib+5] = A[ib+5][jb+3];
    B[ib+1][ib+5] = tmpVal;
    // deal row 2 of Z3
    B[jb  ][ib+6] = A[ib+6][jb  ];
    B[jb+1][ib+6] = A[ib+6][jb+1];
    tmpVal        = A[ib+6][jb+2];
    B[jb+3][ib+6] = A[ib+6][jb+3];
    B[ib+2][ib+6] = tmpVal;
    // deal row 3 of Z3
    B[jb  ][ib+7] = A[ib+7][jb  ];
    B[jb+1][ib+7] = A[ib+7][jb+1];
    B[jb+2][ib+7] = A[ib+7][jb+2];
    tmpVal        = A[ib+7][jb+3];
    B[ib+3][ib+7] = tmpVal;

    // Z2
    // load 1 2 3 4 (A Row 0)
    tmp1 = A[ib][jb+4];  // 1
    tmp2 = A[ib][jb+5];  // 2
    tmp3 = A[ib][jb+6];  // 3
    tmp4 = A[ib][jb+7];  // 4
    // load 5 9 13 (B Row 0) 
    tmp5 = A[ib+1][jb+4];  // 5
    tmp6 = A[ib+2][jb+4];  // 9
    tmp7 = A[ib+3][jb+4];  // 13
    // write 1 5 9 13 (B Row 0)
    B[ib+4][jb] = tmp1;
    B[ib+4][jb+1] = tmp5;
    B[ib+4][jb+2] = tmp6;
    B[ib+4][jb+3] = tmp7;
    // load 6 7 8 (A Row 1)
    tmp1 = A[ib+1][jb+5];  // 6
    tmp5 = A[ib+1][jb+6];  // 7
    tmp6 = A[ib+1][jb+7];  // 8
    // load 10 14 (B Row 1)
    tmp7 = A[ib+2][jb+5];  // 10 
    tmp8 = A[ib+3][jb+5];  // 14
    // write 2 6 10 14 (B Row 1)
    B[ib+5][jb] = tmp2;  // 2
    B[ib+5][jb+1] = tmp1;  // 6
    B[ib+5][jb+2] = tmp7;  // 10
    B[ib+5][jb+3] = tmp8;  // 14
    // load 11 12 (A Row 2)  
    tmp1 = A[ib+2][jb+6];  // 11
    tmp2 = A[ib+2][jb+7];  // 12
    // load 15 (B Row 2)
    tmp7 = A[ib+3][jb+6];  // 15
    // write 3 7 11 15 (B Row 2)
    B[ib+6][jb] = tmp3;  // 3
    B[ib+6][jb+1] = tmp5;  // 7
    B[ib+6][jb+2] = tmp1;  // 11
    B[ib+6][jb+3] = tmp7;  // 15
    // load 16
    tmp1 = A[ib+3][jb+7];  // 16
    // write 4 8 12 16 (B Row 3)
    B[ib+7][jb] = tmp4;  // 4
    B[ib+7][jb+1] = tmp6;  // 8
    B[ib+7][jb+2] = tmp2;  // 12
    B[ib+7][jb+3] = tmp1;  // 16

    // Z4
    // deal row 0 of Z4
    tmpVal      = A[ib+4][jb+4];
    B[jb+5][ib+4] = A[ib+4][jb+5];
    B[jb+6][ib+4] = A[ib+4][jb+6];
    B[jb+7][ib+4] = A[ib+4][jb+7];
    B[jb+4][ib+4] = tmpVal;
    // deal row 1 of Z4
    B[jb+4][ib+5] = A[ib+5][jb+4];
    tmpVal        = A[ib+5][jb+5];
    B[jb+6][ib+5] = A[ib+5][jb+6];
    B[jb+7][ib+5] = A[ib+5][jb+7];
    B[ib+5][ib+5] = tmpVal;
    // deal row 2 of Z4
    B[jb+4][ib+6] = A[ib+6][jb+4];
    B[jb+5][ib+6] = A[ib+6][jb+5];
    tmpVal        = A[ib+6][jb+6];
    B[jb+7][ib+6] = A[ib+6][jb+7];
    B[ib+6][ib+6] = tmpVal;
    // deal row 3 of Z4
    B[jb+4][ib+7] = A[ib+7][jb+4];
    B[jb+5][ib+7] = A[ib+7][jb+5];
    B[jb+6][ib+7] = A[ib+7][jb+6];
    tmpVal        = A[ib+7][jb+7];
    B[ib+7][ib+7] = tmpVal;
}


/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    REQUIRES(M > 0);
    REQUIRES(N > 0);

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

    ENSURES(is_transpose(M, N, A, B));
}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

