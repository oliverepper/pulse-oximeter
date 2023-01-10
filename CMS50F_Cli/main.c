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
#include <locale.h>

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
            LOG_ERROR("could not open file: %s", buffer);
        }
        realpath(buffer, fullpath);
        LOG_DEBUG("file %s opened", fullpath);

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
            LOG_ERROR("could not open file: %s", buffer);
        }
        realpath(buffer, fullpath);
        LOG_DEBUG("file %s opened", fullpath);
        fprintf(out, "DATE,TIME,SPO2,PULSE\n");

    }
    print_orig_format(out, timestamp, spo2, bpm);
    if (rest == 0) {
        if (fclose(out) == EOF) LOG_ERROR("could not close file: %s", strerror(errno));
        else LOG_DEBUG("%s", "file closed");
    }
}

static void print_to_gnuplot_file(time_t *timestamp, spo2_t spo2, bpm_t bpm, unsigned rest)
{
    static FILE *out = {0};
    static char plot_commands[64] = {0};
    if (out == 0) {
        char fullpath[PATH_MAX];
        strftime(plot_commands, sizeof(plot_commands), "%Y%m%d%H%M%S_plot_commands.txt", localtime(timestamp));
        if ((out = fopen(plot_commands, "w")) == NULL) {
            LOG_ERROR("could not open file: %s", plot_commands);
        }
        realpath(plot_commands, fullpath);
        LOG_DEBUG("file %s opened", fullpath);
    }

    if (spo2 == 0 || bpm == 0) return;

    static unsigned min_spo2 = {100};
    static unsigned max_spo2 = {0};
    static unsigned mean_spo2 = {0};

    static unsigned min_bpm = {200};
    static unsigned max_bpm = {0};
    static unsigned mean_bpm = {0};

    static unsigned below_90 = {0};
    static unsigned below_91 = {0};
    static unsigned below_92 = {0};

    static unsigned count = {0};

    static unsigned last_spo2 = {100};

    if (spo2 < min_spo2) min_spo2 = spo2;
    if (spo2 > max_spo2) max_spo2 = spo2;
    mean_spo2 = mean_spo2 + spo2;

    if (bpm < min_bpm) min_bpm = bpm;
    if (bpm > max_bpm) max_bpm = bpm;
    mean_bpm = mean_bpm + bpm;
    ++count;

    if (spo2 >= 90 && last_spo2 < 90) ++below_90;
    if (spo2 >= 91 && last_spo2 < 91) ++below_91;
    if (spo2 >= 92 && last_spo2 < 92) ++below_92;

    last_spo2 = spo2;

    static char first_timestamp[32] = {0};
    if (strlen(first_timestamp) == 0)
        strftime(first_timestamp, sizeof(first_timestamp), "%Y-%m-%d, %H:%M:%S", localtime(timestamp));

    static char csv_filename[32] = {0};
    if (strlen(csv_filename) == 0)
        strftime(csv_filename, sizeof(csv_filename), "%Y%m%d%H%M%S.csv", localtime(timestamp));

    static char title[32] = {0};
    if (strlen(title) == 0) {
        setlocale(LC_ALL, "de_DE");
        strftime(title, sizeof(title), "%d %B %Y", localtime(timestamp));
    }

    static char pdf[32] = {0};
    if (strlen(pdf) == 0)
        strftime(pdf, sizeof(pdf), "%Y%m%d_%H%M%S.pdf", localtime(timestamp));

    if (rest == 0) {
        fprintf(out, "min_spo2 = %d\n", min_spo2);
        fprintf(out, "max_spo2 = %d\n", max_spo2);
        fprintf(out, "mean_spo2 = %d\n\n", mean_spo2 / count);

        fprintf(out, "min_bpm = %d\n", min_bpm);
        fprintf(out, "max_bpm = %d\n", max_bpm);
        fprintf(out, "mean_bpm = %d\n\n", mean_bpm / count);

        fprintf(out, "below_90 = %d\n", below_90);
        fprintf(out, "below_91 = %d\n", below_91);
        fprintf(out, "below_92 = %d\n\n", below_92);

        fprintf(out, "%s\n", "set xdata time");
        fprintf(out, "%s\n", "set timefmt \"%Y-%m-%d, %H:%M:%S\"");
        fprintf(out, "%s\n", "set format x \"%H:%M\"");
        char last_timestamp[32];
        strftime(last_timestamp, sizeof(last_timestamp), "%Y-%m-%d, %H:%M:%S", localtime(timestamp));
        fprintf(out, "set xrange [\"%s\":\"%s\"]\n", first_timestamp, last_timestamp);
        fprintf(out, "%s\n", "set yrange [40:100]");
        fprintf(out, "%s\n", "set key autotitle columnhead");
        fprintf(out, "%s\n", "set key bottom");
        fprintf(out, "%s\n", "set terminal pdf size 29.7cm,21cm font \"Menlo\"");
        fprintf(out, "set output '%s'\n", pdf);
        fprintf(out, "%s\n", "set grid ytics");
        fprintf(out, "set title '%s'\n", title);

        fprintf(out, "%s\n", "set label sprintf(\" min SpO2 = %d\\n max SpO2 = %d\\nmean SpO2 = %d\", min_spo2, max_spo2, mean_spo2) at character 10,5\n");
        fprintf(out, "%s\n", "set label sprintf(\" min BPM = %d\\n max BPM = %d\\nmean BPM = %d\", min_bpm, max_bpm, mean_bpm) at character 30,5");

        fprintf(out, "%s\n", "set label sprintf(\"<90 = %d\\n<91 = %d\\n<92 = %d\", below_90, below_91, below_92) at character 50,5");

        fprintf(out, "plot '%s' %s\n", csv_filename, "u 1:($3 == 0 ? NaN : $3) w l t 'SpO2' lt rgb \"blue\" lw 0, '' u 1:($4 == 0 ? NaN : $4) w l t 'BPM' lt rgb \"red\" lw 0");


        if (fclose(out) == EOF) LOG_ERROR("could not close file: %s", strerror(errno));
        else LOG_DEBUG("%s", "file closed");

        char cmd[128] = {0};
        snprintf(cmd, sizeof(cmd), "gnuplot < %s", plot_commands);
        printf("%s\n", cmd);
        system(cmd);
    }
}

static void print_all(time_t *timestamp, spo2_t spo2, bpm_t bpm, unsigned rest)
{
    print_to_stdout(timestamp, spo2, bpm, rest);
    print_to_file(timestamp, spo2, bpm, rest);
    print_to_csv_file(timestamp, spo2, bpm, rest);
    print_to_gnuplot_file(timestamp, spo2, bpm, rest);
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
