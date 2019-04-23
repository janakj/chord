#define _POSIX_C_SOURCE 2 /* getopt */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <libdaemon/daemon.h>

#include "log.h"
#include "utils.h"
#include "chord.h"

static int fg;

static void
print_help(void)
{
    static char help_msg[] = "\
Usage: " NAME " [options]\n\
Options:\n\
    -h  This help text\n\
    -v  Increase verbosity (Use repeatedly to increase more)\n\
    -E  Write log messages to standard output instead of syslog\n\
    -i  TUN/TAP network interface name\n\
    -f  Stay in foreground\n\
";

    fprintf(stdout, "%s", help_msg);
    exit(EXIT_SUCCESS);
}

int
main(int argc, char **argv)
{
    pid_t pid;
    int rv = EXIT_FAILURE;
    int opt, rc, sigfd = -1;

    while((opt = getopt(argc, argv, "hvEfi:")) != -1) {
        switch(opt) {
        case 'h': print_help();             break;
        case 'v': log_threshold--;          break;
        case 'E': log_syslog = 0;           break;
        case 'f': fg++;                     break;
        case 'i':
            if (ifname) xfree(ifname);
            ifname = xstrdup(optarg);
            break;
        default:
            fprintf(stderr, "Use the -h option for list of supported "
                    "program arguments.\n");
            exit(rv);
        }
    }

    /* Initial sanity checks before we attempt to create a daemon. All
     * messages should to to the standard error output here. */

    if (getuid()) {
        fprintf(stderr, "This program must be run as root.\n");
        exit(rv);
    }

    if (!fg) {
        if (daemon_reset_sigs(-1) < 0) {
            fprintf(stderr, "Failed to reset all signal handlers: %s",
                    strerror(errno));
            exit(rv);
        }

        if (daemon_unblock_sigs(-1) < 0) {
            fprintf(stderr, "Failed to unblock all signals: %s",
                    strerror(errno));
            exit(rv);
        }
    }

    daemon_pid_file_ident = daemon_ident_from_argv0(argv[0]);
    if ((pid = daemon_pid_file_is_running()) >= 0) {
        fprintf(stderr, "Daemon already running, PID %u\n", pid);
        exit(rv);
    }

    if (!fg && (daemon_retval_init() < 0)) {
        fprintf(stderr, "Failed to create a pipe.\n");
        exit(rv);
    }

    /* Fork a daemon unless the user asked us to run in foreground.
     * From now on all messages should be logged through the standard
     * logging facilities. That may be syslog if we start a daemon or
     * standard output if we run in foreground mode. */

    if (!fg) {
        if ((pid = daemon_fork()) < 0) {
            daemon_retval_done();
            exit(rv);
        } else if (pid) {
            if ((rc = daemon_retval_wait(20)) < 0) {
                fprintf(stderr, "No value received from daemon process: %s\n",
                        strerror(errno));
                exit(255);
            }
            exit(rc);
        }
    }

    start_logger(daemon_pid_file_ident);
    if (!fg) {
        if (daemon_close_all(-1) < 0) {
            daemon_retval_send(__LINE__);
            ERR("Failed to close file descriptors: %s", strerror(errno));
            goto out;
        }
        if (daemon_pid_file_create() < 0) {
            daemon_retval_send(__LINE__);
            ERR("Could not create a pid file: %s", strerror(errno));
            goto out;
        }
    }

    if (daemon_signal_init(SIGINT, SIGTERM, SIGQUIT, 0) < 0) {
        if (!fg) daemon_retval_send(__LINE__);
        ERR("Could not register signal handlers (%s).", strerror(errno));
        goto out;
    }
    sigfd = daemon_signal_fd();
    if (sigfd < 0) {
        ERR("Error while registering daemon signal handler");
        goto out;
    }

    /* Continue with initialization within the daemon process with
     * dropped privileges. */

    if (chord_init(sigfd) < 0) {
        if (!fg) daemon_retval_send(__LINE__);
        goto out;
    }
    if (!fg) daemon_retval_send(0);

    if (chord_run() < 0) goto out;

    /* If we get here than the daemon was asked to shut down gracefully with a
     * signal or the user pressed ctrl-c while we were running in foreground
     * mode. In either case terminate the process and report EXIT_SUCCESS. */
    rv = EXIT_SUCCESS;
out:
    chord_cleanup();
    daemon_signal_done();
    if (!fg) daemon_pid_file_remove();
    stop_logger();
    exit(rv);
}
