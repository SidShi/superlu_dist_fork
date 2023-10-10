/* superlu_dist_config.h.in */

/* Enable CUDA */
#define HAVE_CUDA TRUE

/* Enable NVSHMEM */
#define HAVE_NVSHMEM TRUE

/* Enable HIP */
/* #undef HAVE_HIP */

/* Enable parmetis */
#define HAVE_PARMETIS TRUE

/* Enable colamd */
/* #undef HAVE_COLAMD */

/* Enable LAPACK */
#define SLU_HAVE_LAPACK TRUE

/* Enable CombBLAS */
/* #undef HAVE_COMBBLAS */

/* Enable MAGMA */
#define HAVE_MAGMA TRUE

/* enable 64bit index mode */
/* #undef XSDK_INDEX_SIZE */

#if (XSDK_INDEX_SIZE == 64)
#define _LONGINT 1
#endif
