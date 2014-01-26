#include <gtk/gtk.h>

#include <gtkdatabox.h>
#include <gtkdatabox_points.h>
#include <gtkdatabox_ruler.h>
#include <gtkdatabox_bars.h>

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#include <sys/socket.h>
#include <linux/netlink.h>

#include "gnuplot_i.h"

#define sqr(x)	((x)*(x))
#define ln(x)	log(x)

#define NUM_TIME_SPK	12
#define NUM_SUM_TIME_SPK	6
#define NUM_ENERGY_SPK	4
#define NUMS	4096
#define SPK_GROUP	4
#define EPS	0.000001

#define BG_START	500
#define BG_STOP	600
#define TAU	80E-10

#define COMP	20

#define SIMPLE_DEVICE "/dev/simple"
#define MAX_NLH_MSG	5
#define NETLINK_NITRO 17
#define MAX_CH 4096

static char *folder2spk = "./";
static char *folder2readspk = "./spk/";
static double results[42];
static double aniz[NUMS], anizd[NUMS/COMP];

static GtkWidget *databox_zero[NUM_SUM_TIME_SPK];

struct truedata {
	int timespk[12][MAX_CH];
	int energyspk[4][MAX_CH];
};

typedef struct {
	GtkWidget *LCH;
	GtkWidget *RCH;
	GtkWidget *res_entry;
	gfloat *N;
	gnuplot_ctrl *hctrl;
} lchrch;

typedef struct {
	GtkWidget *spk[4][4];
	GtkWidget *time;
} kernel_msg1;

int readspk(void);
int DenisCalc(const gfloat N[], const unsigned int F[], int LCH, int RCH, char flag, float time_N, float time_F, float u[]);

void close_window(GtkWidget *widget, gpointer window)
{
    gtk_widget_destroy(GTK_WIDGET(window));
}

static gint
show_motion_notify_cb (GtkWidget *entry, GdkEventMotion * event
		       /*, GtkWidget *widget */ )
{
//	printf("motion ok\n");
	
	gfloat x, y;
	gchar *text;

	x = event->x_root;
	y = event->y_root;
	
	text = g_strdup_printf ("%f ; %f", x, y);
	gtk_entry_set_text (GTK_ENTRY (entry), text);
	g_free ((gpointer) text);
	
	return FALSE;
}

void d_calc(GtkWidget *widget, lchrch *data)
{
	printf("dcalc work ok\n");
	int i;
	
//	struct lchrch *borders = data;
	GtkWidget *left_ch = data->LCH; 
	GtkWidget *rght_ch = data->RCH;
	
	const char *s_LCH = gtk_entry_get_text(GTK_ENTRY(left_ch));
	const char *s_RCH = gtk_entry_get_text(GTK_ENTRY(rght_ch));
	
	printf("%s %s\n", s_LCH, s_RCH);
	
	int lch = atoi(s_LCH);
	int rch = atoi(s_RCH);
	
	printf("new = %f\n", data->N[1000]);
	
	float u[5];
	
	DenisCalc(data->N, NULL, lch, rch, 1, 0, 0, u);

	for(i=0; i<=4; i++)
		printf("u[%d] = %e;\n", i, u[i]); 
		
	gchar *text = g_strdup_printf ("S = %f \ndS = %f \nPick postion = %f \nError in pick position = %f \nFWHM = %f", u[0], u[1], u[2], u[3], u[4]);
	gtk_label_set_text (GTK_LABEL (data->res_entry), text);
	g_free ((gpointer) text);
	
//	gnuplot_cmd(data->hctrl, "set arrow from 280,0.0 to 280,1000");
//	gnuplot_cmd(data->hctrl, "replot");
}

static gint
show_button_press_cb (GtkDatabox * box, GdkEventButton * event,
		      gfloat *E)
{
	printf("button pressed DANGER! 123 Y[0] = %f\n", E[0]);
	
	gnuplot_ctrl *h1;
	
	h1 = gnuplot_init();

	GtkWidget *window;
	GtkWidget *socket = gtk_socket_new ();
	GtkWidget *hbox = gtk_hbox_new(FALSE, 0);
	GtkWidget *entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entry), "123");
	
	GtkWidget *table = gtk_table_new(6, 2, TRUE);
	GtkWidget *ok_button = gtk_button_new_with_label("OK");
	
//	GtkWidget *results_entry = gtk_entry_new();
	
	lchrch *borders = (lchrch *)malloc(sizeof(lchrch));
	borders->LCH = gtk_entry_new();
	borders->RCH = gtk_entry_new();
	borders->N = E;
	borders->res_entry = gtk_label_new(NULL);
	borders->hctrl = h1;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_set_size_request (window, 800, 600);

	g_signal_connect (G_OBJECT (window), "destroy",
		G_CALLBACK (close_window), G_OBJECT (window));

	gtk_widget_show (socket);

	gtk_widget_add_events(GTK_WIDGET(window), GDK_POINTER_MOTION_MASK);
	g_signal_connect_swapped(G_OBJECT(window), "motion_notify_event",
			     G_CALLBACK (show_motion_notify_cb), entry);

	gtk_box_pack_start(GTK_BOX(hbox), socket, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), table, FALSE, TRUE, 5);
//	gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, TRUE, 5);

	gtk_table_attach_defaults(GTK_TABLE(table), borders->LCH, 0, 1, 1, 2);
	gtk_table_attach_defaults(GTK_TABLE(table), borders->RCH, 1, 2, 1, 2);
	gtk_table_attach_defaults(GTK_TABLE(table), ok_button, 0, 2, 2, 3);
	gtk_table_attach_defaults(GTK_TABLE(table), borders->res_entry, 0, 2, 3, 5);
	
	gtk_signal_connect(GTK_OBJECT(ok_button), "clicked", GTK_SIGNAL_FUNC(d_calc), borders);
	
	gtk_container_add (GTK_CONTAINER (window), hbox);
	gtk_widget_realize (socket);

	gnuplot_cmd(h1, "set terminal x11 window \"%x\"", gtk_socket_get_id (GTK_SOCKET(socket)));
	
	gtk_widget_show_all (window);
	
	graph_gnuplot(h1, E);
	
	/*
	char *nameBuff = (char *)malloc(36*sizeof(char));
    // buffer to hold data to be written/read to/from temporary file
    char *buffer = (char *)malloc(24*sizeof(char));
    int filedes = -1, count=0;

    // memset the buffers to 0
    memset(nameBuff,0,sizeof(nameBuff));
    memset(buffer,0,sizeof(buffer));

    // Copy the relevant information in the buffers
    strncpy(nameBuff,"/tmp/myTmpFile-XXXXXX",21);
    strncpy(buffer,"Hello World",11);

    errno = 0;
    // Create the temporary file, this function will replace the 'X's
    filedes = mkstemp(nameBuff);

    // Call unlink so that whenever the file is closed or the program exits
    // the temporary file is deleted
    unlink(nameBuff);

    if(filedes<1)
    {
        printf("\n Creation of temp file failed with error [%s]\n",strerror(errno));
        return 1;
    }
    else
    {
        printf("\n Temporary file [%s] created\n", nameBuff);
    }
    
    FILE *stream = fopen(nameBuff, "w");
    
    int i;
    for(i=0; i<NUMS; i++) {
		fprintf(stream, "%f %f\n", (float)i, (float)E[i]);
	//	borders->N[i] = (unsigned int)E[i];
	}
	fclose(stream);
	
	gnuplot_cmd(h1, "set decimalsign locale \"ru_RU.UTF-8\"");
	gnuplot_cmd(h1, "set xrange[0:4000]");
	gnuplot_cmd(h1, "set nomultiplot");
	gnuplot_cmd(h1, "set mouse doubleclick 0");
	gnuplot_cmd(h1, "plot '%s'", nameBuff);

//	g_signal_connect (G_OBJECT (window), "button-clicked", G_CALLBACK (printf("socket was clicked\n")), NULL);

	free(nameBuff);
	free(buffer);
	*/
	
	return FALSE;
}

void itoa(int n, char s[])
{
	if ((int) (n/10) != 0) {
		s[0] = (n/10) + '0';
		s[1] = n-10 + '0';
		s[2] = '\0';
	}
	else {
		s[0] = n + '0';
		s[1] = '\0';
	}
}

void zoom (GtkWidget *window, gpointer data)
{
	gfloat left, right, top, bottom;

//	printf("%f");

	gtk_databox_get_visible_limits (GTK_DATABOX(data), &left, &right, &top, &bottom);
	printf("%f %f %f %f\n", left, right, top, bottom);
	
	int i;
	
	for(i=0; i<=NUM_TIME_SPK-1; i++)
		gtk_databox_set_visible_limits (GTK_DATABOX(data), left+50.0, right-50.0, top-0.1, bottom+0.1);
}

void zoom_out(GtkWidget *window, gpointer data)
{
	gtk_databox_auto_rescale (GTK_DATABOX (data), 0.05);
	
/*	gfloat left, right, top, bottom;

	gtk_databox_get_visible_limits (GTK_DATABOX(data), &left, &right, &top, &bottom);
	printf("%f %f %f %f\n", left, right, top, bottom);
	
	int i;
	
	for(i=0; i<=NUM_TIME_SPK-1; i++) {
		gtk_databox_set_visible_limits (GTK_DATABOX(data), 0.0, (double) NUMS, results[i+2*NUM_SUM_TIME_SPK], 0.0);
	} */
}

void move_right (GtkWidget *window, gpointer data)
{
	gfloat left, right, top, bottom;
	
	gtk_databox_get_visible_limits (GTK_DATABOX(data), &left, &right, &top, &bottom);
	printf("%f %f %f %f\n", left, right, top, bottom);
	
	gtk_databox_set_visible_limits (GTK_DATABOX(data), left+50.0, right+50.0, top, bottom);
}

void move_left (GtkWidget *window, gpointer data)
{
	gfloat left, right, top, bottom;
	
	gtk_databox_get_visible_limits (GTK_DATABOX(data), &left, &right, &top, &bottom);
	printf("%f %f %f %f\n", left, right, top, bottom);
	
	gtk_databox_set_visible_limits (GTK_DATABOX(data), left-50.0, right-50.0, top, bottom);
}

void move_up (GtkWidget *window, gpointer data)
{
	gfloat left, right, top, bottom;
	
	gtk_databox_get_visible_limits (GTK_DATABOX(data), &left, &right, &top, &bottom);
	printf("%f %f %f %f\n", left, right, top, bottom);
	
	gtk_databox_set_visible_limits (GTK_DATABOX(data), left, right, top+0.1, bottom+0.1);
}

void move_down (GtkWidget *window, gpointer data)
{
	gfloat left, right, top, bottom;
	
	gtk_databox_get_visible_limits (GTK_DATABOX(data), &left, &right, &top, &bottom);
	printf("%f %f %f %f\n", left, right, top, bottom);
	
	gtk_databox_set_visible_limits (GTK_DATABOX(data), left, right, top-0.1, bottom-0.1);
}

void shrink_databox (GtkWidget *window, gpointer user_data)
{
	printf("shrink func started\n");

	GtkWidget *databox_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW(databox_window), "Databox shrink func");
	gtk_container_set_border_width (GTK_CONTAINER(databox_window), 5);
	gtk_widget_set_size_request (databox_window, 400, 300);
	
	g_signal_connect (G_OBJECT(databox_window), "destroy", G_CALLBACK (gtk_main_quit), NULL);
	
	printf("shrink point = %p\n", window);
	
	GtkWidget *vbox = gtk_vbox_new(TRUE, 0);
	GList *children = gtk_container_get_children(GTK_CONTAINER(window));
	children = g_list_first(children);
	gtk_widget_reparent(children->data, databox_window);
//	gtk_container_add (GTK_CONTAINER (databox_window), children->data);

	gtk_widget_show_all(databox_window);
}

int anisotropy(double *res, int **n, int **initial_spk, int **e, double **y, double **F, \
double *compX90, double *compX180, double *R, double *dRcomp)
{
	int i, j, k=0, z1, z2, z3, p;
	int in;
	int c, max_el[NUM_SUM_TIME_SPK];
	int m[NUM_SUM_TIME_SPK][NUMS], cent[NUM_SUM_TIME_SPK];
	double u, v1, v2;
	int max_el_num[NUM_SUM_TIME_SPK];
	double t = 0.0, K[SPK_GROUP], K_min;
	int S_start[SPK_GROUP], S_stop[SPK_GROUP];
	char name[7];
	char ItoA[3];
	
	double xc[NUM_SUM_TIME_SPK], s[NUM_SUM_TIME_SPK], shift[NUM_SUM_TIME_SPK], T[NUMS];

	for(i=0; i<=NUM_TIME_SPK-1; i++) {
		j = 0;
		strcpy(name, folder2spk);
		strcat(name, "TIME");
		itoa(i+1, ItoA);
		strcat(name, ItoA);
		strcat(name, ".SPK");
		in = open(name, O_RDONLY);
		lseek(in, 512, 0);
		while(read(in, &n[i][j], sizeof(int))>0) {
//			printf("%d %f %d\n", i, t, n[i][j]);
//			t+=0.01;
			initial_spk[i][j] = n[i][j];
			j++;
		}
//		printf("%d\n", j);
		close(in);
	}

	for(i=0; i<=3; i++) {
		p=0;
		strcpy(name, folder2spk);
		strcat(name, "BUFKA");
		itoa(i+1, ItoA);
		strcat(name, ItoA);
		strcat(name, ".SPK");
		in = open(name, O_RDONLY);
		lseek(in, 512, 0);
		while(read(in, &e[i][p], sizeof(int))>0) {
			p++;
		}
		close(in);
	}

	printf("n = %d\n", n[1][0]);

	//Iterr #1 shift
	for(i=0; i<=NUMS-1; i++) {
		m[0][i] = n[0][i]+n[1][i]; //D1-2
		m[1][i] = n[2][i]+n[3][i]; //D1-3
		m[2][i] = n[4][i]+n[5][i]; //D1-4
		m[3][i] = n[6][i]+n[7][i]; //D2-3
		m[4][i] = n[8][i]+n[9][i]; //D2-4
		m[5][i] = n[10][i]+n[11][i]; //D3-4
	}

	
	for(i=0; i<=NUM_SUM_TIME_SPK-1; i++) {
		max_el[i] = m[i][0]; max_el_num[i] = 0;
		for(j=0; j<=NUMS-1; j++)
			if(j!= 0 && j!=NUMS-1 && m[i][j]>max_el[i] && m[i][j]<=2*(m[i][j-1]+m[i][j+1])) {max_el[i]=m[i][j]; max_el_num[i]=j;}
	}
	
	for(j=0, k=0; j<=NUM_SUM_TIME_SPK-1; j++) {
		for(i=max_el_num[j]-10; i<=max_el_num[j]+9; i++) {
			xc[j] += (double)i*((double)n[k][i]+(double)n[k+1][i]);
			s[j] += (double)m[j][i];
		}
		if(s[j]<EPS) xc[j] = 0; else xc[j] = xc[j]/s[j];
		k += 2;
	}
	
	for(i=0; i<=NUM_SUM_TIME_SPK-1; i++)
		shift[i] = xc[0]-xc[i];
		
	for(i=0; i<=NUM_SUM_TIME_SPK-1; i++) {
		res[i] = xc[i];
		res[i+NUM_SUM_TIME_SPK] = shift[i];
		res[i+2*NUM_SUM_TIME_SPK] = max_el[i];
	}

	for(z3=0; z3<=0; z3++) {
	
	
	for(i=2, p=1; i<=NUM_TIME_SPK-1; i++, p++) {
		if(i%2==0 && i!=2) p--;
		if(abs(i-p)<512) k=(int)shift[i-p]; else k=0;
		printf("i-p = %d k= %d \n", i-p, k);
		if(k>0) {
			for(j=k+1; (j-k)<=NUMS-1; j++) {
				y[i][j] = (double)n[i][j-k]*(k+1.0-shift[i-p])+(double)n[i][j-k-1]*(shift[i-p]-k);
//				printf("%f %d %f %d %f\n", y[i][j], n[i][j-k], k+1.0-shift[i-p], n[i][j-k-1], shift[i-p]-k);
			}
		for(j=0; j<=k; j++)
			y[i][j] = 0.0;
		}
		
		else {
			for(j=0; (j-k)<=NUMS-2; j++) {
				y[i][j] = (double)n[i][j-k]*(1.0+shift[i-p]-(double)k)+(double)n[i][j-k+1]*((double)k-shift[i-p]);
			}
		for(j=NUMS-1+k; j<=NUMS-1; j++)
			y[i][j] = 0.0;
		}
	}
	
		for(i=2; i<=NUM_TIME_SPK-1; i++)
			for(j=0; j<=NUMS-1; j++) {
				u=round(y[i][j]);
				n[i][j] = (int)u;
			}
	
		for(i=0; i<=NUMS-1; i++) {
			m[0][i] = n[0][i]+n[1][i]; //D1-2
			m[1][i] = n[2][i]+n[3][i]; //D1-3
			m[2][i] = n[4][i]+n[5][i]; //D1-4
			m[3][i] = n[6][i]+n[7][i]; //D2-3
			m[4][i] = n[8][i]+n[9][i]; //D2-4
			m[5][i] = n[10][i]+n[11][i]; //D3-4
		}
	
		for(i=0; i<=NUM_SUM_TIME_SPK-1; i++) {
			max_el[i] = m[i][0]; max_el_num[i] = 0;
			for(j=0; j<=NUMS-1; j++)
				if(j!=0 && j!=NUMS-1 && m[i][j]>max_el[i]) {max_el[i]=m[i][j]; max_el_num[i]=j;}
		}
		printf("max el num = %d\n", max_el_num[1]);
	
		for(i=0; i<=NUM_SUM_TIME_SPK-1; i++)
			xc[i] = s[i] = 0.0;
	
		for(j=0, k=0; j<=NUM_SUM_TIME_SPK-1; j++) {
			for(i=max_el_num[j]-10; i<=max_el_num[j]+9; i++) {
				xc[j] += (double)i*((double)n[k][i]+(double)n[k+1][i]);
				s[j] += (double)m[j][i];
			}
		xc[j] = xc[j]/s[j];
		k += 2;
		}
	
		for(i=0; i<=NUM_SUM_TIME_SPK-1; i++)
			shift[i] = xc[0]-xc[i];
	
	}
	
	//SWAP
	for(i=0, k=0; i<=NUM_TIME_SPK-1; i+=2, k++) {
	//	printf("swap i = %d\n", i);
		for(j=0, z1=(int)round(xc[k]); j<=z1; j++) {
			if(j+z1>NUMS-1) break;
			p = n[i][z1-j];
			n[i][z1-j] = n[i][z1+j];
			n[i][z1+j] = p;
	//		printf("jswap i = %d\n", i);
		}
	}
	
	for(i=0; i<=NUMS-1; i++) {
		S_start[0] += n[1][i]+n[3][i]+n[5][i];
		S_start[1] += n[0][i]+n[7][i]+n[9][i];
		S_start[2] += n[2][i]+n[6][i]+n[11][i];
		S_start[3] += n[4][i]+n[8][i]+n[10][i];
//		printf("%d %d %d\n", n[2][i], n[6][i], n[11][i]);
		
		S_stop[0] += n[0][i]+n[2][i]+n[4][i];
		S_stop[1] += n[1][i]+n[6][i]+n[8][i];
		S_stop[2] += n[3][i]+n[7][i]+n[10][i];
		S_stop[3] += n[4][i]+n[9][i]+n[11][i];
	}
	
	for (i=0; i<=SPK_GROUP-1; i++)
		K[i]=(double)S_start[i]*S_stop[i];
	
	K_min = K[0];	
	for(i=1; i<=SPK_GROUP-1; i++) {
		if(K[i]<K_min) K_min=K[i];
	}
	
	for(i=0; i<=SPK_GROUP-1; i++) {
		K[i] = 1.0;
	}
	
	for(j=0; j<=NUMS-1; j++) {
			y[1][j] = (double)n[1][j];
			y[3][j] = (double)n[3][j];
			y[5][j] = (double)n[5][j];
			y[0][j] = (double)n[0][j];
			y[7][j] = (double)n[7][j];
			y[9][j] = (double)n[9][j];
			y[2][j] = (double)n[2][j];
			y[6][j] = (double)n[6][j];
			y[11][j] = (double)n[11][j];
			y[4][j] = (double)n[4][j];
			y[8][j] = (double)n[8][j];
			y[10][j] = (double)n[10][j];
	}
	// y_background = A * exp(Bx_j)
/*	double alpha[NUM_TIME_SPK], beta[NUM_TIME_SPK];
	double sxlny[NUM_TIME_SPK], sx[NUM_TIME_SPK], slny[NUM_TIME_SPK], ssqrx[NUM_TIME_SPK], ssqrlny[NUM_TIME_SPK];
	
	for(i=0; i<=NUM_TIME_SPK-1; i++)
		for(j=BG_START; j<BG_STOP; j++) {
			sxlny[i] += j*ln(y[i][j]);
			sx[i] += (double)j;
			slny[i] += ln(y[i][j]);
			ssqrx[i] += sqr((double)j);
			ssqrlny[i] += sqr(ln(y[i][j]));
		}
	
	for(i=0; i<=NUM_TIME_SPK-1; i++) {
		beta[i] = ( (BG_STOP-BG_START)*sxlny[i] - sx[i]*slny[i] ) / ( (BG_STOP-BG_START)*ssqrx[i]-sqr(sx[i]) ); 
		alpha[i] = exp( (slny[i]-beta[i]*sx[i])/(BG_STOP-BG_START) );
		printf("\n alpha[%d] = %f; beta[%d] = %.20f\n", i, alpha[i], i, beta[i]);
	}

	for(i=0; i<=NUM_TIME_SPK-1; i++) {
		res[3*NUM_SUM_TIME_SPK+i] = alpha[i];
		res[3*NUM_SUM_TIME_SPK+NUM_TIME_SPK+i] = beta[i];
	}
		
	for(i=0; i<=NUM_TIME_SPK-1; i++)
		for(j=0; j<=NUMS-1; j++)
			F[i][j] = alpha[i]*exp( beta[i]*j ); */
	
	//y_background = const	
	for(i=0; i<=NUM_TIME_SPK-1; i++)
		for(j=BG_START; j<BG_STOP; j++)
			F[i][0] += y[i][j];
			
	for(i=0; i<=NUM_TIME_SPK-1; i++)
		F[i][0] = F[i][0] / (double)(BG_STOP-BG_START);
	
	for(i=0; i<=NUM_TIME_SPK-1; i++)
		for(j=1; j<=NUMS-1; j++)
			F[i][j] = F[i][0];
			
	//y_background = A + B*exp(x_j)
	/*double sy[NUM_TIME_SPK], sex[NUM_TIME_SPK], syex[NUM_TIME_SPK], se2x[NUM_TIME_SPK];
	double alpha[NUM_TIME_SPK], beta[NUM_TIME_SPK]; 
	
	for(i=0; i<=NUM_TIME_SPK-1; i++)
		for(j=BG_START; j<BG_STOP; j++) {
			sy[i] += y[i][j];
			sex[i] += exp ((double)j);
			syex[i] += y[i][j]*exp(j);
			se2x[i] += exp(2.0*j);
		}
	 printf("%.2f %.2f\n", sy[0], sex[0]);
	for(i=0; i<=NUM_TIME_SPK-1; i++) {
		beta[i] = ( (BG_STOP-BG_START)*syex[i]-sy[i]*sex[i] ) / ( (BG_STOP-BG_START)*se2x[i] - sqr(sex[i]) );
		alpha[i] = (sy[i] - beta[i]*sex[i])/(BG_STOP-BG_START);
		printf("\n alpha[%d] = %f; beta[%d] = %e\n", i, alpha[i], i, beta[i]);
	}
	 
	for(i=0; i<=NUM_TIME_SPK-1; i++)
		for(j=0; j<=NUMS-1; j++)
			F[i][j] = alpha[i] + beta[i]*exp(j*TAU/3.6);
	*/
	
	printf("before background y[11][550] = %E\n", y[11][550]);
	printf("F[11][550] = %E\n", F[11][550]);
	for(i=0; i<=NUM_TIME_SPK-1; i++)
		for(j=0; j<=NUMS-1; j++)
			if((v1=(y[i][j] - F[i][j]))<EPS) 
				y[i][j] = 1.0;
			else
				y[i][j] = v1;
	
	printf("afterbackground y[11][2172] = %E; background = %E\n", y[11][2172], F[11][2172]);
	
	for(i=0; i<=NUM_TIME_SPK-1; i++)
		printf("y[%d][2172] = %E\n", i, y[i][2172]);
	
	double X90[NUMS], X180[NUMS];
	for(j=0; j<=NUMS-1; j++) {
	//	v1 = y[0][j]*y[1][j]*y[4][j]*y[5][j]*y[6][j]*y[7][j]*y[10][j]*y[11][j];
		X90[j] = pow(y[0][j], 1.0/8.0)*pow(y[1][j],1.0/8.0)*pow(y[4][j],1.0/8.0)*pow(y[5][j],1.0/8.0)*pow(y[6][j],1.0/8.0)*pow(y[7][j],1.0/8.0)*pow(y[10][j],1.0/8.0)*pow(y[11][j],1.0/8.0);
		if(X90[j]<=EPS) X90[j]=0.0;
		if (isnan(X90[j])) printf("Error! X90[%d] is NaN!\n", j);
		v2 = y[2][j]*y[3][j]*y[8][j]*y[9][j];
		if(v2<=EPS) X180[j]=0.0;
		else X180[j] = pow(v2, 1.0/4.0);
		if (isnan(X180[j])) printf("Error! X180[%d] is NaN!\n", j);
	}

	for(i=0; i<=NUMS/COMP-1; i++)
		compX90[i] = compX180[i] = 0.0;

	for(p=0; p<=NUMS/COMP-1; p++)
		for(j=0; j<=COMP-1 && p*COMP+j<=NUMS-1;j++) {
			compX90[p] += X90[p*COMP+j];
			compX180[p] += X180[p*COMP+j];
			if(p==108) printf("x90[%d] = %E\n", p*COMP+j, X90[p*COMP+j]);
		}

	for(j=0; j<=NUMS/COMP-1; j++) {
		if ((compX180[j]+2.0*compX90[j]) <= EPS) { R[j] = 0.0; printf("anis=0!\n"); }
		else R[j] = 2.0*(compX180[j]-compX90[j])/(compX180[j]+2.0*compX90[j]);
		if (isnan(R[j])) printf("Error! R[%d] is NaN! compX90[%d] = %E; comX180[%d] = %E\n", j, j, compX90[j], j, compX180[j]);
		if (isinf(R[j])) printf("Error! R[%d] is Inf! and = %E\n", j, R[j]);
	}

	double errSum[2][NUMS];
	for(j=0; j<=NUMS-1; j++) {
		errSum[0][j] = (y[0][j] + 2.0*F[0][j])/sqr(y[0][j]) + \
						(y[1][j] + 2.0*F[1][j])/sqr(y[1][j]) + \
						(y[4][j] + 2.0*F[4][j])/sqr(y[4][j]) + \
						(y[5][j] + 2.0*F[5][j])/sqr(y[5][j]) + \
						(y[6][j] + 2.0*F[6][j])/sqr(y[6][j]) + \
						(y[7][j] + 2.0*F[7][j])/sqr(y[7][j]) + \
						(y[10][j] + 2.0*F[10][j])/sqr(y[10][j]) + \
						(y[11][j] + 2.0*F[11][j])/sqr(y[11][j]);
		errSum[0][j] = errSum[0][j]/64.0;
		
		errSum[1][j] = (y[2][j] + 2.0*F[2][j])/sqr(y[2][j]) + \
						(y[3][j] + 2.0*F[3][j])/sqr(y[3][j]) + \
						(y[8][j] + 2.0*F[8][j])/sqr(y[8][j]) + \
						(y[9][j] + 2.0*F[9][j])/sqr(y[9][j]);
		errSum[1][j] = errSum[1][j]/16.0;
	}

	double dR[NUMS];
	for(j=0; j<=NUMS-1; j++) {
		v1 = sqr(X90[j]+2.0*X180[j])*pow((errSum[0][j]+errSum[1][j]), 0.5);
		if (v1 <= EPS) return -1;
		dR[j] = 6.0*X90[j]*X180[j]/v1;
	}
	
	for(p=0; p<=NUMS/COMP-1; p++)
		for(j=0; j<=COMP-1 && p*COMP+j<=NUMS-1; j++) {
			dRcomp[p] += sqr(dR[p*COMP+j]);
		}
		
	for(p=0; p<=NUMS/COMP-1; p++)
		dRcomp[p] = pow(dRcomp[p], 1.0/2.0);

	FILE *out = fopen("./output_dR5.txt", "w");
	
	for(j=0; j<=NUMS/COMP-1; j++)
		fprintf(out, "%6d  %10.8f  %10.8f\n", j-99, R[j], dRcomp[j]);

	printf("anisotropy function end\n");
	return 0;
}

void anisotropy_plot(gpointer notebook)
{
	int i, j, p;
	GtkWidget *table;	
	table = gtk_table_new(8, 6, TRUE);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	
	GtkWidget *ruler[2], *table_mini[2], *databox[2], *label_graph;
	GtkWidget *ruler_y1, *ruler_y2;
	GtkDataboxGraph *graph[2];
	GdkColor color;
	
	gfloat *X, *Y[2];
	
	X = g_new0 (gfloat, NUMS/COMP);
	
	for(i=0; i<2; i++)
		Y[i] = g_new0 (gfloat, NUMS/COMP);

	for (i = 0; i <= NUMS/COMP-1; i++) {
		X[i] = i;
	}
	
	for(i=0; i<2; i++)
		table_mini[i] = gtk_table_new(3, 3, FALSE);
		
	for(j=0; j<=NUMS/COMP-1; j++) {
			Y[0][j] = aniz[j];
	}

	printf("ANIZOTROPY!!!!\n");

	for(i=0; i<=0; i++) {
			
			databox[i] = gtk_databox_new ();
			gtk_table_attach (GTK_TABLE (table_mini[i]), databox[i], 1, 2, 1, 2,
				GTK_FILL | GTK_EXPAND | GTK_SHRINK,
				GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
		
			ruler[i] = gtk_databox_ruler_new (GTK_ORIENTATION_HORIZONTAL);
			gtk_widget_set_sensitive (ruler[i], TRUE);
			gtk_table_attach (GTK_TABLE (table_mini[i]), ruler[i], 1, 2, 2, 3,
				GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
			gtk_databox_set_ruler_x (GTK_DATABOX (databox[i]), GTK_DATABOX_RULER (ruler[i]));
		
			gtk_table_attach_defaults (GTK_TABLE(table), table_mini[i], i*3, (i+2)*3, 2, 8);
		
			gdk_color_parse("white", &color);
			gtk_widget_modify_bg (databox[i], GTK_STATE_NORMAL, &color);
		
			gdk_color_parse("black", &color);
			graph[i] = gtk_databox_points_new (NUMS/COMP, X, Y[i], &color, 2);
			gtk_databox_graph_add (GTK_DATABOX (databox[i]), graph[i]);
		
			gtk_databox_auto_rescale (GTK_DATABOX (databox[i]), 0.05);
		}
	
	GtkWidget *tab_label = gtk_label_new("Anistoropy");
	
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), table, tab_label);
	
	
	gtk_widget_show_all(GTK_WIDGET(notebook));
}

void coincidence(gpointer notebook, double *compX90, double *compX180)
{
	int i, j, p;
	GtkWidget *table;	
	table = gtk_table_new(8, 4, TRUE);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	
	GtkWidget *ruler[2], *table_mini[2], *databox[2], *label_graph;
	GtkWidget *ruler_y1, *ruler_y2;
	GtkDataboxGraph *graph[2];
	GdkColor color;
	
	gfloat *X, *Y[2];
	
	X = g_new0 (gfloat, NUMS/COMP);
	
	for(i=0; i<2; i++)
		Y[i] = g_new0 (gfloat, NUMS/COMP);

	for (i = 0; i <= NUMS/COMP-1; i++) {
		X[i] = i;
	}
	
	for(i=0; i<2; i++)
		table_mini[i] = gtk_table_new(3, 3, FALSE);

	for(p=0; p<=NUMS/COMP-1; p++) {
			Y[0][p] = compX90[p];
			Y[1][p] = compX180[p];
	}

	for(i=0; i<=1; i++) {
			
			databox[i] = gtk_databox_new ();
			gtk_table_attach (GTK_TABLE (table_mini[i]), databox[i], 1, 2, 1, 2,
				GTK_FILL | GTK_EXPAND | GTK_SHRINK,
				GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
		
			ruler[i] = gtk_databox_ruler_new (GTK_ORIENTATION_HORIZONTAL);
			gtk_widget_set_sensitive (ruler[i], TRUE);
			gtk_table_attach (GTK_TABLE (table_mini[i]), ruler[i], 1, 2, 2, 3,
				GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
			gtk_databox_set_ruler_x (GTK_DATABOX (databox[i]), GTK_DATABOX_RULER (ruler[i]));
		
			gtk_table_attach_defaults (GTK_TABLE(table), table_mini[i], i*2, (i+1)*2, 2, 8);
		
			gdk_color_parse("white", &color);
			gtk_widget_modify_bg (databox[i], GTK_STATE_NORMAL, &color);
		
			gdk_color_parse("black", &color);
			graph[i] = gtk_databox_points_new (NUMS/COMP, X, Y[i], &color, 2);
			gtk_databox_graph_add (GTK_DATABOX (databox[i]), graph[i]);
		
			gtk_databox_auto_rescale (GTK_DATABOX (databox[i]), 0.05);
		}
	
	GtkWidget *tab_label = gtk_label_new("X90 and X180");
	
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), table, tab_label);
	
	
	gtk_widget_show_all(GTK_WIDGET(notebook));
}

void background(gpointer notebook, double **y, double **F, double *res)
{
	int i, j, p;
	GtkWidget *table;	
	table = gtk_table_new(8, 6, TRUE);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	
	GtkWidget *ruler[NUM_TIME_SPK], *table_mini[NUM_TIME_SPK], *databox[NUM_TIME_SPK], *label_graph;
	GtkWidget *ruler_y1, *ruler_y2;
	GtkDataboxGraph *graph[NUM_TIME_SPK], *graph_bg[NUM_TIME_SPK];
	GdkColor color;
	
	gfloat *X, *Y[NUM_TIME_SPK], *Ybg[NUM_TIME_SPK];


	X = g_new0 (gfloat, NUMS);
	
	for(i=0; i<12; i++) {
		Y[i] = g_new0 (gfloat, NUMS);
		Ybg[i] = g_new0 (gfloat, NUMS);
	}

	for (i = 0; i <= NUMS-1; i++) {
		X[i] = i;
	}
	
	for(i=0; i<=11; i++)
		table_mini[i] = gtk_table_new(3, 3, FALSE);
	
//	printf("F[2][3500] = %E; exp = %E\n", F[2][3500], 0.85*exp(3500*0.0024));
	for(j=0; j<=5; j++) 
		for(i=0; i<=1; i++) {
			
			for(p=0; p<=NUMS-1; p++) {
				Y[i+j*2][p] = y[i+j*2][p]-F[i+j*2][p];
				Ybg[i+j*2][p] = F[i+j*2][p];
			}
			
			databox[i+j*2] = gtk_databox_new ();
			gtk_table_attach (GTK_TABLE (table_mini[i+j*2]), databox[i+j*2], 1, 2, 1, 2,
				GTK_FILL | GTK_EXPAND | GTK_SHRINK,
				GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
		
			ruler[i+j*2] = gtk_databox_ruler_new (GTK_ORIENTATION_HORIZONTAL);
			gtk_widget_set_sensitive (ruler[i+j*2], TRUE);
			gtk_table_attach (GTK_TABLE (table_mini[i+j*2]), ruler[i+j*2], 1, 2, 2, 3,
				GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
			gtk_databox_set_ruler_x (GTK_DATABOX (databox[i+j*2]), GTK_DATABOX_RULER (ruler[i+j*2]));
		
			gtk_table_attach_defaults (GTK_TABLE(table), table_mini[i+j*2], j, j+1, 2+i*3, 2+(i+1)*3);
		
			gdk_color_parse("white", &color);
			gtk_widget_modify_bg (databox[i+j*2], GTK_STATE_NORMAL, &color);
		
			gdk_color_parse("black", &color);
			graph[i+j*2] = gtk_databox_points_new (NUMS, X, Y[i+j*2], &color, 2);
			gtk_databox_graph_add (GTK_DATABOX (databox[i+j*2]), graph[i+j*2]);
			gtk_databox_auto_rescale (GTK_DATABOX (databox[i+j*2]), 0.05);
			
			printf("Y[0][200] = %E\n", Y[0][200]);
			gdk_color_parse("blue", &color);
			graph_bg[i+j*2] = gtk_databox_points_new (NUMS, X, Ybg[i+j*2], &color, 2);
			gtk_databox_graph_add (GTK_DATABOX (databox[i+j*2]), graph_bg[i+j*2]);
		}
		
	ruler_y1 = gtk_databox_ruler_new (GTK_ORIENTATION_VERTICAL);
	gtk_widget_set_sensitive (ruler_y1, FALSE);
	gtk_table_attach (GTK_TABLE (table_mini[0]), ruler_y1, 0, 1, 1, 2,
		GTK_FILL, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
	for(i=0; i<12; i+=2) {
		gtk_databox_set_ruler_y (GTK_DATABOX (databox[i]), GTK_DATABOX_RULER (ruler_y1));
	}
	
	ruler_y2 = gtk_databox_ruler_new (GTK_ORIENTATION_VERTICAL);
	gtk_widget_set_sensitive (ruler_y2, FALSE);
	gtk_table_attach (GTK_TABLE (table_mini[1]), ruler_y2, 0, 1, 1, 2,
		GTK_FILL, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
	for(i=1; i<12; i+=2) {
		gtk_databox_set_ruler_y (GTK_DATABOX (databox[i]), GTK_DATABOX_RULER (ruler_y2));
	}
	
	GtkWidget *tab_label = gtk_label_new("Background");
	
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), table, tab_label);
	
	
	gtk_widget_show_all(GTK_WIDGET(notebook));
}

void swap_spk(gpointer notebook, int **n, double *res)
{
	int i, j, p;
	GtkWidget *table;	
	table = gtk_table_new(8, 6, TRUE);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	
	GtkWidget *ruler[NUM_TIME_SPK], *table_mini[NUM_TIME_SPK], *databox[NUM_TIME_SPK], *label_graph;
	GtkWidget *ruler_y1, *ruler_y2;
	GtkDataboxGraph *graph[NUM_TIME_SPK], *pick[NUM_TIME_SPK];
	GdkColor color;
	
	gfloat *X, *Y[NUM_TIME_SPK];
	gfloat *X_pick[NUM_TIME_SPK], *Y_pick[NUM_TIME_SPK];
	
	X = g_new0 (gfloat, NUMS);
	
	for(i=0; i<=NUM_TIME_SPK-1; i++)
		Y[i] = g_new0 (gfloat, NUMS);

	for (i = 0; i <= NUMS-1; i++) {
		X[i] = i;
	}
	
	for(i=0; i<=NUM_TIME_SPK-1; i++)
		table_mini[i] = gtk_table_new(3, 3, FALSE);

	for(i=0; i<=NUM_SUM_TIME_SPK-1; i++) {
		X_pick[i*2] = g_new0 (gfloat, 1);
		X_pick[i*2+1] = g_new0 (gfloat, 1);
		Y_pick[i*2] = g_new0 (gfloat, 1);
		Y_pick[i*2+1] = g_new0 (gfloat, 1);
		
		X_pick[i*2][0] = res[i];
		X_pick[i*2+1][0] = res[i];
		Y_pick[i*2][0] = Y_pick[i*2+1][0] = res[i+2*NUM_SUM_TIME_SPK];

	}


	for(j=0; j<=5; j++) 
		for(i=0; i<=1; i++) {
			
			for(p=0; p<=NUMS-1; p++) {
				Y[i+j*2][p] = (double)n[i+j*2][p];
			}
			
			databox[i+j*2] = gtk_databox_new ();
			gtk_table_attach (GTK_TABLE (table_mini[i+j*2]), databox[i+j*2], 1, 2, 1, 2,
				GTK_FILL | GTK_EXPAND | GTK_SHRINK,
				GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
		
			ruler[i+j*2] = gtk_databox_ruler_new (GTK_ORIENTATION_HORIZONTAL);
			gtk_widget_set_sensitive (ruler[i+j*2], TRUE);
			gtk_table_attach (GTK_TABLE (table_mini[i+j*2]), ruler[i+j*2], 1, 2, 2, 3,
				GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
			gtk_databox_set_ruler_x (GTK_DATABOX (databox[i+j*2]), GTK_DATABOX_RULER (ruler[i+j*2]));
		
			gtk_table_attach_defaults (GTK_TABLE(table), table_mini[i+j*2], j, j+1, 2+i*3, 2+(i+1)*3);
		
			gdk_color_parse("white", &color);
			gtk_widget_modify_bg (databox[i+j*2], GTK_STATE_NORMAL, &color);
		
			gdk_color_parse("black", &color);
			graph[i+j*2] = gtk_databox_points_new (NUMS, X, Y[i+j*2], &color, 2);
			gtk_databox_graph_add (GTK_DATABOX (databox[i+j*2]), graph[i+j*2]);
			
			gdk_color_parse("red", &color);
			pick[i+j*2] = gtk_databox_bars_new (1, X_pick[i+j*2], Y_pick[i+j*2], &color, 1);
			gtk_databox_graph_add (GTK_DATABOX (databox[i+j*2]), pick[i+j*2]);
		
			gtk_databox_auto_rescale (GTK_DATABOX (databox[i+j*2]), 0.05);
		}
		
	ruler_y1 = gtk_databox_ruler_new (GTK_ORIENTATION_VERTICAL);
	gtk_widget_set_sensitive (ruler_y1, FALSE);
	gtk_table_attach (GTK_TABLE (table_mini[0]), ruler_y1, 0, 1, 1, 2,
		GTK_FILL, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
	for(i=0; i<=NUM_TIME_SPK-1; i+=2) {
		gtk_databox_set_ruler_y (GTK_DATABOX (databox[i]), GTK_DATABOX_RULER (ruler_y1));
	}
	
	ruler_y2 = gtk_databox_ruler_new (GTK_ORIENTATION_VERTICAL);
	gtk_widget_set_sensitive (ruler_y2, FALSE);
	gtk_table_attach (GTK_TABLE (table_mini[1]), ruler_y2, 0, 1, 1, 2,
		GTK_FILL, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
	for(i=1; i<=NUM_TIME_SPK; i+=2) {
		gtk_databox_set_ruler_y (GTK_DATABOX (databox[i]), GTK_DATABOX_RULER (ruler_y2));
	}
	
	label_graph = gtk_label_new("D2-D1");
	gtk_table_attach (GTK_TABLE (table_mini[0]), label_graph, 1, 2, 0, 1,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
	label_graph = gtk_label_new("D1-D2");
	gtk_table_attach (GTK_TABLE (table_mini[1]), label_graph, 1, 2, 0, 1,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
	label_graph = gtk_label_new("D3-D1");
	gtk_table_attach (GTK_TABLE (table_mini[2]), label_graph, 1, 2, 0, 1,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
	label_graph = gtk_label_new("D1-D3");
	gtk_table_attach (GTK_TABLE (table_mini[3]), label_graph, 1, 2, 0, 1,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
	label_graph = gtk_label_new("D4-D1");
	gtk_table_attach (GTK_TABLE (table_mini[4]), label_graph, 1, 2, 0, 1,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
	label_graph = gtk_label_new("D1-D4");
	gtk_table_attach (GTK_TABLE (table_mini[5]), label_graph, 1, 2, 0, 1,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
	label_graph = gtk_label_new("D3-D2");
	gtk_table_attach (GTK_TABLE (table_mini[6]), label_graph, 1, 2, 0, 1,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
	label_graph = gtk_label_new("D2-D3");
	gtk_table_attach (GTK_TABLE (table_mini[7]), label_graph, 1, 2, 0, 1,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
	label_graph = gtk_label_new("D4-D2");
	gtk_table_attach (GTK_TABLE (table_mini[8]), label_graph, 1, 2, 0, 1,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
	label_graph = gtk_label_new("D2-D4");
	gtk_table_attach (GTK_TABLE (table_mini[9]), label_graph, 1, 2, 0, 1,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
	label_graph = gtk_label_new("D4-D3");
	gtk_table_attach (GTK_TABLE (table_mini[10]), label_graph, 1, 2, 0, 1,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
	label_graph = gtk_label_new("D3-D4");
	gtk_table_attach (GTK_TABLE (table_mini[11]), label_graph, 1, 2, 0, 1,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
	
	
	GtkWidget *tab_label = gtk_label_new("Spectras after swap");
	
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), table, tab_label);
	
	
	gtk_widget_show_all(GTK_WIDGET(notebook));
	
//	g_free((gpointer) X);
}
	
void zero_channel_find(gpointer notebook, int **n, double *res)
{
	int i, j;
	GtkWidget *table;
	GtkWidget *event_box[NUM_SUM_TIME_SPK];	
	table = gtk_table_new(8, 6, TRUE);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	
	GtkWidget *ruler[NUM_TIME_SPK]; //, *databox[NUM_TIME_SPK];
	GtkWidget *table_mini[NUM_TIME_SPK];
	GtkDataboxGraph *graph[NUM_TIME_SPK], *pick[NUM_TIME_SPK];
	GdkColor color;
	
	gfloat *X, *Y[NUM_SUM_TIME_SPK];
	gfloat *X_pick[NUM_SUM_TIME_SPK], *Y_pick[NUM_SUM_TIME_SPK];

	for(i=0; i<=NUM_SUM_TIME_SPK-1; i++)
		event_box[i] = gtk_event_box_new();

	X = g_new0 (gfloat, NUMS);
	for (i=0; i<=NUMS-1; i++) {
		X[i] = i;
	}
	
	for(i=0; i<=NUM_SUM_TIME_SPK-1; i++)
		Y[i] = g_new0 (gfloat, NUMS);
	
	for(i=0; i<=NUM_SUM_TIME_SPK-1; i++) {
		X_pick[i] = g_new0 (gfloat, 1);
		Y_pick[i] = g_new0 (gfloat, 1);
		X_pick[i][0] = res[i];
		Y_pick[i][0] = res[i+2*NUM_SUM_TIME_SPK];
	}
	
	for(i=0; i<=NUMS-1; i++) {
		Y[0][i] = n[0][i]+n[1][i]; //D1-2
		Y[1][i] = n[2][i]+n[3][i]; //D1-3
		Y[2][i] = n[4][i]+n[5][i]; //D1-4
		Y[3][i] = n[6][i]+n[7][i]; //D2-3
		Y[4][i] = n[8][i]+n[9][i]; //D2-4
		Y[5][i] = n[10][i]+n[11][i]; //D3-4
	}
	
	for(i=0; i<=NUM_SUM_TIME_SPK-1; i++)
		table_mini[i] = gtk_table_new(3, 3, FALSE);

	for(i=0; i<=NUM_SUM_TIME_SPK-1; i++) {

		databox_zero[i] = gtk_databox_new ();
		
		gtk_table_attach (GTK_TABLE (table_mini[i]), databox_zero[i], 1, 2, 1, 2,
			GTK_FILL | GTK_EXPAND | GTK_SHRINK,
			GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
		
		ruler[i] = gtk_databox_ruler_new (GTK_ORIENTATION_HORIZONTAL);
		gtk_widget_set_sensitive (ruler[i], TRUE);
		gtk_table_attach (GTK_TABLE (table_mini[i]), ruler[i], 1, 2, 2, 3,
			GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
		gtk_databox_set_ruler_x (GTK_DATABOX (databox_zero[i]), GTK_DATABOX_RULER (ruler[i]));
		gtk_databox_ruler_set_range (GTK_DATABOX_RULER (ruler[i]), 0, 4096, 2000);
		
		gtk_container_add(GTK_CONTAINER(event_box[i]), table_mini[i]);
		gtk_table_attach_defaults (GTK_TABLE(table), event_box[i], i, i+1, 2, 5);
		
		gdk_color_parse("white", &color);
		gtk_widget_modify_bg (databox_zero[i], GTK_STATE_NORMAL, &color);
		
		gdk_color_parse("black", &color);
		graph[i] = gtk_databox_points_new (NUMS, X, Y[i], &color, 2);
		gtk_databox_graph_add (GTK_DATABOX (databox_zero[i]), graph[i]);
					
		gdk_color_parse("red", &color);
		pick[i] = gtk_databox_bars_new (1, X_pick[i], Y_pick[i], &color, 1);
		gtk_databox_graph_add (GTK_DATABOX (databox_zero[i]), pick[i]);
		
		gtk_databox_auto_rescale (GTK_DATABOX (databox_zero[i]), 0.05);
		
		printf("datapox point = %p\n", event_box[i]);

		g_signal_connect(GTK_OBJECT(event_box[i]), "button_press_event", G_CALLBACK(shrink_databox), NULL);
	}
	
	GtkWidget *tab_label = gtk_label_new("Zero channel");
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), table, tab_label);
	
	gtk_widget_show_all(GTK_WIDGET(notebook));
}

void create_maintable(gpointer notebook, int **n, int **e)
{
	int i, j;
	GtkWidget *table;	
	table = gtk_table_new(8, 6, FALSE);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	
	gfloat *X;
	gfloat *Y[NUM_TIME_SPK], *E[NUM_ENERGY_SPK];
	X = g_new0 (gfloat, NUMS);
	
	for(i=0; i<=NUM_ENERGY_SPK-1; i++)
		E[i] = g_new0 (gfloat, NUMS);
	
	for(i=0; i<=NUM_TIME_SPK-1; i++)
		Y[i] = g_new0 (gfloat, NUMS);

	for (i = 0; i<=NUMS-1; i++) {
		X[i] = i;
	}
	
	for(i=0; i<=NUM_TIME_SPK-1; i++)
		for(j=0; j<=NUMS-1; j++)
			Y[i][j] = (double) n[i][j];
			
	for(i=0; i<=NUM_ENERGY_SPK-1; i++)
		for(j=0; j<=NUMS-1; j++)
			E[i][j] = (double) e[i][j];
	
	GtkWidget *table_time_mini[NUM_TIME_SPK], *table_energy_mini[NUM_ENERGY_SPK];
	GtkWidget *ruler_time[NUM_TIME_SPK], *ruler_time_y1, *ruler_time_y2, *ruler_energy_y, *ruler_energy[NUM_ENERGY_SPK];
	GtkWidget *datatable_time[NUM_TIME_SPK], *databox_time[NUM_TIME_SPK];
	GtkWidget *datatable_energy[NUM_ENERGY_SPK], *databox_energy[NUM_ENERGY_SPK];
	GtkDataboxGraph *graph[NUM_TIME_SPK], *graph_e[NUM_ENERGY_SPK];
	GdkColor color;
	GtkWidget *label_graph;
	char ItoA[3];
	char str[2];
	
	printf("1\n");
	for(i=0; i<=NUM_ENERGY_SPK-1; i++) {
		table_energy_mini[i] = gtk_table_new(3, 3, FALSE);
		
		databox_energy[i] = gtk_databox_new ();
		gtk_table_attach (GTK_TABLE (table_energy_mini[i]), databox_energy[i], 1, 2, 1, 2,
		     GTK_FILL | GTK_EXPAND | GTK_SHRINK,
		     GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
		
		ruler_energy[i] = gtk_databox_ruler_new (GTK_ORIENTATION_HORIZONTAL);
		gtk_widget_set_sensitive (ruler_energy[i], TRUE);
		gtk_table_attach (GTK_TABLE (table_energy_mini[i]), ruler_energy[i], 1, 2, 2, 3,
		     GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
		gtk_databox_set_ruler_x (GTK_DATABOX (databox_energy[i]), GTK_DATABOX_RULER (ruler_energy[i]));
		
		strcpy(str, "E");
		itoa(i+1, ItoA);
		strcat(str, ItoA);
		label_graph = gtk_label_new(str);
		gtk_table_attach (GTK_TABLE (table_energy_mini[i]), label_graph, 1, 2, 0, 1,
		     GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
		
		gtk_table_attach_defaults (GTK_TABLE(table), table_energy_mini[i], i, i+1, 0, 2);
		
		gdk_color_parse("white", &color);
		gtk_widget_modify_bg (databox_energy[i], GTK_STATE_NORMAL, &color);
		
		gdk_color_parse("black", &color);
		graph_e[i] = gtk_databox_points_new (NUMS, X, E[i], &color, 2);
		gtk_databox_graph_add (GTK_DATABOX (databox_energy[i]), graph_e[i]);
		
		gtk_databox_auto_rescale (GTK_DATABOX (databox_energy[i]), 0.05);
		
//		gtk_box_pack_start (GTK_BOX (vbox_table), table, TRUE, TRUE, 0);
	}
	
	ruler_energy_y = gtk_databox_ruler_new (GTK_ORIENTATION_VERTICAL);
	gtk_table_attach (GTK_TABLE (table_energy_mini[0]), ruler_energy_y, 0, 1, 1, 2,
			GTK_FILL, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
	gtk_widget_set_sensitive (ruler_energy_y, FALSE);
	
	for(i=0; i<=3; i++) {
		gtk_databox_set_ruler_y (GTK_DATABOX (databox_energy[i]), GTK_DATABOX_RULER (ruler_energy_y));
	}
	
	for(i=0; i<=NUM_TIME_SPK-1; i++)
		table_time_mini[i] = gtk_table_new(3, 3, FALSE);
	
	for(j=0; j<=5; j++)
		for(i=0; i<=1; i++) {			
			databox_time[i+j*2] = gtk_databox_new ();
			gtk_table_attach (GTK_TABLE (table_time_mini[i+j*2]), databox_time[i+j*2], 1, 2, 1, 2,
				GTK_FILL | GTK_EXPAND | GTK_SHRINK,
				GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
		
			ruler_time[i+j*2] = gtk_databox_ruler_new (GTK_ORIENTATION_HORIZONTAL);
			gtk_widget_set_sensitive (ruler_time[i+j*2], TRUE);
			gtk_table_attach (GTK_TABLE (table_time_mini[i*6+j]), ruler_time[i+j*2], 1, 2, 2, 3,
				GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
			gtk_databox_set_ruler_x (GTK_DATABOX (databox_time[i+j*2]), GTK_DATABOX_RULER (ruler_time[i+j*2]));
		
			gtk_table_attach_defaults (GTK_TABLE(table), table_time_mini[i+j*2], j, j+1, 2+i*3, 2+(i+1)*3);
		
			gdk_color_parse("white", &color);
			gtk_widget_modify_bg (databox_time[i+j*2], GTK_STATE_NORMAL, &color);
		
			gdk_color_parse("black", &color);
			graph[i+j*2] = gtk_databox_points_new (NUMS, X, Y[i+j*2], &color, 2);
			gtk_databox_graph_add (GTK_DATABOX (databox_time[i+j*2]), graph[i+j*2]);
		
			gtk_databox_auto_rescale (GTK_DATABOX (databox_time[i+j*2]), 0.05);
		}

	ruler_time_y1 = gtk_databox_ruler_new (GTK_ORIENTATION_VERTICAL);
	gtk_widget_set_sensitive (ruler_time_y1, FALSE);
	gtk_table_attach (GTK_TABLE (table_time_mini[0]), ruler_time_y1, 0, 1, 1, 2,
		GTK_FILL, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
	for(i=0; i<=NUM_TIME_SPK-1; i+=2) {
		gtk_databox_set_ruler_y (GTK_DATABOX (databox_time[i]), GTK_DATABOX_RULER (ruler_time_y1));
	}
	
	ruler_time_y2 = gtk_databox_ruler_new (GTK_ORIENTATION_VERTICAL);
	gtk_widget_set_sensitive (ruler_time_y2, FALSE);
	gtk_table_attach (GTK_TABLE (table_time_mini[1]), ruler_time_y2, 0, 1, 1, 2,
		GTK_FILL, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
	for(i=1; i<=NUM_TIME_SPK-1; i+=2) {
		gtk_databox_set_ruler_y (GTK_DATABOX (databox_time[i]), GTK_DATABOX_RULER (ruler_time_y2));
	}
	printf("2\n");
	label_graph = gtk_label_new("D2-D1");
	gtk_table_attach (GTK_TABLE (table_time_mini[0]), label_graph, 1, 2, 0, 1,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
	label_graph = gtk_label_new("D1-D2");
	gtk_table_attach (GTK_TABLE (table_time_mini[1]), label_graph, 1, 2, 0, 1,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
	label_graph = gtk_label_new("D3-D1");
	gtk_table_attach (GTK_TABLE (table_time_mini[2]), label_graph, 1, 2, 0, 1,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
	label_graph = gtk_label_new("D1-D3");
	gtk_table_attach (GTK_TABLE (table_time_mini[3]), label_graph, 1, 2, 0, 1,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
	label_graph = gtk_label_new("D4-D1");
	gtk_table_attach (GTK_TABLE (table_time_mini[4]), label_graph, 1, 2, 0, 1,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
	label_graph = gtk_label_new("D1-D4");
	gtk_table_attach (GTK_TABLE (table_time_mini[5]), label_graph, 1, 2, 0, 1,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
	label_graph = gtk_label_new("D3-D2");
	gtk_table_attach (GTK_TABLE (table_time_mini[6]), label_graph, 1, 2, 0, 1,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
	label_graph = gtk_label_new("D2-D3");
	gtk_table_attach (GTK_TABLE (table_time_mini[7]), label_graph, 1, 2, 0, 1,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
	label_graph = gtk_label_new("D4-D2");
	gtk_table_attach (GTK_TABLE (table_time_mini[8]), label_graph, 1, 2, 0, 1,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
	label_graph = gtk_label_new("D2-D4");
	gtk_table_attach (GTK_TABLE (table_time_mini[9]), label_graph, 1, 2, 0, 1,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
	label_graph = gtk_label_new("D4-D3");
	gtk_table_attach (GTK_TABLE (table_time_mini[10]), label_graph, 1, 2, 0, 1,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
	label_graph = gtk_label_new("D3-D4");
	gtk_table_attach (GTK_TABLE (table_time_mini[11]), label_graph, 1, 2, 0, 1,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
	
	
	GtkWidget *zoom_button;
	
	zoom_button = gtk_button_new_with_label ("zoom");
	for(i=0; i<=NUM_TIME_SPK-1; i++) 
		g_signal_connect(GTK_OBJECT (zoom_button), "clicked",
			G_CALLBACK (zoom), databox_time[i]);
		
	GtkWidget *zoomout_button;
	
	zoomout_button = gtk_button_new_with_label ("zoom out");
	for(i=0; i<=NUM_TIME_SPK-1; i++) 
		g_signal_connect(GTK_OBJECT (zoomout_button), "clicked",
			G_CALLBACK (zoom_out), databox_time[i]);
	
	GtkWidget *up, *down, *right, *left;
	
	up = gtk_button_new_with_label ("UP");
	for(i=0; i<=NUM_TIME_SPK-1; i++) 
		g_signal_connect(GTK_OBJECT (up), "clicked",
			G_CALLBACK (move_up), databox_time[i]);
	
	down = gtk_button_new_with_label ("DOWN");
	for(i=0; i<=NUM_TIME_SPK-1; i++) 
		g_signal_connect(GTK_OBJECT (down), "clicked",
			G_CALLBACK (move_down), databox_time[i]);
	
	right = gtk_button_new_with_label ("RIGHT");
	for(i=0; i<=NUM_TIME_SPK-1; i++) 
		g_signal_connect(GTK_OBJECT (right), "clicked",
			G_CALLBACK (move_right), databox_time[i]);
		
	
	left = gtk_button_new_with_label ("LEFT");
	for(i=0; i<=NUM_TIME_SPK-1; i++) 
		g_signal_connect(GTK_OBJECT (left), "clicked",
			G_CALLBACK (move_left), databox_time[i]);
	
	GtkWidget *button_table = gtk_table_new(3, 9, FALSE);
	
	gtk_table_attach_defaults(GTK_TABLE(table), button_table, 4, 6, 0, 2);
	
	gtk_table_attach_defaults(GTK_TABLE(button_table), zoom_button, 0, 1, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(button_table), zoomout_button, 0, 1, 1, 2);
	gtk_table_attach_defaults(GTK_TABLE(button_table), up, 1, 2, 1, 2);
	gtk_table_attach_defaults(GTK_TABLE(button_table), down, 1, 2, 2, 3);
	gtk_table_attach_defaults(GTK_TABLE(button_table), left, 0, 1, 2, 3);
	gtk_table_attach_defaults(GTK_TABLE(button_table), right, 2, 3, 2, 3);
	
	
	GtkWidget *tab_label = gtk_label_new("Spectras");
	
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), table, tab_label);
	
/*	g_signal_connect (G_OBJECT (databox_energy[0]), "button_press_event",
		     G_CALLBACK (show_button_press_cb), E[0]); */
	for(i=0; i<=NUM_ENERGY_SPK-1; i++)
		g_signal_connect (G_OBJECT (databox_energy[i]), "button_press_event",
		     G_CALLBACK (show_button_press_cb), E[i]);
	for(i=0; i<=NUM_TIME_SPK-1; i++)
		g_signal_connect (G_OBJECT (databox_time[i]), "button_press_event",
		     G_CALLBACK (show_button_press_cb), Y[i]);
	
	gtk_widget_show_all(GTK_WIDGET(notebook));
	
	
/*	g_free((gpointer) X);
	
	for(i=0; i<=NUM_ENERGY_SPK-1; i++)
		g_free((gpointer) E[i]);
	
	for(i=0; i<=NUM_TIME_SPK-1; i++)
		g_free((gpointer) Y[i]); */
}

void choose_folder(GtkWidget *menuitem, gpointer user_data)
{
	read_spk();
	
	GtkWidget *image = GTK_WIDGET(NULL);
	GtkWidget *toplevel = gtk_widget_get_toplevel (image);
	GtkFileFilter *filter = gtk_file_filter_new ();
	GtkWidget *dialog = gtk_file_chooser_dialog_new (("Open image"),
	                                                 GTK_WINDOW (toplevel),
	                                                 GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
	                                                 GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
	                                                 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	                                                 NULL);
	                                                 
	int *N[NUM_TIME_SPK], *initial_spk[NUM_TIME_SPK], *e[NUM_TIME_SPK];
	double *y[NUM_TIME_SPK];
	double *F[NUM_TIME_SPK];
	double *compX90, *compX180;                   
	int i;
	
	for(i=0; i<=NUM_TIME_SPK-1; i++) {
		N[i] = g_new0(int, NUMS);
		initial_spk[i] = g_new0(int, NUMS);
		e[i] = g_new0(int, NUMS);
		y[i] = g_new0(double, NUMS);
		F[i] = g_new0(double, NUMS);
	}
	
	compX90 = g_new0(double, NUMS/COMP);
	compX180 = g_new0(double, NUMS/COMP);
	
	gtk_file_filter_add_pattern (filter, "*.SPK");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog),
	                             filter);

	switch (gtk_dialog_run (GTK_DIALOG (dialog)))
	{
		case GTK_RESPONSE_ACCEPT:
		{
			folder2spk =
				gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
			printf("\nfolder_chosen = %s\n", folder2spk);
			strcat(folder2spk, "/");
			
			gint i;
			for(i=0; i<=5; i++)
				gtk_notebook_remove_page(GTK_NOTEBOOK (user_data), -1);
			
			printf ("RESULT= %d\n", anisotropy (results, N, initial_spk, e, y, F, compX90, compX180, aniz, anizd));
			
			printf("user_data %p\n", user_data);
			create_maintable(user_data, initial_spk, e);
			
			zero_channel_find(user_data, initial_spk, results);
		
			swap_spk(user_data, N, results);
			
			background(user_data, y, F, results);
			
			coincidence(user_data, compX90, compX180);
			
			anisotropy_plot(user_data);
			break;
		}
		default:
			break;
	}
	
	while(g_main_context_iteration(NULL, FALSE));
	gtk_widget_destroy (dialog);
}

void next_prev_tab(GtkWidget *menuitem, gpointer user_data)
{
	
}

void send_settings_cb( GtkWidget *widget,
               kernel_msg1 *message2kernel )
{
	char s[4][32];
	int i;
	char msg_text[128];
	
	for(i=0; i<=NUM_ENERGY_SPK-1; i++)
	 sprintf(s[i], "{%04d-%04d;%04d-%04d}", atoi( gtk_entry_get_text(GTK_ENTRY(message2kernel->spk[i][0])) ), \
	 atoi( gtk_entry_get_text(GTK_ENTRY(message2kernel->spk[i][1])) ), atoi( gtk_entry_get_text(GTK_ENTRY(message2kernel->spk[i][2])) ), \
	 atoi( gtk_entry_get_text(GTK_ENTRY(message2kernel->spk[i][3])) )  );
	
	for(i=0; i<=NUM_ENERGY_SPK-1; i++)
		if(atoi( gtk_entry_get_text(GTK_ENTRY(message2kernel->spk[i][0])) ) > atoi( gtk_entry_get_text(GTK_ENTRY(message2kernel->spk[i][1])) ) || \
		atoi( gtk_entry_get_text(GTK_ENTRY(message2kernel->spk[i][1])) ) > atoi( gtk_entry_get_text(GTK_ENTRY(message2kernel->spk[i][2])) ) || \
		atoi( gtk_entry_get_text(GTK_ENTRY(message2kernel->spk[i][2])) ) > atoi( gtk_entry_get_text(GTK_ENTRY(message2kernel->spk[i][3])) ) ) {
			GtkWidget *err_dialog = gtk_message_dialog_new (NULL, 
										GTK_DIALOG_DESTROY_WITH_PARENT,
										GTK_MESSAGE_INFO,
										GTK_BUTTONS_OK,
										"Please, check typped channels in string E%d", i+1);
			gtk_window_set_title(GTK_WINDOW(err_dialog), "WARNING!");
			gtk_dialog_run(GTK_DIALOG(err_dialog));
			gtk_widget_destroy(err_dialog);
			break;
		}
	
	sprintf(msg_text, "%s%s%s%s-%04d", s[0], s[1], s[2], s[3], atoi(gtk_entry_get_text(GTK_ENTRY(message2kernel->time))) );
	
/*	for(i=0; i<=NUM_ENERGY_SPK-1; i++)
		g_print ("Hello again - %s msg\n", s[i]); */
		
	g_print("MSG = %s\n", msg_text);
}

/* Create callbacks that implement our Actions */
static void     
lookup_character_action ()
{
	g_print ("settings\n");
        
	GtkWidget *window, *table, *vbox;
	GtkWidget *label, *entry1_left[NUM_ENERGY_SPK], *entry1_right[NUM_ENERGY_SPK];
	GtkWidget *entry2_left[NUM_ENERGY_SPK], *entry2_right[NUM_ENERGY_SPK];
	GtkWidget *entry_time;
	GtkWidget *label_mark;
	GtkWidget *send_button = gtk_button_new_with_label("Send settings");
	int i;
	char s[3];
        
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW(window), "Settings");
	gtk_container_set_border_width (GTK_CONTAINER(window), 0);
	gtk_widget_set_size_request (window, 330, 165);
	
	vbox = gtk_vbox_new (FALSE, 0);
	
	table = gtk_table_new(7, 6, TRUE);
	
	for(i=0; i<=NUM_ENERGY_SPK-1; i++) {
		entry1_left[i] = gtk_entry_new();
		entry1_right[i] = gtk_entry_new();
		entry2_left[i] = gtk_entry_new();
		entry2_right[i] = gtk_entry_new();
	}
	
//	label = gtk_label_new("E1");
	for(i=0; i<=NUM_ENERGY_SPK-1; i++) {
		label_mark = gtk_label_new("-");
		sprintf(s, "E%d", i+1);
		label = gtk_label_new(s);
		gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, i, i+1);
		gtk_table_attach_defaults(GTK_TABLE(table), entry1_left[i], 1, 2, i, i+1);
		gtk_table_attach_defaults(GTK_TABLE(table), label_mark, 2, 3, i, i+1);
		gtk_table_attach_defaults(GTK_TABLE(table), entry1_right[i], 3, 4, i, i+1);
		label_mark = gtk_label_new("-");
		gtk_table_attach_defaults(GTK_TABLE(table), entry2_left[i], 4, 5, i, i+1);
		gtk_table_attach_defaults(GTK_TABLE(table), label_mark, 5, 6, i, i+1);
		gtk_table_attach_defaults(GTK_TABLE(table), entry2_right[i], 6, 7, i, i+1);
	}
	
	kernel_msg1 *message2kernel = (kernel_msg1 *)malloc(sizeof(kernel_msg1));
/*	sprintf(message2kernel->spk1, "{%d-%d;%d-%d}", atoi( gtk_entry_get_text(GTK_ENTRY(entry1_left[0])) ), \
	atoi( gtk_entry_get_text(GTK_ENTRY(entry1_right[0])) ), atoi( gtk_entry_get_text(GTK_ENTRY(entry2_left[0])) ), \
	atoi( gtk_entry_get_text(GTK_ENTRY(entry2_right[0])) )  ); */
	
	for(i=0; i<=NUM_ENERGY_SPK-1; i++) {
		message2kernel->spk[i][0] = entry1_left[i];
		message2kernel->spk[i][1] = entry1_right[i];
		message2kernel->spk[i][2] = entry2_left[i];
		message2kernel->spk[i][3] = entry2_right[i];
	/*	if( atoi( gtk_entry_get_text(GTK_ENTRY(entry1_left[i])) ) > atoi( gtk_entry_get_text(GTK_ENTRY(entry1_right[i])) ) || \
			atoi( gtk_entry_get_text(GTK_ENTRY(entry1_right[i])) ) > atoi( gtk_entry_get_text(GTK_ENTRY(entry2_left[i])) ) || \
			atoi( gtk_entry_get_text(GTK_ENTRY(entry2_left[i])) ) > atoi( gtk_entry_get_text(GTK_ENTRY(entry2_right[i])) ) ) 
		{
			GtkWidget *err_dialog = gtk_message_dialog_new (GTK_WINDOW(window), 
									GTK_DIALOG_DESTROY_WITH_PARENT,
									GTK_MESSAGE_INFO,
									GTK_BUTTONS_OK,
									"Error!");
			gtk_window_set_title(GTK_WINDOW(err_dialog), "Error in channel set");
			gtk_dialog_run(GTK_DIALOG(err_dialog));
			gtk_widget_destroy(err_dialog);
		} */
	}
	
	label = gtk_label_new("Time (hrs): ");
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 2, 4, 5);
	entry_time = gtk_entry_new();
	gtk_table_attach_defaults(GTK_TABLE(table), entry_time, 2, 4, 4, 5);
	message2kernel->time = entry_time;
	
	gtk_table_attach_defaults(GTK_TABLE(table), send_button, 5, 7, 5, 6);
	
	g_signal_connect (G_OBJECT (send_button), "clicked",
		     G_CALLBACK (send_settings_cb), message2kernel);
	
	gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER(window), vbox);
	
	gtk_widget_show_all(window);
}

static void     
clear_character_action ()
{
        g_print ("clear\n");
}

static void     
quit_action ()
{
        gtk_main_quit();
}

static void prev_tab(GtkWidget *widget, gpointer user_data)
{
	gtk_notebook_prev_page(GTK_NOTEBOOK(user_data));
}

static void next_tab(GtkWidget *widget, gpointer user_data)
{
	gtk_notebook_next_page(GTK_NOTEBOOK(user_data));
}

static GtkActionEntry entries[] = 
{
  { "FileMenuAction", NULL, "_File" },                  /* name, stock id, label */
  { "CharacterMenuAction", NULL, "_Character" },

  { "Open", GTK_STOCK_OPEN, "_Open", 
	"<control>O", "Open a file", 
	G_CALLBACK (choose_folder) },	
  
  { "LookupAction", GTK_STOCK_FIND,                     /* name, stock id */
    "_Settings", "<control>L",                            /* label, accelerator */
    "Set time and chanels",                      /* tooltip */ 
    G_CALLBACK (lookup_character_action) },
    
  { "ClearAction", GTK_STOCK_OPEN,
    "_Clear","<control>C",  
    "Clear the drawing area",
    G_CALLBACK (clear_character_action) },
    
  { "QuitAction", GTK_STOCK_QUIT,
    "_Quit", "<control>Q",    
    "Quit",
    G_CALLBACK (quit_action) },

};

static guint n_entries = G_N_ELEMENTS (entries);

static void create_mainwindow (void)
{
	GtkWidget *window, *scrolled_win, *vbox_scroll, *vbox_main, *menu, *toolbar;
	GtkToolItem *button_item;
	GtkActionGroup *group;
	GtkUIManager *uimanager;
	GtkWidget *table;
	GtkWidget *notebook;
	GtkWidget *label;
	
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW(window), "JINR v 0.2 beta");
	gtk_container_set_border_width (GTK_CONTAINER(window), 5);
	gtk_widget_set_size_request (window, 800, 600);
	
	g_signal_connect (G_OBJECT(window), "destroy", G_CALLBACK (gtk_main_quit), NULL);
	
	notebook = gtk_notebook_new ();
	gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), GTK_POS_TOP);
	printf("%p\n", notebook);
	
	group = gtk_action_group_new ("MainActionGroup");
	gtk_action_group_add_actions (group, entries, n_entries, notebook);

	uimanager = gtk_ui_manager_new ();
	gtk_ui_manager_insert_action_group (uimanager, group, 0);
	gtk_ui_manager_add_ui_from_file (uimanager, "test-ui-manager-ui.xml", NULL);
	menu = gtk_ui_manager_get_widget (uimanager, "/MainMenu");
	
	toolbar = gtk_toolbar_new();
	
	button_item = gtk_tool_button_new_from_stock(GTK_STOCK_GO_BACK);
	g_signal_connect(GTK_OBJECT (button_item), "clicked",
		G_CALLBACK (prev_tab), notebook);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), button_item, 0);
	
	button_item = gtk_tool_button_new_from_stock(GTK_STOCK_GO_FORWARD);
	g_signal_connect(GTK_OBJECT (button_item), "clicked",
		G_CALLBACK (next_tab), notebook);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), button_item, 1);
	
	table = gtk_table_new (3, 6, FALSE);
	
	label = gtk_label_new ("");
	gtk_label_set_markup(GTK_LABEL (label), "<span style=\"italic\" font_desc=\"18.0\">Welcome! Добре дошъл! Willkommen! Добро пожаловать!</span>");
	gtk_table_attach_defaults (GTK_TABLE (table), notebook, 0, 3, 0, 6);
	
	scrolled_win = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_win), table);
	
	vbox_main = gtk_vbox_new (FALSE, 5);
	gtk_box_pack_start (GTK_BOX (vbox_main), menu, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox_main), toolbar, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox_main), scrolled_win, TRUE, TRUE, 5);
	
	gtk_container_add (GTK_CONTAINER(window), vbox_main);
	
	gtk_widget_show_all(window);
}

int main (int argc, char *argv[])
{
	gtk_init(&argc, &argv);
	
	create_mainwindow();
	
	gtk_main();
	return 0;
}

int DenisCalc(const gfloat N[], const unsigned int F[], int LCH, int RCH, char flag, float time_N, float time_F, float u[])
{	
/* IF flag == 0 BEGIN */
	if (flag == 0) {
		/* IF no data */
		if (RCH <= LCH) return -13;
		
		static double *Q;
		double time_ratio1 = 1.0, time_ratio2 = 1.0;
		double avlb, avrb, k, b;
		int i, j;
		double s = 0.0, s_err = 0.0;
		double p = 0.0, p_err = 0.0;
		int c = 0, e = 0 ;
		static double *G;
		double max, hm;
		double t;
		
		/* Allocate memory for Q array + test */
		Q = (double *)malloc(sizeof(double) * (RCH+3));
		if (Q == NULL) return -1;
	
		/* Calculate time ratio */
		if (time_N > time_F) 
		{  
 	     // printf("flag=%d time_N=%e  time_F=%e \n",(int)flag,time_N , time_F );
			if (time_F < EPS) return -2;
			time_ratio1 = time_N/time_F;
		}
		
		if (time_N < time_F) {
			if (time_N < EPS) return -3; 
			time_ratio2 = time_F/time_N;
		}
	
		for (i = 0; i <= RCH+2; i++) {
			double a;
			a = ((double) N[i])/time_ratio1 - ((double) F[i])/time_ratio2;
			if (a > EPS) 
				Q[i] = a;
			else Q[i] = 0.0;
		}
	
		/* Calculate b, k of the background line (y=k*x+b) */
		avlb = (Q[LCH-2]+Q[LCH-1]+Q[LCH])/3.0;
		avrb = (Q[RCH]+Q[RCH+1]+Q[RCH+2])/3.0;
		k = (avrb-avlb)/(RCH-LCH+2);
		b = Q[LCH] - k*(LCH);
	
		/* Calculate S, S error, p */
		for(i = LCH; i <= RCH; i++) {
			if ((t = Q[i] - (k*i + b)) < EPS ) t = 0.0;
			s = s + t;
			if (t > EPS)
				s_err = s_err + (Q[i] + k*i+b);
			p = p + t*i;
		}
	
		/* IF s<=0 or p<=0 only */
		if (s < EPS || p < EPS) 
		{
		 printf("s=%e  p=%e  EPS=%f \n",s ,p ,(float)EPS  );
		  return -14;
		}
	
		u[0] = (float) s; 
		u[1] = (float) sqrt(s_err);
		u[2] = (float) (p = p/s);
	
		/* Calculate p error */
		for(i = LCH; i <= RCH; i++) {
			  if((t = Q[i] - (k*i+b)) < EPS) t = 0.0;
		      p_err = p_err + t*(p - i)*(p - i);
 			//p_err = p_err + (float) ((Q[i] - (k*i+b))*sqr(p - i));
 		}
		
		/* IF p_err <= 0 */	
		if (p_err < EPS) return -4;	
		
		u[3] = (float) ( p_err = sqrt(p_err/((RCH-LCH)*s)) );
	
		/* Allocate memory for G array + test */		
		G = (double *)malloc(sizeof(double) * (RCH-LCH+1));
		if (G == NULL) { free(Q); return -5; }
	
		/* Calculate G[K] = Q[K] - B[K] */
		for(i = LCH; i <= RCH; i++) {
			if ((t = Q[i] - (k*i+b)) < EPS) t = 0.0;
			G[i-LCH] = t;
		}
		
		/* Find max element in G[] array */
		for(i = 0, j = 0, max = G[0]; i <= RCH-LCH; i++)
			if (G[i] >= max) {
				max = G[i];
				j = i;
			}
		
		/* Half maximum */
		hm = G[j]/2;
	
		/* Calculate c and e */
		for(i = 0; i <= RCH-LCH; i++) {
			if (G[i] < hm && G[i+1] > hm && i < j)
				c = i;
			if (G[i] > hm && G[i+1] < hm && i > j)
				e = i;
		}
		
		/* IF if G[c+1]-G[c] == 0 or G[e]-G[e+1] == 0 */
		if ((G[c+1]-G[c]) < EPS || (G[e]-G[e+1]) < EPS) return -6; 
		
		u[4] = (float) ((e-c-1)+(G[c+1]-hm)/(G[c+1]-G[c])+(G[e]-hm)/(G[e]-G[e+1]));
	
		free(Q);
		free(G);
		
	return 0;
/* IF flag == 0 END */
	}
/* IF flag == 1 BEGIN */
	if (flag !=0 ) {
		double avlb, avrb, k, b;
		int i, j;
		double s = 0.0, s_err = 0.0;
		double p = 0.0, p_err = 0.0;
		int c = 0, e = 0 ;
		static double *G;
		double max, hm;
		double t;
	
		/* IF no data */
		if (RCH <= LCH) return -7;
	
		/* Calculate b, k of the background line (y=k*x+b) */
		avlb = (N[LCH-2]+N[LCH-1]+N[LCH])/3.0;
		avrb = (N[RCH]+N[RCH+1]+N[RCH+2])/3.0;
		k = (avrb-avlb)/(RCH-LCH+2);
		b = N[LCH] - k*(LCH);
	
		/* Calculate S, S error, p */
		for(i = LCH; i <= RCH; i++) {
			if ((t = N[i] - (k*i + b)) < EPS ) t = 0.0;
			s = s + t;
			if (t > EPS)
				s_err = s_err + (N[i] + k*i+b);
			p = p + t*i;
		}
	
		/* IF s<=0 or p<=0 only */
		if (s < EPS || p < EPS) return -8;
	
		u[0] = (float) s; 
		u[1] = (float) sqrt(s_err);
		u[2] = (float) (p = p/s);
	
		/* Calculate p error */
		for(i = LCH; i <= RCH; i++) {
			if((t = N[i] - (k*i+b)) < EPS) t = 0.0;
 		    p_err = p_err + t*(p - i)*(p - i);
//			p_err = p_err + (float) ((N[i] - (k*i+b))*sqr(p - i));
		}
			
		/* IF p_err <= 0 */	
		if (p_err < EPS) return -9;
			
		u[3] = (float) ( p_err = sqrt(p_err/((RCH-LCH)*s)) );
		
		/* Allocate memory for G array + test */		
		G = (double *)malloc(sizeof(double) * (RCH-LCH+1));
		if (G == NULL) return -10;
	
		/* Calculate G[K] = N[K] - B[K] */
		for(i = LCH; i <= RCH; i++) {
			if ((t = N[i] - (k*i+b)) < EPS) t = 0.0;
			G[i-LCH] = t;
		}
		
		/* Find max element in G[] array */
		for(i = 0, j = 0, max = G[0]; i <= RCH-LCH; i++)
			if (G[i] >= max) {
				max = G[i];
				j = i;
			}
		
		/* Half maximum */
		hm = G[j]/2;
	
		/* Calculate c and e */
		for(i = 0; i <= RCH-LCH; i++) {
			if (G[i] < hm && G[i+1] > hm && i < j)
				c = i;
			if (G[i] > hm && G[i+1] < hm && i > j)
				e = i;
		}
		
		/* IF if G[c+1]-G[c] == 0 or G[e]-G[e+1] == 0 */
		if ((G[c+1]-G[c]) < EPS || (G[e]-G[e+1]) < EPS) return -11; 
		
		u[4] = (float) ((e-c-1)+(G[c+1]-hm)/(G[c+1]-G[c])+(G[e]-hm)/(G[e]-G[e+1]));
		
		free(G);
	return 0;
/* IF flag == 1 END */	
	}	

return -12;	
}

int read_spk()
{
	int i;
	int fd_in, fd_out[16];
	char name[7];
	char ItoA[3];
	
	fd_in = open(SIMPLE_DEVICE, O_RDWR);
	if (fd_in < 0) {
        perror("error in device");
        return -1;
	}
	
	struct truedata spectras;
	read(fd_in, &spectras, sizeof(struct truedata));
	close(fd_in);
	
	for(i=0; i<=11; i++) {
		strcpy(name, folder2readspk);
		strcat(name, "TIME");
		itoa(i+1, ItoA);
		strcat(name, ItoA);
		strcat(name, ".SPK");
		fd_out[i] = open(name, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR);
		
		if (fd_out[i] < 0) {
			perror("error in device");
			exit(1);
		}
		lseek(fd_out[i], 512, 0);
		
		write(fd_out[i], spectras.timespk[i], MAX_CH*sizeof(int));
		close(fd_out[i]);
	}
	
	for(i=12; i<=15; i++) {
		strcpy(name, folder2readspk);
		strcat(name, "BUFKA");
		itoa(i-11, ItoA);
		strcat(name, ItoA);
		strcat(name, ".SPK");
		fd_out[i] = open(name, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR);
		
		if (fd_out[i] < 0) {
			perror("error in device");
			exit(1);
		}
		lseek(fd_out[i], 512, 0);
		
		write(fd_out[i], spectras.energyspk[i-12], MAX_CH*sizeof(int));
		close(fd_out[i]);
	}
	
	printf("read online spk ok!!!\n");
	
return 0;
}

int send_netlink_msg(char *msg_text)
{
	char buffer[MAX_NLH_MSG];
	struct sockaddr_nl s_nladdr, d_nladdr;
	struct msghdr msg;
	struct nlmsghdr *nlh=NULL;
	struct iovec iov;
	int fd;
	
	strcpy(buffer, msg_text);
	
	fd=socket(AF_NETLINK, SOCK_RAW, NETLINK_NITRO);

	/* source address */
	memset(&s_nladdr, 0, sizeof(s_nladdr));
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

int graph_gnuplot(gnuplot_ctrl *h, gfloat *Y)
{
	printf("gnuploth Y[0] = %d\n", Y[0]);
	
	char *nameBuff = (char *)malloc(36*sizeof(char));
    // buffer to hold data to be written/read to/from temporary file
    char *buffer = (char *)malloc(24*sizeof(char));
    int filedes = -1, count=0;

    // memset the buffers to 0
    memset(nameBuff,0,sizeof(nameBuff));
    memset(buffer,0,sizeof(buffer));

    // Copy the relevant information in the buffers
    strncpy(nameBuff,"/tmp/myTmpFile-XXXXXX",21);
    strncpy(buffer,"Hello World",11);

    errno = 0;
    // Create the temporary file, this function will replace the 'X's
    filedes = mkstemp(nameBuff);

    // Call unlink so that whenever the file is closed or the program exits
    // the temporary file is deleted
    unlink(nameBuff);

    if(filedes<1)
    {
        printf("\n Creation of temp file failed with error [%s]\n",strerror(errno));
        return 1;
    }
    else
    {
        printf("\n Temporary file [%s] created\n", nameBuff);
    }
    
    FILE *stream = fopen(nameBuff, "w");
    
    int i;
    for(i=0; i<NUMS; i++) {
		fprintf(stream, "%f %f\n", (double)i, (double)Y[i]);
	//	borders->N[i] = (unsigned int)E[i];
	}
	fclose(stream);
	
/*		for(i=1000; i<=1005; i++)
		printf("%d - %d - %f\n", i, borders->N[i], E[i]);*/
	
	gnuplot_cmd(h, "set decimalsign locale \"ru_RU.UTF-8\"");
	gnuplot_cmd(h, "set xrange[0:4000]");
//	gnuplot_cmd(h, "set yrange[0:4600]");
	gnuplot_cmd(h, "set autoscale y");
	gnuplot_cmd(h, "set nomultiplot");
	gnuplot_cmd(h, "set mouse doubleclick 0");
	gnuplot_cmd(h, "plot '%s' using 1:2:(1.0) smooth acsplines, '%s' with points lt 2", nameBuff, nameBuff);
//	gnuplot_cmd(h, "plot sin(x) with points, cos(x)");

/*	g_signal_connect (G_OBJECT (window), "button-clicked",
		     G_CALLBACK (printf("socket was clicked\n")), NULL); */

	free(nameBuff);
	free(buffer);	
}
