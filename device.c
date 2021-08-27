#include <linux/usb/functionfs.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define USB_FUNCTIONFS_EVENT_BUFFER 4

struct ep0_data_t {
    int fd;
    void* buffer;
};

// TODO: check malloc return errors (i guess...)
void create_ep0_data(struct ep0_data_t** ep0_data, int fd)
{
    *ep0_data = malloc(sizeof(struct ep0_data_t));
    (*ep0_data)->fd = fd;
    (*ep0_data)->buffer = malloc(4 * sizeof(struct usb_functionfs_event));
}

void destroy_ep0_data(struct ep0_data_t* ep0_data)
{
    free(ep0_data->buffer);
    free(ep0_data);
}

void* ep0_thread(void* ep0_data_void)
{
    struct ep0_data_t* ep0_data = (struct ep0_data_t*)ep0_data_void;
    printf("thread started: %i, %p\n", ep0_data->fd, ep0_data->buffer);
    usleep(1000000);
    printf("thread endinp\n");
    return NULL;
}

int main()
{
    pthread_t ep0_pthread;
    struct ep0_data_t* ep0_data;
    create_ep0_data(&ep0_data, 69);
    pthread_create(&ep0_pthread, 0, ep0_thread, (void*)ep0_data);
    pthread_join(ep0_pthread, 0);

    destroy_ep0_data(ep0_data);

    return 0;
}
