/*
	lbml3rd:main.c
	2016~2022 @ JuRt
	TODO:
		fix segfault when exiting
*/

#include "stdio.h"
#include "stdarg.h"
#include "unistd.h"
#include "string.h"
#include "dirent.h"
#include "fcntl.h"
#include "termios.h"
#include "errno.h"
#include "time.h"
#include "linux/input.h"
#include "linux/fb.h"
#include "sys/stat.h"
#include "sys/wait.h"
#include "sys/mount.h"
#include "sys/reboot.h"

// borrowed from linux kernel source: tools/include/nolibc/types.h
#ifndef makedev
# define makedev(major, minor) ((dev_t)((((major) & 0xfff) << 8) | ((minor) & 0xff)))
#endif
#ifndef major
# define major(dev) ((unsigned int)(((dev) >> 8) & 0xfff))
#endif
#ifndef minor
# define minor(dev) ((unsigned int)(((dev) & 0xff))
#endif

#ifdef LBML_USE_EXTFB
# include "libfbpads.h"
# define FB0_PATH "/dev/fb0"
#endif

#ifndef LBMLVER
# define LBMLVER "UNDEF"
#endif

typedef int lbml_display_device_mask_t;
typedef enum {
	LBML_DD_MASK_TERM = 0b1,
	LBML_DD_MASK_FB = 0b10,
	LBML_DD_MASK_ALL = 0b11
} lbml_display_device_t;
#define LBML_HAS_TERM() (config_display_device&LBML_DD_MASK_TERM)
#define LBML_HAS_FB() (config_display_device&LBML_DD_MASK_FB)

/* config values */
int config_show_splash = 1;
char config_title[255] = "LBML Boot Manager " LBMLVER;
lbml_display_device_mask_t config_display_device = LBML_DD_MASK_TERM;
// int config_device_mtk = 0;

/* TODO intergate into jurt */
unsigned long nanos(){
	struct timespec ts;
	if(clock_gettime(CLOCK_MONOTONIC_RAW, &ts)==-1) return 0;
	return ts.tv_sec*1000000000L + ts.tv_nsec;
}

unsigned long lbml_next_flush_tick = 1;
void lbml_flush(){
	// u_int32_t V = config_device_mtk? FBIOPUT_VSCREENINFO : FBIOPAN_DISPLAY;
	// const int V = FBIOPAN_DISPLAY;
	#ifdef LBML_USE_EXTFB
		if(LBML_HAS_FB()){
			struct fb_var_screeninfo var = fb_vscreeninfo();
			int fd = fb_fd();
			var.yoffset = 0;
			var.activate = FB_ACTIVATE_VBL;
			// ioctl(fd, FBIOPUT_VSCREENINFO, &var);
			ioctl(fd, FBIOPAN_DISPLAY, &var);
		}
	#endif
	// if(config_device_mtk){
		// for(int i=1, p=var.yres, n=var.yres_virtual/var.yres; i<n; ++i){
			// fb_setypan(i);
			// term_redraw(1);
			// var.yoffset = i*p;
			// ioctl(fd, V, &var);
		// }
		// fb_setypan(0);
	// }
}
void lbml_lazy_flush(){
	// flush at fps=15
	if(lbml_next_flush_tick>0 && nanos()>lbml_next_flush_tick){
		lbml_flush();
		lbml_next_flush_tick = nanos() + 1000000000L/15;
	}
}

void lbml_printf(lbml_display_device_mask_t mask, char* fmt, ...){
	va_list args;
	va_start(args, fmt);
	if(mask&LBML_DD_MASK_TERM) vprintf(fmt, args);
	#ifdef LBML_USE_EXTFB
		if(mask&LBML_DD_MASK_FB) libfbpads_vprintf(fmt, args);
		// lbml_lazy_flush();
	#endif
	va_end(args);
}

void lbml_puts(lbml_display_device_mask_t mask, char* str){
	lbml_printf(mask, "%s\n", str);
}

// libudev-zero
#include "udev.h"

#define UU(x) (void)(x)
#define NULARRSTR(x) (!x || x[0]=='\0')
typedef unsigned char uc;

#ifndef LBMLVER
#define LBMLVER "[DEFINE YOUR VERSION HERE]"
#endif

const char DEFAULT_INIT_PATH[]="/init";
const char DEFAULT_KEXEC_PATH[]="/sbin/kexec";

#include "sash.h"
#include "jurt.h"
#include "toml.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// #define _STBIR_DEF_FILTER STBIR_FILTER_BOX
// #define STBIR_DEFAULT_FILTER_UPSAMPLE _STBIR_DEF_FILTER
// #define STBIR_DEFAULT_FILTER_DOWNSAMPLE _STBIR_DEF_FILTER
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

#define LOGO_IMAGE_NAME logo_png
IMPORTF_AUTOV(LOGO_IMAGE_NAME)

const char ascii_mapper[] = " .'`^\",:;Il!i><~+_-?][}{1)(|\\/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$";
// const char ascii_mapper[] = " ###";
#define MAPPER_AT(v) ascii_mapper[(int)((v)*(sizeof(ascii_mapper)-1)/0xff)]
#define XY2I(width, x, y) ((y)*(width)+(x))

#define KEY_CONFIRM (KEY_ENTER<<16)
#define EVINPUT_MAXCOUNT 16
#define EVINPUT_WAIT_PERIOD 10000000

#define ECHOFLAGS (ECHO|ECHOE|ECHOK|ECHONL)
void set_echo_enabled(int state){
	struct termios term;
	if(tcgetattr(STDIN_FILENO, &term)==-1){
		l_err("cannot get attrs from terminal");
		return;
	}
	if(state) term.c_lflag |= ECHOFLAGS;
	else term.c_lflag &= ~ECHOFLAGS;
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &term)==EINTR){
		l_err("cannot set the attrs of terminal");
		return;
	}

	if(state) lbml_printf(config_display_device, "\x1b[?25h");
	else lbml_printf(config_display_device, "\x1b[?25l");
	fflush(stdout);
}

int ismountpoint(const char* path){
	if(!path) return 0;
	struct stat st1, st2;
	if(lstat(path, &st1)) return 0;
	// if(!S_ISDIR(st1.st_mode)) return 0;	// not a directory
	static const char fmt[] = "%s/..";
	char* new_path = (char*) malloc((strlen(path)+sizeof(fmt)));
	sprintf(new_path, fmt, path);
	if(stat(new_path, &st2)){
		free(new_path);
		return 0;
	}
	free(new_path);
	int not_mnt = (st1.st_dev == st2.st_dev) && (st1.st_ino != st2.st_ino);
	return !not_mnt;
}
int isfileexists(const char* path){
	struct stat st;
	if(lstat(path, &st)) return 0;
	return st.st_mode & S_IFCHR;
}

int do_premkdir(const char* path){
	struct stat info;
	if(!stat(path, &info) && (info.st_mode & S_IFDIR)) return 1;	// success
	if(mkdir(path, S_IRWXO)) return 0;
	return 1;
}
const char premountpoints[][3][10] = {
	{"sysfs", "/sys", "sysfs"},
	// {"devtmpfs", "/dev", "devtmpfs"},
	{"tmpfs", "/dev", "tmpfs"}
};
int do_premount(){
	for(int i=0; i<sizeof(premountpoints)/30; ++i){
		const char(* mp)[10] = premountpoints[i];
		const char* dest = mp[1];
		if(!ismountpoint(dest)){
			l_inf("targ=%s", dest);
			if(!do_premkdir(dest)) return 0;
			if(mount(mp[0], dest, mp[2], 0, NULL)) return 0;
		}
	}
	return 1;	// success
}
void undo_premount(){
	for(int i=0; i<sizeof(premountpoints)/30; ++i){
		umount(premountpoints[i][1]);
	}
}

int udevzero_scan(){
	if(!do_premount()){
		l_err("cannot initialize mountpoints");
		return 0;
	}
	// create /dev/input if not exist
	if(!do_premkdir("/dev/input")){
		l_inf("cannot create /dev/input");
		return 0;
	}
	// try to setup udev
	struct udev *udev = udev_new();
	if(!udev){
		l_err("cannot initialize udev");
		return 0;
	}
	struct udev_enumerate *enumerate = udev_enumerate_new(udev);
	if(!enumerate){
		l_err("cannot create enumerate context");
		udev_unref(udev);
		return 0;
	}
	/* framebuffer devices */
	// udev_enumerate_add_match_subsystem(enumerate, "graphics");
	udev_enumerate_add_match_subsystem(enumerate, "input");
	udev_enumerate_scan_devices(enumerate);
	struct udev_list_entry *devices = udev_enumerate_get_list_entry(enumerate);
	if(!devices){
		l_err("cannot scan for devices");
		udev_enumerate_unref(enumerate);
		udev_unref(udev);
		return 0;
	}
	// TODO create all fbs
	// currently only fb0
	mknod(FB0_PATH, S_IFCHR, makedev(29,0));
	struct udev_list_entry *dev_list_entry;
	udev_list_entry_foreach(dev_list_entry, devices){
		const char* path = udev_list_entry_get_name(dev_list_entry);
		struct udev_device *dev = udev_device_new_from_syspath(udev, path);
		const char* dev_path = udev_device_get_devnode(dev);
		dev_t devnum = udev_device_get_devnum(dev);
		// l_inf("udev get: %s", dev_path);
		// try to mknod
		// if(!strcmp(udev_device_get_subsystem(dev), "graphics")){
			// if(!isfileexists(FB0_PATH) && mknod(FB0_PATH, S_IFCHR, devnum)){
				// l_err("cannot mknod for " FB0_PATH);
			// }
		// }
		if(!isfileexists(dev_path) && mknod(dev_path, S_IFCHR, devnum)){
			l_err("cannot mknod for %s", dev_path);
		}
	}
	udev_enumerate_unref(enumerate);
	udev_unref(udev);
	return 1;
}
int evinput_fds[EVINPUT_MAXCOUNT];
int evinput_dev_count = 0;
const int ev_siz = sizeof(struct input_event);
void evinput_deinit(){
	for(int i=0; i<evinput_dev_count; ++i){
		close(evinput_fds[i]);
	}
	evinput_dev_count=0;
	undo_premount();
}
void evinput_init(){
	if(evinput_dev_count>0) evinput_deinit();
	else udevzero_scan();
	DIR* dr = opendir("/dev/input");
	if(!dr){
		l_err("cannot read /dev/input/");
		return;
	}

	struct dirent* dentry;
	evinput_dev_count = 0;
	while((dentry=readdir(dr))!=NULL){
		if(!strncmp(dentry->d_name, "event", 5)){
			char path[0xff] = {};
			int retlen = snprintf(path, sizeof(path), "/dev/input/%s", dentry->d_name);
			if(retlen<0){
				l_inf("device %s path too long", dentry->d_name);
				continue;
			}
			int fd = open(path, O_RDONLY | O_NONBLOCK);
			if(fd<0){
				l_inf("cannot open device %s", path);
				continue;
			}
			evinput_fds[evinput_dev_count++] = fd;
		}
	}
	closedir(dr);
	if(evinput_dev_count == 0){
		l_inf("cannot open any input device");
	}
}
void evinput_reload(){
	evinput_deinit();
	evinput_init();
}
// @return keycode
int evinput_get(){
	nano_sleep(EVINPUT_WAIT_PERIOD);
	for(int i=0; i<evinput_dev_count; ++i){
		struct input_event ev = {};
		int rlen = read(evinput_fds[i], &ev, ev_siz);
		if(rlen<ev_siz && rlen>0){
			l_inf("broken input code");
			lseek(evinput_fds[i], -rlen, SEEK_CUR);
			continue;
		}
		if(rlen<=0) continue;
		if(ev.type == EV_KEY && ev.value){
			switch(ev.code){
				case KEY_ENTER:
				case KEY_POWER:
					return KEY_CONFIRM;
				case KEY_UP:
				case KEY_DOWN:
				case KEY_VOLUMEUP:
				case KEY_VOLUMEDOWN:
				// case KEY_R:
				case KEY_C:
					return ev.code;
			}
		}
	}
	return 0;
}

void wait_enter(){
	while(KEY_CONFIRM != evinput_get());
}

typedef uint32_t Point;
#define PX(n) (int)((n)&0xffff)
#define PY(n) (int)(((n)>>16)&0xffff)
#define mkPoint(x,y) (((x)&0xffff)|(((y)&0xffff)<<16))
// #define MAX_TERMINAL_SIDE 9999
Point get_terminal_size(lbml_display_device_t dd){
	// Point p = get_cursor_pos();
	// set_cursor_pos(MAX_TERMINAL_SIDE, MAX_TERMINAL_SIDE);
	// Point siz = get_cursor_pos();
	// siz = mkPoint(PX(siz)+1, PY(siz)+1);
	// set_cursor_pos(PX(p), PY(p));
	int x = 0, y = 0;

	#ifdef LBML_USE_EXTFB
		if(dd == LBML_DD_MASK_FB){
			y = pad_rows();
			x = pad_cols();
		}else{
	#else
		{
	#endif
			struct winsize w;
			ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
			y = w.ws_row;
			x = w.ws_col;
		}

	return mkPoint(x, y);
}
void reset_pointer(){
	lbml_puts(config_display_device, "\x1b[0;0H");
	// fflush(stdout);
}
void clear_screen(){
	lbml_puts(config_display_device, "\x1b[2J");
	// fflush(stdout);
}
void set_cursor_pos(lbml_display_device_t dd, int x, int y){
	if(x<0)x=0;
	if(y<0)y=0;
	lbml_printf(dd, "\x1b[%d;%dH", y+1, x+1);
	// fflush(stdout);
}
/* deprecated
Point get_cursor_pos(lbml_display_device_t dd){
	l_inf("getting");
	lbml_printf(dd, "\x1b[6n");
	fflush(stdout);
	int x = 0;
	int y = 0;
	// scanf("\x1b[%d;%dR", &y, &x);

	{
		char _ansi = 0;
		int m = 0;
		for(int i=0; i<16; ++i){
			int rlen = fread(&_ansi, 1, 1, stdin);
			if(rlen<=0) break;
			if(_ansi == '\x1b');
			else if(_ansi == '[') m = 1;
			else if(_ansi == ';') m = 2;
			else if(_ansi == 'R') m = 3;
			else if(m == 0){
				l_inf("invalid ANSI code header at get_cursor_pos");
				break;
			}else if(_ansi>='0' && _ansi<='9'){
				switch(m){
					case 1: y*=10; y+=_ansi-48; break;
					case 2: x*=10; x+=_ansi-48; break;
					case 3: goto RED_END;
				}
			}else{
				l_inf("invalid ANSI code at get_cursor_pos, got: (char)%d", _ansi);
				break;
			}
		}
		RED_END:{}
	}

	// conio.h: not found
	// x = wherex();
	// y = wherey();

	return mkPoint(x-1, y-1);
}*/
void _log_center(lbml_display_device_t dd, char* text, int white_bg){
	Point screen_siz = get_terminal_size(dd);
	const int w = PX(screen_siz);
	const int h = PY(screen_siz);

	int len = strlen(text);
	// if(len<1) return;
	// char text[255]={};
	// strcpy(text, msg);
	DynamicArrayPtr lines = DA_create(sizeof(Point));	// x,y = start,end
	{
		int last_start = 0;
		int i=1;
		for(; i<len; ++i){
			if(text[i]=='\n' || i-last_start>=w){
				Point p = mkPoint(last_start, i);
				DA_push(lines, &p);
				if(text[i]=='\n') ++i;
				last_start = i;
			}
		}
		Point p = mkPoint(last_start, i);
		if(last_start<i) DA_push(lines, &p);
	}

	int paddy = (h-lines->len)/2;
	set_cursor_pos(dd, 0, paddy);
	white_bg &= 1;
	char fmt[][6] = {
		"37;40",
		"30;36"
	};
	for(int l=0; l<lines->len; ++l){
		Point str_inf = DA_get_int(lines, l);
		const int start = PX(str_inf);
		const int end = PY(str_inf);
		const int str_len = end-start;
		// l_inf("%d", str_len);
		int paddx = (w-str_len)/2;
		for(int i=0; i<paddx; ++i) lbml_printf(dd, " ");
		// char bkp = text[end];
		// text[end] = '\0';
		lbml_printf(dd, "\x1b[%sm%.*s\x1b[0;0m", fmt[white_bg], str_len, &text[start]);
		lbml_puts(dd, "");
		// text[end] = bkp;
	}
	DA_free(lines);
}
void log_center(lbml_display_device_mask_t mask, char* text, int white_bg){
	clear_screen();
	if(mask&LBML_DD_MASK_TERM) _log_center(LBML_DD_MASK_TERM, text, white_bg);
	if(mask&LBML_DD_MASK_FB) _log_center(LBML_DD_MASK_FB, text, white_bg);
}
void log_center_whitebg(char* text){
	log_center(config_display_device, text, 1);
}
void log_center_blackbg(char* text){
	log_center(config_display_device, text, 0);
}

/***** MAIN *****/

int going_exit = 0;

void _deinit(){
	#ifdef LBML_USE_EXTFB
		libfbpads_destroy();
	#endif
	set_echo_enabled(1);
	evinput_deinit();
}
void deinit(){
	_deinit();
	exit(0);
}
void onSIGINT(int sig, siginfo_t * info, void* ptr){
	UU(sig);
	UU(info);
	UU(ptr);

	going_exit = 1;

	// deinit();
}

int _display_logo(lbml_display_device_t dd){
	Point term_siz = get_terminal_size(dd);
	int tw = PX(term_siz);
	int th = PY(term_siz);
	int itw = tw/2;	// image width in terminal is the half of terminal

	// log_center_blackbg("loading logo...");

	// load image
	int width = 0;
	int height = 0;
	int channels = 0;
	stbi_uc* logo_data = stbi_load_from_memory(
		(unsigned char *)&IMPORTF_PREV_START(LOGO_IMAGE_NAME),
		IMPORTF_PREV_SIZE(LOGO_IMAGE_NAME),
		&width,
		&height,
		&channels,
		1
	);
	if(!logo_data){
		l_err("failed to load logo");
		return 1;
	}

	if(width>=itw || height>=th){
		float scale = 1.0f;
		int d_w = width-itw;
		int d_h = height-th;
		if(d_w>d_h){
			scale = 1.0f*itw/width;
		}else{
			scale = 1.0f*th/height;
		}
		int new_width = width*scale;
		int new_height = height*scale;
		// lbml_printf("scale=%f, width=%d, height=%d\n", scale, width, height);
		// wait_enter();
		int output_len = new_width*new_height;
		stbi_uc* new_data = (stbi_uc*) malloc(output_len*sizeof(stbi_uc));
		if(!stbir_resize_uint8(
			logo_data, width, height, 0,
			new_data, new_width, new_height, 0,
			1
		)){
			l_inf("cannot resize image");
		}else{
			// replace logo_data with new_data
			stbi_image_free(logo_data);
			logo_data = new_data;
			width = new_width;
			height = new_height;
		}
	}

	// calc paddings
	int paddx = itw-width;
	int paddy = (th-height)/2;

	// reset_pointer();
	set_cursor_pos(dd, 0, paddy);
	for(int y=0; y<height; ++y){
		// if(y) lbml_puts("");
		set_cursor_pos(dd, 0, paddy+y);
		for(int i=0; i<paddx; ++i){lbml_printf(dd, " ");}
		for(int x=0; x<width; ++x){
			// uc* p = &logo_data[XY2I(width, x, y)*4];
			// uc r = *p;
			// uc g = *(p+1);
			// uc b = *(p+2);
			// uc grey = 0.2126 * r + 0.7152 * g + 0.0722 * b;
			// char ch = MAPPER_AT(grey);
			char ch = MAPPER_AT(logo_data[XY2I(width, x, y)]);
			lbml_printf(dd, "%c%c", ch, ch);
		}
		fflush(stdout);
		// lbml_lazy_flush();
		if(going_exit) return 1;
		// nano_sleep(12500000);
	}
	lbml_flush();
	stbi_image_free(logo_data);
	nano_sleep(1000000000);	// 1 sec
	return 0;
}

int display_logo(){
	log_center_blackbg("loading logo...");
	if(LBML_HAS_TERM() && _display_logo(LBML_DD_MASK_TERM)) return 1;
	if(LBML_HAS_FB() && _display_logo(LBML_DD_MASK_FB)) return 1;
}

typedef enum {
	entry_type_init = 1,
	entry_type_kexec,
	entry_type_shell
} entry_type;
// TODO malloc
typedef struct {
	char title[255];
	entry_type type;
	char exe_path[255];
	char cmdline[255];	// for type=shell
	char param_str[512];
} menu_entry;
menu_entry menu_entries[32];
int menu_entries_count = 0;

void parse_menu_config_entry(toml_table_t* conf, char* entry_id){
	toml_table_t* entry = toml_table_in(conf, entry_id);
	if(!entry){
		l_err("cannot parse entry %s", entry_id);
		return;
	}

	menu_entry dest_entry = {};

	toml_datum_t title = toml_string_in(entry, "title");
	if(title.ok){
		strncpy(dest_entry.title, title.u.s, strlen(title.u.s));
		free(title.u.s);
	}else{
		sprintf(dest_entry.title, "[%s]", entry_id);
	}

	toml_datum_t type = toml_string_in(entry, "type");
	if(!type.ok){
		l_err("no type param in entry %s", entry_id);
		return;
	}
	if(!strcmp(type.u.s, "init")) dest_entry.type = entry_type_init;
	else if(!strcmp(type.u.s, "kexec")) dest_entry.type = entry_type_kexec;
	else if(!strcmp(type.u.s, "shell")) dest_entry.type = entry_type_shell;
	else {
		l_inf("invalid type in entry %s", entry_id);
		return;
	}
	free(type.u.s);

	// type-spec
	if(dest_entry.type == entry_type_shell){
		toml_datum_t cmdline = toml_string_in(entry, "cmdline");
		if(!cmdline.ok){
			l_err("cannot get script path in entry %s", entry_id);
			return;
		}
		strcpy(dest_entry.cmdline, cmdline.u.s);
		free(cmdline.u.s);
	}

	toml_datum_t exe_path = toml_string_in(entry, "exe_path");
	if(exe_path.ok){
		strncpy(dest_entry.exe_path, exe_path.u.s, strlen(exe_path.u.s));
		free(exe_path.u.s);
	}else{
		if(dest_entry.type == entry_type_init) strcpy(dest_entry.exe_path, DEFAULT_INIT_PATH);
		else if(dest_entry.type == entry_type_kexec) strcpy(dest_entry.exe_path, DEFAULT_KEXEC_PATH);
		else if(dest_entry.type == entry_type_shell) dest_entry.exe_path[0] = '\0';	// default to sash
	}

	toml_datum_t params = toml_string_in(entry, "params");
	if(params.ok){
		strncpy(dest_entry.param_str, params.u.s, strlen(params.u.s));
		free(params.u.s);
	}

	menu_entries[menu_entries_count++] = dest_entry;
}
int parse_menu_config(char* path){
	log_center_blackbg("loading config...");

	char errbuf[200];
	#ifdef LBML_CONFIG_STATIC
		IMPORTF_AUTOV(lbml_static_config_toml)
		toml_table_t* conf = toml_parse(&IMPORTF_PREV_START(lbml_static_config_toml), errbuf, sizeof(errbuf));
	#else
		FILE* fp = fopen(path, "r");
		if(!fp){
			l_err("cannot open config file %s", path);
			return 1;
		}
		toml_table_t* conf = toml_parse_file(fp, errbuf, sizeof(errbuf));
		fclose(fp);
	#endif

	if(!conf){
		l_err("failed to parse config file %s", path);
		return 1;
	}
	toml_table_t* core = toml_table_in(conf, "core");
	if(core){
		menu_entries_count = 0;
		// show splash
		toml_datum_t show_splash = toml_bool_in(core, "show_splash");
		if(show_splash.ok){
			config_show_splash = show_splash.u.b;
		} 
		// title
		toml_datum_t title = toml_string_in(core, "title");
		if(title.ok){
			strncpy(config_title, title.u.s, strlen(title.u.s)+1);
			free(title.u.s);
		}
		// device-specific settings
		#ifdef LBML_USE_EXTFB
			toml_datum_t display_device = toml_string_in(core, "display_device");
			if(display_device.ok){
				const char* v = display_device.u.s;
				if(!strcmp(v, "console")) config_display_device = LBML_DD_MASK_TERM;
				else if(!strcmp(v, "framebuffer")) config_display_device = LBML_DD_MASK_FB;
				else if(!strcmp(v, "all")) config_display_device = LBML_DD_MASK_ALL;
				// otherwise use default setting
			}
		#endif
		// entries
		toml_array_t *entries = toml_array_in(core, "entries");
		if(entries){
			for(int i=0; ; ++i){
				toml_datum_t entry_config_name = toml_string_at(entries, i);
				if(entry_config_name.ok){
					parse_menu_config_entry(conf, entry_config_name.u.s);
					free(entry_config_name.u.s);
				}else{
					// l_inf("cannot parse entry %d", i+1);
					break;
				}
			}
		}else{
			l_inf("no entries config found");
		}
	}

	toml_free(conf);
	return 0;
}

char* header = config_title;
char footer[255] =
	"Use Up/Down/VolumeUp/VolumeDown to highlight item\n"
	"Press ENTER to BOOT\n"
	"R to REBOOT\n"
	"or C into SASH\n"
;
int menu_sel_curr = 0;
int sel_changed = 0;
void menu_sel_rel(int dir){
	sel_changed = 1;
	menu_sel_curr += dir;
	if(menu_sel_curr<0) menu_sel_curr=0;
	else if(menu_sel_curr>=menu_entries_count) menu_sel_curr=menu_entries_count-1;
}
void printUI(){
	clear_screen();
	reset_pointer();
	lbml_printf(config_display_device, "%s\n", header);
	int header_len = strlen(header);
	for(int i=0; i<header_len; ++i) lbml_printf(config_display_device, "=");
	lbml_puts(config_display_device, "\n");
	for(int i=0; i<menu_entries_count; ++i){
		if(menu_sel_curr == i){
			lbml_printf(config_display_device, "\x1b[30;47m%s", menu_entries[i].title);
		}else{
			lbml_printf(config_display_device, "\x1b[37;40m%s", menu_entries[i].title);
		}
		lbml_puts(config_display_device, "\x1b[0;0m\n");
	}
	lbml_puts(config_display_device, footer);
}
// sash internal funcs
int sash_main(int argc, char** argv);
int sash_execCommand(char* cmd){
	char* argv[] = {
		"sash",
		"-a",
		"-c",
		cmd,
		NULL
	};
	fseek(stdin, 0, SEEK_END);
	set_echo_enabled(1);
	int s = sash_main((sizeof(argv)/sizeof(void*))-1, argv);
	if(!s) deinit();
	wait_enter();
	set_echo_enabled(0);
	return 1;
}
int enter_shell(){
	char* argv[] = {
		"sash",
		"-a",
		"-p",
		"sash> ",
		"-i",
		NULL
	};
	clear_screen();
	reset_pointer();
	fseek(stdin, 0, SEEK_END);
	set_echo_enabled(1);
	int s = sash_main((sizeof(argv)/sizeof(void*))-1, argv);
	set_echo_enabled(0);
	return s;
}
int exec_params(char* exe_path, char* params){
	int exec_argc = 1;
	char* exec_argv[16] = {
		exe_path, NULL
	};

	if(!NULARRSTR(params)){
		int argc = 0;
		char** argv = NULL;
		if(!makeArgs(params, &argc, (const char***)&argv)){
			log_center_whitebg("cannot parse params");
			wait_enter();
			return 1;
		}
		for(int i=0; i<argc; ++i){
			exec_argv[exec_argc++] = argv[i];
			if(exec_argc>=16-1){
				l_inf("params out of length");
				break;
			}
		}
		exec_argv[exec_argc] = NULL;
	}

	return execv(exe_path, exec_argv);
}
void do_entry(int curr){
	menu_entry curr_entry = menu_entries[curr];
	switch(curr_entry.type){
		case entry_type_init:
			// TODO params
			set_echo_enabled(1);
			int ret = exec_params(curr_entry.exe_path, curr_entry.param_str);
			// int ret = execl(curr_entry.exe_path, "init", NULL);
			set_echo_enabled(0);
			if(ret<0){
				log_center_whitebg("Cannot execute INIT");
			}
			break;
		case entry_type_kexec:
			// TODO impl
			log_center_whitebg("IMPLEMENT THIS FUNCTION FOR ENTRY_TYPE_KEXEC");
			break;
		case entry_type_shell:
			sash_execCommand(curr_entry.cmdline);
			log_center_whitebg("Shell script may fail");
			break;
	}
	wait_enter();
}
void handle_event_loop(){
	if(menu_entries_count<=0) return;

	// input
	int key = evinput_get();
	switch(key){
		case KEY_UP:
		case KEY_VOLUMEUP:
			menu_sel_rel(-1);
			break;
		case KEY_DOWN:
		case KEY_VOLUMEDOWN:
			menu_sel_rel(1);
			break;
		case KEY_CONFIRM:
			// TODO enter selection
			// TODO _deinit
			clear_screen();
			reset_pointer();
			// set_echo_enabled(1);
			do_entry(menu_sel_curr);
			// set_echo_enabled(0);
			printUI();
			break;
		case KEY_R:
			_deinit();
			l_inf("Rebooting");
			reboot(RB_AUTOBOOT);
			break;
		case KEY_C:{
			// vfork() would not pause current process on ChromeOS kernel
			pid_t fpid = fork();
			if(fpid<0){
				l_inf("cannot fork");
			}else if(fpid==0){
				int r = enter_shell();
				// _deinit();
				exit(r);
			}else{
				int st=0;
				// int ret = wait(&st);
				int ret = waitpid(fpid, &st, WUNTRACED);
				if(ret>0 && !(st&0x7f)){
					// l_inf("shell exited normally");
				}else{
					if(ret>0){
						l_inf("shell exited with code %d", st&0x7f);
					}else{
						l_inf("shell may not exit");
					}
					nano_sleep(1000000000);	// 1s
				}
				going_exit = 0;
				evinput_reload();
				// TODO intergate with fbpads to redirect I/O
				set_echo_enabled(0);
				printUI();
			}
			break;
		}
	}


	// output
	if(sel_changed){
		printUI();
		sel_changed = 0;
	}
	lbml_lazy_flush();
}

int main(void){
	evinput_init();
	int stat_libfbpads = 0;
	#ifdef LBML_USE_EXTFB
		if(stat_libfbpads = libfbpads_init(FB0_PATH)){
			l_err("cannot init fbconsole");
			// deinit();
		}
	#endif
	if(parse_menu_config("/lbml.toml")){
		log_center_whitebg("cannot parse config file");
		wait_enter();
		deinit();
	}

	#ifdef LBML_USE_EXTFB
		if(LBML_HAS_FB() && stat_libfbpads) deinit();
		// unblank screen
		if(LBML_HAS_FB()) ioctl(fb_fd(), FBIOBLANK, FB_BLANK_UNBLANK);
	#endif

	// if(config_device_mtk){
		// struct fb_var_screeninfo var = fb_vscreeninfo();
		// int off = var.yres_virtual/var.yres-1;
		// // var.yoffset = var.yres * off;
		// fb_setypan(off);
	// }

	// signal(SIGINT, onSIGINT)
	static struct sigaction _sigact;
	memset(&_sigact, 0, sizeof(_sigact));
	_sigact.sa_sigaction = onSIGINT;
	_sigact.sa_flags = SA_SIGINFO;
	sigaction(SIGINT, &_sigact, NULL);

	set_echo_enabled(0);
	clear_screen();

	if(config_show_splash){
		if(display_logo()){
			l_err("logo display interrupted");
			// deinit();
		}
	}

	printUI();
	while(1){
		if(going_exit) break;
		handle_event_loop();
		// nano_sleep(EVINPUT_WAIT_PERIOD);	// 10ms
	}
	deinit();
	return 0;
}
