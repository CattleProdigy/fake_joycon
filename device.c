#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/hid.h>
#include <linux/kernel.h>
#include <linux/usb/ch9.h>
#include <linux/usb/functionfs.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define USB_FUNCTIONFS_EVENT_BUFFER 4
#define FUNCTIONFS_MOUNT_POINT "/tmp/mount_point"

#define cpu_to_le16(x) (x)
#define cpu_to_le32(x) (x)

#define le32_to_cpu(x) le32toh(x)
#define le16_to_cpu(x) le16toh(x)

static const char* const names[] = {
    [FUNCTIONFS_BIND] = "BIND",
    [FUNCTIONFS_UNBIND] = "UNBIND",
    [FUNCTIONFS_ENABLE] = "ENABLE",
    [FUNCTIONFS_DISABLE] = "DISABLE",
    [FUNCTIONFS_SETUP] = "SETUP",
    [FUNCTIONFS_SUSPEND] = "SUSPEND",
    [FUNCTIONFS_RESUME] = "RESUME",
};

#define STRINGID_MFGR 1
#define STRINGID_PRODUCT 2
#define STRINGID_SERIAL 3
#define STRINGID_CONFIG 4
#define STRINGID_INTERFACE 5

#define STRING_MFGR "HORI CO.,LTD."
#define STRING_PRODUCT "POKKEN CONTROLLER"
#define STRING_SERIAL "69420"
#define STRING_CONFIG "fakejoycon"
#define STRING_INTERFACE "Source/Sink"

const uint8_t hid_report_descriptor[] = {
    0x05, 0x01, // Usage Page (Generic Desktop Ctrls)
    0x09, 0x04, // Usage (Joystick)
    0xA1, 0x01, // Collection (Application)
    0x15, 0x00, //   Logical Minimum (0)
    0x25, 0x01, //   Logical Maximum (1)
    0x35, 0x00, //   Physical Minimum (0)
    0x45, 0x01, //   Physical Maximum (1)
    0x75, 0x01, //   Report Size (1)
    0x95, 0x10, //   Report Count (16)
    0x05, 0x09, //   Usage Page (Button)
    0x19, 0x01, //   Usage Minimum (0x01)
    0x29, 0x10, //   Usage Maximum (0x10)
    0x81, 0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x01, //   Usage Page (Generic Desktop Ctrls)
    0x25, 0x07, //   Logical Maximum (7)
    0x46, 0x3B, 0x01, //   Physical Maximum (315)
    0x75, 0x04, //   Report Size (4)
    0x95, 0x01, //   Report Count (1)
    0x65, 0x14, //   Unit (System: English Rotation, Length: Centimeter)
    0x09, 0x39, //   Usage (Hat switch)
    0x81, 0x42, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
    0x65, 0x00, //   Unit (None)
    0x95, 0x01, //   Report Count (1)
    0x81, 0x01, //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x26, 0xFF, 0x00, //   Logical Maximum (255)
    0x46, 0xFF, 0x00, //   Physical Maximum (255)
    0x09, 0x30, //   Usage (X)
    0x09, 0x31, //   Usage (Y)
    0x09, 0x32, //   Usage (Z)
    0x09, 0x35, //   Usage (Rz)
    0x75, 0x08, //   Report Size (8)
    0x95, 0x04, //   Report Count (4)
    0x81, 0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x06, 0x00, 0xFF, //   Usage Page (Vendor Defined 0xFF00)
    0x09, 0x20, //   Usage (0x20)
    0x95, 0x01, //   Report Count (1)
    0x81, 0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x0A, 0x21, 0x26, //   Usage (0x2621)
    0x95, 0x08, //   Report Count (8)
    0x91,
    0x02, //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0xC0, // End Collection
};
// 86 bytes

struct hid_descriptor {
    __u8 bLength;
    __u8 bDescriptorType;
    __le16 bcdHID;
    __u8 bCountryCode;
    __u8 bNumDescriptors;
    __u8 bReportType;
    __le16 wReportLength;
} __attribute__((packed));

static const struct {
    struct usb_functionfs_strings_head header;
    struct {
        __le16 code;
        const char str1[sizeof STRING_MFGR];
        const char str2[sizeof STRING_PRODUCT];
        const char str3[sizeof STRING_SERIAL];
        const char str4[sizeof STRING_CONFIG];
        const char str5[sizeof STRING_INTERFACE];
    } __attribute__((packed)) english_stringtab;
} __attribute__((packed)) strings = {
    .header = {
        .magic = cpu_to_le32(FUNCTIONFS_STRINGS_MAGIC),
        .length = cpu_to_le32(sizeof strings),
        .str_count = cpu_to_le32(5),
        .lang_count = cpu_to_le32(1),
    },
    .english_stringtab = {
        cpu_to_le16(0x0409), /* en-us */
        STRING_MFGR,
        STRING_PRODUCT,
        STRING_SERIAL,
        STRING_CONFIG,
        STRING_INTERFACE
    },
};

// struct hid_descriptor {
//     __u8 bLength;
//     __u8 bDescriptorType;
//     __le16 bcdHID;
//     __u8 bCountryCode;
//     __u8 bNumDescriptors;
//
//     struct hid_class_descriptor desc[1];
// } __attribute__((packed));

static const struct {
    struct usb_functionfs_descs_head_v2 header;
    __le32 hs_count;
    //__le32 ss_count;
    struct {
        // struct usb_config_descriptor config_desc;
        struct usb_interface_descriptor intf;
        struct hid_descriptor hid_desc;
        struct usb_endpoint_descriptor_no_audio hid_ep;
        // struct usb_device_descriptor device_desc

    } __attribute__((packed)) hs_descs;
} __attribute__((packed)) descriptors = {
    .header = {
        .magic = cpu_to_le32(FUNCTIONFS_DESCRIPTORS_MAGIC_V2),
        .flags = cpu_to_le32( FUNCTIONFS_HAS_HS_DESC),
        .length = cpu_to_le32(sizeof descriptors),
    },
    .hs_count = cpu_to_le32(3),
    .hs_descs = {
       .intf = {
            .bLength = sizeof descriptors.hs_descs.intf,
            .bDescriptorType = USB_DT_INTERFACE,
            .bNumEndpoints = 1,
            .bInterfaceClass = USB_CLASS_HID,
            .iInterface = STRINGID_INTERFACE,
        },
         .hid_desc = {
             .bLength = sizeof descriptors.hs_descs.hid_desc,
             .bDescriptorType = HID_DT_HID,
             .bcdHID =  __constant_cpu_to_le16(0x0111),
             .bCountryCode = 0x00,
             .bNumDescriptors = 1,
             .bReportType = HID_DT_REPORT,
             .wReportLength = __constant_cpu_to_le16(sizeof(hid_report_descriptor)),
         },
        .hid_ep = {
            .bLength = sizeof descriptors.hs_descs.hid_ep,
            .bDescriptorType = USB_DT_ENDPOINT,
            .bEndpointAddress = 1 | USB_DIR_IN,
            .bmAttributes = USB_ENDPOINT_XFER_INT,
            .wMaxPacketSize = cpu_to_le16(0x40), // switch mandates 64 bytes allegedly
            .bInterval = 0x05, // TODO what does this mean, it's for interrupt endpoints
        },
    },
};

void handle_setup(int fd, const struct usb_ctrlrequest* setup)
{
    printf("bRequestType = %d\n", setup->bRequestType);
    printf("bRequest     = %d\n", setup->bRequest);
    printf("wValue       = %d\n", le16_to_cpu(setup->wValue));
    printf("wIndex       = %d\n", le16_to_cpu(setup->wIndex));
    printf("wLength      = %d\n", le16_to_cpu(setup->wLength));
    int status;
    __u16 value, index, length;

    value = __le16_to_cpu(setup->wValue);
    index = __le16_to_cpu(setup->wIndex);
    length = __le16_to_cpu(setup->wLength);

    fprintf(stderr,
        "SETUP %02x.%02x "
        "v%04x i%04x %d\n",
        setup->bRequestType, setup->bRequest, value, index, length);

    /*
    if ((setup->bRequestType & USB_TYPE_MASK) != USB_TYPE_STANDARD)
        goto special;
    */

    switch (setup->bRequest) { /* usb 2.0 spec ch9 requests */
    case USB_REQ_GET_DESCRIPTOR:
        printf("USB_REQ_GET_DESCRIPTOR\n");
        // if (setup->bRequestType != USB_DIR_IN)
        //    goto stall;
        switch (value >> 8) {
        case HID_DT_REPORT:
            status = write(fd, hid_report_descriptor, sizeof(hid_report_descriptor));
            if (status < 0) {
                if (errno == EIDRM)
                    printf("string timeout\n");
                else
                    perror("wrote report desc");
            } else if (status != sizeof(hid_report_descriptor)) {
                fprintf(stderr, "short string write, %d\n", status);
            }
            break;
        default:
            goto stall;
        }
        break;
    case USB_REQ_SET_CONFIGURATION:
        printf("USB_REQ_SET_CONFIGURATION\n");
        printf("CONFIG #%d\n", value);
        break;
    case USB_REQ_GET_INTERFACE:
        printf("USB_REQ_GET_INTERFACE\n");
        if (setup->bRequestType != (USB_DIR_IN | USB_RECIP_INTERFACE) || index != 0 || length > 1) {
            printf("Assumptoins violated\n");
            goto stall;
        }
        char b = 0;
        status = write(fd, &b, 1);
        break;
    case USB_REQ_SET_INTERFACE:
        printf("USB_REQ_SET_INTERFACE");
    default:
        goto stall;
    }

    return;

stall:
    fprintf(stderr, "... protocol stall %02x.%02x\n", setup->bRequestType, setup->bRequest);

    /* non-iso endpoints are stalled by issuing an i/o request
     * in the "wrong" direction.  ep0 is special only because
     * the direction isn't fixed.
     */
    if (setup->bRequestType & USB_DIR_IN)
        status = read(fd, &status, 0);
    else
        status = write(fd, &status, 0);
    if (status != -1)
        fprintf(stderr, "can't stall ep0 for %02x.%02x\n", setup->bRequestType, setup->bRequest);
    else
        perror("ep0 stall");
}

struct usb_endpoint_thread_t {
    void* data;
    bool (*setup_fn)(void*); // true if successfull
    bool (*loop_fn)(void*); // true
    void (*cleanup_fn)(void*);
    pthread_t pthread;
};

void* thread_run_body(void* usb_endpoint_thread_void)
{
    struct usb_endpoint_thread_t* thread = usb_endpoint_thread_void;
    if (!thread->setup_fn(&thread->data)) {
        return NULL;
    }
    pthread_cleanup_push(thread->cleanup_fn, &thread->data);
    while (true) {
        pthread_testcancel();
        if (!thread->loop_fn(thread->data)) {
            break;
        }
    }
    pthread_cleanup_pop(0);

    thread->cleanup_fn(&thread->data);
    return NULL;
}

void thread_run(struct usb_endpoint_thread_t* thread)
{
    pthread_create(&thread->pthread, 0, thread_run_body, (void*)thread);
}

// Joystick HID report structure. We have an input and an output.
struct USB_JoystickReport_Input_t {
    uint16_t Button; // 16 buttons; see JoystickButtons_t for bit mapping
    uint8_t HAT; // HAT switch; one nibble w/ unused nibble
    uint8_t LX; // Left  Stick X
    uint8_t LY; // Left  Stick Y
    uint8_t RX; // Right Stick X
    uint8_t RY; // Right Stick Y
    uint8_t VendorSpec;
};

// The output is structured as a mirror of the input.
// This is based on initial observations of the Pokken Controller.
typedef struct {
    uint16_t Button; // 16 buttons; see JoystickButtons_t for bit mapping
    uint8_t HAT; // HAT switch; one nibble w/ unused nibble
    uint8_t LX; // Left  Stick X
    uint8_t LY; // Left  Stick Y
    uint8_t RX; // Right Stick X
    uint8_t RY; // Right Stick Y
} USB_JoystickReport_Output_t;

struct ep1_data_t {
    int fd;
    void* buffer;
};

bool ep1_setup(void* data)
{
    struct ep1_data_t** ep1_data_ptr = data;
    printf("ep1 setup\n");

    char* ep1_path;
    int r = asprintf(&ep1_path, "%s/%s", FUNCTIONFS_MOUNT_POINT, "ep1");
    if (r <= 0) {
        printf("ep1_path alloc failed\n");
        return false;
    }

    int fd = open(ep1_path, O_RDWR);
    printf("ep1 fd: %i\n", fd);
    free(ep1_path);
    if (fd < 0) {
        printf("ep1 fd open failed\n");
        return false;
    }

    struct ep1_data_t* ep1_data;
    ep1_data = malloc(sizeof(struct ep1_data_t));
    ep1_data->buffer = malloc(USB_FUNCTIONFS_EVENT_BUFFER * sizeof(struct usb_functionfs_event));
    ep1_data->fd = fd;

    printf("ep1 thread finished initial setup: %i, %p\n", ep1_data->fd, ep1_data->buffer);

    *ep1_data_ptr = ep1_data;

    return true;
}

void ep1_cleanup(void* data)
{
    printf("ep1 cleanup\n");
    void** ep1_ptr_ptr = data;

    struct ep1_data_t* ep1_data = *ep1_ptr_ptr;
    if (ep1_data == NULL) {
        return;
    }

    if (ep1_data->fd >= 0) {
        close(ep1_data->fd);
    }

    if (ep1_data->buffer != NULL) {
        free(ep1_data->buffer);
        ep1_data->buffer = NULL;
    }

    free(ep1_data);
    *ep1_ptr_ptr = NULL;
}

bool ep1_loop(void* ep1_data_void)
{
    struct ep1_data_t* ep1_data = ep1_data_void;
    static bool flip_flop = true;
    struct USB_JoystickReport_Input_t in = { 0 };
    in.Button = 0b0101010101010101;
    if (!flip_flop) {
        in.Button = ~in.Button;
        in.LX = 255;
        in.LY = 255;
        in.RX = 255;
        in.RY = 255;
    }
    flip_flop = !flip_flop;

    ssize_t bytes_written = write(ep1_data->fd, &in, sizeof(in));
    // printf("ep1 wrote bytes: %i\n", ep1_data->fd);
     //printf("ep1 wrote bytes: %i, %u %li\n", flip_flop, in.Button, bytes_written);
    // if (bytes_written < 0) {
    //     return false;
    // } elsei
    if (bytes_written < (ssize_t)sizeof(in)) {
        return false;
    }
    int status;
    ssize_t bytes_read = read(ep1_data->fd, &status, 0);

    // char* ep1_path;
    // int r = asprintf(&ep1_path, "%s/%s", FUNCTIONFS_MOUNT_POINT, "ep1");
    // if (r <= 0) {
    //     printf("ep1_path alloc failed\n");
    //     return NULL;
    // }

    // int fd = open(ep1_path, O_RDWR);
    // printf("ep1 fd: %i\n", fd);
    // free(ep1_path);
    // if (fd < 0) {
    //     printf("ep1 fd open failed\n");
    //     return NULL;
    // }
    // pthread_t ep1_pthread;
    // pthread_create(&ep1_pthread, 0, ep1_thread, (void*)fd);

    // const size_t max_data_size
    //     = (USB_FUNCTIONFS_EVENT_BUFFER * sizeof(struct usb_functionfs_event));
    // printf("reading from ep0\n");
    // ssize_t bytes_read = read(ep0_data->fd, ep0_data->buffer, max_data_size);
    // printf("done reading from ep0: %li %lu %lu \n", bytes_read,
    //     bytes_read / sizeof(struct usb_functionfs_event),
    //     bytes_read % sizeof(struct usb_functionfs_event));
    // if (bytes_read < 0) {
    //     printf("Reading ep0 failed: %i, %p\n", ep0_data->fd, ep0_data->buffer);
    //     return false;
    // }
    // const struct usb_functionfs_event* event = ep0_data->buffer;
    // assert(bytes_read % sizeof(*event) == 0);
    // for (size_t n = 0; n < bytes_read / sizeof(*event); ++n, ++event) {
    //     switch (event->type) {
    //     case FUNCTIONFS_BIND:
    //     case FUNCTIONFS_UNBIND:
    //     case FUNCTIONFS_ENABLE:
    //     case FUNCTIONFS_DISABLE:
    //     case FUNCTIONFS_SUSPEND:
    //     case FUNCTIONFS_RESUME:
    //         printf("Event %s\n", names[event->type]);
    //         break;
    //     case FUNCTIONFS_SETUP:
    //         printf("Got a setup request\n");
    //         handle_setup(ep0_data->fd, &event->u.setup);
    //         break;

    //     default:
    //         printf("Event %03u (unknown)\n", event->type);
    //     }
    // }

    return true;
}

struct ep0_data_t {
    int fd;
    void* buffer;
    struct usb_endpoint_thread_t io_endpoints[1];
};
bool ep0_setup(void* data)
{
    struct ep0_data_t** ep0_data_ptr = data;
    printf("ep0 setup\n");
    char* ep0_path;
    int r = asprintf(&ep0_path, "%s/%s", FUNCTIONFS_MOUNT_POINT, "ep0");
    if (r <= 0) {
        printf("ep0_path alloc failed\n");
        return false;
    }

    struct ep0_data_t* ep0_data;
    ep0_data = malloc(sizeof(struct ep0_data_t));
    ep0_data->buffer = malloc(USB_FUNCTIONFS_EVENT_BUFFER * sizeof(struct usb_functionfs_event));

    int ep0_fd = open(ep0_path, O_RDWR);
    free(ep0_path);
    if (ep0_fd < 0) {
        printf("ep0 fd open failed\n");
        free(ep0_data->buffer);
        ep0_data->buffer = NULL;
        free(ep0_data);
        ep0_data = NULL;
        return false;
    }
    ep0_data->fd = ep0_fd;

    printf("ep0 thread finished initial setup: %i, %p\n", ep0_data->fd, ep0_data->buffer);

    ssize_t written = write(ep0_data->fd, &descriptors, sizeof descriptors);
    printf("wrote desc: %li\n", written);
    written = write(ep0_data->fd, &strings, sizeof strings);
    printf("wrote strings: %li\n", written);

    {
        ep0_data->io_endpoints[0].data = NULL;
        ep0_data->io_endpoints[0].setup_fn = ep1_setup;
        ep0_data->io_endpoints[0].loop_fn = ep1_loop;
        ep0_data->io_endpoints[0].cleanup_fn = ep1_cleanup;
        thread_run(&ep0_data->io_endpoints[0]);
    }

    *ep0_data_ptr = ep0_data;

    return true;
}

void ep0_cleanup(void* data)
{
    printf("ep0 cleanup\n");
    void** ep0_ptr_ptr = data;

    struct ep0_data_t* ep0_data = *ep0_ptr_ptr;
    if (ep0_data == NULL) {
        return;
    }

    if (ep0_data->fd >= 0) {
        close(ep0_data->fd);
    }

    if (ep0_data->buffer != NULL) {
        free(ep0_data->buffer);
        ep0_data->buffer = NULL;
    }

    pthread_cancel(ep0_data->io_endpoints[0].pthread);

    free(ep0_data);
    *ep0_ptr_ptr = NULL;
}

bool ep0_loop(void* ep0_data_void)
{
    struct ep0_data_t* ep0_data = ep0_data_void;

    const size_t max_data_size
        = (USB_FUNCTIONFS_EVENT_BUFFER * sizeof(struct usb_functionfs_event));
    printf("reading from ep0\n");
    ssize_t bytes_read = read(ep0_data->fd, ep0_data->buffer, max_data_size);
    printf("done reading from ep0: %li %lu %lu \n", bytes_read,
        bytes_read / sizeof(struct usb_functionfs_event),
        bytes_read % sizeof(struct usb_functionfs_event));
    if (bytes_read < 0) {
        printf("Reading ep0 failed: %i, %p\n", ep0_data->fd, ep0_data->buffer);
        return false;
    }
    const struct usb_functionfs_event* event = ep0_data->buffer;
    assert(bytes_read % sizeof(*event) == 0);
    for (size_t n = 0; n < bytes_read / sizeof(*event); ++n, ++event) {
        switch (event->type) {
        case FUNCTIONFS_BIND:
        case FUNCTIONFS_UNBIND:
        case FUNCTIONFS_ENABLE:
        case FUNCTIONFS_DISABLE:
        case FUNCTIONFS_SUSPEND:
        case FUNCTIONFS_RESUME:
            printf("Event %s\n", names[event->type]);
            break;
        case FUNCTIONFS_SETUP:
            printf("Got a setup request\n");
            handle_setup(ep0_data->fd, &event->u.setup);
            break;

        default:
            printf("Event %03u (unknown)\n", event->type);
        }
    }

    return true;
}

int main()
{
    struct usb_endpoint_thread_t ep0_thread = {
        .data = NULL,
        .setup_fn = ep0_setup,
        .loop_fn = ep0_loop,
        .cleanup_fn = ep0_cleanup,
    };
    thread_run(&ep0_thread);

    pthread_join(ep0_thread.pthread, NULL);

    printf("join done\n");

    return 0;
}
