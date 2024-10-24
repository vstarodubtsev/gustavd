#ifndef __MAIN_H
#define __MAIN_H

#define DPRINTF(format, ...) fprintf(stderr, "%s(%d): " format, __func__, __LINE__, ## __VA_ARGS__)

#ifndef TTY_Q_SZ
#define TTY_Q_SZ 8192
#endif

#define TTY_RD_SZ 256

struct tty_q {
	int len;
	char buff[TTY_Q_SZ];
} tty_q;

void fatal(const char *format, ...);

extern int sig_exit;

extern int efd_notify_tty;
extern int efd_signal;
extern int ubus_pipefd[];

#endif /* __SERIALD_H */
