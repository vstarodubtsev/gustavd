#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "fdio.h"
#include "main.h"
#include "term.h"

static int fd_tty;

#define STO STDOUT_FILENO
#define STI STDIN_FILENO
#define TTY_WRITE_SZ_DIV 10
#define TTY_WRITE_SZ_MIN 8
int tty_write_sz;
#define set_tty_write_sz(baud) \
	do { \
		tty_write_sz = (baud) / TTY_WRITE_SZ_DIV; \
		if (tty_write_sz < TTY_WRITE_SZ_MIN) tty_write_sz = TTY_WRITE_SZ_MIN; \
	} while (0)
struct tty_q tty_q;

int sig_exit = 0;

int echo = 0;

static struct {
	char port[128];
	int baud;
	enum flowcntrl_e flow;
	enum parity_e parity;
	int databits;
	int stopbits;
	int noreset;
	char *socket;
} opts = {
	.port = "",
	.baud = 115200,
	.flow = FC_NONE,
	.parity = P_NONE,
	.databits = 8,
	.stopbits = 1,
	.noreset = 0,
	.socket = NULL, /* the library fall back to default socket when it is NULL */
};

static void show_usage(void);
static void parse_args(int argc, char *argv[]);
static void deadly_handler(int signum);
static void register_signal_handlers(void);
static void loop(void);
static void tty_read_line_splitter(const int n, const char *buff_rd);
static void tty_read_line_cb(const char *line);
int main(int argc, char *argv[]);

static void show_usage()
{
	printf("Usage: gustavd [options] <TTY device>\n");
	printf("\n");
	printf("Options:\n");
	printf("  -b <baudrate>\n");
	printf("    baudrate should be one of: 0, 50, 75, 110, 134, 150, 200, 300, 600,\n");
	printf("    1200, 1800, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400\n");
	printf("    default to 115200\n");
	printf("  -f flow control s (=soft) | h (=hard) | n (=none)\n");
	printf("    default to n\n");
	printf("\n");
}

void fatal(const char *format, ...)
{
	char *s, buf[256];
	va_list args;
	int len;

	va_start(args, format);
	len = vsnprintf(buf, sizeof(buf), format, args);
	buf[sizeof(buf)-1] = '\0';
	va_end(args);

	s = "\r\nFATAL: ";
	writen_ni(STO, s, strlen(s));
	writen_ni(STO, buf, len);
	s = "\r\n";
	writen_ni(STO, s, strlen(s));

	/* wait a bit for output to drain */
	sleep(1);

	exit(EXIT_FAILURE);
}

static void parse_args(int argc, char *argv[])
{
	int c;
	int r = 0;

	while ((c = getopt(argc, argv, "hf:b:s:")) != -1) {
		switch (c) {
			case 'f':
				switch (optarg[0]) {
					case 'X':
					case 'x':
						opts.flow = FC_XONXOFF;
						break;
					case 'H':
					case 'h':
						opts.flow = FC_RTSCTS;
						break;
					case 'N':
					case 'n':
						opts.flow = FC_NONE;
						break;
					default:
						DPRINTF("Invalid flow control: %c\n", optarg[0]);
						r = -1;
						break;
				}
				break;
			case 'b':
				opts.baud = atoi(optarg);
				if (opts.baud == 0 || !term_baud_ok(opts.baud)) {
					DPRINTF("Invalid baud rate: %d\n", opts.baud);
					r = -1;
				}
				break;
			case 's':
				opts.socket = optarg;
				break;
			case 'h':
				r = 1;
				break;
			default:
				r = -1;
				break;
		}
	}

	if (r) {
		show_usage();
		exit((r > 0) ? EXIT_SUCCESS : EXIT_FAILURE);
	}

	if ((argc - optind) < 1) {
		DPRINTF("No port given\n");
		show_usage();
		exit(EXIT_FAILURE);
	}

	strncpy(opts.port, argv[optind], sizeof(opts.port) - 1);
	opts.port[sizeof(opts.port)-1] = '\0';
}

static void deadly_handler(int signum)
{
	DPRINTF("seriald is signaled with TERM\n");
	if (!sig_exit) {
		sig_exit = 1;
	}
}

static void register_signal_handlers(void)
{
	struct sigaction exit_action, ign_action;

	/* Set up the structure to specify the exit action. */
	exit_action.sa_handler = deadly_handler;
	sigemptyset (&exit_action.sa_mask);
	exit_action.sa_flags = 0;

	/* Set up the structure to specify the ignore action. */
	ign_action.sa_handler = SIG_IGN;
	sigemptyset (&ign_action.sa_mask);
	ign_action.sa_flags = 0;

	sigaction(SIGTERM, &exit_action, NULL);

	sigaction(SIGALRM, &ign_action, NULL);
	sigaction(SIGHUP, &ign_action, NULL);
	//sigaction(SIGINT, &ign_action, NULL);
	sigaction(SIGPIPE, &ign_action, NULL);
	sigaction(SIGQUIT, &ign_action, NULL);
	sigaction(SIGUSR1, &ign_action, NULL);
	sigaction(SIGUSR2, &ign_action, NULL);
}

static void loop(void)
{
	fd_set rdset, wrset;
	int r;
	int n;
	char buff_rd[TTY_RD_SZ];
	int write_sz;
	int max_fd;

	max_fd = fd_tty;
	tty_q.len = 0;

	while (!sig_exit) {
		FD_ZERO(&rdset);
		FD_ZERO(&wrset);
		FD_SET(fd_tty, &rdset);

		if (tty_q.len) FD_SET(fd_tty, &wrset);

		r = select(max_fd + 1, &rdset, &wrset, NULL, NULL);
		if (r < 0)  {
			if (errno == EINTR) continue;
			else fatal("select failed: %d : %s", errno, strerror(errno));
		}

		if (FD_ISSET(fd_tty, &rdset)) {
			/* read from port */
			do {
				n = read(fd_tty, &buff_rd, sizeof(buff_rd));
			} while (n < 0 && errno == EINTR);
			if (n == 0) {
				fatal("term closed");
			} else if (n < 0) {
				if (errno != EAGAIN && errno != EWOULDBLOCK)
					fatal("read from term failed: %s", strerror(errno));
			} else {
				tty_read_line_splitter(n, buff_rd);
			}
		}

		if (FD_ISSET(fd_tty, &wrset)) {
			/* write to port */
			write_sz = (tty_q.len < tty_write_sz) ? tty_q.len : tty_write_sz;
			do {
				n = write(fd_tty, tty_q.buff, write_sz);
			} while (n < 0 && errno == EINTR);
			if (n <= 0) fatal("write to term failed: %s", strerror(errno));
			memmove(tty_q.buff, tty_q.buff + n, tty_q.len - n);
			tty_q.len -= n;
		}
	}
}

static void tty_read_line_splitter(const int n, const char *buff_rd)
{
	static char buff[TTY_RD_SZ+1] = "";
	static int buff_len = 0;
	const char *p;

	p = buff_rd;

	while (p - buff_rd < n) {
			if (buff_len == sizeof(buff) - 1) {
				tty_read_line_cb(buff);
				*buff = '\0';
				buff_len = 0;
			}
			if (*p && *p != '\r' && *p != '\n') {
				buff[buff_len] = *p;
				buff[++buff_len] = '\0';
			} else if ((!*p || *p == '\n' || *p == '\r') && buff_len > 0) {
				tty_read_line_cb(buff);
				*buff = '\0';
				buff_len = 0;
			}

			p++;
	}
}

static void tty_write_line(const char *line)
{
	if( line == NULL )
	{
		return;
	}

	const int len = strlen(line);

	if (tty_q.len + len < TTY_Q_SZ) {
		memmove(tty_q.buff + tty_q.len, line, len);
		tty_q.len += len;
		tty_q.buff[tty_q.len] = '\n';
		++tty_q.len;
		tty_q.buff[tty_q.len] = '\r';
		++tty_q.len;
	}
}

static void tty_read_line_cb(const char *line)
{
	if( echo )
	{
		tty_write_line(line);
	}

	if (!strcasecmp(line, "AT") ||
		!strcasecmp(line, "AT+CMEE=1") ||
		!strncasecmp(line, "AT+CFUN=", 8) ||
		!strcasecmp(line, "AT+CREG=0") ||
		!strcasecmp(line, "AT+CEREG=0") ||
		!strcasecmp(line, "AT+DIALMODE=0") ||
		!strcasecmp(line, "AT+CNETCI=0") ||
		!strcasecmp(line, "AT+CNMI=2,1") ||
		!strcasecmp(line, "AT+USBNETIP=1") ||
		!strncasecmp(line, "AT+CNBP=", 8) ||
		!strncasecmp(line, "AT+CNMP=", 8) ||
		!strncasecmp(line, "AT+CGDCONT=1,", 13) ||
		!strncasecmp(line, "AT+CGAUTH=", 10) ||
		!strncasecmp(line, "AT+AUTOAPN=", 11) ||
		!strcasecmp(line, "AT+CMGF=0") ||
		!strcasecmp(line, "AT+COPS=0") ||
		!strcasecmp(line, "AT*CELL=0") ||
		!strcasecmp(line, "AT+CGACT=1,1") ||
		!strcasecmp(line, "AT+COPS=3,0") ||
		!strncasecmp(line, "AT+CUSD=1,", 10) ||
		!strcasecmp(line, "AT+CMEE=1")) {
	} else if (!strcasecmp(line, "ATE1")) {
		echo = 1;
	} else if (!strcasecmp(line, "ATE0")) {
		echo = 0;
	} else if (!strcasecmp(line, "ATI")) {
		tty_write_line("Manufacturer: SIMCOM BY GUSTAV");
		tty_write_line("Model: A7909E");
		tty_write_line("Revision: V1.0.009");
		tty_write_line("IMEI: 65812565308830");
	} else if (!strcasecmp(line, "AT+SIMCOMATI")) {
		tty_write_line("Manufacturer: SIMCOM BY GUSTAV");
		tty_write_line("Model: A7909E");
		tty_write_line("Revision: 242203B02A7909M11A-M2_CUS");
		tty_write_line("IMEI: 65812565308830");
	} else if (!strcasecmp(line, "AT+CGMR")) {
		tty_write_line("+CGMR: 242203B02A7909M11A-M2_CUS");
	} else if (!strcasecmp(line, "AT+CSUB")) {
		tty_write_line("+CSUB: B02V01");
	} else if (!strcasecmp(line, "AT+UIMHOTSWAPLEVEL?")) {
		tty_write_line("+UIMHOTSWAPLEVEL: 1");
	} else if (!strcasecmp(line, "AT+UIMHOTSWAPON?")) {
		tty_write_line("+UIMHOTSWAPON: 1");
	} else if (!strcasecmp(line, "AT+USBNETIP?")) {
		tty_write_line("USBNETIP=1");
	} else if (!strcasecmp(line, "AT+CPIN?")) {
		tty_write_line("+CPIN: READY");
	} else if (!strcasecmp(line, "AT+CNBP?")) {
		tty_write_line("+CNBP: 0X000700000FEB0180,0X000007FF3FDF3FFF");
	} else if (!strcasecmp(line, "AT+CICCID")) {
		tty_write_line("+ICCID: 897010269813267917FF");
	} else if (!strcasecmp(line, "AT+CSIM=10,\"0020000100\"")) {
		tty_write_line("+CSIM:4,\"63C3\"");
	} else if (!strcasecmp(line, "AT+CIMI")) {
		tty_write_line("250026983126791");
	} else if (!strcasecmp(line, "AT+CNUM")) {
		tty_write_line("+CME ERROR: 4");
		return;
	} else if (!strcasecmp(line, "AT+CSPN?")) {
		tty_write_line("+CSPN: \"MegaFon\",0");
	} else if (!strcasecmp(line, "AT+CREG?")) {
		tty_write_line("+CREG: 0,0");
	} else if (!strcasecmp(line, "AT+CEREG?")) {
		tty_write_line("+CEREG: 0,1");
	} else if (!strcasecmp(line, "AT+CPMS=\"SM\";+CMGL=4")) {
		tty_write_line("+CPMS: 15,15,15,15,15,15");
		tty_write_line("+CMGL: 0,1,,160");
		tty_write_line("07919762020041F7400DD0CDF2396C7CBB010008223081916324218C05000303030100310039002E00300033002E003200300032003200200432002000310039003A00330036002004370430043F043B0430043D04380440043E04320430043D043E00200441043F043804410430043D043804350020043F043B04300442044B0020043F043E00200442043004400438044404430020201300200037003000300020044004430431");
		tty_write_line("+CMGL: 1,1,,160");
		tty_write_line("07919762020041F7400DD0CDF2396C7CBB010008223081916324218C050003030302002E000A041D04300441043B04300436043404300439044204350441044C0020043E043104490435043D04380435043C0020002D002004340435043D043504330020043D043000200441044704350442043500200434043E0441044204300442043E0447043D043E002E041204300448002004310430043B0430043D0441003A002000390039");
		tty_write_line("+CMGL: 2,1,,108");
		tty_write_line("07919762020041F7440DD0CDF2396C7CBB01000822308191632421580500030303030031002E003200370020044004430431002E000A000A041F043E043F043E043B043D04380442044C00200441044704350442003A0020007000610079002E006D0065006700610066006F006E002E00720075");
		tty_write_line("+CMGL: 3,1,,160");
		tty_write_line("07919762020041F7400DD0CDF2396C7CBB010008223091916324218C0500031104010421043F043804410430043D043E00200037003000300020044004430431002E0020043F043E00200442043004400438044404430020002204170430043A04300447043004390441044F00210020041B04350433043A043E00220020043704300020043F043504400438043E043400200441002000310039002E00300033002E003200300032");
		tty_write_line("+CMGL: 4,1,,160");
		tty_write_line("07919762020041F7400DD0CDF2396C7CBB010008223091916324218C05000311040200320020043F043E002000310038002E00300034002E0032003000320032002E000A000A0418043D044204350440043D043504420020043D043000200441043A043E0440043E04410442043800200434043E0020003200350020041C043104380442002F044100200431044304340435044200200434043E044104420443043F0435043D0020");
		tty_write_line("+CMGL: 5,1,,160");
		tty_write_line("07919762020041F7400DD0CDF2396C7CBB010008223091916324218C0500031104030434043E00200441043B043504340443044E044904350433043E00200441043F043804410430043D0438044F0020043F043E0020044204300440043804440443002000310038002E00300034002E0032003000320032002E0020000A000A041F043E043F043E043B043D04380442044C002004310430043B0430043D0441003A002000700061");
		tty_write_line("+CMGL: 6,1,,50");
		tty_write_line("07919762020041F7440DD0CDF2396C7CBB010008223091916324211E0500031104040079002E006D0065006700610066006F006E002E00720075");
		tty_write_line("+CMGL: 7,1,,160");
		tty_write_line("07919762020041F7600DD0CDF2396C7CBB010008224031718101218C050003C407010421002004430441043B04430433043E0439002000AB0414043E043F043E043B043D043804420435043B044C043D044B04390020043D043E043C0435044000BB00200412044B0020043C043E04360435044204350020043F043E0434043A043B044E044704380442044C0020043D043000200412043004480443002000530049004D002D043A");
		tty_write_line("+CMGL: 8,1,,160");
		tty_write_line("07919762020041F7600DD0CDF2396C7CBB010008224031718101218C050003C407020430044004420443002004350449043500200434043E00200033002D04450020043D043E043C04350440043E0432002E0020041804450020043C043E0436043D043E002004380441043F043E043B044C0437043E043204300442044C002C0020043A043E04330434043000200412044B0020043D043500200445043E04420438044204350020");
		tty_write_line("+CMGL: 9,1,,160");
		tty_write_line("07919762020041F7600DD0CDF2396C7CBB010008224031718101218C050003C40703043E0441044204300432043B044F0442044C002004410432043E04390020043E0441043D043E0432043D043E04390020043D043E043C04350440002C0020043D0430043F04400438043C04350440002C00200434043B044F002004410432044F0437043800200441043E00200441043B0443043604310430043C043800200434043E04410442");
		tty_write_line("+CMGL: 10,1,,160");
		tty_write_line("07919762020041F7600DD0CDF2396C7CBB010008224031718101218C050003C4070404300432043A0438002C00200438043D044204350440043D043504420020043C04300433043004370438043D0430043C0438002004380020043C0430043B043E0437043D0430043A043E043C044B043C04380020043B044E0434044C043C0438002E000A04170432043E043D043804420435002004380020043E0442043F044004300432043B");
		tty_write_line("+CMGL: 11,1,,160");
		tty_write_line("07919762020041F7600DD0CDF2396C7CBB010008224031718101218C050003C40705044F04390442043500200053004D00530020043F043E002004430441043B043E04320438044F043C0020043E0441043D043E0432043D043E0433043E0020044204300440043804440430002E002004210442043E0438043C043E04410442044C0020043F043E0434043A043B044E04470435043D0438044F00200434043E043F043E043B043D");
		tty_write_line("+CMGL: 12,1,,160");
		tty_write_line("07919762020041F7600DD0CDF2396C7CBB010008224031718101218C050003C40706043804420435043B044C043D043E0433043E0020043D043E043C043504400430002020140020003300300020044004430431043B04350439002E00200415043604350434043D04350432043D0430044F0020043F043B04300442043000202014002000320020044004430431043B044F00200432002004340435043D044C002E0020041F043E");
		tty_write_line("+CMGL: 13,1,,156");
		tty_write_line("07919762020041F7640DD0CDF2396C7CBB0100082240317181012188050003C407070434043A043B044E044704380442044C002000680074007400700073003A002F002F006C006B002E006D0065006700610066006F006E002E00720075002F0069006E006100700070002F006100640064006900740069006F006E0061006C004E0075006D006200650072007300200438043B04380020002A0034003800310023000A");
		tty_write_line("+CMGL: 14,1,,27");
		tty_write_line("07919762020041F7040B919781314259F800084290526173402108041E043A04300439");
	} else if (!strcasecmp(line, "AT+CPMS=\"ME\";+CMGL=4")) {
		tty_write_line("+CPMS: 0,200,0,200,0,200");
	} else if (!strncasecmp(line, "AT+CPMS=\"SM\"",12)) {
		tty_write_line("+CPMS: 15,15,15,15,15,15");
	} else if (!strncasecmp(line, "AT+CPMS=\"ME\"",12)) {
		tty_write_line("+CPMS: 0,200,0,200,0,200");
	} else if (!strcasecmp(line, "AT+CGCONTRDP")) {
		tty_write_line("+CGCONTRDP: 1,5,\"test.MNC002.MCC255.GPRS\",\"10.36.130.148\",\"\",\"10.97.52.68\",\"10.97.52.76\",\"\",\"\",0,0");
	} else if (!strcasecmp(line, "AT+CSQ")) {
		tty_write_line("+CSQ: 23,99");
	} else if (!strcasecmp(line, "AT+CGATT?")) {
		tty_write_line("+CGATT: 1");
	} else if (!strcasecmp(line, "AT+CPSI?")) {
		tty_write_line("+CPSI: LTE,Online,250-02,0x260A,196089506,299,EUTRAN-BAND7,2850,5,5,21,47,43,17");
	} else if (!strcasecmp(line, "AT+COPS?")) {
		tty_write_line("+COPS: 0,0,\"MegaFon\",9");
	} else if (!strcasecmp(line, "AT+CGPADDR=1")) {
		tty_write_line("+CGPADDR: 1, \"10.36.130.148\"");
	} else if (!strcasecmp(line, "AT+CPMUTEMP")) {
		tty_write_line("+CPMUTEMP: 46");
	} else if (!strcasecmp(line, "AT+CNETCI?")) {
		tty_write_line("+CNETCISRVINFO: MCC-MNC: 250-02,TAC: 9738,cellid: 196089506,rsrp: 47,rsrq: 21, pci: 299,earfcn: 2850");
		tty_write_line("+CNETCINONINFO: 0,MCC-MNC: 000-00,TAC: 0,cellid: -1,rsrp: 23,rsrq: 0,pci: 195,earfcn: 1602");
		tty_write_line("+CNETCINONINFO: 1,MCC-MNC: 000-00,TAC: 0,cellid: -1,rsrp: 31,rsrq: 17,pci: 92,earfcn: 1602");
		tty_write_line("+CNETCI: 0");
	} else if (!strcasecmp(line, "AT+SIGNS")) {
		tty_write_line("+RSRP0: -109");
		tty_write_line("+RSRP1: -112");
		tty_write_line("+RSRQ0: -11");
		tty_write_line("+RSRQ1: -11");
		tty_write_line("+RSSI0: -61");
		tty_write_line("+RSSI1: -64");
	} else
	{
		tty_write_line("ERROR");
		return;
	}

	tty_write_line("OK");
}

int main(int argc, char *argv[])
{
	int r;

	parse_args(argc, argv);
	register_signal_handlers();

	r = term_lib_init();
	if (r < 0) fatal("term_init failed: %s", term_strerror(term_errno, errno));

	fd_tty = open(opts.port, O_RDWR | O_NONBLOCK | O_NOCTTY);
	if (fd_tty < 0) fatal("cannot open %s: %s", opts.port, strerror(errno));

	r = term_set(fd_tty,
			1,              /* raw mode. */
			opts.baud,      /* baud rate. */
			opts.parity,    /* parity. */
			opts.databits,  /* data bits. */
			opts.stopbits,  /* stop bits. */
			opts.flow,      /* flow control. */
			1,              /* local or modem */
			!opts.noreset); /* hup-on-close. */
	if (r < 0) {
		fatal("failed to add device %s: %s",
				opts.port, term_strerror(term_errno, errno));
	}

	r = term_apply(fd_tty, 0);
	if (r < 0) {
		fatal("failed to config device %s: %s",
				opts.port, term_strerror(term_errno, errno));
	}

	set_tty_write_sz(term_get_baudrate(fd_tty, NULL));

	loop();

	return EXIT_SUCCESS;
}
