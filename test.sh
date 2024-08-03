#!/bin/bash

# Configuration
TAG=$(date +%Y%m%d-%H%M%S)
DEST=helios007
SRC=helios008

HCA="-d mlx5_0 -i 1"
BINDS="numactl --cpunodebind=2 "
BINDC="numactl --cpunodebind=2 "

SERVER_PORT=18515
MESSAGE_SIZE=4096
MTU=1024
RX_DEPTH=500
ITERS=1000
SERVICE_LEVEL=0
GID_IDX=-1

# Copy server executable to remote machine
scp ./server $DEST:~/wdc/server

# Start the server on local machine
$BINDS ./server -p $SERVER_PORT -d mlx5_0 -i 1 -s $MESSAGE_SIZE -m $MTU -r $RX_DEPTH -n $ITERS -l $SERVICE_LEVEL -g $GID_IDX &
LOCAL_SERVER_PID=$!
echo "Server started on local machine, waiting for connections..."

# Start the server on remote machine
ssh $DEST "$BINDC ~/wdc/server -p $SERVER_PORT -d mlx5_0 -i 1 -s $MESSAGE_SIZE -m $MTU -r $RX_DEPTH -n $ITERS -l $SERVICE_LEVEL -g $GID_IDX" &
REMOTE_SERVER_PID=$!
echo "Server started on remote machine $DEST"

# Wait for a few seconds to ensure the server is up and running
sleep 5

# Start the client on local machine connecting to remote server
$BINDS ./client $DEST -p $SERVER_PORT -d mlx5_0 -i 1 -s $MESSAGE_SIZE -m $MTU -r $RX_DEPTH -n $ITERS -l $SERVICE_LEVEL -g $GID_IDX
echo "Client started, connecting to server at $SRC"

# Wait for the client and server to finish
wait $LOCAL_SERVER_PID
wait $REMOTE_SERVER_PID

echo "RDMA communication test completed."

