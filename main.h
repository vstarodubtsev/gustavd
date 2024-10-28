#ifndef __MAIN_H
#define __MAIN_H

#define DPRINTF(format, ...) fprintf(stderr, "%s(%d): " format, __func__, __LINE__, ## __VA_ARGS__)

#ifndef TTY_Q_SZ
#define TTY_Q_SZ 8192
#endif

#define TTY_RD_SZ 512

void fatal(const char *format, ...);

extern void tty_write_line(const char *line);

#endif /* __MAIN_H */
