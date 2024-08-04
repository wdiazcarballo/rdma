# RDMA Verbs Bandwidth Measurement

This repository contains the code and scripts to measure point-to-point (unidirectional) bandwidth using the RDMA Verbs API. The provided solutions improve upon the initial template to solve the competition problem.

## Files

- `0_template_bw.c`: Original template code for measuring bandwidth using the Verbs API.
- `1_sol_withErr.c`: First solution attempt with errors.
- `2_sol_withSameErr.c`: Second solution attempt with similar errors.
- `3_test_gid_probe.c`: Test script for GID probing.
- `bw_test_529576.log`: Log file from the first test run.
- `bw_test_529577.log`: Log file from the second test run.
- `job_ibperf_test.sh`: Original job script for ib_write_bw and ib_read_bw tests.
- `job_verbs_bw_test.sh`: Updated job script for bandwidth measurement, based on the script from [HPC Advisory Council](https://hpcadvisorycouncil.atlassian.net/wiki/x/AQDdD).

## Compilation

To compile the template code, use the following command:

```sh
gcc 0_template_bw.c -libverbs -o server && ln -s server client


# Compiling the solutions
```sh
gcc 1_sol_withErr.c -libverbs -o server && ln -s server client
gcc 2_sol_withSameErr.c -libverbs -o server && ln -s server client
```

# Running the test
SLURM Job Script
The job_verbs_bw_test.sh script is an updated version of the job script from HPC Advisory Council. It is used to run the bandwidth measurement test on a multi-node cluster using SLURM. It starts the server on one node and the client on another node.

job_verbs_bw_test.sh
```sh
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


# Start the server on the first node

srun --nodes=1 --ntasks=1 --nodelist=$SERVER_NODE ./server &


# Give the server a moment to start
sleep 8

# Start the client on the second node
srun --nodes=1 --ntasks=1 --nodelist=$CLIENT_NODE ./client $SERVER_NODE

wait
```

# Submitting the Job
To submit the job to SLURM, use the following command:
```sh
sbatch job_verbs_bw_test.sh
```

# Output
The output of the bandwidth measurement will be logged in a file named bw_test_<job_id>.log where <job_id> is the SLURM job ID.