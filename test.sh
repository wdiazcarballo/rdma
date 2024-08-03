#!/bin/bash
#SBATCH --job-name=bw_test
#SBATCH --partition=helios
#SBATCH --nodes=2
#SBATCH --ntasks=2
#SBATCH --exclusive
#SBATCH --time=01:00:00
#SBATCH --output=bw_test_%j.log

# Load necessary modules (if required)
# module load <module-name>

# Define a timestamp for the log files
TAG=$(date +%Y%m%d-%H%M%S)

# Get the list of allocated nodes
NODELIST=$(scontrol show hostnames $SLURM_JOB_NODELIST)
NODES=($NODELIST)

# Assign the first node as the server and the second as the client
SERVER_NODE=${NODES[0]}
CLIENT_NODE=${NODES[1]}

# Define the options for the server and client commands
HCA="-d mlx5_0 -i 1"
BINDS="numactl --cpunodebind=2"
BINDC="numactl --cpunodebind=2"

# Define the flags for latency and bandwidth tests
LTFLAGS="-a $HCA -F"
BWFLAGS="-a $HCA --report_gbits -F"

# Start the server on the server node
srun -N1 -w $SERVER_NODE $BINDS ./server &

# Wait a bit to ensure the server starts before the client tries to connect
sleep 5

# Start the client on the client node and connect to the server
srun -N1 -w $CLIENT_NODE $BINDC ./client $SERVER_NODE 2>&1 | tee log-bw-$TAG.txt
