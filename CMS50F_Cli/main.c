//
//  main.c
//  FMS50F_Cli
//
//  Created by Oliver Epper on 15.12.22.
//

#include "cms50f.h"
#include "log.h"
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#define DEVICE "/dev/tty.usbserial-0001"

static void print(FILE *stream, time_t *timestamp, spo2_t spo2, bpm_t bpm)
{
    static char timezone_buffer[7] = {0};
    if (timezone_buffer[0] == 0) {
        tzset();
        long hours = -timezone / 3600;
        if (hours == 0) {
            timezone_buffer[0] = 'Z';
        } else if (hours > 0) {
            long minutes = labs(timezone / 3600) / 60;
            snprintf(timezone_buffer, sizeof(timezone_buffer), "+%02ld:%02ld", hours, minutes);
        } else if (hours < 0) {
            long minutes = labs(timezone / 3600) / 60;
            snprintf(timezone_buffer, sizeof(timezone_buffer), "+%02ld:%02ld", hours, minutes);
        }
    }

    char buffer[32] = {0};
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", localtime(timestamp));
    strncat(buffer, timezone_buffer, sizeof(buffer) - strlen(buffer) - 1);
    fprintf(stream, "%s, spo: %d, bpm: %d\n", buffer, spo2, bpm);
}

static void print_orig_format(FILE *stream, time_t *timestamp, spo2_t spo2, bpm_t bpm)
{
    char date_buffer[16] = {0};
    strftime(date_buffer, sizeof(date_buffer), "%Y-%m-%d", localtime(timestamp));
    char time_buffer[16] = {0};
    strftime(time_buffer, sizeof(time_buffer), "%H:%M:%S", localtime(timestamp));
    fprintf(stream, "%s, %s, %d, %d\n", date_buffer, time_buffer, spo2, bpm);
}

static void print_to_stdout(time_t *timestamp, spo2_t spo2, bpm_t bpm, unsigned rest)
{
    print(stdout, timestamp, spo2, bpm);
    if (rest == 0) { LOG_DEBUG("%s", "Done\n\n"); }
}

static void print_to_file(time_t *timestamp, spo2_t spo2, bpm_t bpm, unsigned rest)
{
    static FILE *out = {0};
    if (out == 0) {
        char buffer[32] = {0};
        char fullpath[PATH_MAX];
        strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S.txt", localtime(timestamp));
        if ((out = fopen(buffer, "w")) == NULL) {
            LOG_ERROR("could not open outputfile: %s", buffer);
        }
        realpath(buffer, fullpath);
        LOG_DEBUG("outputfile %s opened", fullpath);

    }
    print(out, timestamp, spo2, bpm);
    if (rest == 0) {
        if (fclose(out) == EOF) LOG_ERROR("could not close file: %s", strerror(errno));
        else LOG_DEBUG("%s", "file closed");
    }
}

static void print_to_csv_file(time_t *timestamp, spo2_t spo2, bpm_t bpm, unsigned rest)
{
    static FILE *out = {0};
    if (out == 0) {
        char buffer[32] = {0};
        char fullpath[PATH_MAX];
        strftime(buffer, sizeof(buffer), "%Y%m%d%H%M%S.csv", localtime(timestamp));
        if ((out = fopen(buffer, "w")) == NULL) {
            LOG_ERROR("could not open outputfile: %s", buffer);
        }
        realpath(buffer, fullpath);
        LOG_DEBUG("outputfile %s opened", fullpath);
        fprintf(out, "DATE,TIME,SPO2,PULSE\n");

    }
    print_orig_format(out, timestamp, spo2, bpm);
    if (rest == 0) {
        if (fclose(out) == EOF) LOG_ERROR("could not close file: %s", strerror(errno));
        else LOG_DEBUG("%s", "file closed");
    }
}

static void print_all(time_t *timestamp, spo2_t spo2, bpm_t bpm, unsigned rest)
{
    print_to_stdout(timestamp, spo2, bpm, rest);
    print_to_file(timestamp, spo2, bpm, rest);
    print_to_csv_file(timestamp, spo2, bpm, rest);
}

void die(cms50f_device_t device, cms50f_status_t status) {
    LOG_ERROR("%s", cms50f_strerror(status));
    if (status == CMS50F_EUNEXP) { /* can this be handled better? */}
    cms50f_device_destroy(&device);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    int option;
    unsigned force_count = 0;
    while ((option = getopt(argc, argv, "c:")) != -1)
    {
        switch (option)
        {
            case 'c':
                force_count = atoi(optarg);
                break;
            default:
                abort();
        }
    }

    cms50f_device_t device = cms50f_device_open(DEVICE);
    if (!device) { LOG_ERROR("Could not open device %s – %s", DEVICE, strerror(errno)); return 1; }

    cms50f_status_t status = cms50f_terminal_configure(device);
    if (status != CMS50F_SUCCESS) die(device, status);

    status = cms50f_stop_sending_storage_data(device);
    if (status != CMS50F_SUCCESS) die(device, status);

    status = cms50f_stop_sending_realtime_data(device);
    if (status != CMS50F_SUCCESS) die(device, status);

    int duration = {0};
    if (force_count == 0) {
        status = cms50f_storage_data_length(device, &duration);
        if (status != CMS50F_SUCCESS) die(device, status);
        else printf("Duration: %d\n", duration);
    } else {
        duration = force_count;
    }

    time_t starttime = {0};
    status = cms50f_storage_start_time(device, &starttime);
    if (status != CMS50F_SUCCESS) die(device, status);
    else printf("Starttime: %s", asctime(localtime(&starttime)));

    status = cms50f_storage_data(device, duration, starttime, &print_all);
    if (status != CMS50F_SUCCESS) die(device, status);

    cms50f_device_destroy(&device);
    printf("\n");

    return 0;
}
