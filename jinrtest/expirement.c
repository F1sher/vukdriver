#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include <cairo.h>
#include <gtk/gtk.h>
#include <math.h>
#include <sys/socket.h>
#include <linux/netlink.h>

#define EPS 0.00001

#define MAX_CH	4096
#define NETLINK_NITRO 17
#define MAX_PAYLOAD 2048

#define READ_ONLINE_PERIOD 20000

typedef struct {
	int timespk[12][MAX_CH];
	int energyspk[4][MAX_CH];
} dataspk;

extern struct {
	int status;	
	
	char dir[60];
	char start[80];
	int expos;
	int time;
	int dtime;
	double intensity[4];
	
	int LD[8];
	int RD[8];
	
	guint tag_get_online_cycle;
} infostruct;

struct {
	GtkWidget **entry_range;
  	GtkWidget *entry_expostime;
  	GtkWidget *entry_time;
} startinfo;

struct true_exp_data {
	int status;
	
	int time, expos;
	
	int LD[8];
	int RD[8];
};

void save_conf_file();
void get_online_data_cycle(gpointer data);
void clock_func(gpointer data);

extern dataspk *spectras;
int COMP;
double zero_R;
double *aniz;
double *coincidence[2];

GtkWidget *label_info;
GtkWidget *spinner_get_data;
GtkTextBuffer *info_buffer;
gchar *path_to_save_spk;

char *convert_time_to_hms(long int time_in_seconds)
{
	int h, m, s;
	
	printf("%ld\n", time_in_seconds);
	
	h = time_in_seconds/3600;
	m = (time_in_seconds%3600)/60;
	s = ((time_in_seconds%3600)%60);
	
	return g_strdup_printf("%d:%d:%d", h, m, s);
}

long int convert_time_to_seconds(const char *hms)
{
	long int s;
	
	s = 3600*10*hms[0] + 3600*hms[1] + 10*60*hms[3] + 60*hms[4] + 10*hms[6] + hms[7];
	
	return s;
}

long int compare_dates(const char *date)
{
	printf("date = %s\n", date);
	// date format %d/%m/%y %T 00/00/00 00:00:00
	// return current time - date time in seconds
	
	int i;
	int day, month, year;
	int hour, minute, second;
	long int accumulation_time_in_s = infostruct.time;
	
	if (strcmp(date, "Stoped") == 0) return 1;
	
	day = 10*(date[0]-'0') + (date[1]-'0');
	month = 10*(date[3]-'0') + (date[4]-'0');
	year = 10*(date[8]-'0') + (date[9]-'0');
	
//	printf("date9 = %c; date10 = %c; %d\n", date[9], date[10], 10*date[9] + date[10]);
	hour = 10*(date[9]-'0') + (date[10]-'0') + accumulation_time_in_s/3600;
	minute = 10*(date[12]-'0') + (date[13]-'0') + (accumulation_time_in_s%3600)/60;
	second = 10*(date[15]-'0') + (date[16]-'0') + ((accumulation_time_in_s%3600)%60);
	
//	printf("acc_time = %s;  accumulation_time[6] = %d, accumulation_time[7] = %d\n", accumulation_time, (int)(accumulation_time[0]-'0'), (int)(accumulation_time[1]-'0'));
	
	char *time_str = (char *)malloc(40*sizeof(char));
	time_t curtime;
	struct tm *loctime;
	int curday, curmonth, curyear;
	int curhour, curminute, cursecond;
	
	curtime = time(NULL);
	loctime = localtime(&curtime);
	strftime(time_str, 40, "%d/%m/%y %T", loctime);
	
	curday = 10*(time_str[0]-'0') + (time_str[1]-'0');
	curmonth = 10*time_str[3] + time_str[4];
	curyear = 10*time_str[6] + time_str[7];
		
	curhour = 10*(time_str[9]-'0') + (time_str[10]-'0');
	curminute = 10*(time_str[12]-'0') + (time_str[13]-'0');
	cursecond = 10*(time_str[15]-'0') + (time_str[16]-'0'); 
	
	printf("day = %d, curday = %d\n", day, curday);
	
	if(curday == day) {
		printf("slu4ai 1, curh=%d h=%d; curm=%d m=%d; curs=%d s=%d\n", curhour, hour, curminute, minute, cursecond, second);
		return ( (long int)(3600*(curhour-hour)+60*(curminute-minute)+(cursecond-second)) );
	}
	else {
		printf("slu4ai 2, curh=%d h=%d; curm=%d m=%d; curs=%d s=%d\n", curhour, hour, curminute, minute, cursecond, second);
		return ( (long int)( 24*3600*(day-curday-1) + 3600*(24-curhour) + 3600*(hour) + 60*(minute) - 60*(curminute) ) );
	}
	
	free(time_str);
}

void trueread_nl_msg(const char *msg, struct true_exp_data *struct_to_info)
{	
	int i;
	int truetime = 0;
	int expos_truetime = 0;
	
	int status_pos = (sizeof("STATUS=")-1)/sizeof(char);
	int time_pos = status_pos + 1 + 1 + (sizeof("TIME=")-1)/sizeof(char);
	int expos_pos = time_pos + 6 + 1 + (sizeof("EXPOS=")-1)/sizeof(char);
	int LD_pos[8], RD_pos[8];
	
	struct_to_info->status = msg[status_pos]-'0';
	printf("status_pos = %d; status = %c; status = %d\n", status_pos, msg[status_pos], struct_to_info->status);
	
	truetime = 1e5*(msg[time_pos]-'0') + 1e4*(msg[time_pos+1]-'0') + 1e3*(msg[time_pos+2]-'0') + 1e2*(msg[time_pos+3]-'0') + 1e1*(msg[time_pos+4]-'0') + 1e0*(msg[time_pos+5]-'0');
	struct_to_info->time = truetime;
	printf("time_pos = %d; truetime = %d\n", time_pos, truetime);
	
	expos_truetime = 1e5*(msg[expos_pos]-'0') + 1e4*(msg[expos_pos+1]-'0') + 1e3*(msg[expos_pos+2]-'0') + 1e2*(msg[expos_pos+3]-'0') + 1e1*(msg[expos_pos+4]-'0') + 1e0*(msg[expos_pos+5]-'0');
	struct_to_info->expos = expos_truetime;
	printf("expos_pos = %d; expos_truetime = %d\n", expos_pos, expos_truetime);
	
	for(i=0; i<8; i+=2) {
		LD_pos[i] = expos_pos + 6 + 1 + 1 + 22*i/2;
		RD_pos[i] = expos_pos + 6 + 1 + 1 + 10 + 22*i/2;
	}
	
	for(i=1; i<8; i+=2) {
		LD_pos[i] = expos_pos + 6 + 1 + 1 + 22*(i-1)/2 + 5;
		RD_pos[i] = expos_pos + 6 + 1 + 1 + 10 + 22*(i-1)/2 + 5;
	}
	
	for(i=0; i<8; i++) {
		(struct_to_info->LD)[i] = 1e3*(msg[LD_pos[i]]-'0') + 1e2*(msg[LD_pos[i]+1]-'0') + 1e1*(msg[LD_pos[i]+2]-'0') + 1e0*(msg[LD_pos[i]+3]-'0');
		printf("%d - %d\n", LD_pos[i], (struct_to_info->LD)[i]);
		(struct_to_info->RD)[i] = 1e3*(msg[RD_pos[i]]-'0') + 1e2*(msg[RD_pos[i]+1]-'0') + 1e1*(msg[RD_pos[i]+2]-'0') + 1e0*(msg[RD_pos[i]+3]-'0');
		printf("%d - %d\n", RD_pos[i], (struct_to_info->RD)[i]);
	}
}

void send_msg_to_kernel(const char *msg_to_kernel)
{
	printf("%s\n", msg_to_kernel);
	
	struct true_exp_data test_struct;
	
	trueread_nl_msg(msg_to_kernel, &test_struct);
	
	static gboolean first_execution = TRUE;
	 
	static struct sockaddr_nl s_nladdr, d_nladdr;
	static struct msghdr msg ;
	static struct nlmsghdr *nlh=NULL ;
	static struct iovec iov;
	static int fd;
	 
	 fd=socket(AF_NETLINK, SOCK_RAW, NETLINK_NITRO);
	 
	 if(first_execution) {
		// source address
		memset(&s_nladdr, 0, sizeof(s_nladdr));
		s_nladdr.nl_family = AF_NETLINK ;
		s_nladdr.nl_pad = 0;
		s_nladdr.nl_pid = getpid();
		bind(fd, (struct sockaddr*)&s_nladdr, sizeof(s_nladdr));

		// destination address
		memset(&d_nladdr, 0 ,sizeof(d_nladdr));
		d_nladdr.nl_family = AF_NETLINK ;
		d_nladdr.nl_pad = 0;
		d_nladdr.nl_pid = 0; // destined to kernel
	}
	
	// Fill the netlink message header
	nlh = (struct nlmsghdr *)malloc(1024);
	memset(nlh, 0, 1024);
	strcpy(NLMSG_DATA(nlh), msg_to_kernel);
	nlh->nlmsg_len = 1024;
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_flags = 1;
	nlh->nlmsg_type = 0;

	// iov structure
	iov.iov_base = (void *)nlh;
	iov.iov_len = nlh->nlmsg_len;
	
	// msg
	memset(&msg, 0, sizeof(msg));
	msg.msg_name = (void *)&d_nladdr ;
	msg.msg_namelen = sizeof(d_nladdr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	sendmsg(fd, &msg, 0);
	
	first_execution = FALSE;
}

void start_expos_callback(GtkWidget *dialog, gint response, gpointer data)
{
	int i;
	int time_s[3];
	char *msg = (char *)malloc(1024*sizeof(char));
	char *range_msg[4];
	
	for(i=0; i<4; i++)
		range_msg[i] = (char *)malloc(64*sizeof(char));
	
	switch (response)
	{
    case GTK_RESPONSE_ACCEPT: {
		infostruct.LD[0] = (int)atof( gtk_entry_get_text(GTK_ENTRY(startinfo.entry_range[0])) );
		infostruct.LD[1] = (int)atof( gtk_entry_get_text(GTK_ENTRY(startinfo.entry_range[1])) );
		infostruct.LD[2] = (int)atof( gtk_entry_get_text(GTK_ENTRY(startinfo.entry_range[4])) );
		infostruct.LD[3] = (int)atof( gtk_entry_get_text(GTK_ENTRY(startinfo.entry_range[5])) );
		infostruct.LD[4] = (int)atof( gtk_entry_get_text(GTK_ENTRY(startinfo.entry_range[8])) );
		infostruct.LD[5] = (int)atof( gtk_entry_get_text(GTK_ENTRY(startinfo.entry_range[9])) );
		infostruct.LD[6] = (int)atof( gtk_entry_get_text(GTK_ENTRY(startinfo.entry_range[12])) );
		infostruct.LD[7] = (int)atof( gtk_entry_get_text(GTK_ENTRY(startinfo.entry_range[13])) );
		
		infostruct.RD[0] = (int)atof( gtk_entry_get_text(GTK_ENTRY(startinfo.entry_range[2])) );
		infostruct.RD[1] = (int)atof( gtk_entry_get_text(GTK_ENTRY(startinfo.entry_range[3])) );
		infostruct.RD[2] = (int)atof( gtk_entry_get_text(GTK_ENTRY(startinfo.entry_range[6])) );
		infostruct.RD[3] = (int)atof( gtk_entry_get_text(GTK_ENTRY(startinfo.entry_range[7])) );
		infostruct.RD[4] = (int)atof( gtk_entry_get_text(GTK_ENTRY(startinfo.entry_range[10])) );
		infostruct.RD[5] = (int)atof( gtk_entry_get_text(GTK_ENTRY(startinfo.entry_range[11])) );
		infostruct.RD[6] = (int)atof( gtk_entry_get_text(GTK_ENTRY(startinfo.entry_range[14])) );
		infostruct.RD[7] = (int)atof( gtk_entry_get_text(GTK_ENTRY(startinfo.entry_range[15])) );
		
		infostruct.status = 2;
		
		//time format hh:mm:ss
		sscanf(gtk_entry_get_text(GTK_ENTRY(startinfo.entry_time)), "%d:%d:%d", &time_s[0], &time_s[1], &time_s[2]);
		printf("time_s = %d\n", time_s[0]);
		infostruct.time = 3600*time_s[0] + 60*time_s[1] + time_s[2];
		printf("hms = %s\n", convert_time_to_hms(infostruct.time));
		
		infostruct.expos = (int)atof( gtk_entry_get_text(GTK_ENTRY(startinfo.entry_expostime)) );
		
		for(i=0; i<4; i++)
			sprintf(range_msg[i], "{%04d-%04d;%04d-%04d}", infostruct.LD[2*i], infostruct.LD[2*i+1], infostruct.RD[2*i], infostruct.RD[2*i+1]);
	
		for(i=0; i<4; i++)
			printf("range_msg = %s\n", range_msg[i]);

		sprintf(msg, "STATUS=%d\nTIME=%06d\nEXPOS=%06d\n%s\n%s\n%s\n%s\n", infostruct.status,\
						infostruct.time,\
						infostruct.expos,\
						range_msg[0],\
						range_msg[1],\
						range_msg[2],\
						range_msg[3]);
						
		send_msg_to_kernel(msg);
		
		time_t curtime;
		struct tm *loctime;
		GtkTextIter iter, end;

		curtime = time(NULL);
		loctime = localtime(&curtime);
		strftime(infostruct.start, 80, "%d/%m/%y %T", loctime);

		gtk_text_buffer_get_iter_at_line(info_buffer, &iter, 2);
		gtk_text_buffer_get_iter_at_line_offset(info_buffer, &end, 3, 0);
		gtk_text_buffer_delete(info_buffer, &iter, &end);
		gtk_text_buffer_insert_with_tags_by_name(info_buffer, &iter, 
			g_strdup_printf("Start: %s\n", infostruct.start), -1, "bold", NULL);
		
		gtk_text_buffer_get_iter_at_line(info_buffer, &iter, 4);
		gtk_text_buffer_get_iter_at_line_offset(info_buffer, &end, 5, 0);
		gtk_text_buffer_delete(info_buffer, &iter, &end);
	
		gtk_text_buffer_insert_with_tags_by_name(info_buffer, &iter, 
			g_strdup_printf("Accumulation: %s\n", convert_time_to_hms(infostruct.time)), -1, "bold", NULL);
		
		save_conf_file();
	}
       break;
    
    case GTK_RESPONSE_CANCEL:
		break;
    default:
       break;
	}
	gtk_widget_destroy (dialog);	
}

void start_expos(GtkWidget *widget, gpointer window)
{
	int i, j;
	GtkWidget *dialog, *content_area;
  	GtkWidget *grid;

    dialog = gtk_dialog_new_with_buttons ("Set expirement details",
                                         window,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         NULL);
	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    
	grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
	
	gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Set energy range:"), 1, 0, 5, 1);
	for(i=0; i<4; i++)
		gtk_grid_attach(GTK_GRID(grid), gtk_label_new(g_strdup_printf("D%d: ", i+1)), 0, i+1, 1, 1);
	
	startinfo.entry_range = g_new(GtkWidget *, 16);
	for(i = 0, j = 0; i<16; i++, j+=2) {
		startinfo.entry_range[i] = gtk_entry_new();
		gtk_entry_set_max_length(GTK_ENTRY(startinfo.entry_range[i]), 4);
		gtk_entry_set_width_chars(GTK_ENTRY(startinfo.entry_range[i]), 8);
		if(j==8) j = 0;
		gtk_grid_attach(GTK_GRID(grid), startinfo.entry_range[i], j+1, i/4+1, 1, 1);
		printf("(%d, %d)\t", j+1, i/4+1);
	}
	
	for(i = 0, j = 0; i<8; i++, j+=4) {
		if(j==8) j = 0;
		gtk_grid_attach(GTK_GRID(grid), gtk_label_new("-"), j+2, i/2+1, 1, 1);
		printf("(%d, %d)\t", j+2, i/4+1);
	}
	
	startinfo.entry_expostime = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(startinfo.entry_expostime), 8);
	gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Expos time:"), 0, 5, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), startinfo.entry_expostime, 1, 5, 1, 1);
	
	startinfo.entry_time = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(startinfo.entry_time), 8);
	gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Time in format hh:mm:ss:"), 0, 6, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), startinfo.entry_time, 1, 6, 1, 1);
   
	gtk_container_add (GTK_CONTAINER (content_area), grid);
	gtk_widget_show_all (dialog);
	
	gtk_entry_set_text(GTK_ENTRY(startinfo.entry_range[0]), g_strdup_printf("%d", infostruct.LD[0]));
	gtk_entry_set_text(GTK_ENTRY(startinfo.entry_range[1]), g_strdup_printf("%d", infostruct.LD[1]));
	gtk_entry_set_text(GTK_ENTRY(startinfo.entry_range[4]), g_strdup_printf("%d", infostruct.LD[2]));
	gtk_entry_set_text(GTK_ENTRY(startinfo.entry_range[5]), g_strdup_printf("%d", infostruct.LD[3]));
	gtk_entry_set_text(GTK_ENTRY(startinfo.entry_range[8]), g_strdup_printf("%d", infostruct.LD[4]));
	gtk_entry_set_text(GTK_ENTRY(startinfo.entry_range[9]), g_strdup_printf("%d", infostruct.LD[5]));
	gtk_entry_set_text(GTK_ENTRY(startinfo.entry_range[12]), g_strdup_printf("%d", infostruct.LD[6]));
	gtk_entry_set_text(GTK_ENTRY(startinfo.entry_range[13]), g_strdup_printf("%d", infostruct.LD[7]));
	
	gtk_entry_set_text(GTK_ENTRY(startinfo.entry_range[2]), g_strdup_printf("%d", infostruct.RD[0]));
	gtk_entry_set_text(GTK_ENTRY(startinfo.entry_range[3]), g_strdup_printf("%d", infostruct.RD[1]));
	gtk_entry_set_text(GTK_ENTRY(startinfo.entry_range[6]), g_strdup_printf("%d", infostruct.RD[2]));
	gtk_entry_set_text(GTK_ENTRY(startinfo.entry_range[7]), g_strdup_printf("%d", infostruct.RD[3]));
	gtk_entry_set_text(GTK_ENTRY(startinfo.entry_range[10]), g_strdup_printf("%d", infostruct.RD[4]));
	gtk_entry_set_text(GTK_ENTRY(startinfo.entry_range[11]), g_strdup_printf("%d", infostruct.RD[5]));
	gtk_entry_set_text(GTK_ENTRY(startinfo.entry_range[14]), g_strdup_printf("%d", infostruct.RD[6]));
	gtk_entry_set_text(GTK_ENTRY(startinfo.entry_range[15]), g_strdup_printf("%d", infostruct.RD[7]));
	
	gtk_entry_set_text(GTK_ENTRY(startinfo.entry_expostime), g_strdup_printf("%d", infostruct.expos));
	gtk_entry_set_text(GTK_ENTRY(startinfo.entry_time), g_strdup_printf("%s", convert_time_to_hms(infostruct.time)));
	
	g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(start_expos_callback), NULL);
}

void stop_expos(GtkWidget *widget, gpointer window)
{
	printf("stop expos\n");
	char *msg = (char *)malloc(1024*sizeof(char));
	
	infostruct.status = 0;

	sprintf(msg, "STATUS=%d\nTIME=%06d\nEXPOS=%06d\n%s\n%s\n%s\n%s\n", infostruct.status,\
						infostruct.time,\
						infostruct.expos,\
						"{0000-2048;2048-4096}",\
						"{0000-2048;2048-4096}",\
						"{0000-2048;2048-4096}",\
						"{0000-2048;2048-4096}");
						
	send_msg_to_kernel(msg);
	
	strcpy(infostruct.start, "Stoped");
	get_online_data_cycle(window);
	clock_func(NULL);
	
	if(!g_source_remove(infostruct.tag_get_online_cycle))
		perror("Can't remove online cycle\n");
}

void clear_expos(GtkWidget *widget, gpointer window)
{
	printf("clear expos\n");
	int prev_status;
	char *msg = (char *)malloc(1024*sizeof(char));
	
	prev_status = infostruct.status;
	infostruct.status = 3;

	sprintf(msg, "STATUS=%d\nTIME=%06d\nEXPOS=%06d\n%s\n%s\n%s\n%s\n", infostruct.status,\
						infostruct.time,\
						infostruct.expos,\
						"{0000-2048;2048-4096}",\
						"{0000-2048;2048-4096}",\
						"{0000-2048;2048-4096}",\
						"{0000-2048;2048-4096}");
						
	send_msg_to_kernel(msg);
	strcpy(infostruct.start, "Stoped");
	
	if(prev_status == 2)
		start_expos(widget, window);
	else
		g_source_remove(infostruct.tag_get_online_cycle);
}

void choose_dir_to_save(GtkWidget *widget, gpointer entry)
{
	GtkWidget *image = GTK_WIDGET(NULL);
	GtkWidget *toplevel = gtk_widget_get_toplevel (image);
	GtkWidget *dialog = gtk_file_chooser_dialog_new (("Choose dir to save"),
	                                                 GTK_WINDOW (toplevel),
	                                                 GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
	                                                 GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
	                                                 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	                                                 NULL);
	                                                          

	switch (gtk_dialog_run (GTK_DIALOG (dialog)))
	{
		case GTK_RESPONSE_ACCEPT:
		{
			path_to_save_spk = g_strdup_printf("%s/", gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog)) );
			printf("ptss = %s\n", path_to_save_spk);

			break;
		}
		default:
			break;
	}
	
	while(g_main_context_iteration(NULL, FALSE));
		gtk_widget_destroy (dialog);
		
	gtk_entry_set_text( GTK_ENTRY(entry), path_to_save_spk );
}

void save_as_spk(GtkWidget *widget, gpointer window)
{
	GtkWidget *dialog, *content_area;
	GtkWidget *entry_path, *button_dir, *img_dir;
  	GtkWidget *grid;

    dialog = gtk_dialog_new_with_buttons ("Save Spk",
                                         window,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         NULL);
	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	
	grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 20);
	
	entry_path = gtk_entry_new();
	gtk_entry_set_text( GTK_ENTRY(entry_path), g_strdup_printf("%s", getenv("HOME")) );
	gtk_entry_set_width_chars(GTK_ENTRY(entry_path), 40);
	
	button_dir = gtk_button_new ();
	img_dir = gtk_image_new_from_file("./dir.png");
	gtk_button_set_image(GTK_BUTTON(button_dir), img_dir);
	g_signal_connect (G_OBJECT (button_dir), "clicked",
					G_CALLBACK (choose_dir_to_save), (gpointer) entry_path);
	
	gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Path to save: "), 0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), entry_path, 1, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), button_dir, 2, 0, 1, 1);
	gtk_container_add (GTK_CONTAINER (content_area), grid);
	
	gtk_widget_show_all (dialog);
	
	gint result = gtk_dialog_run (GTK_DIALOG (dialog));
	switch (result)
	{
    case GTK_RESPONSE_ACCEPT: {
		path_to_save_spk = g_strdup_printf("%s", gtk_entry_get_text(GTK_ENTRY(entry_path)));
		if(path_to_save_spk[strlen(path_to_save_spk)-1] != '/') path_to_save_spk = g_strdup_printf("%s/", path_to_save_spk);
		printf("path to save = %s\n", path_to_save_spk);
		
		int i;
		int fd_out[16];
		FILE *fd_aniz;
		FILE *fd_X90, *fd_X180;
		char name[60];
		char ItoA[3];
		struct stat st = {0};

		//open(path_to_save_spk, O_RDWR|O_CREAT, S_IRUSR|S_IWUSR);
		if (stat(path_to_save_spk, &st) == -1)
   			 mkdir(path_to_save_spk, 0700);	

		for(i=0; i<12; i++) {
			sprintf(name, "%sTIME%d.SPK", path_to_save_spk, i+1);
			fd_out[i] = open(name, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR);
		
			if (fd_out[i] < 0) {
				perror("error in device");
				exit(1);
			}
			
			lseek(fd_out[i], 512, 0);
			write(fd_out[i], spectras->timespk[i], MAX_CH*sizeof(int));
			close(fd_out[i]);
		}
	
		for(i=12; i<16; i++) {
			sprintf(name, "%sBUFKA%d.SPK", path_to_save_spk, i-11);
			fd_out[i] = open(name, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR);
		
			if (fd_out[i] < 0) {
				perror("error in device");
				exit(1);
			}
			
			lseek(fd_out[i], 512, 0);
			write(fd_out[i], spectras->energyspk[i-12], MAX_CH*sizeof(int));
			close(fd_out[i]);
		}
	
		if(zero_R > EPS) {
			sprintf(name, "%sanizc%d", path_to_save_spk, COMP);
			fd_aniz = fopen(name, "w");
		
			printf("zero_R = %.2f\n", zero_R/COMP);
			for(i=(int)zero_R/COMP; i<=MAX_CH/COMP-1; i++)
				fprintf(fd_aniz, "%4d %10.8f\n", i-(int)zero_R/COMP+1, aniz[i+1]);
	
			fclose(fd_aniz);
			
			sprintf(name, "%sX90c%d", path_to_save_spk, COMP);
			fd_X90 = fopen(name, "w");
			for(i=0; i<=MAX_CH/COMP-1; i++)
				fprintf(fd_X90, "%4d %10.8f\n", i, coincidence[0][i]);
			fclose(fd_X90);
			
			sprintf(name, "%sX180c%d", path_to_save_spk, COMP);
			fd_X180 = fopen(name, "w");
			for(i=0; i<=MAX_CH/COMP-1; i++)
				fprintf(fd_X180, "%4d %10.8f\n", i, coincidence[1][i]);
			fclose(fd_X180);
		}
		
		printf("Save ok!\n");
	
	} 
       break;
    case GTK_RESPONSE_CANCEL:
		break;
    default:
       break;
	}
	gtk_widget_destroy (dialog);
}

void view_expos(GtkWidget *widget, gpointer window)
{
	get_online_data_cycle(window);
}

void save_conf_file()
{
	printf("save_conf_file func\n");
	
	int i;
	int time_s[3];
	char *msg = (char *)malloc(1024*sizeof(char));
	char *range_msg[4];
	FILE *in;
	char *curr_path = (char *)malloc(1024*sizeof(char));
	
	getcwd(curr_path, 1024);
	printf("curr_path = %s\n", curr_path);
	
	sprintf(curr_path, "%s/%s", curr_path, "d.conf");
	
	in = fopen(curr_path, "w+");
	
	for(i=0; i<4; i++)
		range_msg[i] = (char *)malloc(64*sizeof(char));
	
	for(i=0; i<4; i++)
		sprintf(range_msg[i], "{%04d-%04d;%04d-%04d}", infostruct.LD[2*i], infostruct.LD[2*i+1], infostruct.RD[2*i], infostruct.RD[2*i+1]);
	
	sprintf(msg, "STATUS=%d\nTIME=%06d\nEXPOS=%06d\n%s\n%s\n%s\n%s\nINT1=%0.f\nINT2=%0.f\nINT3=%0.f\nINT4=%0.f\nSTART=%s\n", infostruct.status,\
					infostruct.time,\
					infostruct.expos,\
					range_msg[0],\
					range_msg[1],\
					range_msg[2],\
					range_msg[3],\
					infostruct.intensity[0],\
					infostruct.intensity[1],\
					infostruct.intensity[2],\
					infostruct.intensity[3], \
					infostruct.start);
	
	fputs(msg, in);
	if (ferror (in))
		printf("Error Writing to %s\n", curr_path);
	
	fclose(in);
	free(curr_path);
	free(msg);
}

void gtk_main_quit_and_save_conf(GtkWidget *widget, gpointer data)
{
	save_conf_file();
	
	gtk_main_quit();
}

void read_conf_file()
{
	int i;
	FILE *in;
	char *curr_path = (char *)malloc(1024*sizeof(char));
	char *buffer = (char *)malloc(1024*sizeof(char));
	char *range_msg[4];
	
	char *time_str = (char *)malloc(80*sizeof(char));
	
	GtkTextIter iter, end;
	
	for(i=0; i<4; i++)
		range_msg[i] = (char *)malloc(64*sizeof(char));
	
	getcwd(curr_path, 1024);
	printf("curr_path = %s\n", curr_path);
	
	sprintf(curr_path, "%s/%s", curr_path, "d.conf");
	
	in = fopen(curr_path, "r");
	
	if(!in) {
		printf("Error Opening file %s\n", curr_path);
		return ;
	}

	if(fread(buffer, 1, 1024, in) > 1024) {
		printf("Error Reading file %s\n", curr_path);
		return ;
	}
	
	printf("buffer = %s", buffer);
	
	sscanf(buffer, "STATUS=%d\nTIME=%06d\nEXPOS=%06d\n%s\n%s\n%s\n%s\nINT1=%lf\nINT2=%lf\nINT3=%lf\nINT4=%lf\nSTART=%[^\n]s\n", &(infostruct.status),\
					&(infostruct.time),\
					&(infostruct.expos),\
					range_msg[0],\
					range_msg[1],\
					range_msg[2],\
					range_msg[3],\
					&(infostruct.intensity[0]),\
					&(infostruct.intensity[1]),\
					&(infostruct.intensity[2]),\
					&(infostruct.intensity[3]), \
					infostruct.start);
	
	printf("start time = %s\n", infostruct.start);
	
	if(strcmp(infostruct.start, "Stoped"))
		for(i=0; i<4; i++)
			infostruct.intensity[i] = 0.0;
	
	for(i=0; i<4; i++) {
		printf("range_msg[%d] = %s; int = %lf\n", i, range_msg[i], infostruct.intensity[i]);
		sscanf(range_msg[i], "{%04d-%04d;%04d-%04d}", &(infostruct.LD[2*i]), &(infostruct.LD[2*i+1]), &(infostruct.RD[2*i]), &(infostruct.RD[2*i+1]));
	}
	
	gtk_text_buffer_get_iter_at_line(info_buffer, &iter, 2);
	gtk_text_buffer_get_iter_at_line_offset(info_buffer, &end, 3, 0);
	gtk_text_buffer_delete(info_buffer, &iter, &end);
	
	if( compare_dates(infostruct.start) >= 0)
		strcpy(infostruct.start, "Stoped");	
	
	gtk_text_buffer_insert_with_tags_by_name(info_buffer, &iter, 
		g_strdup_printf("Start: %s\n", infostruct.start), -1, "bold", NULL);
	
	gtk_text_buffer_get_iter_at_line(info_buffer, &iter, 4);
	gtk_text_buffer_get_iter_at_line_offset(info_buffer, &end, 5, 0);
	gtk_text_buffer_delete(info_buffer, &iter, &end);
	
	gtk_text_buffer_insert_with_tags_by_name(info_buffer, &iter, 
		g_strdup_printf("Accumulation: %s\n", convert_time_to_hms(infostruct.time)), -1, "bold", NULL);
	
	printf("status readed = %d\n", infostruct.status);
	printf("seconds NOW = %ld, seconds to WAIT = %ld\n", convert_time_to_seconds(infostruct.start), convert_time_to_seconds(time_str) - (convert_time_to_seconds(infostruct.start)+infostruct.time));
	printf("dates compare = %ld\n", compare_dates(infostruct.start));
	
	free(curr_path);
	free(buffer);
}
