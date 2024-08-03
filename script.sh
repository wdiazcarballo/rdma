TAG=$(date +%Y%m%d-%H%M%S)
DEST=helios021
SRC=helios020
 
HCA="-d mlx5_0 -i 1"
BINDS="numactl --cpunodebind=2 "
BINDC="numactl --cpunodebind=2 "
 
LTFLAGS="-a $HCA -F "
BWFLAGS="-a $HCA --report_gbits -F "
 
$BINDS ib_write_lat $LTFLAGS & ssh $DEST "$BINDC ib_write_lat $LTFLAGS $SRC " 2>&1 |tee log-lat-$TAG.txt
$BINDS ib_write_bw $BWFLAGS & ssh $DEST "$BINDC ib_write_bw $BWFLAGS $SRC " 2>&1 |tee log-bw-$TAG.txt
 
$BINDS ib_read_lat $LTFLAGS & ssh $DEST "$BINDC ib_read_lat $LTFLAGS $SRC " 2>&1 |tee log-lat-$TAG.txt
$BINDS ib_read_bw $BWFLAGS & ssh $DEST "$BINDC ib_read_bw $BWFLAGS $SRC " 2>&1 |tee log-bw-$TAG.txt

