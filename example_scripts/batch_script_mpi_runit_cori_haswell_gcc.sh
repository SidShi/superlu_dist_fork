#!/bin/bash
# Bash script to submit many files to Cori/Edison/Queue

EXIT_SUCCESS=0
EXIT_HOST=1
EXIT_PARAM=2

module swap PrgEnv-intel PrgEnv-gnu
export MKLROOT=/opt/intel/compilers_and_libraries_2019.3.199/linux/mkl
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/opt/intel/compilers_and_libraries_2018.1.163/linux/mkl/lib/intel64
module unload cmake
module load cmake


CUR_DIR=`pwd`
FILE_DIR=$CUR_DIR/EXAMPLE
INPUT_DIR=$CFS/m2957/liuyangz/my_research/matrix
FILE_NAME=pddrive
FILE=$FILE_DIR/$FILE_NAME

TMP_BATCH_FILE=tmp_batch_file.slurm
# ^^^ Should check that this is not taken,
#     but I would never use this, so ...

> $TMP_BATCH_FILE

if [[ $NERSC_HOST == edison ]]; then
  CORES_PER_NODE=24
  THREADS_PER_NODE=48
elif [[ $NERSC_HOST == cori ]]; then
  CORES_PER_NODE=32
  THREADS_PER_NODE=64
  # This does not take hyperthreading into account
else
  # Host unknown; exiting
  exit $EXIT_HOST
fi

nprows=(8 )
npcols=(8 )
npz=(8 )
 
for ((i = 0; i < ${#npcols[@]}; i++)); do
NROW=${nprows[i]}
NCOL=${npcols[i]}
NPZ=${npz[i]}

# NROW=36
CORE_VAL=`expr $NCOL \* $NROW \* $NPZ`
NODE_VAL=`expr $CORE_VAL / $CORES_PER_NODE`
MOD_VAL=`expr $CORE_VAL % $CORES_PER_NODE`
if [[ $MOD_VAL -ne 0 ]]
then
  NODE_VAL=`expr $NODE_VAL + 1`
fi
#PARTITION=debug
PARTITION=regular
LICENSE=SCRATCH
TIME=00:20:00

if [[ $NERSC_HOST == edison ]]
then
  CONSTRAINT=0
fi
if [[ $NERSC_HOST == cori ]]
then
  CONSTRAINT=haswell
fi


#for NTH in 8 
for NTH in 1 
do
OMP_NUM_THREADS=$NTH
TH_PER_RANK=`expr $NTH \* 2`


#for NSUP in 128 64 32 16 8
#do
  # for MAT in atmosmodl.rb nlpkkt80.mtx torso3.mtx Ga19As19H42.mtx A22.mtx cage13.rb 
  # for MAT in torso3.bin
  # for MAT in big.rua
  for MAT in s1_mat_0_126936.bin
  # for MAT in matrix121.dat matrix211.dat tdr190k.dat tdr455k.dat nlpkkt80.mtx torso3.mtx helm2d03.mtx  
  # for MAT in tdr190k.dat Ga19As19H42.mtx
 # for MAT in torso3.mtx hvdc2.mtx matrix121.dat nlpkkt80.mtx helm2d03.mtx
# for MAT in A22.bin DG_GrapheneDisorder_8192.bin DNA_715_64cell.bin LU_C_BN_C_4by2.bin Li4244.bin atmosmodj.bin Ga19As19H42.bin Geo_1438.bin StocF-1465.bin    
  # for MAT in  A22.bin DNA_715_64cell.bin LU_C_BN_C_4by2.bin
 # for MAT in Ga19As19H42.mtx   
  do
    export OMP_NUM_THREADS=$OMP_NUM_THREADS
    export OMP_PLACES=threads
    export OMP_PROC_BIND=spread
    export MPICH_MAX_THREAD_SAFETY=multiple
    # srun -n $CORE_VAL -c $TH_PER_RANK --cpu_bind=cores $FILE_DIR/pddrive -c $NCOL -r $NROW $INPUT_DIR/$MAT | tee SLU.o_mpi_${NROW}x${NCOL}_${OMP_NUM_THREADS}
    echo "srun -n $CORE_VAL -c $TH_PER_RANK --cpu_bind=cores $FILE_DIR/pddrive3d -c $NCOL -r $NROW -d $NPZ $INPUT_DIR/$MAT | tee SLU.o_mpi_${NROW}x${NCOL}x${NPZ}_${OMP_NUM_THREADS}_3d"
    srun -n $CORE_VAL -c $TH_PER_RANK --cpu_bind=cores $FILE_DIR/pddrive3d -c $NCOL -r $NROW -d $NPZ $INPUT_DIR/$MAT | tee SLU.o_mpi_${NROW}x${NCOL}x${NPZ}_${OMP_NUM_THREADS}_3d
    

  done
#one

done
done

exit $EXIT_SUCCESS
