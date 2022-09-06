/*
 * Copyright (c) 2006,2007 Stefan Bethke <stb@lassitu.de>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sysexits.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>

#if !defined(DEFAULTDEVICE)
#define DEFAULTDEVICE	"cuad0"
#endif
#if !defined(DEFAULTSPEED)
#define DEFAULTSPEED	"9600"
#endif
#if !defined(DEFAULTPARMS)
#define DEFAULTPARMS	"8n1"
#endif
#if !defined(PATH_DEV)
#define PATH_DEV "/dev"
#endif

#if B2400 == 2400 && B9600 == 9600 && B38400 == 38400
#define TERMIOS_SPEED_IS_INT
#endif

#if !defined(TERMIOS_SPEED_IS_INT)
struct termios_speed {
	long code;
	long speed;
};
struct termios_speed termios_speeds[] = {
	{ B50, 50 },
	{ B75, 75 },
	{ B110, 110 },
	{ B134, 134 },
	{ B150, 150 },
	{ B200, 200 },
	{ B300, 300 },
	{ B600, 600 },
	{ B1200, 1200 },
	{ B1800, 1800 },
	{ B2400, 2400 },
	{ B4800, 4800 },
#if defined(B7200)
	{ B7200, 7200 },
#endif
	{ B9600, 9600 },
#if defined(B14400)
	{ B14400, 14400 },
#endif
	{ B19200, 19200 },
#if defined(B28800)
	{ B28800, 28800 },
#endif
	{ B38400, 38400 },
#if defined(B57600)
	{ B57600, 57600 },
#endif
#if defined(B76800)
	{ B76800, 76800 },
#endif
#if defined(B115200)
	{ B115200, 115200 },
#endif
#if defined(B153600)
	{ B153600, 153600 },
#endif
#if defined(B230400)
	{ B230400, 230400 },
#endif
#if defined(B307200)
	{ B307200, 307200 },
#endif
#if defined(B460800)
	{ B460800, 460800 },
#endif
#if defined(B500000)
	{ B500000, 500000 },
#endif
#if defined(B576000)
	{ B576000, 576000 },
#endif
#if defined(B921600)
	{ B921600, 921600 },
#endif
#if defined(B1000000)
	{ B1000000, 1000000 },
#endif
#if defined(B1152000)
	{ B1152000, 1152000 },
#endif
#if defined(B1500000)
	{ B1500000, 1500000 },
#endif
#if defined(B2000000)
	{ B2000000, 2000000 },
#endif
#if defined(B2500000)
	{ B2500000, 2500000 },
#endif
#if defined(B3000000)
	{ B3000000, 3000000 },
#endif
#if defined(B3500000)
	{ B3500000, 3500000 },
#endif
#if defined(B4000000)
	{ B4000000, 4000000 },
#endif
	{ 0, 0 }
};
#endif


enum escapestates {
	ESCAPESTATE_WAITFORCR = 0,
	ESCAPESTATE_WAITFOREC,
	ESCAPESTATE_PROCESSCMD,
	ESCAPESTATE_WAITFOR1STHEXDIGIT,
	ESCAPESTATE_WAITFOR2NDHEXDIGIT,
};


static volatile int scrunning = 1;
static char *path_dev = PATH_DEV "/";
static int qflag = 0;

#ifdef __CYGWIN__
static int
cfmakeraw(struct termios *termios_p)
{
  termios_p->c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
  termios_p->c_oflag &= ~OPOST;
  termios_p->c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
  termios_p->c_cflag &= ~(CSIZE|PARENB);
  termios_p->c_cflag |= CS8;
  return 0;
}

static int
cfsetspeed(struct termios *termios_p, speed_t speed)
{
  int r=cfsetospeed(termios_p, speed);
  if(r<0) return r;
  return cfsetispeed(termios_p, speed);
}
#endif

static void
sighandler(int sig)
{
	scrunning = 0;
}


static speed_t
parsespeed(char *speed)
{
	long s;
	char *ep;
#if !defined(TERMIOS_SPEED_IS_INT)
	struct termios_speed *ts = termios_speeds;
#endif

	s = strtol(speed, &ep, 0);
	if (ep == speed || ep[0] != '\0') {
		warnx("Unable to parse speed \"%s\"", speed);
		return(B9600);
	}
#if defined(TERMIOS_SPEED_IS_INT)
	return s;
#else
	while(ts->speed != 0) {
		if (ts->speed == s)
			return ts->code;
		ts++;
	}
	warnx("Undefined speed \"%s\"", speed);
	return(B9600);
#endif
}


static int
parseparms(tcflag_t *c, char *p, int f, int m)
{
	if (strlen(p) != 3) {
		warnx("Invalid parameter specification \"%s\"", p);
		return 1;
	}
	*c &= ~CSIZE;
	switch(p[0]) {
		case '5':	*c |= CS5; break;
		case '6':	*c |= CS6; break;
		case '7':	*c |= CS7; break;
		case '8':	*c |= CS8; break;
		default:
			warnx("Invalid character size \"%c\": must be 5, 6, 7, or 8",
					p[0]);
			return 1;
	}
	switch(tolower(p[1])) {
		case 'e':	*c |= PARENB; *c &= ~PARODD; break;
		case 'n':	*c &= ~PARENB;               break;
		case 'o':	*c |= PARENB | PARODD;       break;
		default:
			warnx("Invalid parity \"%c\": must be E, N, or O", p[1]);
			return 1;
	}
	switch(p[2]) {
		case '1':	*c &= ~CSTOPB; break;
		case '2':	*c |= CSTOPB;  break;
		default:
			warnx("Invalid stop bit \"%c\": must be 1 or 2", p[2]);
			return 1;
	}
	*c &= ~CRTSCTS;
	if (f) *c |= CRTSCTS;
	*c |= CLOCAL;
	if (m) *c &= ~CLOCAL;
	return 0;
}


static void
printparms(struct termios *ti, char *tty)
{
	long sp = 0;
	char bits, parity, stops;
#if !defined(TERMIOS_SPEED_IS_INT)
	struct termios_speed *ts = termios_speeds;
	speed_t sc;
#endif

#if defined(TERMIOS_SPEED_IS_INT)
	sp = cfgetispeed(ti);
#else
	sc = cfgetispeed(ti);
	while (ts->speed != 0) {
		if (ts->code == sc) {
			sp = ts->speed;
			break;
		}
		ts++;
	}
#endif
	switch(ti->c_cflag & CSIZE) {
		case CS5: bits = '5'; break;
		case CS6: bits = '6'; break;
		case CS7: bits = '7'; break;
		case CS8: bits = '8'; break;
		default:
			bits ='?';
	}
	if (ti->c_cflag & PARENB) {
		parity = ti->c_cflag & PARODD ? 'O' : 'E';
	} else {
		parity = 'N';
	}
	stops = ti->c_cflag & CSTOPB ? '2' : '1';

	fprintf(stderr, "Connected to %s at %ld %c%c%c, modem status %s, %shardware handshake\n",
		tty, sp, bits, parity, stops,
		ti->c_cflag & CLOCAL ? "ignored" : "observed",
		ti->c_cflag & CRTSCTS ? "" : "no ");
}

static int
hex2dec(char c)
{
  switch(c)
    {
    case '0': return 0;
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;
    case 'a': return 10;
    case 'b': return 11;
    case 'c': return 12;
    case 'd': return 13;
    case 'e': return 14;
    case 'f': return 15;
    case 'A': return 10;
    case 'B': return 11;
    case 'C': return 12;
    case 'D': return 13;
    case 'E': return 14;
    case 'F': return 15;
    }
  return -1;
}

static int
loop(const int sfd, const int escchr, const int msdelay, const char *key_sequence, int key_sequence_len)
{
	enum escapestates escapestate = ESCAPESTATE_WAITFOREC;
	unsigned char escapedigit;
	int i;
	char c;

#if defined(HAS_BROKEN_POLL)
	while (scrunning) {
		fd_set fds;
		struct timeval tv;
		struct timeval *tvp = NULL;

		if (key_sequence && key_sequence_len > 0) {
			tv.tv_sec = 1;
			tv.tv_usec = 0;
			tvp = &tv;
		}

		FD_ZERO(fds);
		FD_SET(STDIN_FILENO, fds);
		FD_SET(sfd, fds);

		if ((i = select(sfd+1, fds, NULL, NULL, tvp)) < 0
				&& errno != EINTR) {
			warn("select()");
			return EX_OSERR;
		}
#else
	struct pollfd pfds[2];
	int poll_timeout = -1;

	if (key_sequence && key_sequence_len > 0) {
		poll_timeout = 1000; /* milliseconds */
	}

	memset(pfds, 0, sizeof(pfds));
	pfds[0].fd = STDIN_FILENO;
	pfds[0].events = POLLIN;
	pfds[1].fd = sfd;
	pfds[1].events = POLLIN;
	while (scrunning) {
		if ((i = poll(pfds, sizeof(pfds)/sizeof(pfds[0]), poll_timeout)) < 0
				&& errno != EINTR) {
			warn("poll()");
			return EX_OSERR;
		}
		if ((pfds[0].revents | pfds[1].revents) & POLLNVAL) {
			warnx("poll() does not support devices");
			return EX_OSERR;
		}
		if (pfds[0].revents & (POLLERR|POLLHUP)) {
			read(STDIN_FILENO, &c, 1);
			warn("poll mask %04x read(tty)", pfds[0].revents);
			return(EX_OSERR);
		}
		if (pfds[1].revents & (POLLERR|POLLHUP)) {
			read(sfd, &c, 1);
			warn("poll mask %04x read(serial)", pfds[1].revents);
			return(EX_OSERR);
		}
#endif

		/* check timeout */
		if (key_sequence && key_sequence_len > 0 && i == 0) {
			i = write(sfd, key_sequence, key_sequence_len);
			if (i != key_sequence_len) {
				err(EX_OSERR, "could not write key sequence to serial device.");
			}
		}

#if defined(HAS_BROKEN_POLL)
		if (FD_ISSET(STDIN_FILENO, fds)) {
#else
		if (pfds[0].revents & POLLIN) {
#endif
			i = read(STDIN_FILENO, &c, 1);
			if (i < 0) {
				err(EX_OSERR, "could not read from STDIN.");
			}
			if (i > 0) {
				switch (escapestate) {
					case ESCAPESTATE_WAITFORCR:
						if (c == '\r') {
							escapestate = ESCAPESTATE_WAITFOREC;
						}
						break;

					case ESCAPESTATE_WAITFOREC:
						if (escchr != -1 && ((unsigned char)c) == escchr) {
							escapestate = ESCAPESTATE_PROCESSCMD;
							continue;
						}
						if (c != '\r') {
							escapestate = ESCAPESTATE_WAITFORCR;
						}
						break;

					case ESCAPESTATE_PROCESSCMD:
						escapestate = ESCAPESTATE_WAITFORCR;
						switch (c) {
							case '.':
								scrunning = 0;
								continue;

							case 'b':
							case 'B':
								if(!qflag)
									fprintf(stderr, "->sending a break<-\r\n");
								tcsendbreak(sfd, 0);
								continue;

							case 'k':
							case 'K':
								fprintf(stderr, "->stop sending key sequence<-\r\n");
								key_sequence = NULL;
								key_sequence_len = 0;
								continue;

							case 'x':
							case 'X':
								escapestate = ESCAPESTATE_WAITFOR1STHEXDIGIT;
								continue;

							default:
								if (((unsigned char)c) != escchr) {
									i = write(sfd, &escchr, 1);
									if (i < 0) {
										err(EX_OSERR, "could not write to serial device.");
									}
								}
						}
						break;

					case ESCAPESTATE_WAITFOR1STHEXDIGIT:
						if (isxdigit(c)) {
							escapedigit = hex2dec(c) * 16;
							escapestate = ESCAPESTATE_WAITFOR2NDHEXDIGIT;
						} else {
							escapestate = ESCAPESTATE_WAITFORCR;
							if(!qflag)
								fprintf(stderr, "->invalid hex digit '%c'<-\r\n", c);
						}
						continue;

					case ESCAPESTATE_WAITFOR2NDHEXDIGIT:
						escapestate = ESCAPESTATE_WAITFORCR;
						if(isxdigit(c)) {
							escapedigit += hex2dec(c);
							write(sfd, &escapedigit, 1);
							if(!qflag)
								fprintf(stderr, "->wrote 0x%02X character '%c'<-\r\n", escapedigit, isprint(escapedigit)?escapedigit:'.');
						} else {
							if(!qflag)
								fprintf(stderr, "->invalid hex digit '%c'<-\r\n", c);
						}
						continue;
				}
				i = write(sfd, &c, 1);
				if(c == '\n' && msdelay > 0) {
					struct timespec d = {msdelay / 1000, (msdelay % 1000 ) * 1000 * 1000};
					nanosleep(&d, &d);
				}
			}
			if (i < 0) {
				err(EX_OSERR, "could not write to serial device.");
			}
		}
#if defined(HAS_BROKEN_POLL)
		if (FD_ISSET(sfd, fds)) {
#else
		if (pfds[1].revents & POLLIN) {
#endif
			i = read(sfd, &c, 1);
			if (i < 0) {
				err(EX_OSERR, "could not read from serial device.");
			}
			if (i > 0) {
				i = write(STDOUT_FILENO, &c, 1);
				if (i < 0) {
					err(EX_OSERR, "could not write to STDIN.");
				}
			}
		}
	}
	return(0);
}


static void
modemcontrol(int sfd, int dtr)
{
#if defined(TIOCSDTR)
	ioctl(sfd, dtr ? TIOCSDTR : TIOCCDTR);
#elif defined(TIOCMSET) && defined(TIOCM_DTR)
	int flags;
	if (ioctl(sfd, TIOCMGET, &flags) >= 0) {
		if (dtr)
			flags |= TIOCM_DTR;
		else
			flags &= ~TIOCM_DTR;
		ioctl(sfd, TIOCMSET, &flags);
	}
#endif
}

/**
 * parse a key sequence.
 * The string key_sequence is modified in place.
 * The string key_sequence may only contain hex-digits and white space.
 * It must have a length of at least 2 characters to parse a key.
 * @param[in,out] key_sequence string of hex-digits on input; binary values on output.
 * @return number of bytes parsed and converted in key_sequence.
 */
static int
parse_key_sequence(char *key_sequence)
{
	int digits_read = 0;
	unsigned char b = 0;
	char *c;
	int key_sequence_len = 0;

	if (key_sequence == NULL) {
		return 0;
	}

	for(c = key_sequence; *c; ++c) {
		if (isspace(*c)) continue;
		if (! isxdigit(*c)) {
			fprintf(stderr, "invalid character in key sequence: %c (0x%02x)\n", *c, (int)*c);
			return -1;
		}
		if (digits_read == 0) {
			b = hex2dec(*c) << 4;
		} else if (digits_read == 1) {
			b |= hex2dec(*c);
			key_sequence[key_sequence_len++] = b;
			key_sequence[key_sequence_len] = 0;
			b = 0;
		}
		++digits_read;
		digits_read &= 1;
	}

	return key_sequence_len;
}

/**
 * parse a key identifier into a key sequence.
 * @param key_id a key identifier.
 * @param[out] key_sequence will be set to an allocated array of bytes. The caller has to free the returned value.
 * @param[out] key_sequence_len size of key_sequence in bytes.
 * @return 0 if key_id is NULL;
 *         0 if key_id is a valid identifier, key_sequence and key_sequence_len are set;
 *        -1 if key_id is an invalid identifier;
 *        -2 upon parameter error.
 */
static int
parse_key_identifier(const char *key_id, char **key_sequence, int *key_sequence_len)
{
	struct key_s {
		const char *id;
		const char *seq;
		const char *comment;
	};

	struct key_s key[] = {
		{ "F1", "\x1bOP", "VT100 F1" },
		{ "F2", "\x1bOQ", "VT100 F2" },
		{ "F3", "\x1bOR", "VT100 F3" },
		{ "F4", "\x1bOS", "VT100 F4" },
		{ "xtermF1", "\x1b[11~", "xterm F1" },
		{ "xtermF2", "\x1b[12~", "xterm F2" },
		{ "xtermF3", "\x1b[13~", "xterm F3" },
		{ "xtermF4", "\x1b[14~", "xterm F4" },
		{ "F5",  "\x1b[15~", "xterm F5" },
		{ "F6",  "\x1b[17~", "xterm F6" },
		{ "F7",  "\x1b[18~", "xterm F7" },
		{ "F8",  "\x1b[19~", "xterm F8" },
		{ "F9",  "\x1b[20~", "xterm F9" },
		{ "F10", "\x1b[21~", "xterm F10" },
		{ "F11", "\x1b[23~", "xterm F11" },
		{ "F12", "\x1b[24~", "xterm F12" },
		{ "DEL", "\x1b[3~", "xterm DEL" },
		{ NULL, NULL, NULL }
	};

	struct key_s *k;

	if (key_id == NULL) {
		return 0;
	}

	if (strcmp(key_id, "list") == 0) {
		fprintf(stderr,
			"id\tcomment\t\tkey sequence\n"
			"------------------------------------------------------------------------------\n");
		for(k = key; k->id; ++k) {
			const char *c;
			fprintf(stderr, "%s\t%s:\t", k->id, k->comment);
			for(c = k->seq; *c; ++c) {
				fprintf(stderr, "%02x ", *c);
			}
			fprintf(stderr, "\n");
		}
		return -1;
	}

	if (! key_sequence) { return -2; }
	if (! key_sequence_len) { return -2; }

	for(k = key; k->id; ++k) {
		if (strcmp(key_id, k->id) == 0) {
			*key_sequence = strdup(k->seq);
			*key_sequence_len = strlen(k->seq);
			return 0;
		}
	}

	return -1;
}


static void
unittest()
{
	{
		char *s = strdup(" 1b   5b\t 33 7e  ");
		assert(parse_key_sequence(s) == 4);
		assert(s[0] == 0x1b);
		assert(s[1] == 0x5b);
		assert(s[2] == 0x33);
		assert(s[3] == 0x7e);
		free(s);

		assert(parse_key_sequence(NULL) == 0);

		s = strdup("4");
		assert(parse_key_sequence(s) == 0);
		free(s);

		s = strdup("44");
		assert(parse_key_sequence(s) == 1);
		assert(s[0] == 0x44);
		free(s);
	}
	{
		char *key_sequence;
		int key_sequence_len;
		assert(parse_key_identifier(NULL, &key_sequence, &key_sequence_len) == 0);
		assert(parse_key_identifier("does not exist", &key_sequence, &key_sequence_len) < 0);
		assert(parse_key_identifier("F1", NULL, &key_sequence_len) < 0);
		assert(parse_key_identifier("F8", &key_sequence, NULL) < 0);

		assert(parse_key_identifier("F4", &key_sequence, &key_sequence_len) == 0);
		assert(key_sequence_len == 3);
		assert(key_sequence[0] == 0x1b);
		assert(key_sequence[1] == 0x4f);
		assert(key_sequence[2] == 0x53);
		free(key_sequence);
	}
}

static void
usage(void)
{
	fprintf(stderr, "Connect to a serial device, using this system as a console. Version %s.\n"
			"usage:\tsc [-fmq] [-d ms] [-e escape] [-p parms] [-s speed] [-k 'key sequence'] [-K <key>] device\n"
			"\t-f: use hardware flow control (CRTSCTS)\n"
			"\t-m: use modem lines (!CLOCAL)\n"
			"\t-q: don't show connect, disconnect and escape action messages\n"
 			"\t-d: delay in milliseconds after each newline character\n"
			"\t-e: escape char or \"none\", default '~'\n"
			"\t-p: bits per char, parity, stop bits, default \"%s\"\n"
			"\t-s: speed, default \"%s\"\n"
		        "\t-k: send key(s) once per second. 'key sequence' contains hex digits and white space.\n"
			"\t-K: send a single key once per second. Use 'list' to show valid key identifiers.\n"
			"\tdevice, default \"%s\"\n",
			SC_VERSION, DEFAULTPARMS, DEFAULTSPEED, DEFAULTDEVICE);
	fprintf(stderr, "escape actions are started with the 3 character combination: CR + ~ +\n"
		        "\t~ - send '~' character\n"
		        "\t. - disconnect\n"
		        "\tb - send break\n"
		        "\tk - stop sending the key (sequence)\n"
   		        "\tx<2 hex digits> - send decoded character\n");
#if defined(TERMIOS_SPEED_IS_INT)
	fprintf(stderr, "available speeds depend on device\n");
#else
	{
		struct termios_speed *ts = termios_speeds;

		fprintf(stderr, "available speeds: ");
		while (ts->speed != 0) {
			fprintf(stderr, "%ld ", ts->speed);
			ts++;
		}
		fprintf(stderr, "\n");
	}
#endif
	exit(EX_USAGE);
}


int
main(int argc, char **argv)
{
	int escchr = '~';
	char *tty = DEFAULTDEVICE;
	char *speed = DEFAULTSPEED;
	char *parms = DEFAULTPARMS;
	int fflag = 0;
	int mflag = 0;
	int sfd = -1;
	char buffer[PATH_MAX+1];
	struct termios serialti, consoleti, tempti;
	int ec = 0;
	int msdelay = 0;
	int i;
	char c;
	char *key_sequence = NULL;
	int key_sequence_len = 0;

	unittest();

	while ((c = getopt(argc, argv, "d:e:fhk:K:mp:qs:?")) != -1) {
		switch (c) {
			case 'd':
				msdelay=atoi(optarg);
				if(msdelay <= 0)
					fprintf(stderr, "warning: ignoring negative or zero delay: %i\n", msdelay);
				break;
			case 'e':
				if (strcmp(optarg, "none") == 0) {
					escchr = -1;
				} else if (strlen(optarg) == 1) {
					escchr = (unsigned char)optarg[0];
				} else if (strlen(optarg) == 2 && optarg[0] == '^' &&
						toupper(optarg[1]) >= '@' && toupper(optarg[1]) <= '_') {
					escchr = toupper(optarg[1]) & 0x1f;
				} else {
					errx(EX_USAGE, "Invalid escape character \"%s\"", optarg);
				}
				break;
			case 'f':
				fflag = 1;
				break;
			case 'm':
				mflag = 1;
				break;
			case 'p':
				parms = optarg;
				break;
			case 'q':
				qflag = 1;
			case 's':
				speed = optarg;
				break;
			case 'k':
				key_sequence = optarg;
				key_sequence_len = parse_key_sequence(key_sequence);
				if (key_sequence_len < 0) {
					errx(EX_USAGE, "invalid key in key_sequence");
				}
				break;
			case 'K':
				if (parse_key_identifier(optarg, &key_sequence, &key_sequence_len) < 0) {
					return EX_USAGE;
				}
				break;
			case 'h':
			case '?':
			default:
				usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc == 1) {
		tty = argv[0];
	}
	if (argc > 1) {
		usage();
	}

	if (key_sequence_len > 0) {
		fprintf(stderr, "will send %i bytes/keys every second\n", key_sequence_len);
	}

	if (strchr(tty, '/') == NULL) {
		if (strlen(path_dev) + strlen(tty) > PATH_MAX) {
			errx(EX_USAGE, "Device name \"%s\" is too long.", tty);
		}
		memcpy(buffer, path_dev, strlen(path_dev)+1);
		memcpy(buffer+strlen(path_dev), tty, strlen(tty)+1);
		tty = buffer;
	}
	sfd = open(tty, O_RDWR);
	if (sfd < 0) {
		err(EX_OSERR, "open %s", tty);
	}
	/* save tty configuration */
	if (tcgetattr(STDIN_FILENO, &consoleti)) {
		close(sfd);
		err(EX_OSERR, "tcgetattr() tty");
	}
	/* save serial port configuration */
	if (tcgetattr(sfd, &serialti)) {
		close(sfd);
		err(EX_OSERR, "tcgetattr(%s)", tty);
	}
	/* configure serial port */
	memcpy(&tempti, &serialti, sizeof(tempti));
	cfmakeraw(&tempti);
	tempti.c_cc[VMIN] = 1;
	tempti.c_cc[VTIME] = 0;
	if (cfsetspeed(&tempti, parsespeed(speed))) {
		ec = EX_OSERR;
		warn("cfsetspeed(%s)", tty);
		goto error;
	}
	if (parseparms(&tempti.c_cflag, parms, fflag, mflag)) {
		ec = EX_USAGE;
		goto error;
	}
	if (tcsetattr(sfd, TCSANOW, &tempti)) {
		ec = EX_OSERR;
		warn("tcsetattr(%s)", tty);
		goto error;
	}
	signal(SIGHUP, sighandler);
	signal(SIGINT, sighandler);
	signal(SIGQUIT, sighandler);
	signal(SIGTERM, sighandler);

	if (!qflag) {
		/* re-read serial port configuration */
		if (tcgetattr(sfd, &tempti)) {
			close(sfd);
			err(EX_OSERR, "tcgetattr(%s)", tty);
		}
		printparms(&tempti, tty);
		fflush(stderr);
	}
	/* put tty into raw mode */
	i = fcntl(STDIN_FILENO, F_GETFL);
	if (i == -1 || fcntl(STDIN_FILENO, F_SETFL, i | O_NONBLOCK)) {
		close(sfd);
		err(EX_OSERR, "fcntl() tty");
	}
	memcpy(&tempti, &consoleti, sizeof(tempti));
	cfmakeraw(&tempti);
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &tempti)) {
		ec = EX_OSERR;
		warn("tcsetattr() tty");
		goto error;
	}
	modemcontrol(sfd, 1);

	ec = loop(sfd, escchr, msdelay, key_sequence, key_sequence_len);

error:
	if (sfd >= 0) {
		modemcontrol(sfd, 0);
		tcsetattr(sfd, TCSAFLUSH, &serialti);
		tcsetattr(STDIN_FILENO, TCSAFLUSH, &consoleti);
		close(sfd);
	}
	fprintf(stderr, "\n");
	if (!qflag) fprintf(stderr, "Connection closed.\n");
	return ec;
}

/*
 * Editor modelines  -  https://www.wireshark.org/tools/modelines.html
 *
 * Local variables:
 * c-basic-offset: 8
 * tab-width: 8
 * indent-tabs-mode: t
 * c-default-style: bsd
 * End:
 *
 * vi: set shiftwidth=8 tabstop=8 noexpandtab:
 * :indentSize=8:tabSize=8:noTabs=false:
 */
