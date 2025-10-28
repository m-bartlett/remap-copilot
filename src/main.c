#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <libevdev/libevdev.h>
#include <linux/limits.h>
#include <libevdev/libevdev-uinput.h>
#include <linux/input.h>


#define eprintf(format, ...) fprintf(stderr, format __VA_OPT__(,) __VA_ARGS__)
#define fail(format, ...) eprintf(format __VA_OPT__(,) __VA_ARGS__); exit(EXIT_FAILURE);

#define DEFAULT_DELAY_MS 300
#define KEY_PRESSED 1
#define KEY_RELEASED 0

#ifndef COPILOT_REPLACE_KEY
#define COPILOT_REPLACE_KEY KEY_RIGHTCTRL
#endif

static struct libevdev *dev = NULL;
static struct libevdev_uinput *uidev = NULL;
static int fd = -1;
static volatile sig_atomic_t running = 1;
static bool verbose = false;

static pthread_t release_thread = 0;
static pthread_mutex_t release_mutex = PTHREAD_MUTEX_INITIALIZER;
static int release_pending = 0;

struct release_data {
    struct libevdev_uinput *uidev;
    unsigned int delay_ms;
};

static struct {
    bool meta_pressed:   1;
    bool shift_pressed:  1;
    bool copilot_active: 1;
} key_state = {false};


void print_usage(const char *prog_name) {
    eprintf("Usage: %s [OPTIONS]\n\n", prog_name);
    eprintf("Remap Windows Copilot key (Meta+Shift+F23) to Right Control.\n\n");
    eprintf("Options:\n");
    eprintf("  -d, --device <path>    Specify keyboard device (e.g., /dev/input/event3)\n");
    eprintf("  -t, --delay <ms>       Delay in milliseconds before releasing Right Control\n");
    eprintf("                         (default: %d)\n", DEFAULT_DELAY_MS);
    eprintf("  -l, --list             List available input devices\n");
    eprintf("  -h, --help             Show this help message\n\n");
}


void list_devices(void) {
    char devpath[PATH_MAX];
    int fd;
    struct libevdev *test_dev;
    
    printf("Available input devices:\n");
    printf("%-20s %-40s %s\n", "Device", "Name", "Capabilities");
    printf("================================================================================\n");
    
    for (int i = 0; i < 32; i++) {
        snprintf(devpath, sizeof(devpath), "/dev/input/event%d", i);
        
        fd = open(devpath, O_RDONLY);
        if (fd < 0) continue;
        
        if (libevdev_new_from_fd(fd, &test_dev) < 0) {
            close(fd);
            continue;
        }
        
        const char *name = libevdev_get_name(test_dev);
        int has_keys = libevdev_has_event_type(test_dev, EV_KEY);
        int has_f23  = libevdev_has_event_code(test_dev, EV_KEY, KEY_F23);
        int has_kbd  = libevdev_has_event_code(test_dev, EV_KEY, KEY_A) &&
                       libevdev_has_event_code(test_dev, EV_KEY, KEY_Z);
        
        if (has_keys) {
            printf("%-20s %-40s", devpath, name);
            if (has_f23) printf(" [F23]");
            if (has_kbd) printf(" [Keyboard]");
            printf("\n");
        }
        
        libevdev_free(test_dev);
        close(fd);
    }
}


void signal_handler(int sig) {
    eprintf("Got signal %s, cleaning up...\n", strsignal(sig));
    running = 0;
}


void *delayed_release_thread(void *arg) {
    struct release_data *data = (struct release_data *)arg;
    struct timespec ts;
    
    ts.tv_sec = data->delay_ms / 1000;
    ts.tv_nsec = (data->delay_ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
    
    pthread_mutex_lock(&release_mutex);
    if (release_pending) {
        libevdev_uinput_write_event(data->uidev, EV_KEY, COPILOT_REPLACE_KEY, 0);
        libevdev_uinput_write_event(data->uidev, EV_SYN, SYN_REPORT, 0);
        release_pending = 0;
    }
    pthread_mutex_unlock(&release_mutex);
    
    free(data);
    return NULL;
}


void schedule_delayed_release(struct libevdev_uinput *uidev, unsigned int delay_ms) {
    pthread_mutex_lock(&release_mutex);
    
    if (release_pending && release_thread) {
        pthread_cancel(release_thread);
        pthread_join(release_thread, NULL);
    }
    
    release_pending = 1;
    pthread_mutex_unlock(&release_mutex);
    
    struct release_data *data = malloc(sizeof(struct release_data));
    data->uidev = uidev;
    data->delay_ms = delay_ms;
    
    if (pthread_create(&release_thread, NULL, delayed_release_thread, data) != 0) {
        perror("Failed to create release thread");
        free(data);
        release_pending = 0;
    }
}


void cancel_delayed_release(void) {
    pthread_mutex_lock(&release_mutex);
    if (release_pending && release_thread) {
        pthread_cancel(release_thread);
        pthread_join(release_thread, NULL);
        release_pending = 0;
    }
    pthread_mutex_unlock(&release_mutex);
}


bool find_keyboard_device(char **path) {
    char devpath[PATH_MAX];
    int fd;
    struct libevdev *test_dev;
    bool return_value = false;
    
    // Try to find a device with F23
    for (int i = 0; i < 32; i++) {
        snprintf(devpath, sizeof(devpath), "/dev/input/event%d", i);
        
        fd = open(devpath, O_RDONLY);
        if (fd < 0) continue;
        
        if (libevdev_new_from_fd(fd, &test_dev) < 0) {
            close(fd);
            continue;
        }
        if (libevdev_has_event_code(test_dev, EV_KEY, KEY_F23)) goto keyboard_found;
        libevdev_free(test_dev);
        close(fd);
    }
    
    // Fallback: find first device with common keyboard keys
    for (int i = 0; i < 32; i++) {
        snprintf(devpath, sizeof(devpath), "/dev/input/event%d", i);
        
        fd = open(devpath, O_RDONLY);
        if (fd < 0) continue;
        
        if (libevdev_new_from_fd(fd, &test_dev) < 0) {
            close(fd);
            continue;
        }
        
        if (libevdev_has_event_code(test_dev, EV_KEY, KEY_A) &&
            libevdev_has_event_code(test_dev, EV_KEY, KEY_Z)) goto keyboard_found;
        libevdev_free(test_dev);
        close(fd);
    }

    goto find_keyboard_device_end;

keyboard_found:
    *path = strdup(devpath);
    return_value = true;

find_keyboard_device_end:
    libevdev_free(test_dev);
    close(fd);
    return return_value;
}


void clear_keyboard_state(struct libevdev *dev, struct libevdev_uinput *uidev) {
    for (unsigned int code = 0; code < KEY_MAX; code++) {
        if (!libevdev_has_event_code(dev, EV_KEY, code)) continue;
        int state = libevdev_get_event_value(dev, EV_KEY, code);
        if (state != KEY_RELEASED) {
            libevdev_uinput_write_event(uidev, EV_KEY, code, KEY_RELEASED);
        }
    }
    libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
}


int main(int argc, char **argv) {
    int return_code;
    struct input_event event;
    char *device_path = NULL;
    uint delay_ms = DEFAULT_DELAY_MS;

    const static struct option long_options[] = {
        {"device",  required_argument, 0, 'd'},
        {"delay",   required_argument, 0, 't'},
        {"list",    no_argument,       0, 'l'},
        {"verbose", no_argument,       0, 'v'},
        {"help",    no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
    

    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "d:t:lhv", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'd':
                device_path = optarg;
                break;
            case 't':
                delay_ms = atoi(optarg);
                if (delay_ms < 0 || delay_ms > 10000) {
                    fail("Error: Delay must be between 0 and 10000 ms\n");
                }
                break;
            case 'v':
                verbose = true;
                break;
            case 'l':
                list_devices();
                return 0;
            case 'h':
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    if (device_path == NULL) {
        if (!find_keyboard_device(&device_path)) {
            fail("Error: No suitable keyboard device found!\n"
                 "Use --list to see available devices.\n");
        }
    } else {
        if (access(device_path, R_OK) != 0) {
            fail("Error: Cannot access device %s\n%s\n", device_path, strerror(errno));
        }
    }
    
    fd = open(device_path, O_RDONLY);
    if (fd < 0) {
        fail("Failed to open device %s\n%s\n", device_path, strerror(errno));
    }
    
    return_code = libevdev_new_from_fd(fd, &dev);
    if (return_code < 0) {
        close(fd);
        fail("Failed to init libevdev (%d)\n", return_code);
    }
    
    if (verbose) {
        eprintf("Input device: %s\n", libevdev_get_name(dev));
        eprintf("Device path: %s\n", device_path);
        eprintf("Release delay: %u ms\n", delay_ms);
    }

    return_code = libevdev_grab(dev, LIBEVDEV_GRAB);
    if (return_code < 0) {
        libevdev_free(dev);
        close(fd);
        fail("Failed to grab device: %s\nAre you running as root?\n", strerror(-return_code));
    }
    
    libevdev_enable_event_code(dev, EV_KEY, COPILOT_REPLACE_KEY, NULL);
    
    return_code = libevdev_uinput_create_from_device(dev, LIBEVDEV_UINPUT_OPEN_MANAGED, &uidev);
    if (return_code < 0) {
        fail("Failed to create uinput device (%d)\n", return_code);
        libevdev_grab(dev, LIBEVDEV_UNGRAB);
        libevdev_free(dev);
        close(fd);
        return 1;
    }
    
    if (verbose) {
        eprintf("Virtual device created: %s\n", libevdev_uinput_get_devnode(uidev));
        eprintf("\nRemapper active. Press Ctrl+C to exit.\n");
    }

    clear_keyboard_state(dev, uidev);

    while (running) {
        return_code = libevdev_next_event(dev,
                                          LIBEVDEV_READ_FLAG_NORMAL | LIBEVDEV_READ_FLAG_BLOCKING,
                                          &event);
        if (return_code == LIBEVDEV_READ_STATUS_SUCCESS) {
            if (event.type != EV_KEY) goto event_write;

            switch (event.code) {
            case KEY_LEFTMETA:
                key_state.meta_pressed = (event.value != KEY_RELEASED);
                if (key_state.copilot_active) goto skip_event_write;
                break;
            case KEY_LEFTSHIFT:
                key_state.shift_pressed = (event.value != KEY_RELEASED);
                if (key_state.copilot_active) goto skip_event_write;
                break;
            case KEY_F23:
                if (key_state.meta_pressed && key_state.shift_pressed) {
                    if (event.value == KEY_PRESSED) {
                        cancel_delayed_release();
                        libevdev_uinput_write_event(uidev, EV_KEY, COPILOT_REPLACE_KEY, 1);
                        libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
                        key_state.copilot_active = true;
                    }
                    else if (event.value == KEY_RELEASED) {
                        if (key_state.copilot_active) {
                            schedule_delayed_release(uidev, delay_ms);
                            key_state.copilot_active = false;
                        }
                    }
                    goto skip_event_write;
                }
                break;
            }

            event_write:
            libevdev_uinput_write_event(uidev, event.type, event.code, event.value);

            skip_event_write:
            true;
        }
        else if (return_code == -EAGAIN) {
            continue;
        }
        else if (return_code < 0) {
            eprintf("Error reading event: %s\n", strerror(-return_code));
            break;
        }
    }
    
    cancel_delayed_release();
    
    if (uidev) libevdev_uinput_destroy(uidev);
    if (dev) { libevdev_grab(dev, LIBEVDEV_UNGRAB); libevdev_free(dev); }
    if (fd >= 0) close(fd);
    pthread_mutex_destroy(&release_mutex);
    
    return 0;
}