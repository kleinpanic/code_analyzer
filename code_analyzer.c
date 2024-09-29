#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#include <sys/statvfs.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/hdreg.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

// Function to get the CPU usage of a process
double get_cpu_usage(int pid) {
    char path[40];
    FILE *file;
    sprintf(path, "/proc/%d/stat", pid);
    file = fopen(path, "r");
    if (!file) return -1;

    long unsigned int utime, stime, cutime, cstime;
    double total_time;
    long unsigned int starttime;
    unsigned long int total_jiffies;

    fscanf(file, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*u %*u %lu %lu %lu %lu %*d %*d %*d %*d %lu",
           &utime, &stime, &cutime, &cstime, &starttime);
    fclose(file);

    total_time = (double) (utime + stime + cutime + cstime);
    total_jiffies = sysconf(_SC_CLK_TCK);
    return total_time / total_jiffies;
}

// Function to get the memory usage of a process
long get_memory_usage(int pid) {
    char path[40], line[100];
    FILE *file;
    sprintf(path, "/proc/%d/status", pid);
    file = fopen(path, "r");
    if (!file) return -1;

    long memory = -1;
    while (fgets(line, 100, file)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line + 6, "%ld", &memory);
            break;
        }
    }
    fclose(file);
    return memory;
}

// Function to get disk usage
void get_disk_usage(const char *path, long *total, long *free) {
    struct statvfs stat;
    if (statvfs(path, &stat) != 0) {
        *total = -1;
        *free = -1;
        return;
    }
    *total = stat.f_blocks * stat.f_frsize;
    *free = stat.f_bfree * stat.f_frsize;
}

// Function to get disk I/O usage
void get_disk_io_usage(int pid, long *read_bytes, long *write_bytes) {
    char path[40], line[100];
    FILE *file;
    sprintf(path, "/proc/%d/io", pid);
    file = fopen(path, "r");
    if (!file) {
        *read_bytes = -1;
        *write_bytes = -1;
        return;
    }

    while (fgets(line, 100, file)) {
        if (strncmp(line, "read_bytes:", 11) == 0) {
            sscanf(line + 11, "%ld", read_bytes);
        } else if (strncmp(line, "write_bytes:", 12) == 0) {
            sscanf(line + 12, "%ld", write_bytes);
        }
    }
    fclose(file);
}

// Function to get network usage
void get_network_usage(long *rx_bytes, long *tx_bytes) {
    FILE *file;
    char line[256];
    *rx_bytes = 0;
    *tx_bytes = 0;

    file = fopen("/proc/net/dev", "r");
    if (!file) return;

    // Skip the first two lines
    fgets(line, sizeof(line), file);
    fgets(line, sizeof(line), file);

    while (fgets(line, sizeof(line), file)) {
        char iface[32];
        long rx, tx;
        sscanf(line, "%s %ld %*d %*d %*d %*d %*d %*d %*d %ld", iface, &rx, &tx);
        *rx_bytes += rx;
        *tx_bytes += tx;
    }
    fclose(file);
}

// Function to run a command and analyze its usage for a given duration
void analyze_command(char *command[], int duration) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child process: Execute the command
        execvp(command[0], command);
        perror("execvp");
        exit(1);
    } else if (pid > 0) {
        // Parent process: Monitor the child process
        double start_cpu_time = get_cpu_usage(pid);
        long start_rx_bytes, start_tx_bytes;
        get_network_usage(&start_rx_bytes, &start_tx_bytes);

        long total_memory_usage = 0;
        long total_read_bytes = 0, total_write_bytes = 0;
        int sample_count = 0;

        time_t start = time(NULL);
        while (time(NULL) - start < duration) {
            long memory_usage = get_memory_usage(pid);
            long read_bytes, write_bytes;
            get_disk_io_usage(pid, &read_bytes, &write_bytes);

            if (memory_usage < 0 || read_bytes < 0 || write_bytes < 0) {
                fprintf(stderr, "Error: Could not retrieve information for PID %d\n", pid);
                break;
            }

            total_memory_usage += memory_usage;
            total_read_bytes = read_bytes;  // Update to reflect actual read bytes
            total_write_bytes = write_bytes;  // Update to reflect actual write bytes
            sample_count++;

            sleep(1);
        }

        double end_cpu_time = get_cpu_usage(pid);
        long end_rx_bytes, end_tx_bytes;
        get_network_usage(&end_rx_bytes, &end_tx_bytes);

        kill(pid, SIGKILL); // Terminate the process after the duration
        waitpid(pid, NULL, 0); // Wait for the child process to finish

        double cpu_time_diff = end_cpu_time - start_cpu_time;
        long avg_memory_usage = total_memory_usage / sample_count;
        long total_rx_bytes = end_rx_bytes - start_rx_bytes;
        long total_tx_bytes = end_tx_bytes - start_tx_bytes;

        int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
        double elapsed_time = duration;
        double cpu_usage_percentage = (cpu_time_diff / elapsed_time) * 100.0 * num_cores;

        long disk_total, disk_free;
        get_disk_usage("/", &disk_total, &disk_free);

        printf("Command: %s\n", command[0]);
        printf("CPU Usage: %.2f%%\n", cpu_usage_percentage);
        printf("Average Memory Usage: %ld kB\n", avg_memory_usage);
        printf("Disk Read Bytes: %ld\n", total_read_bytes);
        printf("Disk Write Bytes: %ld\n", total_write_bytes);
        printf("Network Received Bytes: %ld\n", total_rx_bytes);
        printf("Network Transmitted Bytes: %ld\n", total_tx_bytes);
        printf("Disk Total: %ld bytes\n", disk_total);
        printf("Disk Free: %ld bytes\n", disk_free);
    } else {
        perror("fork");
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <command> [args] <duration>\n", argv[0]);
        return 1;
    }

    int duration = atoi(argv[argc - 1]); // The last argument is the duration
    argv[argc - 1] = NULL; // Remove the duration from the command arguments

    analyze_command(argv + 1, duration);

    return 0;
}
