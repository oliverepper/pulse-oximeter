//
//  cms50f.h
//  CMS50F
//
//  Created by Oliver Epper on 15.12.22.
//

#ifndef cms50f_h
#define cms50f_h

#include <time.h>

#define CMS50F_SUCCESS              0   /* successful result */
/* error codes */
#define CMS50F_ECLOSE               2
#define CMS50F_EINVAL               3
#define CMS50F_ESETSPEED            4
#define CMS50F_ESETCFG              5
#define CMS50F_EWRITE               6
#define CMS50F_EREAD                7
#define CMS50F_EUNEXP               8

#define CMS50F_ERROR_MSG_SIZE       80

typedef int cms50f_status_t;

typedef struct cms50f_device_instance_t *cms50f_device_t;
typedef unsigned spo2_t;
typedef unsigned bpm_t;
typedef void(*handler_t)(time_t *starttime, spo2_t spo2, bpm_t bpm, unsigned rest);

static const struct {
    int code;
    const char *msg;
} cms50f_errlist[] = {
    { CMS50F_ECLOSE,    "error closing device"                              },
    { CMS50F_EINVAL,    "invalid argument"                                  },
    { CMS50F_ESETSPEED, "error configuring baudrate"                        },
    { CMS50F_ESETCFG,   "tty config could not be configured"                },
    { CMS50F_EWRITE,    "error writing to device"                           },
    { CMS50F_EREAD,     "read from device failed"                           },
    { CMS50F_EUNEXP,    "unexpected answer from device, try reconnecting"   },
};

const char * cms50f_strerror(cms50f_status_t statcode);

cms50f_device_t cms50f_device_open(const char *name);
cms50f_status_t cms50f_device_destroy(cms50f_device_t *device);
cms50f_status_t cms50f_terminal_configure(cms50f_device_t device);

cms50f_status_t cms50f_stop_sending_storage_data(cms50f_device_t device);
cms50f_status_t cms50f_stop_sending_realtime_data(cms50f_device_t device);

cms50f_status_t cms50f_storage_data_length(cms50f_device_t device, int *duration);
cms50f_status_t cms50f_storage_start_time(cms50f_device_t device, time_t *starttime);
cms50f_status_t cms50f_storage_data(cms50f_device_t device, int data_length, time_t starttime, handler_t handler);

#endif /* cms50f_h */
