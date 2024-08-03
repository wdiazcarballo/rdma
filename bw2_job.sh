#!/bin/bash
#SBATCH --job-name=bw_test
#SBATCH --partition=helios
#SBATCH --nodes=2
#SBATCH --ntasks-per-node=1
#SBATCH --time=01:00:00
#SBATCH --output=bw_test_%j.log
#SBATCH --chdir=/global/home/users/rdmaworkshop08/wdc/rdma

# Get the hostnames
NODELIST=($(scontrol show hostnames $SLURM_JOB_NODELIST))
SERVER_NODE=${NODELIST[0]}
CLIENT_NODE=${NODELIST[1]}

HCA="-d mlx5_0 -i 1"
BINDS="numactl --cpunodebind=2 "
BINDC="numactl --cpunodebind=2 "


# Start the client on the second node
srun --nodes=1 --ntasks=1 --nodelist=$CLIENT_NODE ./client $SERVER_NODE

# Start the server on the first node
srun --nodes=1 --ntasks=1 --nodelist=$SERVER_NODE ./server &

# Give the server a moment to start
sleep 5

wait
