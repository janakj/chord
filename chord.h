#ifndef _CHORD_H_
#define _CHORD_H_

#define MAX_PACKET_SIZE 65536

extern int   log_threshold;
extern int   log_syslog;

extern char *ifname;
extern char *serial;

/* Initialize the daemon to the point that chord_run can be called.
 * The parameter fd is an optional file descriptor (-1 if not used)
 * for the main loop to watch for incoming signals. Signals can be
 * sent over the fd as 4-byte integer numbers in host order. The
 * function returnts 0 on success and a negative number on error. */
int chord_init(int fd);

/* Start the main loop of the daemon. This function does not return
 * until a signal is received over the fd configured in chord_init or
 * until another thread of execution calls chord_stop. The function
 * returns 0 if it was terminated explicitly and a negative number on
 * error. The function can be called repeatedly (after chord_stop). */
int chord_run(void);

/* Stop the main loop of the running daemon and indicate to chord_run
 * to return the value in argument rv. This function can be used to
 * stop the main loop running in one thread from another thread of
 * execution. */
void chord_stop(int rv);

/* Shut down the whole application. This function deactivates all
 * internal subsystems an releases and resources held. This function
 * can only be called after chord_run has returned. The only function
 * that can be called after chord_cleanup is chord_init. */
void chord_cleanup(void);


#endif /* _CHORD_H_ */
