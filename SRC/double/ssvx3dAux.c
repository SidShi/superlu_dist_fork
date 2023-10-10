#include <stdlib.h>  // For NULL
#include <mpi.h>



#define LOG_FUNC_ENTER() printf("\033[1;32mEntering function %s at %s:%d\033[0m\n", __func__, __FILE__, __LINE__)

/**
 * @brief Validates the input parameters for a given problem.
 *
 * This function checks the input parameters for a given problem and sets the
 * error code in the 'info' variable accordingly. If there is an error, it
 * prints an error message and returns.
 *
 * @param[in] options Pointer to the options structure containing Fact, RowPerm, ColPerm, and IterRefine values.
 * @param[in] A Pointer to the matrix A structure containing nrow, ncol, Stype, Dtype, and Mtype values.
 * @param[in] ldb The leading dimension of the array B.
 * @param[in] nrhs The number of right-hand sides.
 * @param[in] grid Pointer to the grid structure.
 * @param[out] info Pointer to an integer variable that stores the error code.
 */
void validateInput_ssvx3d(superlu_dist_options_t *options, SuperMatrix *A,int ldb, int_t nrhs, gridinfo3d_t *grid3d, int *info)
{
    gridinfo_t *grid = &(grid3d->grid2d);
    NRformat_loc *Astore = A->Store;
    int Fact = options->Fact;
    if (Fact < 0 || Fact > FACTORED)
        *info = -1;
    else if (options->RowPerm < 0 || options->RowPerm > MY_PERMR)
        *info = -1;
    else if (options->ColPerm < 0 || options->ColPerm > MY_PERMC)
        *info = -1;
    else if (options->IterRefine < 0 || options->IterRefine > SLU_EXTRA)
        *info = -1;
    else if (options->IterRefine == SLU_EXTRA)
    {
        *info = -1;
        fprintf(stderr,
                "Extra precise iterative refinement yet to support.");
    }
    else if (A->nrow != A->ncol || A->nrow < 0 || A->Stype != SLU_NR_loc || A->Dtype != SLU_D || A->Mtype != SLU_GE)
        *info = -2;
    else if (ldb < Astore->m_loc)
        *info = -5;
    else if (nrhs < 0)
    {
        *info = -6;
    }
    if (*info)
    {
        int i = -(*info);
        pxerr_dist("pdgssvx3d", grid, -(*info));
        return;
    }
} 


void scaleRows(int_t m_loc, int_t fst_row, int_t *rowptr, double *a, double *R) {
    int_t irow = fst_row;
    for (int_t j = 0; j < m_loc; ++j) {
        for (int_t i = rowptr[j]; i < rowptr[j + 1]; ++i) {
            a[i] *= R[irow];
        }
        ++irow;
    }
}

void scaleColumns(int_t m_loc, int_t *rowptr, int_t *colind, double *a, double *C) {
    int_t icol;
    for (int_t j = 0; j < m_loc; ++j) {
        for (int_t i = rowptr[j]; i < rowptr[j + 1]; ++i) {
            icol = colind[i];
            a[i] *= C[icol];
        }
    }
}

void scaleBoth(int_t m_loc, int_t fst_row, int_t *rowptr, 
    int_t *colind, double *a, double *R, double *C) {
    int_t irow = fst_row;
    int_t icol;
    for (int_t j = 0; j < m_loc; ++j) {
        for (int_t i = rowptr[j]; i < rowptr[j + 1]; ++i) {
            icol = colind[i];
            a[i] *= R[irow] * C[icol];
        }
        ++irow;
    }
}

void scalePrecomputed(SuperMatrix *A, dScalePermstruct_t *ScalePermstruct) {
    NRformat_loc *Astore = (NRformat_loc *)A->Store;
    int_t m_loc = Astore->m_loc;
    int_t fst_row = Astore->fst_row;
    double *a = (double *)Astore->nzval;
    int_t *rowptr = Astore->rowptr;
    int_t *colind = Astore->colind;
    double *R = ScalePermstruct->R;
    double *C = ScalePermstruct->C;
    switch (ScalePermstruct->DiagScale) {
    case NOEQUIL:
        break;
    case ROW:
        scaleRows(m_loc, fst_row, rowptr, a, R);
        break;
    case COL:
        scaleColumns(m_loc, rowptr, colind, a, C);
        break;
    case BOTH:
        scaleBoth(m_loc, fst_row, rowptr, colind, a, R, C);
        break;
    default:
        break;
    }
}

void scaleFromScratch(
    SuperMatrix *A, dScalePermstruct_t *ScalePermstruct,  
    gridinfo_t *grid, int_t *rowequ, int_t *colequ, int_t*iinfo)  
{
    NRformat_loc *Astore = (NRformat_loc *)A->Store;
    int_t m_loc = Astore->m_loc;
    int_t fst_row = Astore->fst_row;
    double *a = (double *)Astore->nzval;
    int_t *rowptr = Astore->rowptr;
    int_t *colind = Astore->colind;
    double *R = ScalePermstruct->R;
    double *C = ScalePermstruct->C;
    double rowcnd, colcnd, amax;
    // int_t iinfo;
    char equed[1];
    int iam = grid->iam;

    pdgsequ(A, R, C, &rowcnd, &colcnd, &amax, iinfo, grid);

    if (*iinfo > 0) {
#if (PRNTlevel >= 1)
        fprintf(stderr, "The " IFMT "-th %s of A is exactly zero\n", *iinfo <= m_loc ? *iinfo : *iinfo - m_loc, *iinfo <= m_loc ? "row" : "column");
#endif
    } else if (*iinfo < 0) {
        return;
    }

    pdlaqgs(A, R, C, rowcnd, colcnd, amax, equed);

    if      (strncmp(equed, "R", 1) == 0) { ScalePermstruct->DiagScale = ROW; *rowequ = 1; *colequ = 0; }
    else if (strncmp(equed, "C", 1) == 0) { ScalePermstruct->DiagScale = COL; *rowequ = 0; *colequ = 1; }
    else if (strncmp(equed, "B", 1) == 0) { ScalePermstruct->DiagScale = BOTH; *rowequ = 1; *colequ = 1; }
    else                                  { ScalePermstruct->DiagScale = NOEQUIL; *rowequ = 0; *colequ = 0; }

#if (PRNTlevel >= 1)
    if (iam == 0) {
        printf(".. equilibrated? *equed = %c\n", *equed);
        fflush(stdout);
    }
#endif
}

void scaleMatrixDiagonally(fact_t Fact, dScalePermstruct_t *ScalePermstruct, 
                           SuperMatrix *A, SuperLUStat_t *stat, gridinfo_t *grid,
                            int_t *rowequ, int_t *colequ, int_t*iinfo)   
{
    int iam = grid->iam;

#if (DEBUGlevel >= 1)
    CHECK_MALLOC(iam, "Enter equil");
#endif

    double t_start = SuperLU_timer_();

    if (Fact == SamePattern_SameRowPerm) {
        scalePrecomputed(A, ScalePermstruct);
    } else {
        scaleFromScratch(A, ScalePermstruct, grid, rowequ, colequ, iinfo);
    }

    stat->utime[EQUIL] = SuperLU_timer_() - t_start;

#if (DEBUGlevel >= 1)
    CHECK_MALLOC(iam, "Exit equil");
#endif
}

/**
 * Performs a row permutation operation on a sparse matrix (CSC format)
 * using a user-provided permutation array.
 *
 * @param colptr The column pointer array of the sparse matrix (CSC format).
 * @param rowind The row index array of the sparse matrix (CSC format).
 * @param perm_r The user-provided permutation array for the rows.
 * @param n The number of columns in the sparse matrix.
 */
void applyRowPerm(int_t* colptr, int_t* rowind, int_t* perm_r, int_t n) {
    // Check input parameters
    if (colptr == NULL || rowind == NULL || perm_r == NULL) {
        fprintf(stderr, "Error: NULL input parameter.\n");
        return;
    }

    // Iterate through each non-zero element of the sparse matrix
    for (int_t i = 0; i < colptr[n]; ++i) {
        // Get the original row index
        int_t irow = rowind[i];
        // Assign the new row index from the user-provided permutation array
        rowind[i] = perm_r[irow];
    }
}










/**
 * Finds row permutations using the MC64 algorithm in a distributed manner.
 *
 * @param grid The grid info object, which includes the current node's information and MPI communicator.
 * @param job The type of job to be done.
 * @param m The number of rows in the sparse matrix.
 * @param n The number of columns in the sparse matrix.
 * @param nnz The number of non-zero elements in the sparse matrix.
 * @param colptr The column pointer array of the sparse matrix (CSC format).
 * @param rowind The row index array of the sparse matrix (CSC format).
 * @param a_GA The non-zero values of the sparse matrix.
 * @param Equil The equilibration flag.
 * @param perm_r The output permutation array for the rows.
 * @param R1 The output row scaling factors.
 * @param C1 The output column scaling factors.
 * @param iinfo The output status code.
 */
void findRowPerm_MC64(gridinfo_t* grid, int_t job,
                      int_t m, int_t n,
                      int_t nnz,
                      int_t* colptr,
                      int_t* rowind,
                      double* a_GA,
                      int_t Equil,
                      int_t* perm_r,
                      double* R1,
                      double* C1,
                      int_t* iinfo) {
    #if ( DEBUGlevel>=1 )                    
    LOG_FUNC_ENTER();
    #endif
    // Check input parameters
    if (colptr == NULL || rowind == NULL || a_GA == NULL || 
        perm_r == NULL ) {
        fprintf(stderr, "Error: NULL input parameter.\n");
        return;
    }

    int root = 0;

    // If the current node is the root node, perform the permutation computation
    if (grid->iam == root) {
        *iinfo = dldperm_dist(job, m, nnz, colptr, rowind, a_GA, perm_r, R1, C1);
    }

    // Broadcast the status code to all other nodes in the communicator
    MPI_Bcast(iinfo, 1, mpi_int_t, root, grid->comm);

    // If the computation was successful
    if (*iinfo == 0) {
        // Broadcast the resulting permutation array to all other nodes
        MPI_Bcast(perm_r, m, mpi_int_t, root, grid->comm);

        // If job == 5 and Equil == true, broadcast the scaling factors as well
        if (job == 5 && Equil) {
            MPI_Bcast(R1, m, MPI_DOUBLE, root, grid->comm);
            MPI_Bcast(C1, n, MPI_DOUBLE, root, grid->comm);
        }
    }
}


/**
 * This function scales a distributed matrix. 
 *
 
 * @param[in]   rowequ  A flag indicating whether row should be equalized.
 * @param[in]   colequ  A flag indicating whether column should be equalized.
 * @param[in]   m       Number of rows in the matrix.
 * @param[in]   n       Number of columns in the matrix.
 * @param[in]   m_loc   The local row dimension.
 * @param[in]   rowptr  Pointer to the array holding row pointers.
 * @param[in]   colind  Pointer to the array holding column indices.
 * @param[in]   fst_row The first row of the local block.
 * @param[in,out] a     Pointer to the array holding the values of the matrix.
 * @param[in,out] R     Pointer to the row scaling factors.
 * @param[in,out] C     Pointer to the column scaling factors.
 * @param[in,out] R1    Pointer to the array holding the new row scaling factors.
 * @param[in,out] C1    Pointer to the array holding the new column scaling factors.
 */
void scale_distributed_matrix(int_t rowequ, int_t colequ, int_t m, int_t n,
 int_t m_loc, int_t *rowptr, int_t *colind, int_t fst_row, double *a,
  double *R, double *C, double *R1, double *C1) 
{
    #if ( DEBUGlevel>=1 )                    
    LOG_FUNC_ENTER();
    #endif    
    // Scale the row and column factors
    for (int i = 0; i < n; ++i) {
        R1[i] = exp(R1[i]);
        C1[i] = exp(C1[i]);
    }

    // Scale the elements of the matrix
    int rowIndex = fst_row;
    for (int j = 0; j < m_loc; ++j) {
        for (int i = rowptr[j]; i < rowptr[j + 1]; ++i) {
            int columnIndex = colind[i];
            a[i] *= R1[rowIndex] * C1[columnIndex];
#if 0
// this is not support as dmin, dsum and dprod are not used later in pdgssvx3d 
#if (PRNTlevel >= 2)
            if (perm_r[irow] == icol)
            {
                /* New diagonal */
                if (job == 2 || job == 3)
                    dmin = SUPERLU_MIN(dmin, fabs(a[i]));
                else if (job == 4)
                    dsum += fabs(a[i]);
                else if (job == 5)
                    dprod *= fabs(a[i]);
            }
#endif
#endif
        }
        ++rowIndex;
    }

    // Scale the row factors
    for (int i = 0; i < m; ++i)
        R[i] = (rowequ) ? R[i] * R1[i] : R1[i];

    // Scale the column factors
    for (int i = 0; i < n; ++i)
        C[i] = (colequ) ? C[i] * C1[i] : C1[i];
}


/**
 * Performs a permutation operation on the rows of a sparse matrix (CSC format).
 *
 * @param m The number of rows in the sparse matrix.
 * @param n The number of columns in the sparse matrix.
 * @param colptr The column pointer array of the sparse matrix (CSC format).
 * @param rowind The row index array of the sparse matrix (CSC format).
 * @param perm_r The permutation array for the rows.
 */
void permute_global_A(int_t m, int_t n, int_t *colptr, int_t *rowind, int_t *perm_r) {
    // Check input parameters
    if (colptr == NULL || rowind == NULL || perm_r == NULL) {
        fprintf(stderr, "Error: NULL input parameter to: permute_global_A()\n");
        return;
    }
    
    // Iterate through each column
    for (int_t j = 0; j < n; ++j) {
        // For each column, iterate through its non-zero elements
        for (int_t i = colptr[j]; i < colptr[j + 1]; ++i) {
            // Get the original row index
            int_t irow = rowind[i];
            // Assign the new row index from the permutation array
            rowind[i] = perm_r[irow];
        }
    }
}


/**
 * @brief Performs a set of operations on distributed matrices including finding row permutations, scaling, and permutation of global A. 
 * The operations depend on job and iinfo parameters.
 * 
 * @param[in]     options                SuperLU options.
 * @param[in]     Fact                   Factored form of the matrix.
 * @param[in,out] ScalePermstruct        Scaling and Permutation structure. 
 * @param[in,out] LUstruct               LU decomposition structure.
 * @param[in]     m                      Number of rows in the matrix.
 * @param[in]     n                      Number of columns in the matrix.
 * @param[in]     grid                   Grid information for distributed computation.
 * @param[in,out] A                      SuperMatrix A to be operated upon.
 * @param[in,out] GA                     SuperMatrix GA to be operated upon.
 * @param[out]    stat                   SuperLU statistics object to record factorization statistics.
 * @param[in]     job                    The type of job to be done.
 * @param[in]     Equil                  The equilibration flag.
 * @param[in]     rowequ                 Flag indicating whether rows of the matrix should be equalized.
 * @param[in]     colequ                 Flag indicating whether columns of the matrix should be equalized.
 * @param[out]    iinfo                  The output status code.
 *
 * @note The functions findRowPerm_MC64, scale_distributed_matrix and permute_global_A are called in this function.
 */
void perform_LargeDiag_MC64(
    superlu_dist_options_t *options, fact_t Fact,
    dScalePermstruct_t *ScalePermstruct, dLUstruct_t *LUstruct,
    int_t m, int_t n, gridinfo_t *grid, 
    SuperMatrix *A, SuperMatrix *GA, SuperLUStat_t *stat, int_t job, 
    int_t Equil, int_t *rowequ, int_t *colequ, int_t *iinfo) {
    double *R1 = NULL;
    double *C1 = NULL;

    int_t *perm_r = ScalePermstruct->perm_r;
    int_t *perm_c = ScalePermstruct->perm_c;
    int_t *etree = LUstruct->etree;
    double *R = ScalePermstruct->R;
    double *C = ScalePermstruct->C;
    int iam = grid->iam;


    NRformat_loc *Astore = (NRformat_loc *)A->Store;
    int_t nnz_loc = (Astore)->nnz_loc;
    int_t m_loc = (Astore)->m_loc;
    int_t fst_row = (Astore)->fst_row;
    double *a = (double *)(Astore)->nzval;
    int_t *rowptr = (Astore)->rowptr;
    int_t *colind = (Astore)->colind;

    NCformat *GAstore = (NCformat *)GA->Store;
    int_t *colptr = (GAstore)->colptr;
    int_t *rowind = (GAstore)->rowind;
    int_t nnz = (GAstore)->nnz;
    double *a_GA = (double *)(GAstore)->nzval;

    if (job == 5) {
        R1 = doubleMalloc_dist(m);
        if (!R1)
            ABORT("SUPERLU_MALLOC fails for R1[]");
        C1 = doubleMalloc_dist(n);
        if (!C1)
            ABORT("SUPERLU_MALLOC fails for C1[]");
    }

    // int iinfo;
    findRowPerm_MC64(grid, job, m, n,
    nnz,
    colptr,
    rowind,
     a_GA, Equil, perm_r, R1, C1, iinfo);

    if (*iinfo && job == 5) {
        SUPERLU_FREE(R1);
        SUPERLU_FREE(C1);
    }
#if (PRNTlevel >= 2)
    double dmin = damch_dist("Overflow");
    double dsum = 0.0;
    double dprod = 1.0;
#endif
    if (*iinfo == 0) {
        if (job == 5) {
            /* Scale the distributed matrix further.
									   A <-- diag(R1)*A*diag(C1)            */
            if(Equil)
            {
                scale_distributed_matrix( *rowequ, *colequ, m, n, m_loc, rowptr, colind, fst_row, a, R, C, R1, C1);
                ScalePermstruct->DiagScale = BOTH;
                *rowequ = *colequ = 1;
            } /* end if Equil */
            permute_global_A( m, n, colptr, rowind, perm_r);
            SUPERLU_FREE(R1);
            SUPERLU_FREE(C1);
        } else {
            permute_global_A( m, n, colptr, rowind, perm_r);
        }
    }
    else
    { /* if iinfo != 0 */
        for (int_t i = 0; i < m; ++i)
            perm_r[i] = i;
    }
#if (PRNTlevel >= 2)
#warning following is not supported 
    if (job == 2 || job == 3)
    {
        if (!iam)
            printf("\tsmallest diagonal %e\n", dmin);
    }
    else if (job == 4)
    {
        if (!iam)
            printf("\tsum of diagonal %e\n", dsum);
    }
    else if (job == 5)
    {
        if (!iam)
            printf("\t product of diagonal %e\n", dprod);
    }
#endif
} /* perform_LargeDiag_MC64 */
    
void perform_row_permutation(
    superlu_dist_options_t *options,
    fact_t Fact,
    dScalePermstruct_t *ScalePermstruct, dLUstruct_t *LUstruct,
    int_t m, int_t n,
    gridinfo_t *grid,
    SuperMatrix *A,
    SuperMatrix *GA, 
    SuperLUStat_t *stat,
    int_t job,
    int_t Equil,
    int_t *rowequ,
    int_t *colequ,
    int_t *iinfo)
{
    #if ( DEBUGlevel>=1 )                    
    LOG_FUNC_ENTER();
    #endif
    int_t *perm_r = ScalePermstruct->perm_r;
    /* Get NC format data from SuperMatrix GA */
    NCformat* GAstore = (NCformat *)GA->Store;
    int_t* colptr = GAstore->colptr;
    int_t* rowind = GAstore->rowind;
    int_t nnz = GAstore->nnz;
    double* a_GA = (double *)GAstore->nzval;

    int iam = grid->iam;
    /* ------------------------------------------------------------
			   Find the row permutation for A.
    ------------------------------------------------------------ */
    double t;

    if (options->RowPerm != NO)
    {
        t = SuperLU_timer_();

        if (Fact != SamePattern_SameRowPerm)
        {
            if (options->RowPerm == MY_PERMR)
            {
                applyRowPerm(colptr, rowind, perm_r, n);
            }
            else if (options->RowPerm == LargeDiag_MC64)
            {
                
                perform_LargeDiag_MC64(
                options, Fact,
                ScalePermstruct, LUstruct,
                m, n, grid, 
                A, GA, stat, job, 
                Equil, rowequ, colequ, iinfo);
            }
            else // LargeDiag_HWPM
            {
#ifdef HAVE_COMBBLAS
                d_c2cpp_GetHWPM(A, grid, ScalePermstruct);
#else
                if (iam == 0)
                {
                    printf("CombBLAS is not available\n");
                    fflush(stdout);
                }
#endif
            }

            t = SuperLU_timer_() - t;
            stat->utime[ROWPERM] = t;
#if (PRNTlevel >= 1)
            if (!iam)
            {
                printf(".. LDPERM job " IFMT "\t time: %.2f\n", job, t);
                fflush(stdout);
            }
#endif
        }
    }
    else // options->RowPerm == NOROWPERM / NATURAL
    {
        for (int i = 0; i < m; ++i)
            perm_r[i] = i;
    }

    #if (DEBUGlevel >= 2)
	if (!grid->iam)
		PrintInt10("perm_r", m, perm_r);
    #endif
}


/**
 * @brief This function computes the norm of a matrix A.
 * @param notran A flag which determines the norm type to be calculated.
 * @param A The input matrix for which the norm is computed.
 * @param grid The gridinfo_t object that contains the information of the grid.
 * @return Returns the computed norm of the matrix A.
 *
 * the iam process is the root (iam=0), it prints the computed norm to the standard output. 
 */
double computeA_Norm(int_t notran, SuperMatrix *A, gridinfo_t *grid) {
    char norm;
    double anorm;

    /* Compute norm(A), which will be used to adjust small diagonal. */
    if (notran)
        norm = '1';
    else
        norm = 'I';

    anorm = pdlangs(&norm, A, grid);

#if (PRNTlevel >= 1)
    if (!grid->iam) {
        printf(".. anorm %e\n", anorm);
        fflush(stdout);
    }
#endif

    return anorm;
}

void dallocScalePermstruct_RC(dScalePermstruct_t * ScalePermstruct, int_t m, int_t n) {
    /* Allocate storage if not done so before. */
	switch (ScalePermstruct->DiagScale) {
		case NOEQUIL:
			if (!(ScalePermstruct->R = (double *)doubleMalloc_dist(m)))
				ABORT("Malloc fails for R[].");
			if (!(ScalePermstruct->C = (double *)doubleMalloc_dist(n)))
				ABORT("Malloc fails for C[].");
			break;
		case ROW:
			if (!(ScalePermstruct->C = (double *)doubleMalloc_dist(n)))
				ABORT("Malloc fails for C[].");
			break;
		case COL:
			if (!(ScalePermstruct->R = (double *)doubleMalloc_dist(m)))
				ABORT("Malloc fails for R[].");
			break;
		default:
			break;
	}
}


/**
 * @brief This function performs the symbolic factorization on matrix Pc*Pr*A*Pc' and sets up 
 * the nonzero data structures for L & U matrices. In the process, the matrix is also ordered and
 * its memory usage information is fetched.
 * 
 * @param options The options for the SuperLU distribution.
 * @param GA A pointer to the global matrix A.
 * @param perm_c The column permutation vector.
 * @param etree The elimination tree of Pc*Pr*A*Pc'.
 * @param iam The processor number (0 <= iam < Pr*Pc).
 * @param Glu_persist Pointer to the structure which tracks the symbolic factorization information.
 * @param Glu_freeable Pointer to the structure which tracks the space used to store L/U data structures.
 * @param stat Information on program execution.
 */
void permCol_SymbolicFact3d(superlu_dist_options_t *options, int_t n, SuperMatrix *GA, int_t *perm_c, int_t *etree, 
                           Glu_persist_t *Glu_persist, Glu_freeable_t *Glu_freeable, SuperLUStat_t *stat,
						   superlu_dist_mem_usage_t*symb_mem_usage,
						   gridinfo3d_t* grid3d)
{
    #if ( DEBUGlevel>=1 )                    
    LOG_FUNC_ENTER();
    #endif
    SuperMatrix GAC; /* Global A in NCP format */
    NCPformat *GACstore;
    int_t *GACcolbeg, *GACcolend, *GACrowind, irow;
    double t;
    int_t iinfo;
    int iam = grid3d->iam;

    sp_colorder(options, GA, perm_c, etree, &GAC);

    /* Form Pc*A*Pc' to preserve the diagonal of the matrix GAC. */
    GACstore = (NCPformat *)GAC.Store;
    GACcolbeg = GACstore->colbeg;
    GACcolend = GACstore->colend;
    GACrowind = GACstore->rowind;
    for (int_t j = 0; j < n; ++j)
    {
        for (int_t i = GACcolbeg[j]; i < GACcolend[j]; ++i)
        {
            irow = GACrowind[i];
            GACrowind[i] = perm_c[irow];
        }
    }

#if (PRNTlevel >= 1)
    if (!iam)
        printf(".. symbfact(): relax %4d, maxsuper %4d, fill %4d\n",
               sp_ienv_dist(2, options), sp_ienv_dist(3, options), sp_ienv_dist(6, options));
#endif

    t = SuperLU_timer_();
    iinfo = symbfact(options, iam, &GAC, perm_c, etree, Glu_persist, Glu_freeable);
    stat->utime[SYMBFAC] = SuperLU_timer_() - t;

    if (iinfo < 0)
    {
        QuerySpace_dist(n, -iinfo, Glu_freeable, symb_mem_usage);
#if (PRNTlevel >= 1)
        if (!iam)
        {
            printf("\tNo of supers %ld\n", (long)Glu_persist->supno[n - 1] + 1);
            printf("\tSize of G(L) %ld\n", (long)Glu_freeable->xlsub[n]);
            printf("\tSize of G(U) %ld\n", (long)Glu_freeable->xusub[n]);
            printf("\tint %lu, short %lu, float %lu, double %lu\n",
                   sizeof(int_t), sizeof(short), sizeof(float), sizeof(double));
            printf("\tSYMBfact (MB):\tL\\U %.2f\ttotal %.2f\texpansions %d\n",
                   symb_mem_usage->for_lu * 1e-6, symb_mem_usage->total * 1e-6, symb_mem_usage->expansions);
        }
#endif
    }
    else
    {
        if (!iam)
        {
            fprintf(stderr, "symbfact() error returns %d\n", (int)iinfo);
            exit(-1); 
        }
    }

    Destroy_CompCol_Permuted_dist(&GAC);
}



#ifdef REFACTOR_SYMBOLIC 
/**
 * @brief Determines the column permutation vector based on the chosen method.
 *
 * @param[in] options      Pointer to the options structure.
 * @param[in] A            Pointer to the input matrix.
 * @param[in] grid         Pointer to the process grid.
 * @param[in] parSymbFact  Flag indicating whether parallel symbolic factorization is used.
 * @param[out] perm_c      Column permutation vector.
 * @return Error code (0 if successful).
 */
int computeColumnPermutation(const superlu_dist_options_t *options,
                               const SuperMatrix *A,
                               const gridinfo_t *grid,
                               const int parSymbFact,
                               int_t *perm_c);

/**
 * @brief Computes the elimination tree based on the chosen column permutation method.
 *
 * @param[in] options  Pointer to the options structure.
 * @param[in] A        Pointer to the input matrix.
 * @param[in] perm_c   Column permutation vector.
 * @param[out] etree   Elimination tree.
 * @return Error code (0 if successful).
 */
int ComputeEliminationTree(const superlu_dist_options_t *options,
                           const SuperMatrix *A,
                           const int_t *perm_c,
                           int_t *etree);

/**
 * @brief Performs a symbolic factorization on the permuted matrix and sets up the nonzero data structures for L & U.
 *
 * @param[in] options        Pointer to the options structure.
 * @param[in] A              Pointer to the input matrix.
 * @param[in] perm_c         Column permutation vector.
 * @param[in] etree          Elimination tree.
 * @param[out] Glu_persist   Pointer to the global LU data structures.
 * @param[out] Glu_freeable  Pointer to the LU data structures that can be deallocated.
 * @return Error code (0 if successful).
 */
int PerformSymbolicFactorization(const superlu_dist_options_t *options,
                                 const SuperMatrix *A,
                                 const int_t *perm_c,
                                 const int_t *etree,
                                 Glu_persist_t *Glu_persist,
                                 Glu_freeable_t *Glu_freeable);

/**
 * @brief Distributes the permuted matrix into L and U storage.
 *
 * @param[in] options           Pointer to the options structure.
 * @param[in] n                 Order of the input matrix.
 * @param[in] A                 Pointer to the input matrix.
 * @param[in] ScalePermstruct   Pointer to the scaling and permutation structures.
 * @param[in] Glu_freeable      Pointer to the LU data structures that can be deallocated.
 * @param[out] LUstruct         Pointer to the LU data structures.
 * @param[in] grid              Pointer to the process grid.
 * @return Memory usage in bytes (0 if successful).
 */
int DistributePermutedMatrix(const superlu_dist_options_t *options,
                             const int_t n,
                             const SuperMatrix *A,
                             const dScalePermstruct_t *ScalePermstruct,
                             const Glu_freeable_t *Glu_freeable,
                             LUstruct_t *LUstruct,
                             const gridinfo_t *grid);

/**
 * @brief Deallocates the storage used in symbolic factorization.
 *
 * @param[in] Glu_freeable  Pointer to the LU data structures that can be deallocated.
 * @return Error code (0 if successful).
 */
int DeallocateSymbolicFactorizationStorage(const Glu_freeable_t *Glu_freeable);
#endif // REFACTOR_SYMBOLIC


#ifdef REFACTOR_DistributePermutedMatrix


#endif // REFACTOR_DistributePermutedMatrix
#if 0 
// this function is refactored below
void perform_LargeDiag_MC64(
    superlu_dist_options_t* options, 
    fact_t Fact, 
    dScalePermstruct_t *ScalePermstruct,
    dLUstruct_t *LUstruct,
    int_t m, int_t n, 
    gridinfo_t* grid, 
    int_t* perm_r,
    SuperMatrix* A, 
    SuperMatrix* GA,
    SuperLUStat_t* stat, 
    int_t job, 
    int_t Equil, 
    int_t rowequ, 
    int_t colequ) 
{
    /* Note: R1 and C1 are now local variables */
    double* R1 = NULL;
    double* C1 = NULL;

    /* Extract necessary data from the input arguments */
    // dScalePermstruct_t* ScalePermstruct = A->ScalePermstruct;
    // dLUstruct_t* LUstruct = A->LUstruct;
    perm_r = ScalePermstruct->perm_r;
    int_t* perm_c = ScalePermstruct->perm_c;
    int_t* etree = LUstruct->etree;
    double* R = ScalePermstruct->R;
    double* C = ScalePermstruct->C;
    int iam = grid->iam;

    /* Get NR format Data*/
    #warning need to chanck the following code
    NRformat_loc *Astore = (NRformat_loc *)A->Store;
    int_t nnz_loc = Astore->nnz_loc;
    int_t m_loc = Astore->m_loc;
    int_t fst_row = Astore->fst_row;
    double *a = (double *)Astore->nzval;
    int_t *rowptr = Astore->rowptr;
    int_t *colind = Astore->colind;


    /* Get NC format data from SuperMatrix GA */
    NCformat* GAstore = (NCformat *)GA->Store;
    int_t* colptr = GAstore->colptr;
    int_t* rowind = GAstore->rowind;
    int_t nnz = GAstore->nnz;
    double* a_GA = (double *)GAstore->nzval;
    /* Rest of the code goes here... */

    /* Get a new perm_r[] */
    if (job == 5)
    {
        /* Allocate storage for scaling factors. */
        if (!(R1 = doubleMalloc_dist(m)))
            ABORT("SUPERLU_MALLOC fails for R1[]");
        if (!(C1 = doubleMalloc_dist(n)))
            ABORT("SUPERLU_MALLOC fails for C1[]");
    }

    int iinfo;
    if (iam == 0)
    {
        /* Process 0 finds a row permutation */
        iinfo = dldperm_dist(job, m, nnz, colptr, rowind, a_GA,
                             perm_r, R1, C1);
        MPI_Bcast(&iinfo, 1, mpi_int_t, 0, grid->comm);
        if (iinfo == 0)
        {
            MPI_Bcast(perm_r, m, mpi_int_t, 0, grid->comm);
            if (job == 5 && Equil)
            {
                MPI_Bcast(R1, m, MPI_DOUBLE, 0, grid->comm);
                MPI_Bcast(C1, n, MPI_DOUBLE, 0, grid->comm);
            }
        }
    }
    else
    {
        MPI_Bcast(&iinfo, 1, mpi_int_t, 0, grid->comm);
        if (iinfo == 0)
        {
            MPI_Bcast(perm_r, m, mpi_int_t, 0, grid->comm);
            if (job == 5 && Equil)
            {
                MPI_Bcast(R1, m, MPI_DOUBLE, 0, grid->comm);
                MPI_Bcast(C1, n, MPI_DOUBLE, 0, grid->comm);
            }
        }
    }

    if (iinfo && job == 5)
    { /* Error return */
        SUPERLU_FREE(R1);
        SUPERLU_FREE(C1);
    }
#if (PRNTlevel >= 2)
    double dmin = damch_dist("Overflow");
    double dsum = 0.0;
    double dprod = 1.0;
#endif
    if (iinfo == 0)
    {
        if (job == 5)
        {
            if (Equil)
            {
                for (int i = 0; i < n; ++i)
                {
                    R1[i] = exp(R1[i]);
                    C1[i] = exp(C1[i]);
                }

                /* Scale the distributed matrix further.
                   A <-- diag(R1)*A*diag(C1)            */
                int irow = fst_row;
                for (int j = 0; j < m_loc; ++j)
                {
                    for (int i = rowptr[j]; i < rowptr[j + 1]; ++i)
                    {
                        int icol = colind[i];
                        a[i] *= R1[irow] * C1[icol];
#if (PRNTlevel >= 2)
                        if (perm_r[irow] == icol)
                        {
                            /* New diagonal */
                            if (job == 2 || job == 3)
                                dmin = SUPERLU_MIN(dmin, fabs(a[i]));
                            else if (job == 4)
                                dsum += fabs(a[i]);
                            else if (job == 5)
                                dprod *= fabs(a[i]);
                        }
#endif
                    }
                    ++irow;
                }

                /* Multiply together the scaling factors --
                   R/C from simple scheme, R1/C1 from MC64. */
                if (rowequ)
                    for (int i = 0; i < m; ++i)
                        R[i] *= R1[i];
                else
                    for (int i = 0; i < m; ++i)
                        R[i] = R1[i];
                if (colequ)
                    for (int i = 0; i < n; ++i)
                        C[i] *= C1[i];
                else
                    for (int i = 0; i < n; ++i)
                        C[i] = C1[i];

                ScalePermstruct->DiagScale = BOTH;
                rowequ = colequ = 1;

            } /* end if Equil */

            /* Now permute global A to prepare for symbfact() */
            for (int j = 0; j < n; ++j)
            {
                for (int i = colptr[j]; i < colptr[j + 1]; ++i)
                {
                    int irow = rowind[i];
                    rowind[i] = perm_r[irow];
                }
            }
            SUPERLU_FREE(R1);
            SUPERLU_FREE(C1);
        }
        else
        { /* job = 2,3,4 */
            for (int j = 0; j < n; ++j)
            {
                for (int i = colptr[j]; i < colptr[j + 1]; ++i)
                {
                    int irow = rowind[i];
                    rowind[i] = perm_r[irow];
                } /* end for i ... */
            }	  /* end for j ... */
        }		  /* end else job ... */
    }
}


void findRowPerm_MC64(gridinfo_t *grid, int_t job, 
    int_t m, int_t n,
    int_t nnz,
    int_t* colptr,
    int_t* rowind,
    double* a_GA,
    int_t Equil, int_t *perm_r, 
    double *R1, double *C1, int_t*iinfo) 
{
    if (grid->iam == 0) {
        *iinfo = dldperm_dist(job, m, nnz, colptr, rowind, a_GA, perm_r, R1, C1);
        MPI_Bcast(iinfo, 1, mpi_int_t, 0, grid->comm);
        if (*iinfo == 0) {
            MPI_Bcast(perm_r, m, mpi_int_t, 0, grid->comm);
            if (job == 5 && Equil) {
                MPI_Bcast(R1, m, MPI_DOUBLE, 0, grid->comm);
                MPI_Bcast(C1, n, MPI_DOUBLE, 0, grid->comm);
            }
        }
    } else {
        MPI_Bcast(iinfo, 1, mpi_int_t, 0, grid->comm);
        if (*iinfo == 0) {
            MPI_Bcast(perm_r, m, mpi_int_t, 0, grid->comm);
            if (job == 5 && Equil) {
                MPI_Bcast(R1, m, MPI_DOUBLE, 0, grid->comm);
                MPI_Bcast(C1, n, MPI_DOUBLE, 0, grid->comm);
            }
        }
    }
}


#endif 

