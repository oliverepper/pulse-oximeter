//
//  cms50f.c
//  CMS50F
//
//  Created by Oliver Epper on 15.12.22.
//

#include "cms50f.h"
#include "log.h"
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <termios.h>
#include <sys/time.h>
#include <math.h>
#include <assert.h>

#define SMALLBUFFER 20

#define ASSERT_DEVICE(device) \
    do { if (!device) { LOG_DEBUG("%s", "no device"); return CMS50F_EINVAL; } } while (0)
#define HIGH_BIT_OFF(buffer, n) \
    do { for (int i = 0; i < n; ++i) buffer[i] &= 0x7f; } while (0)

#ifdef DEBUG
#define DUMP_BUFFER(format, buffer, n) \
    do { dump_buffer(stderr, __FILE_NAME__, __FUNCTION__, __LINE__, format, buffer, n); } while (0)
#else
#define DUMP_BUFFER(format, buffer, n) \
    do {} while (0)
#endif

enum command_code {
    CMD_STOP_SENDING_STORAGE_DATA   = 0xa7,
    CMD_STOP_SENDING_REALTIME_DATA  = 0xa2,
    CMD_STORAGE_DATA_LENGTH         = 0xa4,
    CMD_STORAGE_START_TIME          = 0xa5,
    CMD_STORAGE_DATA                = 0xa6
};

enum response_code {
    RES_FREE_FEEDBACK               = 0x0c,
    RES_STORAGE_DATA                = 0x0f,
    RES_STORAGE_START_TIME_DATE     = 0x07,
    RES_STORAGE_DATA_LENGTH         = 0x08,
    RES_STORAGE_START_TIME_TIME     = 0x12,
};

static const struct {
    unsigned char code;
    const char *name;
} command_list[] = {
    { CMD_STOP_SENDING_STORAGE_DATA,    "stop sending storage data"     },
    { CMD_STOP_SENDING_REALTIME_DATA,   "stop sending realtime data"    },
    { CMD_STORAGE_DATA_LENGTH,          "ask for storage data length"   },
    { CMD_STORAGE_START_TIME,           "ask for storage start time"    },
    { CMD_STORAGE_DATA,                 "ask for storage date"          },
};

struct cms50f_device_instance_t {
    int fd;
    const char *name;
};

static cms50f_status_t _close(cms50f_device_t);
static const char *command_name(enum command_code code);
static cms50f_status_t send_command(cms50f_device_t device, enum command_code code);
static cms50f_status_t check_reponse(cms50f_device_t device, enum response_code expected);

cms50f_device_t cms50f_device_create(const char *name)
{
    cms50f_device_t instance = calloc(1, sizeof(struct cms50f_device_instance_t));
    if (instance) {
        instance->fd = -1;
        instance->name = name;
    }
    return instance;
}

cms50f_device_t cms50f_device_open(const char *name)
{
    LOG_DEBUG("Trying to open %s", name);
    cms50f_device_t instance = cms50f_device_create(name);
    if (instance && (instance->fd = open(instance->name, O_RDWR | O_NOCTTY | O_NDELAY | O_CLOEXEC)) < 0) {
        LOG_DEBUG("device %s could not be opened: %s", instance->name, strerror(errno));
        free(instance);
        return NULL;
    }
    LOG_DEBUG("device %s opened", instance->name);
    return instance;
}

static cms50f_status_t _close(cms50f_device_t device)
{
    ASSERT_DEVICE(device);

    if (close(device->fd) < 0) {
        LOG_DEBUG("device %s could not be closed: %s", device->name, strerror(errno));
        return CMS50F_ECLOSE;
    }
    device->fd = -1;

    return CMS50F_SUCCESS;
}

cms50f_status_t cms50f_device_destroy(cms50f_device_t *device_ptr)
{
    if (device_ptr && _close(*device_ptr) == CMS50F_SUCCESS) {
        free(*device_ptr);
        *device_ptr = NULL;
        return CMS50F_SUCCESS;
    }
    return CMS50F_EINVAL;
}

const char *cms50f_strerror(cms50f_status_t code)
{
    for (int i = 0; i < sizeof(cms50f_errlist)/sizeof(cms50f_errlist[0]); ++i) {
        if (cms50f_errlist[i].code == code) return cms50f_errlist[i].msg;
    }
    char buffer[CMS50F_ERROR_MSG_SIZE];
    snprintf(buffer, CMS50F_ERROR_MSG_SIZE, "Unknown error:\t%d", code);
    return strdup(buffer);
}

cms50f_status_t cms50f_terminal_configure(cms50f_device_t device)
{
    ASSERT_DEVICE(device);

    LOG_DEBUG("trying to configure device %s", device-> name);

    struct termios cfg  = {0};
    cfg.c_cflag         = CS8 | CREAD | CLOCAL | HUPCL;

    if (cfsetospeed(&cfg, B115200) < 0 || cfsetispeed(&cfg, B115200) < 0) {
        LOG_DEBUG("Could not set baudrate for device: %s", device->name);
        return CMS50F_ESETSPEED;
    }

    if (tcsetattr(device->fd, TCSANOW, &cfg) < 0) {
        LOG_DEBUG("Error configuring terminal %s: %s", device->name, strerror(errno));
        return CMS50F_ESETCFG;
    }

    LOG_DEBUG("device %s configured", device->name);

    return CMS50F_SUCCESS;
}

cms50f_status_t cms50f_stop_sending_storage_data(cms50f_device_t device)
{
    send_command(device, CMD_STOP_SENDING_STORAGE_DATA);
    return check_reponse(device, RES_FREE_FEEDBACK);
}


cms50f_status_t cms50f_stop_sending_realtime_data(cms50f_device_t device)
{
    send_command(device, CMD_STOP_SENDING_REALTIME_DATA);
    return check_reponse(device, RES_FREE_FEEDBACK);
}

cms50f_status_t cms50f_storage_data_length(cms50f_device_t device, int *duration)
{
    send_command(device, CMD_STORAGE_DATA_LENGTH);

    unsigned char expected = RES_STORAGE_DATA_LENGTH;
    LOG_DEBUG("expected answer <%02x>", expected);

    ssize_t n;
    unsigned char buffer[SMALLBUFFER] = {0};
    if ((n = read(device->fd, buffer, sizeof(buffer))) < 0) return CMS50F_EREAD;
    DUMP_BUFFER("%02x", buffer, sizeof(buffer));
    if (buffer[0] != expected) return CMS50F_EUNEXP;
    HIGH_BIT_OFF(buffer, n);

    int x = ((buffer[1] & 0x04) << 5);
    x |= buffer[4];
    x |= (buffer[5] | ((buffer[1] & 0x08) << 4)) << 8;
    x |= (buffer[6] | ((buffer[1] & 0x10) << 3)) << 3;

    *duration = x / 2;

    return CMS50F_SUCCESS;
}

cms50f_status_t cms50f_storage_start_time(cms50f_device_t device, time_t *starttime)
{
    send_command(device, CMD_STORAGE_START_TIME);

    unsigned char expected = RES_STORAGE_START_TIME_DATE;
    LOG_DEBUG("expected answer <%02x>", expected);

    ssize_t n;
    unsigned char buffer[SMALLBUFFER] = {0};
    if ((n = read(device->fd, buffer, sizeof(buffer))) < 0) return CMS50F_EREAD;
    DUMP_BUFFER("%02x", buffer, sizeof(buffer));
    if (buffer[0] != expected) return CMS50F_EUNEXP;
    HIGH_BIT_OFF(buffer, n);

    struct tm info;
    info.tm_year    = buffer[4] * 100 + buffer[5] - 1900;
    info.tm_mon     = buffer[6] - 1;
    info.tm_mday    = buffer[7];

    info.tm_hour    = buffer[12];
    info.tm_min     = buffer[13];
    info.tm_sec     = buffer[14];

    *starttime = mktime(&info);

    return CMS50F_SUCCESS;
}

cms50f_status_t cms50f_storage_data(cms50f_device_t device, int expected_length, time_t starttime, handler_t handler)
{
    send_command(device, CMD_STORAGE_DATA);

    unsigned char expected = RES_STORAGE_DATA;
    LOG_DEBUG("expected answer <%02x>", expected);

    ssize_t n = {0};
    unsigned char buffer[8];
    unsigned count = {0};
    if ((n = read(device->fd, buffer, sizeof(buffer))) < 0) return CMS50F_EREAD;
    while (n > 0) {
        if (buffer[0] != expected) {
            DUMP_BUFFER("%02x", buffer, n);
            return CMS50F_EUNEXP;
        }
        DUMP_BUFFER("%02x", buffer, n);
        HIGH_BIT_OFF(buffer, n);
        handler(&starttime, buffer[2], buffer[3], expected_length - ++count); ++starttime;
        if (count == expected_length) break;
        handler(&starttime, buffer[4], buffer[5], expected_length - ++count); ++starttime;
        if (count == expected_length) break;
        handler(&starttime, buffer[6], buffer[7], expected_length - ++count); ++starttime;
        if (count == expected_length) break;
        usleep(1000);
        n = read(device->fd, buffer, n);
    }

    LOG_DEBUG("%d values expected", expected_length);
    LOG_DEBUG("%d values downloaded", count);
    assert(count == expected_length);

    return CMS50F_SUCCESS;
}


static cms50f_status_t send_command(cms50f_device_t device, enum command_code code)
{
    ASSERT_DEVICE(device);
    LOG_DEBUG("going to send command <%s>", command_name(code));

    unsigned char buffer[] = {0x7d, 0x81, code, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80};
    DUMP_BUFFER("%02x", buffer, sizeof(buffer));
    
    if (write(device->fd, buffer, sizeof(buffer)) < 0) return CMS50F_EWRITE;
    LOG_DEBUG("command <%s> send", command_name(code));

    usleep(4500);

    return CMS50F_SUCCESS;
}

cms50f_status_t check_reponse(cms50f_device_t device, enum response_code expected)
{
    LOG_DEBUG("expected answer <%02x>", expected);

    unsigned char buffer[SMALLBUFFER] = {0};
    if (read(device->fd, buffer, sizeof(buffer)) < 0) return CMS50F_EREAD;
    DUMP_BUFFER("%02x", buffer, sizeof(buffer));

    if (buffer[0] != expected) return CMS50F_EUNEXP;

    return CMS50F_SUCCESS;
}

static const char *command_name(enum command_code code)
{
    for (int i = 0; i < sizeof(command_list)/sizeof(command_list[0]); ++i)
        if (command_list[i].code == code) return command_list[i].name;
    return "Unknown command";
}
