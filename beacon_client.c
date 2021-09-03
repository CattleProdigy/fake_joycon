#include <czmq.h>
#include <stdio.h>

int handler(zloop_t* loop, zsock_t* sock, void* data)
{
    uint32_t time;
    int32_t value;
    uint8_t type;
    uint8_t number;
    int recv_res = zsock_recv(sock, "4i11", &time, &value, &type, &number);
    zsys_info("jsenven: %u, %i, %u, %u", time, value, type, number);
    return 0;
}

int monitor_handler(zloop_t* loop, zsock_t* reader, void* handler_data_void)
{
    char* msg = zstr_recv(reader);
    if (strncmp(msg, "DISCONNECTED", strlen("DISCONNECTED")) == 0) {
        freen(msg);
        return -1;
    }
    freen(msg);
    return 0;
}

enum BeaconClientState {
    Beaconing = 0,
    Paired = 1,
};

char* beacon_listen()
{
    char* endpoint = NULL;
    char* ip_addr = NULL;
    char* magic = NULL;
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

bool paired_streaming(zsock_t* socket)
{
    zsys_info("sending:fisrt");
    zstr_send(socket, "MITCHPURDY");
    zsys_info("sent");

    zactor_t* monitor = zactor_new(zmonitor, socket);
    zstr_sendx(monitor, "VERBOSE", NULL);
    zstr_sendx(monitor, "LISTEN", "DISCONNECTED", NULL);
    zstr_sendx(monitor, "START", NULL);

    // Create a new zloop reactor
    zloop_t* loop = zloop_new();
    zloop_reader(loop, (zsock_t*)monitor, monitor_handler, NULL);
    zloop_reader(loop, socket, handler, NULL);
    return zloop_start(loop) == -1; // if 0 then it was interupted
}

int main(int argc, char** argv)
{
    zsys_set_logstream(stderr);

    enum BeaconClientState state = Beaconing;

    zsock_t* paired_socket = NULL;

    while (true) {
        switch (state) {
        case Beaconing: {
            zsys_info("BEaAC");

            char* endpoint = beacon_listen();
            if (endpoint != NULL) {
                state = Paired;
            } else {
                return -1;
            }
            zsys_info("connecting to: %s", endpoint);
            paired_socket = zsock_new_pair(endpoint);
            freen(endpoint);

            break;
        }
        case Paired: {
            zsys_info("PAIR");
            if (!paired_streaming(paired_socket)) {
                zsock_destroy(&paired_socket);
                return -1;
            }

            zsock_destroy(&paired_socket);
            state = Beaconing;
            break;
        }
        }
    }

    return 0;
}
