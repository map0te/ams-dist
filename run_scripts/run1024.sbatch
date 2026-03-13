#!/bin/bash
#SBATCH --job-name=AMS-ks21
#SBATCH --ntasks=1024
#SBATCH --account=gts-rvuduc3
#SBATCH -qembers
#SBATCH --mem=0
#SBATCH --time=6:00:00
#SBATCH --exclusive
#SBATCH --out=/storage/scratch1/1/mzhulin3/ams-dist-ks21-1024.out

# Activate your virtual environment
module load openmpi

# Run your distributed solver
srun ams-dist 21 ~/scratch/instances/21/ks_21.cnf ~/scratch/instances/21/1024
