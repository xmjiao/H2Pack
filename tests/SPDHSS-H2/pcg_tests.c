#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "H2Pack.h"

#include "pcg.h"
#include "block_jacobi_precond.h"
#include "LRD_precond.h"
#include "FSAI_precond.h"

static DTYPE shift_;

void H2Pack_matvec(const void *h2pack_, const DTYPE *b, DTYPE *x)
{
    H2Pack_t h2pack = (H2Pack_t) h2pack_;
    H2P_matvec(h2pack, b, x);
    #pragma omp simd
    for (int i = 0; i < h2pack->krnl_mat_size; i++) x[i] += shift_ * b[i];
}

void block_jacobi_precond(const void *precond_, const DTYPE *b, DTYPE *x)
{
    block_jacobi_precond_t precond = (block_jacobi_precond_t) precond_;
    apply_block_jacobi_precond(precond, b, x);
}

void LRD_precond(const void *precond_, const DTYPE *b, DTYPE *x)
{
    LRD_precond_t precond = (LRD_precond_t) precond_;
    apply_LRD_precond(precond, b, x);
}

void FSAI_precond(const void *precond_, const DTYPE *b, DTYPE *x)
{
    FSAI_precond_t precond = (FSAI_precond_t) precond_;
    apply_FSAI_precond(precond, b, x);
}

void HSS_ULV_Chol_precond(const void *hssmat_, const DTYPE *b, DTYPE *x)
{
    H2Pack_t hssmat = (H2Pack_t) hssmat_;
    H2P_HSS_ULV_Cholesky_solve(hssmat, 3, b, x);
}

// Test preconditioned conjugate gradient solver with different preconditioner
void pcg_tests(
    const int krnl_mat_size, H2Pack_t h2mat, H2Pack_t hssmat, 
    const DTYPE shift, const int max_rank, const int max_iter, const DTYPE CG_tol
)
{
    DTYPE *x = malloc(sizeof(DTYPE) * krnl_mat_size);
    DTYPE *y = malloc(sizeof(DTYPE) * krnl_mat_size);
    assert(x != NULL && y != NULL);

    for (int i = 0; i < krnl_mat_size; i++) y[i] = 1.0; //drand48();

    int flag, iter;
    DTYPE relres;
    double st, et;

    shift_ = shift;

    printf("\nStarting PCG solve without preconditioner...\n");
    memset(x, 0, sizeof(DTYPE) * krnl_mat_size);
    st = get_wtime_sec();
    pcg(
        krnl_mat_size, CG_tol, max_iter, 
        H2Pack_matvec, h2mat, y, NULL, NULL, x,
        &flag, &relres, &iter, NULL
    );
    et = get_wtime_sec();
    printf("PCG stopped after %d iterations, relres = %e, used time = %.2lf sec\n", iter, relres, et - st);

    printf("\nConstructing block Jacobi preconditioner...");
    st = get_wtime_sec();
    block_jacobi_precond_t bj_precond;
    H2P_build_block_jacobi_precond(h2mat, shift, &bj_precond);
    et = get_wtime_sec();
    printf("done, used time = %.2lf sec\n", et - st);
    printf("Starting PCG solve with block Jacobi preconditioner...\n");
    memset(x, 0, sizeof(DTYPE) * krnl_mat_size);
    st = get_wtime_sec();
    pcg(
        krnl_mat_size, CG_tol, max_iter, 
        H2Pack_matvec, h2mat, y, block_jacobi_precond, bj_precond, x,
        &flag, &relres, &iter, NULL
    );
    et = get_wtime_sec();
    printf("PCG stopped after %d iterations, relres = %e, used time = %.2lf sec\n", iter, relres, et - st);
    free_block_jacobi_precond(bj_precond);

    printf("\nConstructing LRD preconditioner...");
    LRD_precond_t lrd_precond;
    st = get_wtime_sec();
    H2P_build_LRD_precond(h2mat, max_rank, shift, &lrd_precond);
    et = get_wtime_sec();
    printf("done, used time = %.2lf sec\n", et - st);
    printf("Starting PCG solve with LRD preconditioner...\n");
    memset(x, 0, sizeof(DTYPE) * krnl_mat_size);
    st = get_wtime_sec();
    pcg(
        krnl_mat_size, CG_tol, max_iter, 
        H2Pack_matvec, h2mat, y, LRD_precond, lrd_precond, x,
        &flag, &relres, &iter, NULL
    );
    et = get_wtime_sec();
    printf("PCG stopped after %d iterations, relres = %e, used time = %.2lf sec\n", iter, relres, et - st);
    free_LRD_precond(lrd_precond);

    printf("\nConstructing FSAI preconditioner...");
    st = get_wtime_sec();
    FSAI_precond_t fsai_precond;
    H2P_build_FSAI_precond(h2mat, max_rank, shift, &fsai_precond);
    et = get_wtime_sec();
    printf("done, used time = %.2lf sec\n", et - st);
    printf("Starting PCG solve with FSAI preconditioner...\n");
    memset(x, 0, sizeof(DTYPE) * krnl_mat_size);
    st = get_wtime_sec();
    pcg(
        krnl_mat_size, CG_tol, max_iter, 
        H2Pack_matvec, h2mat, y, FSAI_precond, fsai_precond, x,
        &flag, &relres, &iter, NULL
    );
    et = get_wtime_sec();
    printf("PCG stopped after %d iterations, relres = %e, used time = %.2lf sec\n", iter, relres, et - st);
    free_FSAI_precond(fsai_precond);

    printf("\nStarting PCG solve with SPDHSS preconditioner...\n");
    memset(x, 0, sizeof(DTYPE) * krnl_mat_size);
    st = get_wtime_sec();
    pcg(
        krnl_mat_size, CG_tol, max_iter, 
        H2Pack_matvec, h2mat, y, HSS_ULV_Chol_precond, hssmat, x,
        &flag, &relres, &iter, NULL
    );
    et = get_wtime_sec();
    printf("PCG stopped after %d iterations, relres = %e, used time = %.2lf sec\n", iter, relres, et - st);

    free(x);
    free(y);
}
