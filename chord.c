#include "chord.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <ev.h>
#include <signal.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <termios.h>

#include "log.h"
#include "utils.h"
#include "comp.h"

#define MAX_PACKET_SIZE 65536

#define FRAME_BOUNDARY 0x7E
#define CONTROL_ESCAPE 0x7D
#define ABORT          0x7D 0x7E
#define INVERT_BIT5(v) ((v) ^ (uint8_t)(1UL << 5))


typedef enum hdlc_state {
    HDLC_START = 0,
    HDLC_DATA = 1,
    HDLC_ESCAPE = 2
} hdlc_state_t;


static int   init;
static int   retval;
static ev_io sigfd;
static int   tunfd = -1;
static int   serfd = -1;

static ev_io tun_watcher;
static ev_io ser_watcher;


char *ifname = NULL;
char *serial = NULL;


static int
configure_tty(int fd, int speed)
{
    struct termios tty;

    if (tcgetattr(fd, &tty) < 0) {
        ERR("tcgetattr: %s\n", strerror(errno));
        return -1;
    }

    cfsetospeed(&tty, (speed_t)speed);
    cfsetispeed(&tty, (speed_t)speed);

    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_oflag &= ~OPOST;

    if (tcsetattr(fd, TCSAFLUSH, &tty) != 0) {
        ERR("tcsetattr: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}


static int
decode_hdlc_frame(char **packet, size_t *plen, char *data, size_t len)
{
    int i;
    static hdlc_state_t state = HDLC_START;
    static char buf[MAX_PACKET_SIZE];
    static size_t l = 0;

    for(i = 0; i < len; i++) {
        switch(state) {
        case HDLC_START:
            switch(data[i]) {
            case FRAME_BOUNDARY:
                state = HDLC_DATA;
                break;
            default:
                break;
            }
            break;

        case HDLC_DATA:
            switch(data[i]) {
            case CONTROL_ESCAPE:
                state = HDLC_ESCAPE;
                break;

            case FRAME_BOUNDARY:
                *packet = buf;
                *plen = l;
                state = HDLC_START;
                l = 0;
                return i + 1;

            default:
                buf[l++] = data[i];
                break;
            }
            break;

        case HDLC_ESCAPE:
            buf[l++] = INVERT_BIT5(data[i]);
            state = HDLC_DATA;
            break;
        }
    }

    *packet = NULL;
    return i;
}


static void
tty2tun(EV_P_ ev_io *w, int revents)
{
    char *comp, *packet, *p;
    size_t clen, plen;
    static char buf[1 + MAX_PACKET_SIZE * 2 + 1];
    ssize_t rv, left;

    rv = read(w->fd, buf, sizeof(buf));
    if (rv <= 0) {
        if (errno == EAGAIN || errno == EINTR) return;
        ERR("tty read: %s", (rv < 0 ? strerror(errno) : "empty packet"));
        chord_stop(rv < 0 ? rv : -1);
        return;
    }

    p = buf;
    left = rv;

    do {
        rv = decode_hdlc_frame(&comp, &clen, p, left);
        p += rv;
        left -= rv;

        if (comp == NULL) continue;

        DBG("TTY: Got %lu bytes", clen);

        if (comp_expand(&packet, &plen, comp, clen) < 0) {
            ERR("Error while decompressing");
            chord_stop(-1);
            return;
        }

        if (plen != clen)
            DBG("Expanded to %lu bytes", plen);

        rv = write(tunfd, packet, plen);
        if (rv < 0) {
            ERR("Error while writing packet: %s", strerror(errno));
            chord_stop(-1);
        } else if (rv < plen) {
            ERR("Incomplete packet written (%lu < %lu)", rv, plen);
        }
    } while(left);
}


static void
build_hdlc_frame(char **frame, size_t *flen, char *packet, size_t plen)
{
    static char buf[1 + MAX_PACKET_SIZE * 2 + 1];
    int j;

    j = 0;
    buf[j++] = FRAME_BOUNDARY;

    for(int i = 0; i < plen; i++) {
        switch(packet[i]) {
        case FRAME_BOUNDARY:
        case CONTROL_ESCAPE:
            buf[j++] = CONTROL_ESCAPE;
            buf[j++] = INVERT_BIT5(packet[i]);
            break;

        default:
            buf[j++] = packet[i];
            break;
        }
    }

    buf[j++] = FRAME_BOUNDARY;
    *frame = buf;
    *flen = j;
}


static void
tun2tty(EV_P_ ev_io *w, int revents)
{
    char *comp;
    static char packet[MAX_PACKET_SIZE];
    char *frame;
    size_t plen, flen, clen;

    ssize_t rv;

    rv = read(w->fd, packet, sizeof(packet));
    if (rv <= 0) {
        if (errno == EAGAIN || errno == EINTR) return;
        ERR("tun read: %s", (rv < 0 ? strerror(errno) : "empty packet"));
        chord_stop(rv < 0 ? rv : -1);
        return;
    }

    plen = rv;
    DBG("TUN: Got %lu bytes", plen);

    if (comp_shrink(&comp, &clen, packet, plen) < 0) {
        ERR("Error while compressing");
        chord_stop(-1);
        return;
    }

    if (clen != plen)
        DBG("Compressed away %lu bytes", plen - clen);

    build_hdlc_frame(&frame, &flen, comp, clen);

    rv = write(serfd, frame, flen);
    if (rv < 0) {
        ERR("Error while writing frame: %s", strerror(errno));
        chord_stop(-1);
    } else if (rv < flen) {
        ERR("Incomplete frame written (%lu < %lu)", rv, flen);
    }
}



static int
open_tun(char *name)
{
    static char *dev = "/dev/net/tun";
    struct ifreq ifr;
    int fd, err;

    if ((fd = open(dev, O_RDWR | O_NONBLOCK)) < 0) {
        ERR("Error while opening %s: %s", dev, strerror(errno));
        return fd;
    }

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;

    if (name && *name) strncpy(ifr.ifr_name, name, IFNAMSIZ);

    if ((err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0) {
        close(fd);
        ERR("Error in TUN/TAP ioctl: %s", strerror(errno));
        return err;
    }

    if ((err = ioctl(fd, TUNSETPERSIST, 1)) < 0) {
        close(fd);
        ERR("Error in TUN/TAP ioctl: %s", strerror(errno));
        return err;
    }

    DBG("Opened TUN interface '%s'", ifr.ifr_name);
    return fd;
}



static void
read_signal(EV_P_ ev_io *w, int revents)
{
    int s, rv;
    ssize_t rc;

    rc = safe_read(w->fd, &s, sizeof(s));
    if (rc < 0) {
        if (errno == EAGAIN || errno == EINTR) return;
        ERR("Error while receiving signal: %s", strerror(errno));
        rv = -1;
    } else if (rc != sizeof(s)) {
        ERR("Not enough data received on signal file descriptor.");
        rv = -1;
    } else {
        DBG("Signal %d received", s);
        rv = 0;
    }
    chord_stop(rv);
}


int
chord_init(int fd)
{
    /* Initialization is done when we get here. Report to the parent
     * process that we're starting and log the event into the system
     * log. */
    INF("%s version %s (%s-%s-%s) built on %s", NAME,
        VERSION, ARCH, OS, PLATFORM, BUILT);
    DBG("Using libev %d.%d", EV_VERSION_MAJOR, EV_VERSION_MINOR);

    /* Block the SIGPIPE signal so that we don't get killed when we
     * write to a socket that received RST from the remote peer. We
     * check for EPIPE in errno instead. */
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        ERR("Could not block SIGPIPE");
        return -1;
    }

    /* Initialize all variables used by chord_cleanup to determine
     * what cleanup functions to run. This is to ensure that
     * chord_cleanup can be run in case of an error in chord_init.*/
    sigfd.fd = -1;
    retval = 0;
    init = 0;

    if (ev_default_loop(0) == NULL) {
        ERR("Could not initialize the main event loop");
        return -1;
    }

    /* If we were given a signal file descriptor, make it non-blocking
     * and set up a reader for it. */
    if (fd >= 0) {
        if (make_nonblocking(fd) < 0) {
            ERR("Can't make signal file descriptor non-blocking");
            return -1;
        }
        ev_io_init(&sigfd, read_signal, fd, EV_READ);
        ev_io_start(EV_DEFAULT_UC_ &sigfd);
    }

    if (serial == NULL) {
        ERR("Please configure serial port name");
        return -1;
    }

    serfd = open(serial, O_RDWR | O_NOCTTY | O_NONBLOCK | O_NDELAY);
    if (serfd < 0) {
        ERR("Could not open serial port %s: %s", serial, strerror(errno));
        return -1;
    }
    DBG("Opened serial port %s", serial);

    if (configure_tty(serfd, 9600) < 0) return -1;

    ev_io_init(&ser_watcher, tty2tun, serfd, EV_READ);
    ev_io_start(EV_DEFAULT_UC_ &ser_watcher);

    if ((tunfd = open_tun(ifname)) < 0)
        return -1;

    ev_io_init(&tun_watcher, tun2tty, tunfd, EV_READ);
    ev_io_start(EV_DEFAULT_UC_ &tun_watcher);


    if (comp_init() < 0)
        return -1;

    init = 1;
    return 0;
}


void
chord_cleanup(void)
{
    if (init != 0) INF("Shutting down Chord");
    init = 0;

    comp_cleanup();

    if (serfd >= 0) {
        DBG("Closing serial port");
        ev_io_stop(EV_DEFAULT_UC_ &ser_watcher);
        close(serfd);
    }
    if (serial) xfree(serial);

    if (tunfd >= 0) {
        DBG("Closing TUN/TAP interface");
        ev_io_stop(EV_DEFAULT_UC_ &tun_watcher);
        close(tunfd);
    }
    if (ifname) xfree(ifname);

    if (sigfd.fd >= 0) {
        ev_io_stop(EV_DEFAULT_UC_ &sigfd);
        close(sigfd.fd);
    }

    /* Destroy the default event loop and call any ev_cleanup handlers
     * that might have been registered. */
    ev_loop_destroy(EV_DEFAULT_UC);
}


/* FIXME: This should use ev_async to signal the main loop, otherwise
 * the function will not work when called outside of ev_run, e.g.,
 * from another thread of execution. */
void
chord_stop(int rv)
{
    retval = rv;
    ev_break(EV_DEFAULT_UC_ EVBREAK_ALL);
}


int
chord_run(void)
{
    if (!init) {
        ERR("Did you forget to call chord_init before calling chord_run?");
        return -1;
    }

    ev_run(EV_DEFAULT_UC_ 0);
    return retval;
}
