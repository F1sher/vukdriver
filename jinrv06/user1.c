#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <curses.h>
#include <term.h>
#include <termios.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <time.h>


#define SIMPLE_DEVICE "/dev/simple"
#define LCH	1800
#define RCH	900
#define	ARRAY_EL	4098
#define MAX_FILE_NAME	50
#define MAX_NLH_MSG	5
#define NETLINK_NITRO 17

#define	ST_ROW	12
#define FILE_ROW	(ST_ROW+2)
#define	CLEAR_ROW	(ST_ROW+2)
#define EXSPOS_ROW	(ST_ROW+10)
#define INTES_ROW	(ST_ROW+5)
#define TIMER_ROW	(ST_ROW+7)
#define	STATUS_ROW	(ST_ROW+21)
#define	DESCR_ROW	(ST_ROW+13)
#define MAX_ROW		(6)


static int exspos_time, timer_end_pos;
static int start_flag, rflag = 0;
static char *ch;
static struct termios initial_settings, new_settings;
static char *input_file, *output_file;
static int read_spk_ret, save_spk_ret, clear_spk_ret, time_set_ret, stop_ret;
WINDOW *win1, *win2, *wind, *wincl, *winst, *winclr, *winint, *winclock, *wintimer, 
*winexpos, *winstat, *windescr;
static int fd_in, fd_out;
int compres=1;
char *choices[] =
{
    " Save spk",
    " Read spk",
    " Clear spk",
    " Set time",
    " Start",
    " Stop",
    0,
};


void dataprint(const long int rdata[], int left)
{
		static int selected_row=0;
		static int intens_prev=0;
		int start_screenrow = 0, start_screencol = 10;
		static long int A = 0, B = 0, C;
		char x;
		int i, xpos=1, ypos=1, j, min_j;
		static int l = 0;
		struct tm *newtime;
		long ltime;
		
		box(wind, '|', '-');
		wmove(wind, 1, 1);
		for(i=left; i<left+128; i++) {
			min_j=compres*(i-left);
			if((i+min_j+compres-1)>=4095)
				break;
			C = 0;
			for(j=min_j; j<=compres*(i-left+1)-1; j++)
				C+=rdata[left+j];
			wmove(wind, ypos, xpos+=6);
			wprintw(wind, " %d ", C);
			if(xpos>=(COLS-10)) {
				xpos = 1;
				ypos++;
				wmove(wind, ypos, 0);
				wprintw(wind, "%d |", ypos-1);
			}
		}
		xpos=7; i=0; ypos++;
		wmove(wind, ypos, xpos);
		while(xpos<(COLS)) {
			i++;
			wprintw(wind, " %d ", i);
			wmove(wind, ypos, xpos+=6);
		}
			

		B = A;
		A = rdata[4096];
		wmove(wind, ST_ROW, 1);
		wprintw(wind, "Ch from");
		wattron(wind ,A_BOLD);
		wprintw(wind, " %d", left);
		wattroff(wind,A_BOLD);
		wprintw(wind, " to");
		wattron(wind ,A_BOLD);
		wprintw(wind, " %d\t", (left+128)*compres);
		wattroff(wind,A_BOLD);
		wprintw(wind, "char_1 =");
		wattron(wind ,A_BOLD);
		wprintw(wind, " %2c", ch[0]);
		wattroff(wind,A_BOLD);
		wprintw(wind, "char_2 =");
		wattron(wind ,A_BOLD);
		wprintw(wind, " %2c", ch[1]);
		wattroff(wind,A_BOLD);
		wprintw(wind, "TIME =");
		wclrtoeol(wind);
		wattron(wind ,A_BOLD);
		wprintw(wind, " %d", rdata[4097]);
		wattroff(wind,A_BOLD);
		box(winst, '|', '-');
		wmove(winst, 1, 0);
		wprintw(winst, "INPUT FILE:");
		wattron(winst, A_STANDOUT);
		wprintw(winst, " %20s", input_file);
		wattroff(winst, A_STANDOUT);
		wprintw(winst, " | OUTPUT FILE:");
		wattron(winst, A_STANDOUT);
		wprintw(winst, " %20s\n", output_file);
		wattroff(winst, A_STANDOUT);
		
		wmove(win1, CLEAR_ROW, 0);
//		wprintw(win1, "%s\n", "pr. F5+ALT toCLEAR");

		box(winint, '|', '-');
		wmove(winint, 1, 0);
		wclrtoeol(winint);
		wprintw(winint, "INTESTIVITY %4d | ", rdata[4096]-intens_prev);
		intens_prev=rdata[4096];
		
		time(&ltime);  /*взять время в секундах*/
		newtime=localtime(&ltime);
		wprintw(winint, "Current time %02d:%02d:%02d | ", newtime->tm_hour, 
newtime->tm_min, newtime->tm_sec);

		wprintw(winint, "Compression = %d chs", compres);
		
		box(winexpos, '|', '-');
		wmove(winexpos, 1, 0);
		wprintw(winexpos, "EXSPOS TIME = %5d sec; SET TIME = \n", exspos_time);		
		
		box(wintimer, '|', '-');
		wmove(wintimer, 1, 0);	
		wmove(wintimer, 1, exspos_time/10+1);
		waddch(wintimer, '|');
		mvwprintw(wintimer, 1, exspos_time/10+2, "%d", rdata[4097]);
		wclrtoeol(wintimer);
		if(rdata[4097] % 10 == 0 && rdata[4097] != 1000 && rdata[4097] != 0) {
			wmove(wintimer, 1, l++);
			waddch(wintimer, '>');
		}
		
		wmove(win1, STATUS_ROW, 0);
		wclrtoeol(win1);
		
		box(windescr, '|', '-');
		wmove(windescr, 1, 0);
		if (ch[0] == KEY_DOWN) {
			if (selected_row == 0)
				selected_row = MAX_ROW - 1;
			else
				selected_row--;
		}
		if (ch[0] == KEY_UP) {
			if (selected_row == (MAX_ROW - 1))
				selected_row = 0;
			else
				selected_row++;
		}
		draw_menu(choices, selected_row, start_screenrow, start_screencol);
		if(ch[0] == 'S') {save_spk_ret = save_spk(rdata);}
		if(ch[0] == 'R') {read_spk_ret = read_spk(rdata); rflag = 1;}
		if(ch[0] == 'C') {clear_spk_ret = clear_spk(rdata);}
		if(ch[0] == 'T') {time_set_ret = set_time(1);} //задать абсолютное вермя
		if(ch[0] == 'G') start(rdata); //старт экспозиции
		if(ch[0] == 'F') {stop_ret = set_time(0);} // стоп экспозиции
//		wprintw(windescr, "%s", "to SAVE SPK press `+s, to READ SPK press `+r, to CLEAR SPK prees `+c, to SET TIME `+t, to START `+g, to STOP `+f");
		
		if(read_spk_ret == 1) { wprintw(win1, "read ok"); read_spk_ret = 0;}
		else if(read_spk_ret == -1) { wprintw(win1, "read error"); read_spk_ret = 0;}
		
		if(save_spk_ret == 1) { wprintw(win1, "save ok"); save_spk_ret = 0;}
		else if(save_spk_ret == -1) { wprintw(win1, "save error"); save_spk_ret = 0;}
		
		if(clear_spk_ret == 1) wprintw(win1, "clear ok");
		else if(clear_spk_ret == -1) wprintw(win1, "clear error");
		
		if(time_set_ret == 1) wprintw(win1, "set time ok");
		else if(time_set_ret == -1) wprintw(win1, "set time error");
		
		if(stop_ret == 1) wprintw(win1, "stop ok");
		else if(stop_ret == -1) wprintw(win1, "stop error");
		
		wrefresh(win1);
		wrefresh(wind);
		wrefresh(winst);
		wrefresh(winint);
		wrefresh(wintimer);
		wrefresh(winexpos);
		wrefresh(windescr);
		wrefresh(wincl);

		ch[0] = 0;
		ch[1] = 0;
}

void init_keyboard();
void close_keyboard();
int kbhit();
void close_program();

int main()
{
	int i, l=0;
	long int *rdata;
	int left = LCH;
	int retk;
	char keyp;

	ch = (char *)malloc(2*sizeof(char));
	input_file = (char *)malloc(MAX_FILE_NAME*sizeof(char));
	output_file = (char *)malloc(MAX_FILE_NAME*sizeof(char));
	strcpy(input_file, SIMPLE_DEVICE);
	
	initscr();
	init_keyboard();
	rdata = (long int *)malloc(ARRAY_EL*sizeof(long int));
	
    fd_in = open(SIMPLE_DEVICE, O_RDWR);
    if (fd_in < 0) {
        perror("error in device");
        exit(1);
    }

	win1 = newwin(0, 0, 0, 0);
	wind = newwin(ST_ROW+2, COLS, 0, 0);
	winst = newwin(3, COLS, FILE_ROW, 0);
	winint = newwin(3, COLS, INTES_ROW, 0);
	winexpos = newwin(3, COLS, EXSPOS_ROW, 0);
	wintimer = newwin(3, COLS, TIMER_ROW, 0);
	windescr = newwin(8, COLS, DESCR_ROW, 0);
	wincl = newwin(3, COLS, STATUS_ROW, 0);
	box(wincl, '|', '-');

//	wmove(win1, TIMER_ROW, timer_end_pos+1);
	read(fd_in, rdata, ARRAY_EL*sizeof(long int));
	timer_end_pos = exspos_time = 0;

	while(ch[0] != '' && ch[1] != '') {
		dataprint(rdata, left);
		if((retk = kbhit()) != 0) {
			if(retk == 1) {
				if (ch[0] == ']') left++;
				else if (ch[0] == '[') left--;
			}
			else if(retk == 2)
					if(ch[0] == '`')  {
						if(ch[1] == 's') {save_spk_ret = 
save_spk(rdata);}
						if(ch[1] == 'R') {read_spk_ret = 
read_spk(rdata); rflag = 1;}
						if(ch[1] == 'C') {clear_spk_ret = 
clear_spk(rdata);}
						if(ch[1] == 't') {time_set_ret = 
set_time(1);} //задать абсолютное вермя
						if(ch[1] == 'G') start(rdata); 
//старт экспозиции
						if(ch[1] == 'F') {stop_ret = set_time(0);} 
// стоп экспозиции
					}
		} 
		sleep(1);
		if(rflag == 0)
			read(fd_in, rdata, ARRAY_EL*sizeof(long int));
	}

    
	close_program();	

	return 0;
}

void close_program()
{
    close_keyboard();
	delwin(win1);
	delwin(wind);
	delwin(winst);
	delwin(winint);
	delwin(winclock);
	delwin(windescr);
	delwin(wincl);

    close(fd_in);
    endwin();
}

int kbhit()
{
	int nread;

	new_settings.c_cc[VMIN] = 0;
	tcsetattr(0, TCSANOW, &new_settings);
	nread = read(0, ch, 2);
	new_settings.c_cc[VMIN] = 1;
	tcsetattr(0, TCSANOW, &new_settings);
	
	if(nread == 1) {
		return 1;
	}
	if(nread == 2) {
		return 2;
	}
	
	return 0;
}

void init_keyboard()
{
	tcgetattr(0, &initial_settings);
	new_settings = initial_settings;
	new_settings.c_lflag &= ~ICANON;
	new_settings.c_lflag &= ~ECHO;
	new_settings.c_lflag &= ~ISIG;
	new_settings.c_cc[VMIN] = 1;
	new_settings.c_cc[VTIME] = 0;
	tcsetattr(0, TCSANOW, &new_settings);
}

void close_keyboard()
{
	tcsetattr(0, TCSANOW, &initial_settings);
}

int save_spk(const long int *rdata)
{	
	new_settings.c_lflag &= ICANON;
	new_settings.c_lflag &= ECHO;

	mvwscanw(winst, 1, 48, "%s", output_file);
	if(output_file == NULL || output_file[0] != '/') {
		output_file = "ERROR WAY";
		return -1;
	}
		
	fd_out = open(output_file, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR);
    if (fd_out < 0) {
        perror("error in device");
        exit(1);
	close_program();
    }
    write(fd_out, rdata, 4098*sizeof(long int));
    
    close(fd_out);
    ch[0] = 0;
    ch[1] = 0;
    new_settings.c_lflag &= ~ICANON;
	new_settings.c_lflag &= ~ECHO;
	
	return 1;
}

int read_spk(long int *rdata)
{
	close(fd_in);
	int fd_in;
	
	new_settings.c_lflag &= ICANON;
	new_settings.c_lflag &= ECHO;

	wrefresh(winst);
	mvwscanw(winst, 1, 12, "%s", input_file);
	if(input_file == NULL || input_file[0] != '/') {
		input_file = "ERROR WAY";
		ch[0] = 0;
		ch[1] = 0;
		new_settings.c_lflag &= ~ICANON;
		new_settings.c_lflag &= ~ECHO;
		return -1;
	}
		
	fd_in = open(input_file, O_RDWR);
    if (fd_in < 0) {
        perror("error in device");
        return -1;
	close_program();
    }
    read(fd_in, rdata, 4098*sizeof(long int));
    
 //   close(fd_in);
    ch[0] = 0;
    ch[1] = 0;
    new_settings.c_lflag &= ~ICANON;
	new_settings.c_lflag &= ~ECHO;
	
	return 1;
}

int clear_spk(long int *rdata)
{
	int i;
	char status[2];
	
	new_settings.c_lflag &= ICANON;
	new_settings.c_lflag &= ECHO;

	mvwprintw(wincl, 1, 1, "Are you sure? (type y or n)");
	wrefresh(wincl);
	mvwscanw(wincl, 1, sizeof("Are you sure? (type y or n)"), "%c", status);
	if(status[0] != 'y') {
		ch[0] = 0;
		ch[1] = 0;
		new_settings.c_lflag &= ~ICANON;
		new_settings.c_lflag &= ~ECHO;
		wmove(wincl, 1, 0);
		wclrtoeol(wincl);
		mvwprintw(wincl, 1, 1, "clear error");
		return 1;
	}
	
	for(i=0; i<=ARRAY_EL-1; i++)
		rdata[i]= 0;
		
/*	fd_in = open(input_file, O_RDWR);
    	if (fd_in < 0) {
        	perror("error in device");
        	exit(1);
    	} */
    	if (write(fd_in, rdata, (ARRAY_EL)*sizeof(long int)) < 0) {
		wmove(wincl, 1, 0);
		wclrtoeol(wincl);
		mvwprintw(wincl, 1, 1, "write error");
		close_program();
		perror("write error");
		exit(1);
	}
//	close(fd_in);
	
	wmove(wincl, 1, 0);
	wclrtoeol(wincl);
	mvwprintw(wincl, 1, 1, "clear ok");
	for(i=1; i<=ST_ROW-3; i++) {
		wmove(wind, i, 1);
		wclrtoeol(wind);
	}
   	ch[0] = 0;
 	ch[1] = 0;
   	new_settings.c_lflag &= ~ICANON;
	new_settings.c_lflag &= ~ECHO;
	
	return 0;
}

void reverse(char s[])
{
     int i, j;
     char c;
 
     for (i = 0, j = strlen(s)-1; i<j; i++, j--) {
         c = s[i];
         s[i] = s[j];
         s[j] = c;
     }
}

void itoa(int n, char s[])
{
     int i, sign;
 
     if ((sign = n) < 0)  /* записываем знак */
         n = -n;          /* делаем n положительным числом */
     i = 0;
     do {       /* генерируем цифры в обратном порядке */
         s[i++] = n % 10 + '0';   /* берем следующую цифру */
     } while ((n /= 10) > 0);     /* удаляем */
     if (sign < 0)
         s[i++] = '-';
     s[i] = '\0';
     reverse(s);
}

int set_time(int flag)
{
	struct tm *newtime;
	long ltime;
	int hs, mins, secs;
	int need_hs, need_mins, need_secs;
	char buffer[MAX_NLH_MSG], buffer_secs[MAX_NLH_MSG];
	struct sockaddr_nl s_nladdr, d_nladdr;
	struct msghdr msg ;
	struct nlmsghdr *nlh=NULL ;
	struct iovec iov;
	int fd;
	
	if(flag == 0) need_secs = 0;
	else {
		new_settings.c_lflag &= ICANON;
		new_settings.c_lflag &= ECHO;
		mvwscanw(winexpos, 1, 36, "%2d", &hs); mvwprintw(win1, EXSPOS_ROW, 
38, ":");
		mvwscanw(winexpos, 1, 39, "%2d", &mins); mvwprintw(win1, EXSPOS_ROW, 41, 
":");
		mvwscanw(winexpos, 1, 42, "%2d", &secs);
		ch[0] = 0;
		ch[1] = 0;
		new_settings.c_lflag &= ~ICANON;
		new_settings.c_lflag &= ~ECHO;
	//	printf("Time to alarm\t%02d:%02d:%02d\n", hs, mins, secs);
		time(&ltime);  /*взять время в секундах*/
		newtime=localtime(&ltime); 
	//    printf("Current time\t%02d:%02d:%02d\n", newtime->tm_hour, newtime->tm_min, newtime->tm_sec);
		need_hs = hs - newtime->tm_hour;
		need_mins = mins - newtime->tm_min;
		need_secs = secs - newtime->tm_sec;
		if(need_hs < 0) { mvwprintw(win1, EXSPOS_ROW, 36, "ERR TIME SET"); return -1;}
	//    printf("Time to need\t%02d:%02d:%02d\n", need_hs, need_mins, need_secs);
		need_secs = 60*60*need_hs + 60*need_mins + need_secs;
	//	printf("hs = %d\n", need_secs);
		if(need_secs < 0) { mvwprintw(win1, EXSPOS_ROW, 36, "ERR TIME SET"); return -1;}
	}
	
	timer_end_pos = exspos_time = need_secs;
	
	itoa(need_secs, buffer_secs);
	strcpy(buffer, buffer_secs);

	fd=socket(AF_NETLINK, SOCK_RAW, NETLINK_NITRO);

	/* source address */
	memset(&s_nladdr, 0 ,sizeof(s_nladdr));
	s_nladdr.nl_family= AF_NETLINK ;
	s_nladdr.nl_pad=0;
	s_nladdr.nl_pid = getpid();
	bind(fd, (struct sockaddr*)&s_nladdr, sizeof(s_nladdr));

	/* destination address */
	memset(&d_nladdr, 0 ,sizeof(d_nladdr));
	d_nladdr.nl_family= AF_NETLINK ;
	d_nladdr.nl_pad=0;
	d_nladdr.nl_pid = 0; /* destined to kernel */

	/* Fill the netlink message header */
	nlh = (struct nlmsghdr *)malloc(MAX_NLH_MSG);
	memset(nlh , 0 , MAX_NLH_MSG);
	strcpy(NLMSG_DATA(nlh), buffer);
	nlh->nlmsg_len =100;
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_flags = 1;
	nlh->nlmsg_type = 0;

	/*iov structure */
	iov.iov_base = (void *)nlh;
	iov.iov_len = nlh->nlmsg_len;

	/* msg */
	memset(&msg,0,sizeof(msg));
	msg.msg_name = (void *) &d_nladdr ;
	msg.msg_namelen=sizeof(d_nladdr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	sendmsg(fd, &msg, 0);

	close(fd);

	return 0;
}

int start(long int *rdata)
{
	start_flag = 1;
	clear_spk(rdata);
}

void draw_menu(char *options[], int current_highlight, int start_row, int start_col)
{
    int current_row = 0;
    char **option_ptr;
    char *txt_ptr;

    option_ptr = options;
    while (*option_ptr) {
	if (current_row == current_highlight) wattron(windescr, A_STANDOUT);
	txt_ptr = options[current_row];
	txt_ptr++;
	mvwprintw(windescr, start_row + current_row+1, start_col, "%s", txt_ptr);
	if (current_row == current_highlight) wattroff(windescr, A_STANDOUT);
	current_row++;
	option_ptr++;
    }
 //   wrefresh(windescr);
}
