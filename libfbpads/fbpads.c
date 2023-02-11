#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <linux/vt.h>
#include "conf.h"
#include "fbpad.h"
#include "draw.h"

char *_s_hide = "\x1b[2J\x1b[H\x1b[?25l";
char *_s_show = "\x1b[?25h";

struct termios oldtermios;

#define CTRLKEY(x)	((x) - 96)
#define POLLFLAGS	(POLLIN | POLLHUP | POLLERR | POLLNVAL)

#define BRWID		2

static struct term *term_instance = NULL;

static void destroy(){
	write(1, _s_show, strlen(_s_show));
	tcsetattr(0, 0, &oldtermios);
	term_free(term_instance);
	pad_free();
	// scr_done();
	fb_free();
}
static int readchar(void){
	char b;
	if (read(0, &b, 1) > 0) return (unsigned char) b;
	return -1;
}
static void directloadkey(void){
	int c = readchar();
	if (c == ESC) {
		switch ((c = readchar())) {
			case 's':
				term_screenshot();
				return;
			case CTRLKEY('q'):
				destroy();
				exit(0);
				return;
			case ',':
				term_scrl(pad_rows() / 3);
				return;
			case '.':
				term_scrl(-pad_rows() / 3);
				return;
			case '=':
				// t_split(split[ctag] == 1 ? 2 : 1);
				return;
			default:
				term_send(ESC);
		}
	}
	if (c != -1) term_send(c);
}
static int pollterm(void){
	struct pollfd ufds[] = {
		{
			.fd = 0,
			.events = POLLIN
		},{
			.fd = term_fd(term_instance),
			.events = POLLIN
		}
	};
	if (poll(ufds, 2, 1000) < 1)
		return 0;
	struct pollfd ufd = ufds[0];
	struct pollfd ufd_chd = ufds[1];
	if (ufd.revents & (POLLFLAGS & ~POLLIN))
		return 1;
	if (ufd.revents & POLLIN)
		directloadkey();
	// for child
	// term_save(term_instance);
	term_redraw(0);
	if (!(ufd_chd.revents & POLLFLAGS)) return 0;
	if (ufd_chd.revents & POLLIN) {
		term_read();
	} else {
		term_end();
		return 1;
	}
	return 0;
}
static void mainloop(char** argv){
	struct termios termios;
	tcgetattr(0, &termios);
	oldtermios = termios;
	cfmakeraw(&termios);
	tcsetattr(0, TCSAFLUSH, &termios);
	term_load(term_instance, 1);
	term_redraw(1);
	term_exec(argv, 0);
	// pad_conf(0, 0, fb_rows(), fb_cols());
	scr_load(0);
	while (!pollterm());
}

static void onsignal(int n){
	switch (n) {
		case SIGINT:
			destroy();
			exit(1);
		case SIGCHLD:
			while(waitpid(-1, NULL, WNOHANG) > 0);
			break;
	}
}
static void signalsetup(void){
	signal(SIGCHLD, onsignal);
}
int main(int argc, char** argv){
	if(argc == 1){
		fprintf(stderr, "fbpads: usage: fbpads <program [args..]>");
		return 1;
	}
	if (fb_init(getenv("FBDEV"))) {
		fprintf(stderr, "fbpads: failed to initialize framebuffer\n");
		return 1;
	}
	if (pad_init()) {
		fprintf(stderr, "fbpads: cannot load fonts\n");
		return 1;
	}
	term_instance = term_make();
	write(1, _s_hide, strlen(_s_hide));
	signalsetup();
	fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);
	mainloop(argv+1);
	destroy();
	return 0;
}
