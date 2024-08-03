#include <stdio.h>
#include <stdlib.h>
#include <infiniband/verbs.h>
#include <arpa/inet.h>

int main() {
    struct ibv_device **dev_list;
    struct ibv_device *ib_dev;
    struct ibv_context *context;
    union ibv_gid gid;
    int port = 1;
    int gid_index = 0;

    // Get the list of InfiniBand devices
    dev_list = ibv_get_device_list(NULL);
    if (!dev_list) {
        perror("Failed to get IB devices list");
        return 1;
    }

    // Select the first device in the list
    ib_dev = *dev_list;
    if (!ib_dev) {
        fprintf(stderr, "No IB devices found\n");
        return 1;
    }

    // Open the device context
    context = ibv_open_device(ib_dev);
    if (!context) {
        fprintf(stderr, "Couldn't get context for device\n");
        return 1;
    }

    // Query the GID
    if (ibv_query_gid(context, port, gid_index, &gid)) {
        fprintf(stderr, "Could not get local GID for gid index %d\n", gid_index);
        return 1;
    }

    // Print the GID
    char gid_str[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &gid, gid_str, sizeof gid_str);
    printf("GID: %s\n", gid_str);

    // Clean up
    ibv_close_device(context);
    ibv_free_device_list(dev_list);

    return 0;
}
