#define _GNU_SOURCE
#include <assert.h>
#include <czmq.h>
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

#define HAT_TOP 0x00
#define HAT_TOP_RIGHT 0x01
#define HAT_RIGHT 0x02
#define HAT_BOTTOM_RIGHT 0x03
#define HAT_BOTTOM 0x04
#define HAT_BOTTOM_LEFT 0x05
#define HAT_LEFT 0x06
#define HAT_TOP_LEFT 0x07
#define HAT_CENTER 0x08

const char* hat_mapping[9] = {
    "HAT_TOP",
    "HAT_TOP_RIGHT",
    "HAT_RIGHT",
    "HAT_BOTTOM_RIGHT",
    "HAT_BOTTOM",
    "HAT_BOTTOM_LEFT",
    "HAT_LEFT",
    "HAT_TOP_LEFT",
    "HAT_CENTER",
};

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

const uint8_t hid_report_descriptor[80] = {
    0x05, 0x01, // USAGE_PAGE (Generic Desktop)
    0x09, 0x05, // USAGE (Game Pad)
    0xa1, 0x01, // COLLECTION (Application)
    0x15, 0x00, //   LOGICAL_MINIMUM (0)
    0x25, 0x01, //   LOGICAL_MAXIMUM (1)
    0x35, 0x00, //   PHYSICAL_MINIMUM (0)
    0x45, 0x01, //   PHYSICAL_MAXIMUM (1)
    0x75, 0x01, //   REPORT_SIZE (1)
    0x95, 0x0e, //   REPORT_COUNT (14)
    0x05, 0x09, //   USAGE_PAGE (Button)
    0x19, 0x01, //   USAGE_MINIMUM (Button 1)
    0x29, 0x0e, //   USAGE_MAXIMUM (Button 14)
    0x81, 0x02, //   INPUT (Data,Var,Abs)
    0x95, 0x02, //   REPORT_COUNT (2)
    0x81, 0x01, //   INPUT (Cnst,Ary,Abs)
    0x05, 0x01, //   USAGE_PAGE (Generic Desktop)
    0x25, 0x07, //   LOGICAL_MAXIMUM (7)
    0x46, 0x3b, 0x01, //   PHYSICAL_MAXIMUM (315)
    0x75, 0x04, //   REPORT_SIZE (4)
    0x95, 0x01, //   REPORT_COUNT (1)
    0x65, 0x14, //   UNIT (Eng Rot:Angular Pos)
    0x09, 0x39, //   USAGE (Hat switch)
    0x81, 0x42, //   INPUT (Data,Var,Abs,Null)
    0x65, 0x00, //   UNIT (None)
    0x95, 0x01, //   REPORT_COUNT (1)
    0x81, 0x01, //   INPUT (Cnst,Ary,Abs)
    0x26, 0xff, 0x00, //   LOGICAL_MAXIMUM (255)
    0x46, 0xff, 0x00, //   PHYSICAL_MAXIMUM (255)
    0x09, 0x30, //   USAGE (X)
    0x09, 0x31, //   USAGE (Y)
    0x09, 0x32, //   USAGE (Z)
    0x09, 0x35, //   USAGE (Rz)
    0x75, 0x08, //   REPORT_SIZE (8)
    0x95, 0x04, //   REPORT_COUNT (4)
    0x81, 0x02, //   INPUT (Data,Var,Abs)
    0x75, 0x08, //   REPORT_SIZE (8)
    0x95, 0x01, //   REPORT_COUNT (1)
    0x81, 0x03, //   INPUT (Cnst,Var,Abs)
    0xc0 //     END_COLLECTION
};

// const uint8_t hid_report_descriptor[] = {
//     0x05, 0x01, // Usage Page (Generic Desktop Ctrls)
//     0x09, 0x04, // Usage (Joystick)
//     0xA1, 0x01, // Collection (Application)
//     0x15, 0x00, //   Logical Minimum (0)
//     0x25, 0x01, //   Logical Maximum (1)
//     0x35, 0x00, //   Physical Minimum (0)
//     0x45, 0x01, //   Physical Maximum (1)
//     0x75, 0x01, //   Report Size (1)
//     0x95, 0x10, //   Report Count (16)
//     0x05, 0x09, //   Usage Page (Button)
//     0x19, 0x01, //   Usage Minimum (0x01)
//     0x29, 0x10, //   Usage Maximum (0x10)
//     0x81, 0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
//     0x05, 0x01, //   Usage Page (Generic Desktop Ctrls)
//     0x25, 0x07, //   Logical Maximum (7)
//     0x46, 0x3B, 0x01, //   Physical Maximum (315)
//     0x75, 0x04, //   Report Size (4)
//     0x95, 0x01, //   Report Count (1)
//     0x65, 0x14, //   Unit (System: English Rotation, Length: Centimeter)
//     0x09, 0x39, //   Usage (Hat switch)
//     0x81, 0x42, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
//     0x65, 0x00, //   Unit (None)
//     0x95, 0x01, //   Report Count (1)
//     0x81, 0x01, //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
//     0x26, 0xFF, 0x00, //   Logical Maximum (255)
//     0x46, 0xFF, 0x00, //   Physical Maximum (255)
//     0x09, 0x30, //   Usage (X)
//     0x09, 0x31, //   Usage (Y)
//     0x09, 0x32, //   Usage (Z)
//     0x09, 0x35, //   Usage (Rz)
//     0x75, 0x08, //   Report Size (8)
//     0x95, 0x04, //   Report Count (4)
//     0x81, 0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
//     0x06, 0x00, 0xFF, //   Usage Page (Vendor Defined 0xFF00)
//     0x09, 0x20, //   Usage (0x20)
//     0x95, 0x01, //   Report Count (1)
//     0x81, 0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
//     0x0A, 0x21, 0x26, //   Usage (0x2621)
//     0x95, 0x08, //   Report Count (8)
//     0x91,
//     0x02, //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
//     0xC0, // End Collection
// };
// // 86 bytes

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
        struct usb_endpoint_descriptor_no_audio hid_in_ep;
        struct usb_endpoint_descriptor_no_audio hid_out_ep;
        // struct usb_device_descriptor device_desc

    } __attribute__((packed)) hs_descs;
} __attribute__((packed)) descriptors = {
    .header = {
        .magic = cpu_to_le32(FUNCTIONFS_DESCRIPTORS_MAGIC_V2),
        .flags = cpu_to_le32( FUNCTIONFS_HAS_HS_DESC),
        .length = cpu_to_le32(sizeof descriptors),
    },
    .hs_count = cpu_to_le32(4),
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
        .hid_in_ep = {
            .bLength = sizeof descriptors.hs_descs.hid_in_ep,
            .bDescriptorType = USB_DT_ENDPOINT,
            .bEndpointAddress = 1 | USB_DIR_IN,
            .bmAttributes = USB_ENDPOINT_XFER_INT,
            .wMaxPacketSize = cpu_to_le16(0x40), // switch mandates 64 bytes allegedly
            .bInterval = 0x05, // TODO what does this mean, it's for interrupt endpoints
        },
        .hid_out_ep = {
            .bLength = sizeof descriptors.hs_descs.hid_out_ep,
            .bDescriptorType = USB_DT_ENDPOINT,
            .bEndpointAddress = 1 | USB_DIR_OUT,
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
struct USB_JoystickReport_Output_t {
    uint16_t Button; // 16 buttons; see JoystickButtons_t for bit mapping
    uint8_t HAT; // HAT switch; one nibble w/ unused nibble
    uint8_t LX; // Left  Stick X
    uint8_t LY; // Left  Stick Y
    uint8_t RX; // Right Stick X
    uint8_t RY; // Right Stick Y
};

struct ep2_data_t {
    int fd;
};

bool ep2_setup(void* data)
{
    struct ep2_data_t** ep2_data_ptr = data;
    printf("ep2 setup\n");

    char* ep2_path;
    int r = asprintf(&ep2_path, "%s/%s", FUNCTIONFS_MOUNT_POINT, "ep2");
    if (r <= 0) {
        printf("ep2_path alloc failed\n");
        return false;
    }

    int fd = open(ep2_path, O_RDWR);
    printf("ep2 fd: %i\n", fd);
    free(ep2_path);
    if (fd < 0) {
        printf("ep2 fd open failed\n");
        return false;
    }

    struct ep2_data_t* ep2_data;
    ep2_data = malloc(sizeof(struct ep2_data_t));
    ep2_data->fd = fd;

    printf("ep2 thread finished initial setup: %i\n", ep2_data->fd);

    *ep2_data_ptr = ep2_data;

    return true;
}

void ep2_cleanup(void* data)
{
    printf("ep2 cleanup\n");
    void** ep2_ptr_ptr = data;

    struct ep2_data_t* ep2_data = *ep2_ptr_ptr;
    if (ep2_data == NULL) {
        return;
    }

    if (ep2_data->fd >= 0) {
        close(ep2_data->fd);
    }

    free(ep2_data);
    *ep2_ptr_ptr = NULL;
}

bool ep2_loop(void* ep2_data_void)
{
    struct ep2_data_t* ep2_data = ep2_data_void;

    struct USB_JoystickReport_Output_t output = { 0 };

    ssize_t bytes_written = write(ep2_data->fd, &output, sizeof(output));
    if (bytes_written < (ssize_t)sizeof(output)) {
        printf("A!@#\n");
        return false;
    }
    // int status;
    // ssize_t bytes_read = read(ep2_data->fd, &status, 0);

    return true;
}

struct USB_JoystickReport_Input_t g_joystick_data;
pthread_mutex_t g_joystick_data_mutex = PTHREAD_MUTEX_INITIALIZER;

struct ep1_data_t {
    int fd;
    struct USB_JoystickReport_Input_t* joystick_data;
    pthread_mutex_t* joystick_data_mutex;
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
    ep1_data->joystick_data = malloc(sizeof(struct USB_JoystickReport_Input_t));
    ep1_data->fd = fd;

    printf("ep1 thread finished initial setup: %i, %p\n", ep1_data->fd, ep1_data->joystick_data);

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

    if (ep1_data->joystick_data != NULL) {
        free(ep1_data->joystick_data);
        ep1_data->joystick_data = NULL;
    }

    free(ep1_data);
    *ep1_ptr_ptr = NULL;
}

bool ep1_loop(void* ep1_data_void)
{
    struct ep1_data_t* ep1_data = ep1_data_void;

    struct USB_JoystickReport_Input_t in = { 0 };
    pthread_mutex_lock(&g_joystick_data_mutex);
    in = g_joystick_data;
    pthread_mutex_unlock(&g_joystick_data_mutex);

    ssize_t bytes_written = write(ep1_data->fd, &in, sizeof(in));
    if (bytes_written < (ssize_t)sizeof(in)) {
        return false;
    }
    int status;
    ssize_t bytes_read = read(ep1_data->fd, &status, 0);

    return true;
}

struct ep0_data_t {
    int fd;
    void* buffer;
    struct usb_endpoint_thread_t io_endpoints[2];
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
    {
        ep0_data->io_endpoints[1].data = NULL;
        ep0_data->io_endpoints[1].setup_fn = ep2_setup;
        ep0_data->io_endpoints[1].loop_fn = ep2_loop;
        ep0_data->io_endpoints[1].cleanup_fn = ep2_cleanup;
        thread_run(&ep0_data->io_endpoints[1]);
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
// client

#define JS_EVENT_BUTTON 0x01 /* button pressed/released */
#define JS_EVENT_AXIS 0x02 /* joystick moved */
#define JS_EVENT_INIT 0x80 /* initial state of device */
int handler(zloop_t* loop, zsock_t* sock, void* data)
{
    uint32_t time;
    int32_t value;
    uint8_t type;
    uint8_t number;
    int recv_res = zsock_recv(sock, "4i11", &time, &value, &type, &number);
    // zsys_info("jsenven: %u, %i, %u, %u", time, value, type, number);
    pthread_mutex_lock(&g_joystick_data_mutex);
    if ((type & ~JS_EVENT_INIT) == JS_EVENT_BUTTON) {
        const uint8_t mapping[12] = {
            1, // Xbox B (0)
            2, // Xbox A (1)
            0, // Xbox Y (2)
            3, // Xbox X (3)
            4, // Xbox LS (4)
            5, // Xbox RS (5)
            8, // Xbox M (6)
            9, // Xbox P (7)
            12, // Xbox H (8)
            10, // Xbox Lth (9)
            11, // Xbox Rth (10)
        };
        uint16_t mapped_button = mapping[number];
        uint16_t mask = 1 << mapped_button;
        if (value)
            g_joystick_data.Button |= mask;
        else
            g_joystick_data.Button &= ~mask;
    } else if ((type & ~JS_EVENT_INIT) == JS_EVENT_AXIS) {
        uint8_t shaped_axis = ((int32_t)value + 32768) >> 8;
        switch (number) {
        // Left Stick
        case 0:
            g_joystick_data.LX = shaped_axis;
            break;
        case 1:
            g_joystick_data.LY = shaped_axis;
            break;
            // Right Stick
        case 3:
            g_joystick_data.RX = shaped_axis;
            break;
        case 4:
            g_joystick_data.RY = shaped_axis;
            break;
        // Left Trigger
        case 2: {
            bool triggered = value > -29000; // provide some deadzone
            uint16_t mask = 1 << 6;
            if (triggered)
                g_joystick_data.Button |= mask;
            else
                g_joystick_data.Button &= ~mask;
            break;
        }
            // Right Trigger
        case 5: {
            bool triggered = value > -29000; // provide some deadzone
            uint16_t mask = 1 << 7;
            if (triggered)
                g_joystick_data.Button |= mask;
            else
                g_joystick_data.Button &= ~mask;
            break;
        }
        // POV HAT
        case 6: {
            // Get the current u/d movement
            bool up = g_joystick_data.HAT == HAT_TOP || g_joystick_data.HAT == HAT_TOP_RIGHT
                || g_joystick_data.HAT == HAT_TOP_LEFT;
            bool down = g_joystick_data.HAT == HAT_BOTTOM || g_joystick_data.HAT == HAT_BOTTOM_RIGHT
                || g_joystick_data.HAT == HAT_BOTTOM_LEFT;

            bool left = value == -32767;
            bool right = value == 32767;
            printf("HAT: %i, %s", g_joystick_data.HAT, hat_mapping[g_joystick_data.HAT]);
            printf("l/r lrud: %i %i %i %i\n", left, right, up, down);

            if (up) {
                if (left) {
                    printf("up left, topleft\n");
                    g_joystick_data.HAT = HAT_TOP_LEFT;
                } else if (right) {
                    printf("up right, topright\n");
                    g_joystick_data.HAT = HAT_TOP_RIGHT;
                } else {
                    printf("up nlnr, top\n");
                    g_joystick_data.HAT = HAT_TOP;
                }
            } else if (down) {
                if (left) {
                    printf("down left, bottomleft\n");
                    g_joystick_data.HAT = HAT_BOTTOM_LEFT;
                } else if (right) {
                    printf("down right, bottomright\n");
                    g_joystick_data.HAT = HAT_BOTTOM_RIGHT;
                } else {
                    printf("down nlnr, bottom\n");
                    g_joystick_data.HAT = HAT_BOTTOM;
                }
            } else {
                if (left) {
                    printf("nund left, left\n");
                    g_joystick_data.HAT = HAT_LEFT;
                } else if (right) {
                    printf("nund right, right\n");
                    g_joystick_data.HAT = HAT_RIGHT;
                } else {
                    printf("nund nlnr, center\n");
                    g_joystick_data.HAT = HAT_CENTER;
                }
            }
            break;
        }
        case 7: {
            bool left = g_joystick_data.HAT == HAT_TOP_LEFT || g_joystick_data.HAT == HAT_LEFT
                || g_joystick_data.HAT == HAT_BOTTOM_LEFT;
            bool right = g_joystick_data.HAT == HAT_TOP_RIGHT || g_joystick_data.HAT == HAT_RIGHT
                || g_joystick_data.HAT == HAT_BOTTOM_RIGHT;
            bool up = value == -32767;
            bool down = value == 32767;
            printf("HAT: %i, %s", g_joystick_data.HAT, hat_mapping[g_joystick_data.HAT]);
            printf("u/d lrud: %i %i %i %i\n", left, right, up, down);
            if (up) {
                if (left) {
                    g_joystick_data.HAT = HAT_TOP_LEFT;
                } else if (right) {
                    g_joystick_data.HAT = HAT_TOP_RIGHT;
                } else {
                    g_joystick_data.HAT = HAT_TOP;
                }
            } else if (down) {
                if (left) {
                    g_joystick_data.HAT = HAT_BOTTOM_LEFT;
                } else if (right) {
                    g_joystick_data.HAT = HAT_BOTTOM_RIGHT;
                } else {
                    g_joystick_data.HAT = HAT_BOTTOM;
                }
            } else {
                if (left) {
                    g_joystick_data.HAT = HAT_LEFT;
                } else if (right) {
                    g_joystick_data.HAT = HAT_RIGHT;
                } else {
                    g_joystick_data.HAT = HAT_CENTER;
                }
            }
            break;
        }
        }
    }
    pthread_mutex_unlock(&g_joystick_data_mutex);
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

void* comm(void* data)
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
                return NULL;
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
                return NULL;
            }

            zsock_destroy(&paired_socket);
            state = Beaconing;
            break;
        }
        }
    }

    return 0;
}

// client
//

int main()
{
    pthread_t comm_thread;
    pthread_create(&comm_thread, 0, comm, NULL);

    struct usb_endpoint_thread_t ep0_thread = {
        .data = NULL,
        .setup_fn = ep0_setup,
        .loop_fn = ep0_loop,
        .cleanup_fn = ep0_cleanup,
    };
    thread_run(&ep0_thread);

    pthread_join(ep0_thread.pthread, NULL);
    pthread_join(comm_thread, NULL);

    printf("join done\n");

    return 0;
}
