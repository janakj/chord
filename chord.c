#include "chord.h"

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

#include "log.h"
#include "utils.h"


static int   init;
static int   retval;
static ev_io sigfd;
static int   tunfd = -1;

char *ifname = NULL;

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

    if ((tunfd = open_tun(ifname)) < 0)
        return -1;

    init = 1;
    return 0;
}


void
chord_cleanup(void)
{
    if (init != 0) INF("Shutting down Chord");
    init = 0;

    if (tunfd >= 0) {
        DBG("Closing TUN/TAP interface");
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
