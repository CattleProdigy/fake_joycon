#include <czmq.h>
#include <linux/joystick.h>
#include <stdio.h>
#include <unistd.h>

enum BeaconServerState {
    Beaconing = 0,
    Paired = 1,
};

zsock_t* beacon()
{
    zactor_t* beacon = zactor_new(zbeacon, NULL);
    zsock_send(beacon, "si", "CONFIGURE", 9999);
    zstr_sendx(beacon, "VERBOSE", NULL);

    zsock_t* listener = zsock_new(ZMQ_PAIR);
    zactor_t* monitor = zactor_new(zmonitor, listener);
    zstr_sendx(monitor, "VERBOSE", NULL);
    zstr_sendx(monitor, "LISTEN", "LISTENING", "ACCEPTED", "CONNECTED", "DISCONNECTED", NULL);
    zstr_sendx(monitor, "START", NULL);
    int port = zsock_bind(listener, "tcp://*:*");
    zsys_info("listening for replies on port: %i", port);

    const char* filter = "SWITCHCON";
    char magic_port_str[20];
    snprintf(magic_port_str, 20, "%s%i", filter, port);

    zsys_info("starting broadcast: %s", magic_port_str);
    zsock_send(beacon, "ssi", "PUBLISH", magic_port_str, 1000);
    zsys_info("started broadcast");

    char* response_magic = NULL;
    while (true) {
        int recv_res = zsock_recv(listener, "s", &response_magic);
        int result_errno = errno;
        if (recv_res == 0) {
            const char* response_magic_truth = "MITCHPURDY";
            if (strncmp(response_magic, response_magic_truth, strlen(response_magic_truth)) == 0) {
                // PAIRED
                goto cleanup;
            }
        } else if (result_errno == EINTR) {
            zsys_warning("interrupted, quiting");
            goto bad;
        } else {
            zsys_info("Server got a beacon response");
        }
    }

    goto cleanup;

bad:
    zsock_destroy(&listener);
    freen(listener);

cleanup:

    zactor_destroy(&monitor);
    freen(response_magic);
    zstr_sendx(beacon, "SILENCE", NULL);
    zactor_destroy(&beacon);
    zactor_destroy(&monitor);

    return listener;
}

struct controller_handler_data_t {
    int fd;
    zsock_t* output_sock;
};

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

int controller_read_handler(zloop_t* loop, zmq_pollitem_t* pollitem, void* handler_data_void)
{
    fprintf(stderr, "Controllerhandler");
    if (pollitem->socket != NULL) {
        char* msg = zstr_recv(pollitem->socket);
        zsys_info("Msg: %s", msg);
        freen(msg);
    }
    struct controller_handler_data_t* handler_data = (struct controller_handler_data_t*)handler_data_void;
    struct js_event event;
    ssize_t bytes = read(handler_data->fd, &event, sizeof(event));

    if (bytes == sizeof(event)) {
        zsys_info("jsenven: %u, %i, %u, %u", event.time, event.value, event.type, event.number);
        zsock_send(handler_data->output_sock, "4i11", event.time, (int)event.value, event.type,
            event.number);
        return 0;
    } else {
        zsys_info("DISCONNECT");
        fprintf(stderr, ("DISCONNECT"));
        return -1;
    }
}

bool paired_streaming(zsock_t* socket)
{
    int jsfd = open("/dev/input/js0", O_RDONLY);
    struct controller_handler_data_t handler_data = {
        .fd = jsfd,
        .output_sock = socket,
    };
    zactor_t* monitor = zactor_new(zmonitor, socket);
    zstr_sendx(monitor, "VERBOSE", NULL);
    zstr_sendx(monitor, "LISTEN", "DISCONNECTED", NULL);
    zstr_sendx(monitor, "START", NULL);

    zmq_pollitem_t socket_pollitem = {
        .socket = NULL,
        .fd = jsfd,
        .events = ZMQ_POLLIN,
        .revents = 0,
    };

    // Create a new zloop reactor
    zloop_t* loop = zloop_new();
    zloop_reader(loop, (zsock_t*)monitor, monitor_handler, NULL);
    zloop_poller(loop, &socket_pollitem, controller_read_handler, &handler_data);
    zloop_start(loop);

    return true;
}

int main(int argc, char** argv)
{
    zsys_set_logstream(stderr);
    enum BeaconServerState state = Beaconing;

    zsock_t* paired_socket = NULL;

    while (true) {
        switch (state) {
        case Beaconing: {
            zsys_info("BEaAC");
            paired_socket = beacon();
            if (paired_socket != NULL) {
                state = Paired;
            } else {
                return -1;
            }
            break;
        }
        case Paired: {
            zsys_info("PAIR");
            paired_streaming(paired_socket);
            state = Beaconing;
            zsock_destroy(&paired_socket);
            break;
        }
        }
    }

    zsys_set_logstream(stderr);
    zactor_t* beacon = zactor_new(zbeacon, NULL);
    zsock_send(beacon, "si", "CONFIGURE", 9999);
    zstr_sendx(beacon, "VERBOSE", NULL);

    zsock_t* listener = zsock_new(ZMQ_PAIR);
    zactor_t* monitor = zactor_new(zmonitor, listener);
    zstr_sendx(monitor, "VERBOSE", NULL);
    zstr_sendx(monitor, "LISTEN", "LISTENING", "ACCEPTED", "CONNECTED", "DISCONNECTED", NULL);
    zstr_sendx(monitor, "START", NULL);
    int port = zsock_bind(listener, "tcp://*:*");
    zsys_info("listening for replies on port: %i", port);

    const char* filter = "SWITCHCON";
    char magic_port_str[20];
    snprintf(magic_port_str, 20, "%s%i", filter, port);

    zsys_info("starting broadcast: %s", magic_port_str);
    zsock_send(beacon, "ssi", "PUBLISH", magic_port_str, 1000);
    zsys_info("started broadcast");

    zsys_info("waiting for first msg");
    char* msg = zstr_recv(listener);
    zsys_info("first msg: %s", msg);

    while (true) {
        zsys_info("sending: %s", "sendout");
        zstr_send(listener, "sendout");
        usleep(2000000);
    }

    // zsock_destroy(&sock);
    return 0;
}
