#include <czmq.h>
#include <stdio.h>

int handler(zloop_t*, zsock_t* sock, void*)
{
    uint32_t time;
    int32_t value;
    uint8_t type;
    uint8_t number;
    int recv_res = zsock_recv(sock, "4i11", &time, &value, &type, &number);
    zsys_info("jsenven: %u, %i, %u, %u", time, value, type, number);
    return 0;
}

enum EmuRecvState {
    Listening = 0,
    Pairing = 1,
    Paired = 2,
};

char* listen()
{
    char* endpoint = nullptr;
    char* ip_addr = nullptr;
    char* magic = nullptr;
    unsigned int port = 0;

    int recv_res = -1;

    zsys_info("Listening for beacons on udp 9999");
    zactor_t* listener = zactor_new(zbeacon, NULL);
    zsock_send(listener, "si", "CONFIGURE", 9999);

    const char* beacon_prefix = "SWITCHCON";
    zsock_send(listener, "sb", "SUBSCRIBE", beacon_prefix, strlen(beacon_prefix));

    // read listening ip
    {
        char* self_ip_addr = zstr_recv(listener);
        if (self_ip_addr) {
            zsys_info("Using interface at %s", self_ip_addr);
            freen(self_ip_addr);
        } else {
            zsys_error("Couldn't get listening interface");
            goto bad;
        }
    }

    do {
        freen(ip_addr);
        freen(magic);

        zmsg_t* msg = zmsg_recv(listener);
        zmsg_print(msg);

        errno = 0;
        recv_res = zsock_recv(listener, "ss", &ip_addr, &magic);
        int result_errno = errno;
        zsys_info("Got a beacon: %s | %s", ip_addr, magic);

        if (recv_res == 0) {
            if (strlen(magic) <= strlen(beacon_prefix)) {
                zsys_error("malformed packet, missing port?, quiting");
                goto bad;
            }
            char* port_str = magic + strlen(beacon_prefix);
            int conv_result = sscanf(port_str, "%u", &port);
            if (conv_result != 1) {
                zsys_error("malformed packet, quiting");
                goto bad;
            }
            zsys_info("Got a beacon: %s | %s, | %u", ip_addr, magic, port);
            break;
        } else if (result_errno == EINTR) {
            zsys_warning("interrupted, quiting");
            goto bad;
        } else {
            zsys_info("Got a malformed response");
        }

    } while (recv_res < 0 || strncmp(magic, "FUCKBOI", strlen("FUCKBOI") != 0));

    endpoint = (char*)malloc(128);
    snprintf(endpoint, 128, "tcp://%s:%u", ip_addr, port);

    zsys_info("Got a matching magic: %s %s %s %u", endpoint, ip_addr, magic, port);

    goto cleanup;
bad:
    freen(endpoint);

cleanup:
    freen(ip_addr);
    freen(magic);

    zstr_sendx(listener, "UNSUBSCRIBE", NULL);
    zactor_destroy(&listener);
    return endpoint;
}

int main(int, char**)
{
    zsys_set_logstream(stderr);

    char* endpoint = listen();

    zsys_info("connecting to: %s", endpoint);
    zsock_t* sub = zsock_new_pair(endpoint);
    zsys_info("connecteds, %p", sub);

    zloop_t* looper = zloop_new();

    zsys_info("sending:fisrt");
    zstr_send(sub, "MITCHPURDY");
    zsys_info("sent");

    zloop_reader(looper, (zsock_t*)sub, &handler, NULL);
    zloop_start(looper);
    zsys_info("post start");

    zsock_destroy(&sub);

    freen(endpoint);

    // char* endpoint = listen();

    // zsys_info("connecting to: %s", endpoint);
    // zsock_t* sub = zsock_new_pair(endpoint);
    // zsock_send(sub, "sb", "SUBSCRIBE", "", 0);
    // zsys_info("connecteds, %p", sub);

    // zloop_t* looper = zloop_new();

    // usleep(500);

    // zsys_info("sending:fisrt");
    // zstr_send(sub, "first");

    // zloop_reader(looper, (zsock_t*)sub, &handler, NULL);
    // zloop_start(looper);

    // zsock_destroy(&sub);

    // freen(endpoint);
    //    zactor_t* beacon = zactor_new(zbeacon, NULL);
    //    zsock_send(beacon, "si", "CONFIGURE", 9999);
    //
    //    zactor_t* listener = zactor_new(zbeacon, NULL);
    //    zsock_send(listener, "si", "CONFIGURE", 9999);
    //    zsock_send(listener, "sb", "SUBSCRIBE", "", 0);
    //    // read listening ip
    //    {
    //        char* ip_addr = zstr_recv(listener);
    //        freen(ip_addr);
    //    }
    //
    //    const char* filter = "SWITCHCON";
    //    zsock_send(beacon, "sbi", "PUBLISH", filter, strlen(filter), 1000);
    //
    //    zloop_t* looper = zloop_new();
    //
    //    zloop_reader(looper, (zsock_t*)listener, &handler, NULL);
    //
    //    zloop_start(looper);
    //
    //    zactor_destroy(&beacon);

    return 0;
}
