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
    static char plot_commands_filename[64] = {0};
    static char pdf_filename[32] = {0};
    static char csv_filename[32] = {0};
    static char first_timestamp[32] = {0};
    static char title[32] = {0};

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

    if (out == 0) {
        char fullpath[PATH_MAX];
        strftime(plot_commands_filename, sizeof(plot_commands_filename), "%Y%m%d%H%M%S", localtime(timestamp));
        if ((out = fopen(plot_commands_filename, "w")) == NULL) {
            LOG_ERROR("could not open file: %s", plot_commands_filename);
        }
        realpath(plot_commands_filename, fullpath);
        LOG_DEBUG("file %s opened", fullpath);

        strftime(pdf_filename, sizeof(pdf_filename), "%Y%m%d_%H%M%S.pdf", localtime(timestamp));
        strftime(csv_filename, sizeof(csv_filename), "%Y%m%d%H%M%S.csv", localtime(timestamp));
        strftime(first_timestamp, sizeof(first_timestamp), "%Y-%m-%d, %H:%M:%S", localtime(timestamp));

        setlocale(LC_ALL, "de_DE");
        strftime(title, sizeof(title), "%d %B %Y", localtime(timestamp));
    }

    if (spo2 == 0 || bpm == 0) return;

    if (spo2 < min_spo2) min_spo2 = spo2;
    if (spo2 > max_spo2) max_spo2 = spo2;
    mean_spo2 = mean_spo2 + spo2;

    if (bpm < min_bpm) min_bpm = bpm;
    if (bpm > max_bpm) max_bpm = bpm;
    mean_bpm = mean_bpm + bpm;

    if (spo2 >= 90 && last_spo2 < 90) ++below_90;
    if (spo2 >= 91 && last_spo2 < 91) ++below_91;
    if (spo2 >= 92 && last_spo2 < 92) ++below_92;
    last_spo2 = spo2;

    ++count;

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
        fprintf(out, "set output '%s'\n", pdf_filename);
        fprintf(out, "%s\n", "set grid ytics");
        fprintf(out, "set title '%s'\n", title);

        fprintf(out, "%s\n", "set label sprintf(\" min SpO2 = %d\\n max SpO2 = %d\\nmean SpO2 = %d\", min_spo2, max_spo2, mean_spo2) at character 10,5\n");
        fprintf(out, "%s\n", "set label sprintf(\" min BPM = %d\\n max BPM = %d\\nmean BPM = %d\", min_bpm, max_bpm, mean_bpm) at character 30,5");

        fprintf(out, "%s\n", "set label sprintf(\"<90 = %d\\n<91 = %d\\n<92 = %d\", below_90, below_91, below_92) at character 50,5");

        fprintf(out, "plot '%s' %s\n", csv_filename, "u 1:($3 == 0 ? NaN : $3) w l t 'SpO2' lt rgb \"blue\" lw 0, '' u 1:($4 == 0 ? NaN : $4) w l t 'BPM' lt rgb \"red\" lw 0");


        if (fclose(out) == EOF) LOG_ERROR("could not close file: %s", strerror(errno));
        else LOG_DEBUG("%s", "file closed");

        char cmd[128] = {0};
        snprintf(cmd, sizeof(cmd), "gnuplot < %s", plot_commands_filename);
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
    const char *input_file = NULL;
    while ((option = getopt(argc, argv, "c:i:")) != -1)
    {
        switch (option)
        {
            case 'c':
                force_count = atoi(optarg);
                break;
            case 'i':
                input_file = optarg;
                break;
            default:
                abort();
        }
    }

    if (input_file) {
        printf("Loading data from file: %s\n", input_file);

        FILE *in = {0};
        char fullpath[PATH_MAX];

        if ((in = fopen(input_file, "r")) == NULL) {
            LOG_ERROR("could not open file: %s", input_file);
        }
        realpath(input_file, fullpath);
        LOG_DEBUG("file %s opened", fullpath);

        unsigned line_count = {0};
        ssize_t read;
        char *line;
        size_t len;
        while ((read = getline(&line, &len, in)) != -1) ++line_count;

        printf("line_count: %u\n", line_count);

        fseek(in, 0, SEEK_SET);

        struct tm info;
        int spo2, bpm;
        while (line_count > 0) {
            fscanf(in, "%d-%d-%dT%d:%d:%d+01:00, spo: %d, bpm: %d", &info.tm_year, &info.tm_mon, &info.tm_mday, &info.tm_hour, &info.tm_min, &info.tm_sec, &spo2, &bpm);
            info.tm_year = info.tm_year - 1900;
            info.tm_mon = info.tm_mon - 1;
            time_t time = mktime(&info);
            print_to_csv_file(&time, spo2, bpm, --line_count);
            print_to_gnuplot_file(&time, spo2, bpm, line_count);
        }

        printf("Done");

        return 0;
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
