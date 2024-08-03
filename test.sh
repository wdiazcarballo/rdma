#!/bin/bash
#SBATCH --job-name=rdma_bw_test
#SBATCH --nodes=2
#SBATCH --ntasks-per-node=1
#SBATCH --partition=helios
#SBATCH --output=bw_test_%j.log

# Load necessary modules
module load gcc
module load openmpi
module load infiniband

# Get the list of nodes
NODELIST=($(scontrol show hostnames $SLURM_JOB_NODELIST))
SRC=${NODELIST[0]}
DEST=${NODELIST[1]}

# Set the parameters
HCA="-d mlx5_0 -i 1"
BINDS="numactl --cpunodebind=2"
BINDC="numactl --cpunodebind=2"

# Compile the code if needed
gcc bw1.c -libverbs -o server && ln -s server client

# Start the server on the source node
srun -N 1 -n 1 -w $SRC $BINDS ./server &
sleep 5  # Ensure server is up before client starts

# Start the client on the destination node
srun -N 1 -n 1 -w $DEST $BINDC ./client $SRC
