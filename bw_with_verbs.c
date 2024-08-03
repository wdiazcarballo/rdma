#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <infiniband/verbs.h>

#define PORT 12345
#define BUF_SIZE 1024
#define WC_BATCH 10

enum {
    PINGPONG_RECV_WRID = 1,
    PINGPONG_SEND_WRID = 2,
};

struct pingpong_context {
    struct ibv_context *context;
    struct ibv_pd *pd;
    struct ibv_mr *mr;
    struct ibv_cq *cq;
    struct ibv_qp *qp;
    void *buf;
    int size;
    int rx_depth;
    int routs;
    struct ibv_port_attr portinfo;
};

struct pingpong_dest {
    int lid;
    int qpn;
    int psn;
    union ibv_gid gid;
};

static int page_size;

static int pp_post_recv(struct pingpong_context *ctx, int n) {
    struct ibv_sge list = {
        .addr = (uintptr_t)ctx->buf,
        .length = ctx->size,
        .lkey = ctx->mr->lkey
    };
    struct ibv_recv_wr wr = {
        .wr_id = PINGPONG_RECV_WRID,
        .sg_list = &list,
        .num_sge = 1,
        .next = NULL
    };
    struct ibv_recv_wr *bad_wr;
    int i;

    for (i = 0; i < n; ++i)
        if (ibv_post_recv(ctx->qp, &wr, &bad_wr))
            break;

    return i;
}

static int pp_post_send(struct pingpong_context *ctx, int opcode) {
    struct ibv_sge list = {
        .addr = (uint64_t)ctx->buf,
        .length = ctx->size,
        .lkey = ctx->mr->lkey
    };

    struct ibv_send_wr *bad_wr, wr = {
        .wr_id = PINGPONG_SEND_WRID,
        .sg_list = &list,
        .num_sge = 1,
        .opcode = opcode,
        .send_flags = IBV_SEND_SIGNALED,
        .next = NULL
    };

    return ibv_post_send(ctx->qp, &wr, &bad_wr);
}

static int pp_wait_completions(struct pingpong_context *ctx, int iters) {
    int rcnt = 0, scnt = 0;
    while (rcnt + scnt < iters) {
        struct ibv_wc wc[WC_BATCH];
        int ne, i;

        do {
            ne = ibv_poll_cq(ctx->cq, WC_BATCH, wc);
            if (ne < 0) {
                fprintf(stderr, "poll CQ failed %d\n", ne);
                return 1;
            }
        } while (ne < 1);

        for (i = 0; i < ne; ++i) {
            if (wc[i].status != IBV_WC_SUCCESS) {
                fprintf(stderr, "Failed status %s (%d) for wr_id %d\n",
                        ibv_wc_status_str(wc[i].status),
                        wc[i].status, (int)wc[i].wr_id);
                return 1;
            }

            switch ((int)wc[i].wr_id) {
            case PINGPONG_SEND_WRID:
                ++scnt;
                break;
            case PINGPONG_RECV_WRID:
                if (--ctx->routs <= 10) {
                    ctx->routs += pp_post_recv(ctx, ctx->rx_depth - ctx->routs);
                    if (ctx->routs < ctx->rx_depth) {
                        fprintf(stderr, "Couldn't post receive (%d)\n", ctx->routs);
                        return 1;
                    }
                }
                ++rcnt;
                break;
            default:
                fprintf(stderr, "Completion for unknown wr_id %d\n", (int)wc[i].wr_id);
                return 1;
            }
        }
    }
    return 0;
}

static int pp_connect_ctx(struct pingpong_context *ctx, int port, int my_psn,
                          enum ibv_mtu mtu, int sl,
                          struct pingpong_dest *dest, int sgid_idx) {
    struct ibv_qp_attr attr = {
        .qp_state = IBV_QPS_RTR,
        .path_mtu = mtu,
        .dest_qp_num = dest->qpn,
        .rq_psn = dest->psn,
        .max_dest_rd_atomic = 1,
        .min_rnr_timer = 12,
        .ah_attr = {
            .is_global = 0,
            .dlid = dest->lid,
            .sl = sl,
            .src_path_bits = 0,
            .port_num = port
        }
    };

    if (dest->gid.global.interface_id) {
        attr.ah_attr.is_global = 1;
        attr.ah_attr.grh.hop_limit = 1;
        attr.ah_attr.grh.dgid = dest->gid;
        attr.ah_attr.grh.sgid_index = sgid_idx;
    }
    if (ibv_modify_qp(ctx->qp, &attr,
                      IBV_QP_STATE |
                      IBV_QP_AV |
                      IBV_QP_PATH_MTU |
                      IBV_QP_DEST_QPN |
                      IBV_QP_RQ_PSN |
                      IBV_QP_MAX_DEST_RD_ATOMIC |
                      IBV_QP_MIN_RNR_TIMER)) {
        fprintf(stderr, "Failed to modify QP to RTR\n");
        return 1;
    }

    attr.qp_state = IBV_QPS_RTS;
    attr.timeout = 14;
    attr.retry_cnt = 7;
    attr.rnr_retry = 7;
    attr.sq_psn = my_psn;
    attr.max_rd_atomic = 1;
    if (ibv_modify_qp(ctx->qp, &attr,
                      IBV_QP_STATE |
                      IBV_QP_TIMEOUT |
                      IBV_QP_RETRY_CNT |
                      IBV_QP_RNR_RETRY |
                      IBV_QP_SQ_PSN |
                      IBV_QP_MAX_QP_RD_ATOMIC)) {
        fprintf(stderr, "Failed to modify QP to RTS\n");
        return 1;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    struct ibv_device **dev_list;
    struct ibv_device *ib_dev;
    struct pingpong_context *ctx;
    struct pingpong_dest my_dest, *rem_dest;
    char *ib_devname = NULL, *servername = NULL;
    int port = 12345, ib_port = 1, size = 1, sl = 0, gidx = -1, iters = 1000, use_event = 0;
    enum ibv_mtu mtu = IBV_MTU_1024;
    struct timeval start, end;
    double elapsed_time, throughput;

    // Initialize context and connect
    // Similar to the provided code
    // ...

    if (servername) {
        // Client mode
        gettimeofday(&start, NULL);
        for (int i = 0; i < iters; ++i) {
            if (pp_post_send(ctx, IBV_WR_RDMA_WRITE)) {
                fprintf(stderr, "Client couldn't post send\n");
                return 1;
            }
            if (pp_post_recv(ctx, 1) < 1) {
                fprintf(stderr, "Client couldn't post recv\n");
                return 1;
            }
            if (pp_wait_completions(ctx, 1)) {
                fprintf(stderr, "Client completion wait failed\n");
                return 1;
            }
        }
        gettimeofday(&end, NULL);
    } else {
        // Server mode
        if (pp_post_recv(ctx, ctx->rx_depth) < ctx->rx_depth) {
            fprintf(stderr, "Server couldn't post receive\n");
            return 1;
        }
        if (pp_wait_completions(ctx, iters)) {
            fprintf(stderr, "Server completion wait failed\n");
            return 1;
        }
        for (int i = 0; i < iters; ++i) {
            if (pp_post_send(ctx, IBV_WR_SEND)) {
                fprintf(stderr, "Server couldn't post send\n");
                return 1;
            }
        }
    }

    elapsed_time = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
    throughput = (size * iters / (1024 * 1024)) / elapsed_time;
    printf("Throughput: %.2f MBps\n", throughput);

    ibv_free_device_list(dev_list);
    free(rem_dest);

    return 0;
}
