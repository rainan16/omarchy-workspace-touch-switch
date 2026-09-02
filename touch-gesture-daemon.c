#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <linux/input.h>

static volatile sig_atomic_t running = 1;

static void handle_signal(int sig) {
    (void)sig;
    running = 0;
}

static int read_sysfs(const char *path, char *buf, size_t buflen) {
    FILE *f = fopen(path, "r");
    if (!f)
        return -1;
    if (!fgets(buf, (int)buflen, f)) {
        fclose(f);
        return -1;
    }
    fclose(f);
    buf[strcspn(buf, "\n")] = 0;
    return 0;
}

static int hex_bit_set(const char *line, unsigned int bit) {
    unsigned long words[16];
    int n = 0;
    const char *p = line;
    unsigned int idx;
    unsigned int off;
    int word_i;

    while (*p && n < 16) {
        char *end;
        while (*p == ' ' || *p == '\t')
            p++;
        if (!*p)
            break;
        words[n] = strtoul(p, &end, 16);
        if (end == p)
            break;
        n++;
        p = end;
    }
    if (n == 0)
        return 0;
    idx = bit / (sizeof(unsigned long) * 8);
    off = bit % (sizeof(unsigned long) * 8);
    word_i = n - 1 - (int)idx;
    if (word_i < 0 || word_i >= n)
        return 0;
    return (int)((words[word_i] >> off) & 1UL);
}

static int name_preferred(const char *name) {
    static const char *keys[] = {
        "ntrg", "n-trig", "atml", "elan", "goodix", "wacom", "surface", NULL
    };
    int i;
    for (i = 0; keys[i]; i++) {
        if (strcasestr(name, keys[i]))
            return 1;
    }
    return 0;
}

static int find_touchscreen(char *path, size_t pathlen) {
    const char *override = getenv("TOUCHSCREEN_DEVICE");
    DIR *dir;
    char fallback[PATH_MAX] = {0};
    char preferred[PATH_MAX] = {0};
    int preferred_direct = 0;
    struct dirent *entry;

    if (override && override[0]) {
        snprintf(path, pathlen, "%s", override);
        return 0;
    }

    dir = opendir("/sys/class/input");
    if (!dir)
        return -1;

    while ((entry = readdir(dir)) != NULL) {
        char sysbase[PATH_MAX];
        char file[PATH_MAX + 64];
        char name[256];
        char evbits[256];
        char absbits[256];
        char props[64];
        char devpath[PATH_MAX];
        int direct;

        if (strncmp(entry->d_name, "event", 5) != 0)
            continue;

        snprintf(sysbase, sizeof(sysbase), "/sys/class/input/%s/device", entry->d_name);
        snprintf(file, sizeof(file), "%s/name", sysbase);
        if (read_sysfs(file, name, sizeof(name)) < 0)
            continue;

        snprintf(file, sizeof(file), "%s/capabilities/ev", sysbase);
        if (read_sysfs(file, evbits, sizeof(evbits)) < 0)
            continue;
        snprintf(file, sizeof(file), "%s/capabilities/abs", sysbase);
        if (read_sysfs(file, absbits, sizeof(absbits)) < 0)
            continue;

        if (!hex_bit_set(evbits, EV_ABS) ||
            !hex_bit_set(absbits, ABS_MT_POSITION_X) ||
            !hex_bit_set(absbits, ABS_MT_POSITION_Y))
            continue;

        snprintf(file, sizeof(file), "%s/properties", sysbase);
        if (read_sysfs(file, props, sizeof(props)) < 0)
            props[0] = 0;
        direct = hex_bit_set(props, INPUT_PROP_DIRECT);

        snprintf(devpath, sizeof(devpath), "/dev/input/%s", entry->d_name);

        if (name_preferred(name)) {
            if (!preferred[0] || (direct && !preferred_direct)) {
                snprintf(preferred, sizeof(preferred), "%s", devpath);
                preferred_direct = direct;
            }
            continue;
        }

        if (direct && !fallback[0])
            snprintf(fallback, sizeof(fallback), "%s", devpath);
    }
    closedir(dir);

    if (preferred[0]) {
        snprintf(path, pathlen, "%s", preferred);
        return 0;
    }
    if (fallback[0]) {
        snprintf(path, pathlen, "%s", fallback);
        return 0;
    }
    return -1;
}

#define MAX_SLOTS 10

struct contact {
    int down;
    int have_x;
    int have_y;
    int x;
    int y;
};

static void emit_dir(int dx) {
    if (dx > 0) {
        syslog(LOG_INFO, "gesture swipe-right dx=%d", dx);
        printf("{\"gesture\":\"swipe-right\",\"direction\":\"next\"}\n");
        fflush(stdout);
    } else if (dx < 0) {
        syslog(LOG_INFO, "gesture swipe-left dx=%d", dx);
        printf("{\"gesture\":\"swipe-left\",\"direction\":\"prev\"}\n");
        fflush(stdout);
    }
}

static int horizontal(int dx, int dy, int swipe_min, int span_x, int span_y) {
    int adx = dx < 0 ? -dx : dx;
    int ady = dy < 0 ? -dy : dy;
    if (adx < swipe_min)
        return 0;
    if ((long long)adx * span_y <= (long long)ady * span_x)
        return 0;
    return 1;
}

static int centroid(const struct contact *slots, int *cx, int *cy) {
    int i;
    int sx = 0;
    int sy = 0;
    int n = 0;
    for (i = 0; i < MAX_SLOTS; i++) {
        if (!slots[i].down || !slots[i].have_x)
            continue;
        sx += slots[i].x;
        sy += slots[i].have_y ? slots[i].y : 0;
        n++;
    }
    if (!n)
        return 0;
    *cx = sx / n;
    *cy = sy / n;
    return n;
}

static void reset_contacts(struct contact *slots, int *n_down, int *two_finger,
                           int *classified, int *gesture_done, int *edge_zone) {
    memset(slots, 0, sizeof(struct contact) * MAX_SLOTS);
    *n_down = 0;
    *two_finger = 0;
    *classified = 0;
    *gesture_done = 0;
    *edge_zone = 0;
}

int main(void) {
    double edge_ratio = 0.04;
    double swipe_ratio = 0.08;
    const char *env_edge = getenv("EDGE_RATIO");
    const char *env_swipe = getenv("SWIPE_RATIO");
    char devpath[PATH_MAX];
    struct input_absinfo abs_x;
    struct input_absinfo abs_y;
    int fd;
    int span;
    int span_y;
    int left_edge;
    int right_edge;
    int swipe_min;
    struct contact slots[MAX_SLOTS];
    int slot = 0;
    int n_down = 0;
    int two_finger = 0;
    int classified = 0;
    int gesture_done = 0;
    int edge_zone = 0;
    int start_x = 0;
    int start_y = 0;
    int cur_x = 0;
    int cur_y = 0;
    struct input_event ev;

    memset(slots, 0, sizeof(slots));
    openlog("touch-gesture-daemon", LOG_PID | LOG_PERROR, LOG_USER);
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);

    if (env_edge)
        edge_ratio = atof(env_edge);
    if (env_swipe)
        swipe_ratio = atof(env_swipe);

    if (find_touchscreen(devpath, sizeof(devpath)) < 0) {
        syslog(LOG_ERR, "no touchscreen device found");
        return 1;
    }

    fd = open(devpath, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        if (errno == EACCES || errno == EPERM)
            syslog(LOG_ERR,
                   "cannot open %s: %s; disable twoFinger or add the input group and re-login",
                   devpath, strerror(errno));
        else
            syslog(LOG_ERR, "cannot open %s: %s", devpath, strerror(errno));
        return 1;
    }

    if (ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &abs_x) < 0) {
        syslog(LOG_ERR, "cannot read X axis info");
        close(fd);
        return 1;
    }

    span = abs_x.maximum - abs_x.minimum;
    if (span <= 0) {
        syslog(LOG_ERR, "invalid X axis range");
        close(fd);
        return 1;
    }

    span_y = span;
    if (ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &abs_y) == 0) {
        int sy = abs_y.maximum - abs_y.minimum;
        if (sy > 0)
            span_y = sy;
    }

    left_edge = abs_x.minimum + (int)(span * edge_ratio);
    right_edge = abs_x.maximum - (int)(span * edge_ratio);
    swipe_min = (int)(span * swipe_ratio);

    syslog(LOG_INFO, "device %s span=%d left=%d right=%d swipe_min=%d",
           devpath, span, left_edge, right_edge, swipe_min);

    while (running) {
        ssize_t n = read(fd, &ev, sizeof(ev));
        if (n < 0) {
            if (errno == EINTR)
                continue;
            syslog(LOG_ERR, "read: %s", strerror(errno));
            break;
        }
        if (n != sizeof(ev))
            continue;

        if (ev.type == EV_ABS && ev.code == ABS_MT_SLOT) {
            slot = ev.value;
            continue;
        }

        if (ev.type == EV_ABS && slot >= 0 && slot < MAX_SLOTS) {
            if (ev.code == ABS_MT_TRACKING_ID) {
                if (ev.value >= 0) {
                    if (!slots[slot].down)
                        n_down++;
                    slots[slot].down = 1;
                    slots[slot].have_x = 0;
                    slots[slot].have_y = 0;
                    slots[slot].x = 0;
                    slots[slot].y = 0;
                    if (n_down == 1) {
                        classified = 0;
                        gesture_done = 0;
                        two_finger = 0;
                        edge_zone = 0;
                    } else if (n_down == 2) {
                        two_finger = 1;
                        edge_zone = 0;
                        classified = 0;
                    } else if (n_down > 2) {
                        two_finger = 0;
                        gesture_done = 1;
                    }
                } else if (slots[slot].down) {
                    slots[slot].down = 0;
                    n_down--;
                    if (n_down < 0)
                        n_down = 0;
                    if (!gesture_done) {
                        int dx = cur_x - start_x;
                        int dy = cur_y - start_y;
                        if (two_finger && n_down < 2) {
                            if (classified && horizontal(dx, dy, swipe_min, span, span_y))
                                emit_dir(dx);
                            else
                                syslog(LOG_DEBUG, "ignore two-finger dx=%d dy=%d", dx, dy);
                            gesture_done = 1;
                        } else if (!two_finger && n_down == 0) {
                            if (classified && edge_zone == 1 && dx > 0 &&
                                (dx < 0 ? -dx : dx) >= swipe_min)
                                emit_dir(dx);
                            else if (classified && edge_zone == 2 && dx < 0 &&
                                     (dx < 0 ? -dx : dx) >= swipe_min)
                                emit_dir(dx);
                            else
                                syslog(LOG_DEBUG, "ignore edge dx=%d zone=%d", dx, edge_zone);
                            gesture_done = 1;
                        }
                    }
                    if (n_down == 0)
                        reset_contacts(slots, &n_down, &two_finger, &classified,
                                       &gesture_done, &edge_zone);
                }
            } else if (ev.code == ABS_MT_POSITION_X) {
                slots[slot].x = ev.value;
                slots[slot].have_x = 1;
            } else if (ev.code == ABS_MT_POSITION_Y) {
                slots[slot].y = ev.value;
                slots[slot].have_y = 1;
            }
        } else if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
            if (ev.value) {
                if (n_down == 0) {
                    slot = 0;
                    n_down = 1;
                    slots[0].down = 1;
                    slots[0].have_x = 0;
                    slots[0].have_y = 0;
                    classified = 0;
                    gesture_done = 0;
                    two_finger = 0;
                    edge_zone = 0;
                }
            } else if (n_down > 0) {
                if (!gesture_done) {
                    int dx = cur_x - start_x;
                    int dy = cur_y - start_y;
                    if (two_finger) {
                        if (classified && horizontal(dx, dy, swipe_min, span, span_y))
                            emit_dir(dx);
                        gesture_done = 1;
                    } else if (classified && edge_zone == 1 && dx > 0 &&
                               (dx < 0 ? -dx : dx) >= swipe_min) {
                        emit_dir(dx);
                        gesture_done = 1;
                    } else if (classified && edge_zone == 2 && dx < 0 &&
                               (dx < 0 ? -dx : dx) >= swipe_min) {
                        emit_dir(dx);
                        gesture_done = 1;
                    }
                }
                reset_contacts(slots, &n_down, &two_finger, &classified,
                               &gesture_done, &edge_zone);
            }
        } else if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
            int cx = 0;
            int cy = 0;
            int nxy = centroid(slots, &cx, &cy);
            if (nxy > 0) {
                cur_x = cx;
                cur_y = cy;
            }
            if (!classified && nxy > 0 && nxy == n_down) {
                start_x = cx;
                start_y = cy;
                if (two_finger && n_down >= 2) {
                    edge_zone = 0;
                    classified = 1;
                    syslog(LOG_DEBUG, "two-finger start_x=%d start_y=%d", start_x, start_y);
                } else if (!two_finger && n_down == 1) {
                    if (start_x <= left_edge)
                        edge_zone = 1;
                    else if (start_x >= right_edge)
                        edge_zone = 2;
                    else
                        edge_zone = 0;
                    classified = 1;
                    syslog(LOG_DEBUG, "contact start_x=%d zone=%d", start_x, edge_zone);
                }
            }
        }
    }

    syslog(LOG_INFO, "exit");
    close(fd);
    closelog();
    return 0;
}
