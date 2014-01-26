#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <dirent.h>

#include <cairo.h>
#include <gtk/gtk.h>
#include <math.h>

#define SIMPLE_DEVICE "/dev/simple"

#define MAX_CH 4096
#define NUM_TIME_SPK	12
#define NUM_ENERGY_SPK	4

#define Y_SCALE	0.8
#define EPS 0.00001

#define X_SIZE_PREPLOT	150
#define Y_SIZE_PREPLOT 140
#define X_SIZE_ANALYZE	480
#define Y_SIZE_ANALYZE	480

#define ZOOM_EN_X	200
#define ZOOM_EN_Y	0.5
#define LR_EN_X	100

#define READ_ONLINE_PERIOD 20000 

extern int COMP = 8;

const int TIME_SHIFT_MAX = 1000;

typedef struct {
	int timespk[12][MAX_CH];
	int energyspk[4][MAX_CH];
} dataspk;
dataspk *spectras, *spectras0;

struct {
  int flag;
  int flage;
  double x[2];
  double y[2];
  double x3[2];
  double y3[2];
} glob;

struct {
  char cbe[4];
  char x;
  char y;
  char zoom;
  char up;
  char down;
  char draw;
  char button;
  char exec;
  double exec_range[2];
  char manual_range;
  
  char left_en;
  char right_en;
  char zoom_eny;
  char log_en;
  char undo_en;
} gflag;

struct {
	int maximize;
	
	int EN;
	int zoom_en;
	int zoom_eny;
	int log_en;
	int left_en;
	int right_en;
	char undo_en;
	
	int T;
	int zoom_t;
	int zoom_ty;
	int left_t;
	int log_t;
	int right_t;
	char undo_t;
} mainflag;

struct {
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
	int x_start;
	int x_stop;
	int x_left;
	int width;
	
	double y_comp;
	
	GtkWidget *entry[2]; // 0 - entry X, 1 - entry Y
	
	GtkWidget *entry_lrange;
	GtkWidget *entry_rrange;
} coord_xy_en;

extern char *folder2spk = "/home/das/job/EbE/";
static char *folder2initialspk = "./";
extern char *folder2readspk ="./";

//int LD[8] = {401, 660, 235, 440, 267, 439, 292, 563};
//int RD[8] = {744, 1001, 514, 792, 518, 708, 678, 911};

double a_en = 0.0, b_en = 0.0;

GtkTextBuffer *energyspk_buffer_execute;

GtkWidget *timer_label;
extern GtkTextBuffer *info_buffer;

pthread_t tid;

GtkWidget *statusbar;
extern GtkWidget *spinner_get_data;
guint sb_context_id;
GtkWidget *table;

GtkWidget *main_table, *hbox_en, *hbox_t[2], *hgraph_box[16];
GtkWidget *darea_maximize;
extern GtkWidget *label_info;
GtkWidget *button_en_log, *button_t_log;
GtkWidget *button_log_en;

double globscalex;

static void do_drawing(cairo_t *, gpointer);
static void analyze_energy_drawing(GtkWidget *, cairo_t *);

extern cairo_surface_t *pt_surface();
cairo_surface_t *xy_surface(cairo_t *, int , int , double *, double *, int , int);

void maximize_func(GtkWidget *, gpointer);

void choose_aniz_comp(GtkWidget *widget, gpointer window);
void plot_aniz(GtkWidget *widget, gpointer data);
void analyze_time_spk(GtkWidget *widget, gpointer data);

void start_expos(GtkWidget *widget, gpointer data);
void stop_expos(GtkWidget *widget, gpointer data);
void clear_expos(GtkWidget *widget, gpointer data);
void view_expos(GtkWidget *widget, gpointer window);
void save_as_spk(GtkWidget *widget, gpointer data);
void gtk_main_quit_and_save_conf(GtkWidget *widget, gpointer data);
void read_conf_file();

long int compare_dates(const char *date);
char *convert_time_to_hms(long int time_in_seconds);

int DenisCalc(const int N[], const unsigned int F[], int LCH, int RCH, char flag, float time_N, float time_F, float u[])
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
		double s_en = 0.0, s_err_en = 0.0;
		double p = 0.0, p_err = 0.0;
		int c = 0, e = 0 ;
		static double *G;
		double max, hm;
		double t;
		double sqr_clear = 0.0;
	
		/* IF no data */
		if (RCH <= LCH) return -7;
	
		for(i=LCH; i<=RCH; i++)
			sqr_clear = sqr_clear + (double)N[i];
		
		u[8] = (float)sqr_clear;
		printf("sqr clear = %.2f u[7] = %.2f\n", sqr_clear, u[7]);
	
		/* Calculate b, k of the background line (y=k*x+b) */
		avlb = (N[LCH-2]+N[LCH-1]+N[LCH])/3.0;
		avrb = (N[RCH]+N[RCH+1]+N[RCH+2])/3.0;
		k = (avrb-avlb)/(RCH-LCH+2);
		b = N[LCH] - k*(LCH);
	
		u[5] = k;
		u[6] = b;
	
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
		
		u[7] = (float) (( (G[c+1]-hm)/(G[c+1]-G[c])+(G[e]-hm)/(G[e]-G[e+1]) )*sqrt( 1.0/fabs(G[c+1]-hm) + 1.0/fabs(G[c+1]-G[c]) + 1.0/fabs(G[e]-hm) + 1.0/fabs(G[e]-G[e+1]) ));
		
	return 0;
/* IF flag == 1 END */	
	}	

return -12;	
}

void energy_analyze_destroy(GtkWidget *widget, gpointer user_data)
{
	int i;
	
	gtk_widget_destroy((GtkWidget *)user_data);
	
	for(i=0; i<4; i++)
		gflag.cbe[i] = 2;
		
	gflag.x = 0; gflag.y = 0; gflag.zoom = -1; gflag.log_en = 0;
}

void draw_func_energy(GtkWidget *widget, cairo_t *cr, int num, char *str)
{	
	printf("draw_func %d started, mainflag.maximize = %d\n", num, mainflag.maximize);
	
	int width = gtk_widget_get_allocated_width (widget);
	int height = gtk_widget_get_allocated_height (widget);
	
	int i, j;
	
	if(glob.flag==1) {
		static int x_start = 0;
		static double y_comp = 1.0;
		static int x_left = 0;
		static int x_right = 0;
		static int logarithm;
		double max_Y = 1.0;
		
		printf("x_start = %d; y_comp = %.3f; mainflag.zoom_en_y = %d; x_left = %d\n", x_start, y_comp, mainflag.zoom_eny, x_left);
		
		cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);
		int max_e = max_energy();
		double co_e = (double)(max_e/120.0)+1.0;
		char coord[6][10];
		char coord_y[8][10];
		
		if(mainflag.maximize == 0) {
			x_left += LR_EN_X*mainflag.left_en;
			if(mainflag.left_en == 1) mainflag.left_en = 0;
			if(mainflag.left_en == -1) {mainflag.left_en = 0; x_left = 0;}
		
			x_right += LR_EN_X*mainflag.right_en;
			if(mainflag.right_en == 1) mainflag.right_en = 0;
			if(mainflag.right_en == -1) {mainflag.right_en = 0; x_right = 0;}
		
			if(mainflag.zoom_en == -2) {mainflag.zoom_en = 0; x_start = 0;}
			if(mainflag.zoom_eny == -2) {mainflag.zoom_eny = 0; y_comp = 1.0;}
			x_start += mainflag.zoom_en*ZOOM_EN_X;
			y_comp *= 1.0+mainflag.zoom_eny*ZOOM_EN_Y;
			
			if(mainflag.zoom_en == 1) mainflag.undo_en = 1;
			if(mainflag.zoom_en == 1 || mainflag.zoom_en == -1) mainflag.zoom_en = 0;
			if(mainflag.zoom_eny == 1) mainflag.undo_en = 2;
			if(mainflag.zoom_eny == 1 || mainflag.zoom_eny == -1) mainflag.zoom_eny = 0;
			
			if( (x_start+x_left-x_right < 0) ) {printf("out of range\n"); x_start = 0; x_left = 0; x_right = 0;}
		
			if(mainflag.log_en == 1) {
				logarithm = 1; 
				max_Y = max_energy();
			}
			
			cairo_rectangle(cr, 0.0, 0.0, width, height-10.0);
			cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
			cairo_fill(cr);

			cairo_set_font_size(cr, 10);	
			cairo_set_source_rgb(cr, 0.1, 0.1, 0.1); 
			cairo_select_font_face(cr, "Arial",
				CAIRO_FONT_SLANT_NORMAL,
				CAIRO_FONT_WEIGHT_NORMAL);
			cairo_move_to(cr, 0.0, height);
			sprintf(coord[0], "%.1fK", (double)(x_start+x_left-x_right)/1000.0);
			cairo_show_text(cr, coord[0]);
			cairo_move_to(cr, width-25.0, height);
			sprintf(coord[1], "%.1fK", (double)(MAX_CH-x_start+x_left-x_right)/1000.0);
			cairo_show_text(cr, coord[1]);
	
			if(mainflag.log_en != 1) {
				cairo_move_to(cr, width-25.0, 10.0);
				sprintf(coord_y[0], "%.1fK", (height*y_comp)/1000.0);
				cairo_show_text(cr, coord_y[0]);
			}
			else {
				double lg_tic = 1.0;

				cairo_set_line_width(cr, 0.5);
				for(i=0; i<8; i++) {
					if(lg_tic<=1000)
						sprintf(coord_y[i], "%.0f", lg_tic);
					else
						sprintf(coord_y[i], "%.0fK", lg_tic/1e3);
					cairo_move_to(cr, (double)(width-2.0-2.0), height-15.0-(double)(height-15.0)*log10(lg_tic)/log10(max_Y));
					cairo_line_to(cr, (double)(width-2.0+2.0), height-15.0-(double)(height-15.0)*log10(lg_tic)/log10(max_Y));
					cairo_stroke(cr);
					cairo_move_to(cr, (double)(width-25.0), height-10.0-(double)(height-15.0)*log10(lg_tic)/log10(max_Y));
					cairo_show_text(cr, coord_y[i]);
			
					lg_tic *= 10.0;
				}
			}
			
			cairo_set_font_size(cr, 12);	
			cairo_set_source_rgb(cr, 0.1, 0.1, 0.1); 
			cairo_select_font_face(cr, "Garuda",
				CAIRO_FONT_SLANT_NORMAL,
				CAIRO_FONT_WEIGHT_BOLD);
			cairo_move_to(cr, width/2.0, 10.0);
			cairo_show_text(cr, str);
			
			cairo_move_to(cr, 0.0, height-10.0);
			cairo_line_to(cr, width, height-10.0);
			cairo_stroke(cr);
			
			cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);
			
			double *X, *Y;
			X = (double *)calloc(MAX_CH, sizeof(double));
			Y = (double *)calloc(MAX_CH, sizeof(double));
			if(mainflag.log_en != 1) {
				for(j=0; j<=MAX_CH-1; j+=1) {
					//cairo_mask_surface(cr, pt_surface(), (double)(width/(MAX_CH-2.0*x_start)*(j-x_start-x_left+x_right)), (height-15.0-(double)spectras->energyspk[num][j]/y_comp));
					X[j]=(double)(width/(MAX_CH-2.0*x_start)*(j-x_start-x_left+x_right)); 
					Y[j] = (double)spectras->energyspk[num][j]/y_comp;
				}
				cairo_mask_surface(cr, xy_surface(cr, width, height-10.0, X, Y, x_start+x_left-x_right, MAX_CH+x_left-x_right-1), 0.0, 0.0);
				free(X);
				free(Y);
			}
			else
				for(j=0; j<=MAX_CH+x_left-x_right-1 || j<MAX_CH-x_start+x_left-x_right; j+=4)
					cairo_mask_surface(cr, pt_surface(), (double)(width/(MAX_CH-2.0*x_start)*(j-x_start-x_left+x_right)), (height-15.0-(height-15.0)*log10( (double)spectras->energyspk[num][j])/log10(max_Y)));
		
		}
		else {
			cairo_rectangle(cr, 0.0, 0.0, width, height);
			cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
			cairo_fill(cr);
			
			x_left += LR_EN_X*mainflag.left_en;
			if(mainflag.left_en == 1) mainflag.left_en = 0;
			if(mainflag.left_en == -1) {mainflag.left_en = 0; x_left = 0;}
		
			x_right += LR_EN_X*mainflag.right_en;
			if(mainflag.right_en == 1) mainflag.right_en = 0;
			if(mainflag.right_en == -1) {mainflag.right_en = 0; x_right = 0;}
		
			if(mainflag.zoom_en == -2) {mainflag.zoom_en = 0; x_start = 0;}
			if(mainflag.zoom_eny == -2) {mainflag.zoom_eny = 0; y_comp = 1.0;}
			x_start += mainflag.zoom_en*ZOOM_EN_X;
			y_comp *= 1.0+mainflag.zoom_eny*ZOOM_EN_Y;
			
			if(mainflag.zoom_en == 1) mainflag.undo_en = 1;
			if(mainflag.zoom_en == 1 || mainflag.zoom_en == -1) mainflag.zoom_en = 0;
			if(mainflag.zoom_eny == 1) mainflag.undo_en = 2;
			if(mainflag.zoom_eny == 1 || mainflag.zoom_eny == -1) mainflag.zoom_eny = 0;
			
			if( (x_start+x_left-x_right < 0) ) {printf("out of range\n"); x_start = 0; x_left = 0; x_right = 0;}
		
			if(mainflag.log_en == 1) logarithm = 1; 
			
			cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);
			
			printf("en0=%.2f en1=%.2f en5=%.2f\n", (double)spectras->energyspk[num][0]/y_comp, (double)spectras->energyspk[num][1]/y_comp, (double)spectras->energyspk[num][5]/y_comp );
			
			if(mainflag.log_en != 1)
				for(j=x_start+x_left-x_right; j<=MAX_CH+x_left-x_right-1 || j<MAX_CH-x_start+x_left-x_right; j+=2)
					cairo_mask_surface(cr, pt_surface(), (double)(width/(MAX_CH-2.0*x_start)*(j-x_start-x_left+x_right)), (height-(double)spectras->energyspk[num][j]/y_comp - 15.0));
			else
				for(j=x_start+x_left-x_right; j<=MAX_CH+x_left-x_right-1 || j<MAX_CH-x_start+x_left-x_right; j+=2)
					cairo_mask_surface(cr, pt_surface(), (double)(width/(MAX_CH-2.0*x_start)*(j-x_start-x_left+x_right)), (height-100.0*log10( (double)spectras->energyspk[num][j]/y_comp )-15.0));
		
			cairo_set_source_rgb(cr, 0.0, 0.0, 0.0); 
			cairo_set_line_width(cr, 1.0);
			cairo_move_to(cr, 0.0, height-10.0);
			cairo_line_to(cr, width, height-10.0);
			cairo_stroke(cr);
		
			/*cairo_move_to(cr, 0.0, height);
			sprintf(coord[0], "%.1fK", (double)(x_start+x_left-x_right)/1000.0);
			cairo_show_text(cr, coord[0]);
			cairo_move_to(cr, width-25.0, height);
			sprintf(coord[1], "%.1fK", (double)(MAX_CH-x_start+x_left-x_right)/1000.0);
			cairo_show_text(cr, coord[1]);*/
			
			cairo_set_font_size(cr, 16);	
			cairo_set_source_rgb(cr, 0.1, 0.1, 0.1); 
			cairo_select_font_face(cr, "Garuda",
				CAIRO_FONT_SLANT_NORMAL,
				CAIRO_FONT_WEIGHT_BOLD);
			cairo_move_to(cr, width/2.0, 20.0);
			cairo_show_text(cr, str);
			
			cairo_set_font_size(cr, 12);
			
			int tics[6];
			
			if(MAX_CH-2.0*x_start < 90) {
			for(i=0; i<=5; i++) {
				tics[i] = 25*((int)(x_start)/25+i);
				sprintf(coord[i], "%d", tics[i]);
				cairo_move_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height-8.0);
				cairo_line_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height-12.0);
				cairo_stroke(cr);
				cairo_move_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height);
				cairo_show_text(cr, coord[i]);
			}
			}
			else if(MAX_CH-2.0*x_start < 200) {
			for(i=0; i<=5; i++) {
				tics[i] = 50*((int)(x_start)/50+i);
				sprintf(coord[i], "%d", tics[i]);
				cairo_move_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height-8.0);
				cairo_line_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height-12.0);
				cairo_stroke(cr);
				cairo_move_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height);
				cairo_show_text(cr, coord[i]);
			}
			}
			else if(MAX_CH-2.0*x_start < 500) {
			for(i=0; i<=5; i++) {
				tics[i] = 100*((int)(x_start)/100+i);
				if(width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right)>width) break;
				sprintf(coord[i], "%d", tics[i]);
				cairo_move_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height-8.0);
				cairo_line_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height-12.0);
				cairo_stroke(cr);
				cairo_move_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height);
				cairo_show_text(cr, coord[i]);
			}
			}
			else if(MAX_CH-2.0*x_start < 1000) {
			for(i=0; i<=4; i++) {
				tics[i] = 250*((int)(x_start/250.0)+i);
				if(width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right)>width) break;
				sprintf(coord[i], "%d", tics[i]);
				cairo_move_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height-8.0);
				cairo_line_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height-12.0);
				cairo_stroke(cr);
				cairo_move_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height);
				cairo_show_text(cr, coord[i]);
				printf("width = %f, tics[%d] = %d\n", MAX_CH-2.0*x_start, i, tics[i]);
			}
			}
			else if(MAX_CH-2.0*x_start < 2000) {
			for(i=0; i<=5; i++) {
				tics[i] = 400*((int)(x_start/400.0)+i);
				printf("width = %f, tics[%d] = %d\n", MAX_CH-2.0*x_start, i, tics[i]);
				if(width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right)>width) break;
				sprintf(coord[i], "%d", tics[i]);
				cairo_move_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height-8.0);
				cairo_line_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height-12.0);
				cairo_stroke(cr);
				cairo_move_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height);
				cairo_show_text(cr, coord[i]);
			}
			}
			else if(MAX_CH-2.0*x_start < 3000) {
			for(i=0; i<=5; i++) {
				tics[i] = 500*((int)(x_start/500.0)+i);
				if(width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right)>width) break;
				sprintf(coord[i], "%d", tics[i]);
				cairo_move_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height-8.0);
				cairo_line_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height-12.0);
				cairo_stroke(cr);
				cairo_move_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height);
				cairo_show_text(cr, coord[i]);
				printf("width = %f, tics[%d] = %d\n", MAX_CH-2.0*x_start, i, tics[i]);
			}
			}
			else {
			for(i=0; i<=4; i++) {
				tics[i] = 1000*((int)(x_start/1000.0)+i);
				if(width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right)>width) break;
				sprintf(coord[i], "%d", tics[i]);
				cairo_move_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height-8.0);
				cairo_line_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height-12.0);
				cairo_stroke(cr);
				cairo_move_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height);
				cairo_show_text(cr, coord[i]);
				printf("width = %f, tics[%d] = %d\n", MAX_CH-2.0*x_start, i, tics[i]);
			}
			}
			
			char coord_y[4][10];
			
			if(mainflag.log_en != 1) {
				sprintf(coord_y[1], "%.1f K", height*y_comp/1000.0);			
				cairo_move_to(cr, width, height-5.0);
				cairo_line_to(cr, width-5.0, height-5.0);
				cairo_stroke(cr);
				cairo_move_to(cr, width-40.0, 20.0);
				cairo_show_text(cr, coord_y[1]);
			}
			else {
				sprintf(coord_y[0], "%.0f", 1.0); //spk_y*10.0
				cairo_move_to(cr, width, height-15.0-100.0*log10(1.0/y_comp));
				cairo_line_to(cr, width-5.0, height-15.0-100.0*log10(1.0/y_comp));
				cairo_stroke(cr);
				cairo_move_to(cr, width-40.0, height-15.0-100.0*log10(1.0/y_comp));
				cairo_show_text(cr, coord_y[0]);
				sprintf(coord_y[1], "%.0f", 100.0);
				cairo_move_to(cr, width, height-15.0-100.0*log10(100.0/y_comp));
				cairo_line_to(cr, width-5.0, height-15.0-100.0*log10(100.0/y_comp));
				cairo_stroke(cr);
				cairo_move_to(cr, width-40.0, height-15.0-100.0*log10(100.0/y_comp));
				cairo_show_text(cr, coord_y[1]);
				sprintf(coord_y[2], "%.0f", 10000.0);
				cairo_move_to(cr, width, height-15.0-100.0*log10(10000.0/y_comp));
				cairo_line_to(cr, width-5.0, height-15.0-100.0*log10(10000.0/y_comp));
				cairo_stroke(cr);
				cairo_move_to(cr, width-40.0, height-15.0-100.0*log10(10000.0/y_comp));
				cairo_show_text(cr, coord_y[2]);	
			}
		}
	}
}

gboolean draw_func0(GtkWidget *widget, cairo_t *cr, 
    gpointer user_data)
{
	printf("EN flag = %d\n", mainflag.EN);
	
	if(mainflag.EN == 1) {
		draw_func_energy(widget, cr, 0, "E1");
		//mainflag.EN = 0;
	}
	
	return TRUE;
}
gboolean draw_func1(GtkWidget *widget, cairo_t *cr, 
    gpointer user_data)
{	
	if(mainflag.EN == 1) {
		draw_func_energy(widget, cr, 1, "E2");
	//	mainflag.EN = 0;
	}
	
	return TRUE;
}
gboolean draw_func2(GtkWidget *widget, cairo_t *cr, 
    gpointer user_data)
{	
	if(mainflag.EN == 1) {
		draw_func_energy(widget, cr, 2, "E3");
	//	mainflag.EN = 0;
	}
	
	return TRUE;
}
gboolean draw_func3(GtkWidget *widget, cairo_t *cr, 
    gpointer user_data)
{
	if(mainflag.EN == 1) {
		draw_func_energy(widget, cr, 3, "E4");
	//	mainflag.EN = 0;
	}
	
	return TRUE;
}

void draw_func_time(GtkWidget *widget, cairo_t *cr, int num, char *str)
{
	printf("draw_func %d started, mainflag.maximize = %d\n", num, mainflag.maximize);
	
	int width = gtk_widget_get_allocated_width (widget);
	int height = gtk_widget_get_allocated_height (widget);
	
	int i, j;
	
	if(glob.flag==1) {
		static int x_start = 0;
		static double y_comp = 1.0;
		static int x_left = 0;
		static int x_right = 0;
		static int logarithm;
		double max_Y = 1.0;
		
		printf("x_start = %d; y_comp = %.3f; mainflag.zoom_t_y = %d; x_left = %d\n", x_start, y_comp, mainflag.zoom_ty, x_left);
		
		cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);

		char coord[6][10];
		char coord_y[8][10];
		
		if(mainflag.maximize == 0) {
			x_left += LR_EN_X*mainflag.left_t;
			if(mainflag.left_t == 1) mainflag.left_t = 0;
			if(mainflag.left_t == -1) {mainflag.left_t = 0; x_left = 0;}
		
			x_right += LR_EN_X*mainflag.right_t;
			if(mainflag.right_t == 1) mainflag.right_t = 0;
			if(mainflag.right_t == -1) {mainflag.right_t = 0; x_right = 0;}
		
			if(mainflag.zoom_t == -2) {mainflag.zoom_t = 0; x_start = 0;}
			if(mainflag.zoom_ty == -2) {mainflag.zoom_ty = 0; y_comp = 1.0;}
			x_start += mainflag.zoom_t*ZOOM_EN_X;
			y_comp *= 1.0+mainflag.zoom_ty*ZOOM_EN_Y;
			
			if(mainflag.zoom_t == 1) mainflag.undo_t = 1;
			if(mainflag.zoom_t == 1 || mainflag.zoom_t == -1) mainflag.zoom_t = 0;
			if(mainflag.zoom_ty == 1) mainflag.undo_t = 2;
			if(mainflag.zoom_ty == 1 || mainflag.zoom_ty == -1) mainflag.zoom_ty = 0;
			
			if( (2.0*x_start>=MAX_CH) || (x_start > MAX_CH-1) || (MAX_CH-1-x_start < 0)) x_start = 0;
		
			if(mainflag.log_t == 1) { logarithm = 1; 
				max_Y = max_time();
			}
			
			cairo_rectangle(cr, 0.0, 0.0, width, height-10.0);
			cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
			cairo_fill(cr);

			cairo_set_font_size(cr, 10);	
			cairo_set_source_rgb(cr, 0.1, 0.1, 0.1); 
			cairo_select_font_face(cr, "Arial",
				CAIRO_FONT_SLANT_NORMAL,
				CAIRO_FONT_WEIGHT_NORMAL);
			cairo_move_to(cr, 0.0, height);
			sprintf(coord[0], "%.1fK", (double)(x_start+x_left-x_right)/1000.0);
			cairo_show_text(cr, coord[0]);
			cairo_move_to(cr, width-25.0, height);
			sprintf(coord[1], "%.1fK", (double)(MAX_CH-x_start+x_left-x_right)/1000.0);
			cairo_show_text(cr, coord[1]);
			cairo_move_to(cr, width-25.0, 10.0);
			
			if(mainflag.log_t != 1) {
				cairo_move_to(cr, width-25.0, 10.0);
				sprintf(coord_y[0], "%.1fK", (height*y_comp)/1000.0);
				cairo_show_text(cr, coord_y[0]);
			}
			else {
				double lg_tic = 1.0;

				cairo_set_line_width(cr, 0.5);
				for(i=0; i<8; i++) {
					if(lg_tic<=1000)
						sprintf(coord_y[i], "%.0f", lg_tic);
					else
						sprintf(coord_y[i], "%.0fK", lg_tic/1e3);
					cairo_move_to(cr, (double)(width-2.0-2.0), height-15.0-(double)(height-15.0)*log10(lg_tic)/log10(max_Y));
					cairo_line_to(cr, (double)(width-2.0+2.0), height-15.0-(double)(height-15.0)*log10(lg_tic)/log10(max_Y));
					cairo_stroke(cr);
					cairo_move_to(cr, (double)(width-25.0), height-10.0-(double)(height-15.0)*log10(lg_tic)/log10(max_Y));
					cairo_show_text(cr, coord_y[i]);
			
					lg_tic *= 10.0;
				}
			}
			
			cairo_set_font_size(cr, 12);	
			cairo_set_source_rgb(cr, 0.1, 0.1, 0.1); 
			cairo_select_font_face(cr, "Garuda",
				CAIRO_FONT_SLANT_NORMAL,
				CAIRO_FONT_WEIGHT_BOLD);
			cairo_move_to(cr, width/2.0, 10.0);
			cairo_show_text(cr, str);
			
			cairo_move_to(cr, 0.0, height-10.0);
			cairo_line_to(cr, width, height-10.0);
			cairo_stroke(cr);
			
			cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
			
			struct timeval start, end;
			long mtime, seconds, useconds;    
			gettimeofday(&start, NULL);
			
			double *X, *Y;
			X = (double *)calloc(MAX_CH, sizeof(double));
			Y = (double *)calloc(MAX_CH, sizeof(double));
			if(mainflag.log_t != 1) {
				for(j=x_start+x_left-x_right; j<=MAX_CH+x_left-x_right-1 || j<MAX_CH-x_start+x_left-x_right; j+=1) {
					X[j]=(double)(width/(MAX_CH-2.0*x_start)*(j-x_start-x_left+x_right)); 
					Y[j] = (double)spectras->timespk[num][j]/y_comp;
				}
				cairo_mask_surface(cr, xy_surface(cr, width, height-15.0, X, Y, x_start+x_left-x_right, MAX_CH+x_left-x_right-1), 0.0, 0.0);
				free(X);
				free(Y);	
			}
			else
				for(j=x_start+x_left-x_right; j<=MAX_CH+x_left-x_right-1 || j<MAX_CH-x_start+x_left-x_right; j+=4)
					cairo_mask_surface(cr, pt_surface(), (double)(width/(MAX_CH-2.0*x_start)*(j-x_start-x_left+x_right)), (height-15.0-(height-15.0)*log10( (double)spectras->timespk[num][j] )/log10(max_Y)));

			gettimeofday(&end, NULL);
			seconds  = end.tv_sec  - start.tv_sec;
			useconds = end.tv_usec - start.tv_usec;
			mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
			printf("Elapsed time for surface: %ld milliseconds\n", mtime);
		}
		else {
			cairo_rectangle(cr, 0.0, 0.0, width, height);
			cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
			cairo_fill(cr);
			
			x_left += LR_EN_X*mainflag.left_t;
			if(mainflag.left_t == 1) mainflag.left_t = 0;
			if(mainflag.left_t == -1) {mainflag.left_t = 0; x_left = 0;}
		
			x_right += LR_EN_X*mainflag.right_t;
			if(mainflag.right_t == 1) mainflag.right_t = 0;
			if(mainflag.right_t == -1) {mainflag.right_t = 0; x_right = 0;}
		
			if(mainflag.zoom_t == -2) {mainflag.zoom_t = 0; x_start = 0;}
			if(mainflag.zoom_ty == -2) {mainflag.zoom_ty = 0; y_comp = 1.0;}
			x_start += mainflag.zoom_t*ZOOM_EN_X;
			y_comp *= 1.0+mainflag.zoom_ty*ZOOM_EN_Y;
			
			if(mainflag.zoom_t == 1) mainflag.undo_t = 1;
			if(mainflag.zoom_t == 1 || mainflag.zoom_t == -1) mainflag.zoom_t = 0;
			if(mainflag.zoom_ty == 1) mainflag.undo_t = 2;
			if(mainflag.zoom_ty == 1 || mainflag.zoom_ty == -1) mainflag.zoom_ty = 0;
			
			if( (MAX_CH+x_left-x_right>MAX_CH) || (x_start+x_left-x_right < 0) ) {printf("out of range\n"); x_start = 0; x_left = 0; x_right = 0;}
		
			if(mainflag.log_t == 1) logarithm = 1; 
			
			cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
			
			printf("en0=%.2f en1=%.2f en5=%.2f\n", (double)spectras->timespk[num][0]/y_comp, (double)spectras->timespk[num][1]/y_comp, (double)spectras->timespk[num][5]/y_comp );
			
			if(mainflag.log_t != 1)
				for(j=x_start+x_left-x_right; j<=MAX_CH+x_left-x_right-1 || j<MAX_CH-x_start+x_left-x_right; j+=2)
					cairo_mask_surface(cr, pt_surface(), (double)(width/(MAX_CH-2.0*x_start)*(j-x_start-x_left+x_right)), (height-(double)spectras->timespk[num][j]/y_comp - 15.0));
			else
				for(j=x_start+x_left-x_right; j<=MAX_CH+x_left-x_right-1 || j<MAX_CH-x_start+x_left-x_right; j+=2)
					cairo_mask_surface(cr, pt_surface(), (double)(width/(MAX_CH-2.0*x_start)*(j-x_start-x_left+x_right)), (height-10.0*log( (double)spectras->timespk[num][j]/y_comp - 15.0)));
		
			cairo_set_source_rgb(cr, 0.0, 0.0, 0.0); 
			cairo_set_line_width(cr, 1.0);
			cairo_move_to(cr, 0.0, height-10.0);
			cairo_line_to(cr, width, height-10.0);
			cairo_stroke(cr);
		
			cairo_move_to(cr, 0.0, height);
			sprintf(coord[0], "%.1fK", (double)(x_start+x_left-x_right)/1000.0);
			cairo_show_text(cr, coord[0]);
			cairo_move_to(cr, width-25.0, height);
			sprintf(coord[1], "%.1fK", (double)(MAX_CH-x_start+x_left-x_right)/1000.0);
			cairo_show_text(cr, coord[1]);
			
			cairo_set_font_size(cr, 16);	
			cairo_set_source_rgb(cr, 0.1, 0.1, 0.1); 
			cairo_select_font_face(cr, "Garuda",
				CAIRO_FONT_SLANT_NORMAL,
				CAIRO_FONT_WEIGHT_BOLD);
			cairo_move_to(cr, width/2.0, 20.0);
			cairo_show_text(cr, str);
			
			cairo_set_font_size(cr, 12);
			
			int tics[6];
			
			if(MAX_CH-2.0*x_start < 90) {
			for(i=0; i<=5; i++) {
				tics[i] = 25*((int)(x_start)/25+i);
				sprintf(coord[i], "%d", tics[i]);
				cairo_move_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height-8.0);
				cairo_line_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height-12.0);
				cairo_stroke(cr);
				cairo_move_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height);
				cairo_show_text(cr, coord[i]);
			}
			}
			else if(MAX_CH-2.0*x_start < 200) {
			for(i=0; i<=5; i++) {
				tics[i] = 50*((int)(x_start)/50+i);
				sprintf(coord[i], "%d", tics[i]);
				cairo_move_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height-8.0);
				cairo_line_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height-12.0);
				cairo_stroke(cr);
				cairo_move_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height);
				cairo_show_text(cr, coord[i]);
			}
			}
			else if(MAX_CH-2.0*x_start < 500) {
			for(i=0; i<=5; i++) {
				tics[i] = 100*((int)(x_start)/100+i);
				if(width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right)>width) break;
				sprintf(coord[i], "%d", tics[i]);
				cairo_move_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height-8.0);
				cairo_line_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height-12.0);
				cairo_stroke(cr);
				cairo_move_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height);
				cairo_show_text(cr, coord[i]);
			}
			}
			else if(MAX_CH-2.0*x_start < 1000) {
			for(i=0; i<=4; i++) {
				tics[i] = 250*((int)(x_start/250.0)+i);
				if(width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right)>width) break;
				sprintf(coord[i], "%d", tics[i]);
				cairo_move_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height-8.0);
				cairo_line_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height-12.0);
				cairo_stroke(cr);
				cairo_move_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height);
				cairo_show_text(cr, coord[i]);
				printf("width = %f, tics[%d] = %d\n", MAX_CH-2.0*x_start, i, tics[i]);
			}
			}
			else if(MAX_CH-2.0*x_start < 2000) {
			for(i=0; i<=5; i++) {
				tics[i] = 400*((int)(x_start/400.0)+i);
				printf("width = %f, tics[%d] = %d\n", MAX_CH-2.0*x_start, i, tics[i]);
				if(width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right)>width) break;
				sprintf(coord[i], "%d", tics[i]);
				cairo_move_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height-8.0);
				cairo_line_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height-12.0);
				cairo_stroke(cr);
				cairo_move_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height);
				cairo_show_text(cr, coord[i]);
			}
			}
			else if(MAX_CH-2.0*x_start < 3000) {
			for(i=0; i<=5; i++) {
				tics[i] = 500*((int)(x_start/500.0)+i);
				if(width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right)>width) break;
				sprintf(coord[i], "%d", tics[i]);
				cairo_move_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height-8.0);
				cairo_line_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height-12.0);
				cairo_stroke(cr);
				cairo_move_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height);
				cairo_show_text(cr, coord[i]);
				printf("width = %f, tics[%d] = %d\n", MAX_CH-2.0*x_start, i, tics[i]);
			}
			}
			else {
			for(i=0; i<=4; i++) {
				tics[i] = 1000*((int)(x_start/1000.0)+i);
				if(width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right)>width) break;
				sprintf(coord[i], "%d", tics[i]);
				cairo_move_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height-8.0);
				cairo_line_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height-12.0);
				cairo_stroke(cr);
				cairo_move_to(cr, width/(MAX_CH-2.0*x_start)*(tics[i]-x_start-x_left+x_right), height);
				cairo_show_text(cr, coord[i]);
				printf("width = %f, tics[%d] = %d\n", MAX_CH-2.0*x_start, i, tics[i]);
			}
			}
			
			char coord_y[2][10];
			sprintf(coord_y[0], "%.0f", height*y_comp);
			sprintf(coord_y[1], "%.0f", height*y_comp);
			
			cairo_move_to(cr, width, height-5.0);
			cairo_line_to(cr, width-5.0, height-5.0);
			cairo_stroke(cr);
			cairo_move_to(cr, width-25.0, height-7.0);
			cairo_show_text(cr, coord_y[0]);
			cairo_move_to(cr, width-25.0, 10.0);
			cairo_show_text(cr, coord_y[1]);
		}
	}
}

gboolean draw_func4(GtkWidget *widget, cairo_t *cr, 
    gpointer user_data)
{
	draw_func_time(widget, cr, 1, "D1-D2");
	
	return TRUE;
}
gboolean draw_func5(GtkWidget *widget, cairo_t *cr, 
    gpointer user_data)
{
	draw_func_time(widget, cr, 3, "D1-D3");
	
	return TRUE;
}
gboolean draw_func6(GtkWidget *widget, cairo_t *cr, 
    gpointer user_data)
{
	draw_func_time(widget, cr, 5, "D1-D4");
	
	return TRUE;
}
gboolean draw_func7(GtkWidget *widget, cairo_t *cr, 
    gpointer user_data)
{
	draw_func_time(widget, cr, 7, "D2-D3");
	
	return TRUE;
}
gboolean draw_func8(GtkWidget *widget, cairo_t *cr, 
    gpointer user_data)
{
	draw_func_time(widget, cr, 9, "D2-D4");
	
	return TRUE;
}
gboolean draw_func9(GtkWidget *widget, cairo_t *cr, 
    gpointer user_data)
{
	draw_func_time(widget, cr, 11, "D3-D4");
	
	return TRUE;
}
gboolean draw_func10(GtkWidget *widget, cairo_t *cr, 
    gpointer user_data)
{
	draw_func_time(widget, cr, 0, "D2-D1");
	
	return TRUE;
}
gboolean draw_func11(GtkWidget *widget, cairo_t *cr, 
    gpointer user_data)
{
	draw_func_time(widget, cr, 2, "D3-D1");
	
	return TRUE;
}
gboolean draw_func12(GtkWidget *widget, cairo_t *cr, 
    gpointer user_data)
{
	draw_func_time(widget, cr, 4, "D4-D1");
	
	return TRUE;
}
gboolean draw_func13(GtkWidget *widget, cairo_t *cr, 
    gpointer user_data)
{
	draw_func_time(widget, cr, 6, "D3-D2");
	
	return TRUE;
}
gboolean draw_func14(GtkWidget *widget, cairo_t *cr, 
    gpointer user_data)
{
	draw_func_time(widget, cr, 8, "D4-D2");
	
	return TRUE;
}
gboolean draw_func15(GtkWidget *widget, cairo_t *cr, 
    gpointer user_data)
{
	draw_func_time(widget, cr, 10, "D4-D3");
	
	return TRUE;
}

void plot_mini_spk()
{
	printf("plotmini spk\n");
	int i;
	
	GtkWidget *hseparator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	static GtkWidget *darea[16], *button_maximize[16], *image_button[16], *hvgraph_box[16];
	gboolean (*draw_f[16])(GtkWidget *, cairo_t *, gpointer) = {draw_func0, draw_func1, draw_func2, draw_func3, draw_func4, draw_func5, draw_func6, draw_func7, \
														draw_func8, draw_func9, draw_func10, draw_func11, draw_func12, draw_func13, draw_func14, draw_func15};
    int size_array[2] = {X_SIZE_PREPLOT, Y_SIZE_PREPLOT};
	
	if(mainflag.maximize == 1) gtk_container_remove (GTK_CONTAINER(hbox_t[0]), darea_maximize);
	
	mainflag.maximize = 0;
	mainflag.EN = 1;
	
	for(i=0; i<16; i++) {	
		darea[i] = gtk_drawing_area_new();
		hgraph_box[i] = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
		hvgraph_box[i] = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
		button_maximize[i] = gtk_button_new();
		image_button[i] = gtk_image_new_from_file("./zoom-fit-best.png");
		gtk_button_set_image(GTK_BUTTON(button_maximize[i]), image_button[i]);
		g_signal_connect (G_OBJECT (button_maximize[i]), "clicked",
                        G_CALLBACK (maximize_func), (gpointer) i);
		gtk_widget_set_size_request (darea[i], X_SIZE_PREPLOT-50, 40);
		g_signal_connect(G_OBJECT(darea[i]), "draw", 
			G_CALLBACK(*draw_f[i]), size_array);
			
	//	gtk_container_add(GTK_CONTAINER(hgraph_box[i]), darea[i]);
		gtk_container_add(GTK_CONTAINER(hvgraph_box[i]), button_maximize[i]);
		gtk_box_pack_start (GTK_BOX (hgraph_box[i]), darea[i], TRUE, TRUE, 0);
		gtk_box_pack_start (GTK_BOX (hgraph_box[i]), hvgraph_box[i], FALSE, FALSE, 0);
	
	//	gtk_box_pack_end (GTK_BOX (hbox_en), hseparator, TRUE, FALSE, 0);
	
		if(i<4)
			//gtk_grid_attach(GTK_GRID(main_table), hgraph_box[i], i, 0, 1, 1);
			gtk_box_pack_start (GTK_BOX (hbox_en), hgraph_box[i], TRUE, TRUE, 0);
		else if(i<10)
			//gtk_grid_attach(GTK_GRID(main_table), hgraph_box[i], i-4, 2, 1, 1);
			gtk_box_pack_start (GTK_BOX (hbox_t[0]), hgraph_box[i], TRUE, TRUE, 0);
		else
			//gtk_grid_attach(GTK_GRID(main_table), hgraph_box[i], i-10, 3, 1, 1);
			gtk_box_pack_start (GTK_BOX (hbox_t[1]), hgraph_box[i], TRUE, TRUE, 0);

	//	gtk_widget_show (hgraph_box[i]);
	}
	
	gtk_widget_show_all(hbox_en);
	gtk_widget_show_all(hbox_t[0]);
	gtk_widget_show_all(hbox_t[1]);
}

void maximize_func(GtkWidget *widget,
               gpointer   data)
{
	printf("maximize_func, -data = %d\n", (int *)data);
	
	int j = (int *)data;
	int i;
	GtkWidget *button_mini = gtk_button_new_with_label ("Minihthythythyt");
	GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	GtkWidget *label = gtk_label_new("label");
	
	darea_maximize = gtk_drawing_area_new();
	gboolean (*draw_f[16])(GtkWidget *, cairo_t *, gpointer) = {draw_func0, draw_func1, draw_func2, draw_func3, draw_func4, draw_func5, draw_func6, draw_func7, \
														draw_func8, draw_func9, draw_func10, draw_func11, draw_func12, draw_func13, draw_func14, draw_func15};
	
	//gtk_widget_set_size_request (darea_maximize, 6.0*X_SIZE_PREPLOT, 2.0*Y_SIZE_PREPLOT);
	g_signal_connect(G_OBJECT(darea_maximize), "draw", 
			G_CALLBACK(*draw_f[j]), NULL);
	
//	gtk_grid_attach(GTK_GRID(main_table), darea_maximize, 0, 2, 6, 2);
	
	//gtk_widget_show_all(main_table);
	
	for(i=0; i<16; i++)
		if(i<4)
			gtk_container_remove (GTK_CONTAINER(hbox_en), hgraph_box[i]);
		else if(i<10)
			gtk_container_remove (GTK_CONTAINER(hbox_t[0]), hgraph_box[i]);
		else
			gtk_container_remove (GTK_CONTAINER(hbox_t[1]), hgraph_box[i]);
	
	gtk_box_pack_start (GTK_BOX (hbox_t[0]), darea_maximize, TRUE, TRUE, 0);
	
	gtk_widget_show_all(hbox_en);
	gtk_widget_show_all(hbox_t[0]);
	gtk_widget_show_all(hbox_t[1]);
	
	mainflag.maximize = 1;
}

void minimize_func( GtkWidget *widget,
               gpointer   data )
{
	printf("minimize en func\n");
	int i;
	
	mainflag.zoom_en = -2; mainflag.zoom_eny = -2; mainflag.log_en = -1; mainflag.left_en = -1; mainflag.right_en = -1;
	gtk_button_set_label(GTK_BUTTON(button_en_log), "log");
	
	for(i=0; i<16; i++)
		if(i<4)
			gtk_container_remove (GTK_CONTAINER(hbox_en), hgraph_box[i]);
		else if(i<10)
			gtk_container_remove (GTK_CONTAINER(hbox_t[0]), hgraph_box[i]);
		else
			gtk_container_remove (GTK_CONTAINER(hbox_t[1]), hgraph_box[i]);
	
	plot_mini_spk();
	
//	gtk_widget_queue_draw(widget);
}

void main_left_en_func( GtkWidget *widget,
               gpointer   data )
{
	printf("main left en func\n");
	
	mainflag.left_en = 1;
	
	mainflag.EN = 1;
	
	if(mainflag.maximize != 1)
		gtk_widget_queue_draw_area((GtkWidget *)data, 0, 0, gtk_widget_get_allocated_width ((GtkWidget *)data), gtk_widget_get_allocated_height ((GtkWidget *)data)/3);
	else
		gtk_widget_queue_draw_area((GtkWidget *)data, 0, gtk_widget_get_allocated_height ((GtkWidget *)data)/3, gtk_widget_get_allocated_width ((GtkWidget *)data), gtk_widget_get_allocated_height ((GtkWidget *)data)/3);
}

void main_log_en_func( GtkWidget *widget,
               gpointer   data )
{
	printf("main log en func\n");
	
	static number_itt = 0;
	
	if(number_itt == 0) {
		gtk_button_set_label(GTK_BUTTON(button_en_log), "lin");
		number_itt = 1;
		mainflag.log_en = 1;
	}
	else {
		gtk_button_set_label(GTK_BUTTON(button_en_log), "log");
		number_itt = 0;
		mainflag.log_en = 0;
	}
	
	mainflag.EN = 1;
	
	if(mainflag.maximize != 1)
		gtk_widget_queue_draw_area((GtkWidget *)data, 0, 0, gtk_widget_get_allocated_width ((GtkWidget *)data), gtk_widget_get_allocated_height ((GtkWidget *)data)/3);
	else
		gtk_widget_queue_draw_area((GtkWidget *)data, 0, gtk_widget_get_allocated_height ((GtkWidget *)data)/3, gtk_widget_get_allocated_width ((GtkWidget *)data), gtk_widget_get_allocated_height ((GtkWidget *)data)/3);
}

void main_right_en_func( GtkWidget *widget,
               gpointer   data )
{
	printf("main right en func\n");
	
	mainflag.right_en = 1;
	
	mainflag.EN = 1;

	if(mainflag.maximize != 1)
		gtk_widget_queue_draw_area((GtkWidget *)data, 0, 0, gtk_widget_get_allocated_width ((GtkWidget *)data), gtk_widget_get_allocated_height ((GtkWidget *)data)/3);
	else
		gtk_widget_queue_draw_area((GtkWidget *)data, 0, gtk_widget_get_allocated_height ((GtkWidget *)data)/3, gtk_widget_get_allocated_width ((GtkWidget *)data), gtk_widget_get_allocated_height ((GtkWidget *)data)/3);
}

void main_zoom_in_en_func( GtkWidget *widget,
               gpointer   data )
{
	printf("main zoom_in en func\n");
	
	mainflag.zoom_en = 1;
	
	mainflag.EN = 1;

	if(mainflag.maximize != 1)
		gtk_widget_queue_draw_area((GtkWidget *)data, 0, 0, gtk_widget_get_allocated_width ((GtkWidget *)data), gtk_widget_get_allocated_height ((GtkWidget *)data)/3);
	else
		gtk_widget_queue_draw_area((GtkWidget *)data, 0, gtk_widget_get_allocated_height ((GtkWidget *)data)/3, gtk_widget_get_allocated_width ((GtkWidget *)data), gtk_widget_get_allocated_height ((GtkWidget *)data)/3);
}

void main_zoom_in_en_y_func( GtkWidget *widget,
               gpointer   data )
{
	printf("main zoom_in_y en func\n");
	
	mainflag.zoom_eny = 1;
	
	mainflag.EN = 1;

	if(mainflag.maximize != 1)
		gtk_widget_queue_draw_area((GtkWidget *)data, 0, 0, gtk_widget_get_allocated_width ((GtkWidget *)data), gtk_widget_get_allocated_height ((GtkWidget *)data)/3);
	else
		gtk_widget_queue_draw_area((GtkWidget *)data, 0, gtk_widget_get_allocated_height ((GtkWidget *)data)/3, gtk_widget_get_allocated_width ((GtkWidget *)data), gtk_widget_get_allocated_height ((GtkWidget *)data)/3);
}

void main_undo_en_func( GtkWidget *widget,
               gpointer   data )
{
	printf("main undo en func\n");
	
	if (mainflag.undo_en == 1) mainflag.zoom_en=-1;
	else if (mainflag.undo_en == 2) mainflag.zoom_eny=-1;
	
	mainflag.EN = 1;
	
	if(mainflag.maximize != 1)
		gtk_widget_queue_draw_area((GtkWidget *)data, 0, 0, gtk_widget_get_allocated_width ((GtkWidget *)data), gtk_widget_get_allocated_height ((GtkWidget *)data)/3);
	else
		gtk_widget_queue_draw_area((GtkWidget *)data, 0, gtk_widget_get_allocated_height ((GtkWidget *)data)/3, gtk_widget_get_allocated_width ((GtkWidget *)data), gtk_widget_get_allocated_height ((GtkWidget *)data)/3);
}

void minimize_t_func( GtkWidget *widget,
               gpointer   data )
{
	printf("minimize t func\n");
	
	int i;
	
	mainflag.zoom_t = -2; mainflag.zoom_ty = -2; mainflag.log_t = -1; mainflag.left_t = -1; mainflag.right_t = -1;
	gtk_button_set_label(GTK_BUTTON(button_t_log), "log");
	
	for(i=0; i<16; i++)
		if(i<4)
			gtk_container_remove (GTK_CONTAINER(hbox_en), hgraph_box[i]);
		else if(i<10)
			gtk_container_remove (GTK_CONTAINER(hbox_t[0]), hgraph_box[i]);
		else
			gtk_container_remove (GTK_CONTAINER(hbox_t[1]), hgraph_box[i]);
	
	plot_mini_spk();
	
//	gtk_widget_queue_draw(widget);
}

void main_left_t_func( GtkWidget *widget,
               gpointer   data )
{
	printf("main left t func\n");
	
	mainflag.left_t = 1;

	if(mainflag.maximize != 1)
		gtk_widget_queue_draw_area((GtkWidget *)data, 0, gtk_widget_get_allocated_height ((GtkWidget *)data)/3, gtk_widget_get_allocated_width ((GtkWidget *)data), 2*gtk_widget_get_allocated_height ((GtkWidget *)data)/3);
	else
		gtk_widget_queue_draw_area((GtkWidget *)data, 0, gtk_widget_get_allocated_height ((GtkWidget *)data)/3, gtk_widget_get_allocated_width ((GtkWidget *)data), gtk_widget_get_allocated_height ((GtkWidget *)data)/3);
}

void main_log_t_func( GtkWidget *widget,
               gpointer   data )
{
	printf("main log t func\n");
	
	static number_itt = 0;
	
	if(number_itt == 0) {
		gtk_button_set_label(GTK_BUTTON(button_t_log), "lin");
		number_itt = 1;
		mainflag.log_t = 1;
	}
	else {
		gtk_button_set_label(GTK_BUTTON(button_t_log), "log");
		number_itt = 0;
		mainflag.log_t = 0;
	}
	
	if(mainflag.maximize != 1)
		gtk_widget_queue_draw_area((GtkWidget *)data, 0, gtk_widget_get_allocated_height ((GtkWidget *)data)/3, gtk_widget_get_allocated_width ((GtkWidget *)data), 2*gtk_widget_get_allocated_height ((GtkWidget *)data)/3);
	else
		gtk_widget_queue_draw_area((GtkWidget *)data, 0, gtk_widget_get_allocated_height ((GtkWidget *)data)/3, gtk_widget_get_allocated_width ((GtkWidget *)data), gtk_widget_get_allocated_height ((GtkWidget *)data)/3);
}

void main_right_t_func( GtkWidget *widget,
               gpointer   data )
{
	printf("main right en func\n");
	
	mainflag.right_t = 1;
	
	if(mainflag.maximize != 1)
		gtk_widget_queue_draw_area((GtkWidget *)data, 0, gtk_widget_get_allocated_height ((GtkWidget *)data)/3, gtk_widget_get_allocated_width ((GtkWidget *)data), 2*gtk_widget_get_allocated_height ((GtkWidget *)data)/3);
	else
		gtk_widget_queue_draw_area((GtkWidget *)data, 0, gtk_widget_get_allocated_height ((GtkWidget *)data)/3, gtk_widget_get_allocated_width ((GtkWidget *)data), gtk_widget_get_allocated_height ((GtkWidget *)data)/3);
}

void main_zoom_in_t_func( GtkWidget *widget,
               gpointer   data )
{
	printf("main zoom_in t func\n");
	
	mainflag.zoom_t = 1;
	
	if(mainflag.maximize != 1)
		gtk_widget_queue_draw_area((GtkWidget *)data, 0, gtk_widget_get_allocated_height ((GtkWidget *)data)/3, gtk_widget_get_allocated_width ((GtkWidget *)data), 2*gtk_widget_get_allocated_height ((GtkWidget *)data)/3);
	else
		gtk_widget_queue_draw_area((GtkWidget *)data, 0, gtk_widget_get_allocated_height ((GtkWidget *)data)/3, gtk_widget_get_allocated_width ((GtkWidget *)data), gtk_widget_get_allocated_height ((GtkWidget *)data)/3);
}

void main_zoom_in_t_y_func( GtkWidget *widget,
               gpointer   data )
{
	printf("main zoom_in_y t func\n");
	
	mainflag.zoom_ty = 1;
	
	if(mainflag.maximize != 1)
		gtk_widget_queue_draw_area((GtkWidget *)data, 0, gtk_widget_get_allocated_height ((GtkWidget *)data)/3, gtk_widget_get_allocated_width ((GtkWidget *)data), 2*gtk_widget_get_allocated_height ((GtkWidget *)data)/3);
	else
		gtk_widget_queue_draw_area((GtkWidget *)data, 0, gtk_widget_get_allocated_height ((GtkWidget *)data)/3, gtk_widget_get_allocated_width ((GtkWidget *)data), gtk_widget_get_allocated_height ((GtkWidget *)data)/3);
}

void main_undo_t_func( GtkWidget *widget,
               gpointer   data )
{
	printf("main undo t func\n");
	
	if (mainflag.undo_t == 1) mainflag.zoom_t=-1;
	else if (mainflag.undo_t == 2) mainflag.zoom_ty=-1;
	
	if(mainflag.maximize != 1)
		gtk_widget_queue_draw_area((GtkWidget *)data, 0, gtk_widget_get_allocated_height ((GtkWidget *)data)/3, gtk_widget_get_allocated_width ((GtkWidget *)data), 2*gtk_widget_get_allocated_height ((GtkWidget *)data)/3);
	else
		gtk_widget_queue_draw_area((GtkWidget *)data, 0, gtk_widget_get_allocated_height ((GtkWidget *)data)/3, gtk_widget_get_allocated_width ((GtkWidget *)data), gtk_widget_get_allocated_height ((GtkWidget *)data)/3);
}

static gboolean analyze_energy_draw_event(GtkWidget *widget, cairo_t *cr, 
    gpointer user_data)
{
	printf("analyze energy on draw event start 111\n");
	
	analyze_energy_drawing(widget, cr);

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

int max_energy()
{
	int max = spectras->energyspk[0][0];
	int i, j;
	
	for(i=0; i<4; i++)
		for(j=0; j<=MAX_CH-1; j++)
			if(spectras->energyspk[i][j]>max) max = spectras->energyspk[i][j];
			
	return max;
}

int max_time()
{
	int max = spectras->timespk[0][0];
	int i, j;
	
	for(i=0; i<12; i++)
		for(j=TIME_SHIFT_MAX; j<=MAX_CH-1; j++)
			if(spectras->timespk[i][j]>max) max = spectras->timespk[i][j];
			
	return max;
}

char *concat_str(char *s1, int s2)
{
	char *s = (char *)malloc(10*sizeof(char));
	
	sprintf(s, "%s%d", s1, s2);
	
	return s;
}

char *concat_str_inv(char *s1, double s2)
{
	char *s = (char *)malloc(10*sizeof(char));
	
	sprintf(s, "%.1f%s", s2, s1);
	
	return s;
}

extern cairo_surface_t *pt_surface()
{
	float red=0.0;
	float green=0.0;
	float blue=0.0;
	
	cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 6, 6);
  
	cairo_t *context = cairo_create (surface);
	
	cairo_set_line_width (context, 0.001);
	cairo_arc(context, 3, 3, 1.0, 0, 2.0*M_PI);
	cairo_set_source_rgb(context, red, green, blue);
	cairo_stroke_preserve(context);
	cairo_set_source_rgb(context, red, green, blue);
	cairo_fill(context);
	
	return surface;
}

cairo_surface_t *xy_surface(cairo_t *cr, int width, int height, double *x, double *y, int LCH, int RCH)
{
	int i;
	
	cairo_surface_t *surface = cairo_surface_create_similar(cairo_get_target(cr), CAIRO_CONTENT_COLOR_ALPHA, width, height);
  
	cairo_t *context = cairo_create (surface);
	cairo_set_line_width (context, 0.3);
	cairo_move_to(context, 0.0, height);
	for(i = LCH; i<=RCH-1; i+=10) {
		cairo_arc(context, x[i]+(double)width/(RCH-LCH), (double)height-y[i], 0.1, 0, 2*M_PI);
	}
	cairo_stroke_preserve(context);
	//cairo_fill(context);
	
	return surface;
}

static void analyze_energy_drawing(GtkWidget *widget, cairo_t *cr)
{
	int width = gtk_widget_get_allocated_width (widget);
	int height =  gtk_widget_get_allocated_height (widget);
	
	int i, j;
	
	int x_start = 0, x_stop = 0, x_range;
	static int x_left = 0, x_right = 0;
	static double y_comp = 1.0;
	
	int left_shift = 40;
	int bot_shift = 10;
	int tics[6];
	long int tics_y[8];
	int tics_range[8] = {50, 100, 200, 500, 1000, 2000, 3000, 5000};
	long int tics_range_y[8] = {1e3, 5e3, 10e3, 20e3, 90e3, 200e3, 500e3, 1e6};
	int tics_points[8] = {10, 25, 50, 100, 250, 400, 500, 1000};
	int tics_points_y[8] = {250, 1e3, 2e3, 5e3, 10e3, 40e3, 100e3, 200e3};
	char coord[6][10];
	char coord_y[8][10];
	long int y_range;
	double max_Y = 1.0;
	
	cairo_rectangle(cr, 0.0, 0.0, width, height);
	cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0);
	cairo_fill(cr);
	
	cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 1.0);
	cairo_set_line_width (cr, 0.5);
	cairo_rectangle(cr, 0.0, 0.0, width-left_shift, height-bot_shift);
	cairo_stroke(cr);
	
	printf("left flag = %d; x_left = %d; zoom_eny = %d; exec = %d\n", gflag.left_en, x_left, gflag.zoom_eny, gflag.exec);
	
	if(gflag.x == 1) {x_start = glob.x[0]/(width-left_shift)*MAX_CH; x_stop = glob.x[1]/(width-left_shift)*MAX_CH;}
	else {x_start = 0; x_stop = MAX_CH;}
	
	x_left += LR_EN_X*gflag.left_en;
	if(gflag.left_en == 1) gflag.left_en = 0;
	x_right += LR_EN_X*gflag.right_en;
	if(gflag.right_en == 1) gflag.right_en = 0;
	
	y_comp *= 1.0+gflag.zoom_eny*ZOOM_EN_Y;
	
	if(gflag.x == 1) gflag.undo_en = 1;
	if(gflag.zoom_eny == 1) gflag.undo_en = 2;
	if(gflag.exec == 1 ) gflag.undo_en = 3;
	
	if(gflag.zoom_eny == 1) gflag.zoom_eny = 0;
	
	if(gflag.zoom == -1) {x_start = 0; x_stop = MAX_CH; x_left = 0; x_right = 0; y_comp = 1.0; gflag.exec = 0;}
	
	/*
	x_right += LR_EN_X*gflag.right_en;
	if(gflag.right_en == 1) gflag.right_en = 0;
	if(gflag.right_en == -1) {gflag.right_en = 0; x_right = 0;}*/
			
	if( (x_stop+x_left-x_right>MAX_CH) || (x_start+x_left-x_right < 0) ) {printf("out of range\n"); x_start = 0; x_stop = MAX_CH; x_left = 0; x_right = 0;}
	
	printf("x_start = %d; x_stop = %d\n", x_start, x_stop);
	printf("cbe0 = %d cbe1 = %d cbe2 = %d cbe3 = %d\n", gflag.cbe[0], gflag.cbe[1], gflag.cbe[2], gflag.cbe[3]);
	
	x_range = x_stop-x_start;		
	for(j=0; j<8; j++)
		if (x_range < tics_range[j]) {
			for(i=0; i<=5; i++) {
				tics[i] = (int)tics_points[j]*((int)((x_start+x_left-x_right)/tics_points[j])+i);
				if((double)(width-left_shift)/(x_stop-x_start)*(tics[i]-x_start-x_left+x_right)>(double)(width-left_shift)) break;
				sprintf(coord[i], "%d", tics[i]);
				cairo_move_to(cr, (double)(width-left_shift)/(x_stop-x_start)*(tics[i]-x_start-x_left+x_right), height-bot_shift-5.0);
				cairo_line_to(cr, (double)(width-left_shift)/(x_stop-x_start)*(tics[i]-x_start-x_left+x_right), height-bot_shift+5.0);
				cairo_stroke(cr);
				cairo_move_to(cr, (double)(width-left_shift)/(x_stop-x_start)*(tics[i]-x_start-x_left+x_right), height);
				cairo_show_text(cr, coord[i]);
			}
			j = 8;
		}
	
	if(gflag.log_en != 1) {
		y_range = (long int)(height-bot_shift)*y_comp;
		for(j=0; j<8; j++)
			if(y_range < tics_range_y[j]) {
				for(i=0; i<8; i++) {
					tics_y[i] = (long int) (tics_points_y[j]*i);
					if(tics_y[i] > y_range) break;
					if(j!=0) sprintf(coord_y[i], "%.0fK", tics_y[i]/1000.0);
					else sprintf(coord_y[i], "%ld", tics_y[i]);
					cairo_move_to(cr, (double)(width-left_shift-5.0), height-bot_shift-(double)(height-bot_shift)*tics_y[i]/y_range);
					cairo_line_to(cr, (double)(width-left_shift+5.0), height-bot_shift-(double)(height-bot_shift)*tics_y[i]/y_range);
					cairo_stroke(cr);
					cairo_move_to(cr, (double)(width-left_shift+10.0), height-bot_shift-(double)(height-bot_shift)*tics_y[i]/y_range+5.0);
					cairo_show_text(cr, coord_y[i]);
				}
				j=8;
			}
	}
	else {
		double lg_tic = 1.0;
		max_Y = max_energy();

		for(i=0; i<8; i++) {
			if(lg_tic<=1000)
				sprintf(coord_y[i], "%.0f", lg_tic);
			else
				sprintf(coord_y[i], "%.0fK", lg_tic/1e3);
			cairo_move_to(cr, width-left_shift-5.0, height-15.0-(height-15.0)*log10(lg_tic)/log10(max_Y));
			cairo_line_to(cr, width-left_shift+5.0, height-15.0-(height-15.0)*log10(lg_tic)/log10(max_Y));
			cairo_stroke(cr);
			cairo_move_to(cr, width-left_shift+10.0, height-15.0-(height-15.0)*log10(lg_tic)/log10(max_Y));
			cairo_show_text(cr, coord_y[i]);
			
			lg_tic *= 10.0;
		}
		
	}
	
	for(i=0; i<4; i++)
		if(gflag.cbe[i]==1) {
			switch(i) {
				case 0: cairo_set_source_rgb (cr, 0.0, 0.0, 255.0);
						break;
				case 1: cairo_set_source_rgb (cr, 255.0, 0.0, 0.0);
						break;
				case 2: cairo_set_source_rgb (cr, 0.0, 255.0, 0.0);
						break;
				case 3: cairo_set_source_rgb (cr, 1.0, 0.0, .8);
						break;
				default: break;
			}
			printf("x_start = %d; x_stop = %d\n", x_start, x_stop);
			
			if(gflag.log_en != 1)
				for(j=x_start+x_left-x_right; j<=x_stop+x_left-x_right-1; j+=1)
					cairo_mask_surface(cr, pt_surface(), (double)(width-5.0-left_shift)/(x_stop-x_start)*(j-x_start-x_left+x_right), (height-15.0-(double)spectras->energyspk[i][j]/y_comp));
			else
				for(j=x_start+x_left-x_right; j<=x_stop+x_left-x_right-1; j+=1)
					cairo_mask_surface(cr, pt_surface(), (double)(width-5.0-left_shift)/(x_stop-x_start)*(j-x_start-x_left+x_right), height-15.0-(height-15.0)*log10( (double)spectras->energyspk[i][j] )/log10(max_Y));
			
			if(gflag.exec == 1) {
				printf("x0 = %.2f x30 = %.2f x31 = %.2f\n", glob.x[0], glob.x3[0], glob.x3[1]);
				cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
				
				GtkTextIter iter_start, iter_stop;
				int *kindex = (int *)calloc(3, sizeof(int));
				
				if(gflag.manual_range == 1) {
					kindex[0] = (int)gflag.exec_range[0];
					kindex[1] = (int)gflag.exec_range[1];
				}
				else {
					gflag.exec_range[0] = kindex[0] = (int)( x_left+x_start+MAX_CH*(glob.x3[0])/((width-left_shift)*MAX_CH/(x_stop-x_start)) );
					gflag.exec_range[1] = kindex[1] = (int)( x_left+x_start+MAX_CH*(glob.x3[1])/((width-left_shift)*MAX_CH/(x_stop-x_start)) );
				}
				
				for(j=0; j<3; j++)
					printf("kindex[%d] = %d\n", j, kindex[j]);
				
				float *u = (float *)calloc(9, sizeof(float));;
				DenisCalc(spectras->energyspk[i], NULL, kindex[0], kindex[1], 1, 0, 0, u);
				
				kindex[2] = u[2];

				for(j=0; j<9; j++)
					printf("u[%d] = %.1f;\n", j, u[j]);
					
				for(j=0; j<3; j++)
					printf("kindex[%d] = %d\n", j, kindex[j]);
		
				/*gchar *text = g_strdup_printf ("\t\tExecute\n\nS = %.2f +- %.2f (%.2f +- %.2f KeV) \n\nPick postion = %.2f +- %.2f (%.2f +- %.2f KeV) \n\nFWHM = %.2f", u[0], u[1], u[7], u[8], u[2], u[3], u[9], a_en*u[3], u[4]);
				gtk_label_set_text (GTK_LABEL (dcalc_label), text);
				gtk_misc_set_alignment(GTK_MISC(dcalc_label), 0.5, 0.5);
				gtk_widget_set_size_request(dcalc_label, 320, 100);
				g_free ((gpointer) text);*/
				
				gtk_entry_set_text (GTK_ENTRY(coord_xy_en.entry_lrange), g_strdup_printf("%d", kindex[0]));
				gtk_entry_set_text (GTK_ENTRY(coord_xy_en.entry_rrange), g_strdup_printf("%d", kindex[1]));
				
				//u5 = k u6 = b y=kx+b
				cairo_set_line_width (cr, 0.2);
				cairo_set_source_rgb(cr, 180.0/255.0, 59.0/255.0, 91.0/255.0);
				
				cairo_move_to(cr, (double)(width-15.0)/(x_stop-x_start)*(kindex[0]-x_start-x_left+x_right), height-15.0-(u[5]*kindex[0]+u[6])/y_comp);
				
				for(j=kindex[0]; j<kindex[1]; j++)
					if((double)spectras->energyspk[i][j]>=u[5]*j+u[6])
						cairo_line_to(cr, (double)(width-5.0-left_shift)/(x_stop-x_start)*(j-x_start-x_left+x_right)+2.5, height-15.0-(double)spectras->energyspk[i][j]/y_comp-2.5);
					else
						cairo_line_to(cr, (double)(width-5.0-left_shift)/(x_stop-x_start)*(j-x_start-x_left+x_right)+2.5, height-15.0-(double)(u[5]*j+u[6])/y_comp-2.5);
						
			//	cairo_close_path(cr);
			//	cairo_move_to(cr, (double)(width-15.0)/(x_stop-x_start)*(kindex[1]-x_start-x_left+x_right), height-15.0-(double)spectras->energyspk[i][kindex[1]]/y_comp);
				
				cairo_line_to(cr, (double)(width-5.0-left_shift)/(x_stop-x_start)*(kindex[1]-x_start-x_left+x_right)+2.5, height-15.0-(double)(u[5]*kindex[1]+u[6])/y_comp-2.5);
				cairo_line_to(cr, (double)(width-5.0-left_shift)/(x_stop-x_start)*(kindex[0]-x_start-x_left+x_right)+2.5, height-15.0-(double)(u[5]*kindex[0]+u[6])/y_comp-2.5);
				cairo_fill(cr);
			
				printf("kind0 = %d kind1 = %d x_start = %d\n", kindex[0], kindex[1], x_start);
				
				cairo_set_line_width (cr, 1.5);
				cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
				
				cairo_move_to(cr, (double)(width-5.0-left_shift)/(x_stop-x_start)*(kindex[0]-x_start-x_left+x_right)+2.5, height-bot_shift);
				cairo_line_to(cr, (double)(width-5.0-left_shift)/(x_stop-x_start)*(kindex[0]-x_start-x_left+x_right)+2.5, height-15.0-(double)(u[5]*kindex[0]+u[6])/y_comp-2.5);
				cairo_stroke(cr);
				
				cairo_move_to(cr, (double)(width-5.0-left_shift)/(x_stop-x_start)*(kindex[1]-x_start-x_left+x_right)+2.5, height-bot_shift);
				cairo_line_to(cr, (double)(width-5.0-left_shift)/(x_stop-x_start)*(kindex[1]-x_start-x_left+x_right)+2.5, height-15.0-(double)(u[5]*kindex[1]+u[6])/y_comp-2.5);
				cairo_stroke(cr);
					
				cairo_set_source_rgb(cr, 255.0/255.0, 106.0/255.0, 0.0/255.0);
				cairo_move_to(cr, (double)(width-5.0-left_shift)/(x_stop-x_start)*(kindex[2]-x_start-x_left+x_right)+2.5, height-bot_shift);
				cairo_line_to(cr, (double)(width-5.0-left_shift)/(x_stop-x_start)*(kindex[2]-x_start-x_left+x_right)+2.5, height-bot_shift-(double)spectras->energyspk[i][kindex[2]]/(y_comp)-2.5);
				cairo_stroke(cr);
				
				gtk_text_buffer_get_iter_at_line_offset(energyspk_buffer_execute, &iter_start, 1, 0);
				gtk_text_buffer_get_end_iter(energyspk_buffer_execute, &iter_stop);
				gtk_text_buffer_delete(energyspk_buffer_execute, &iter_start, &iter_stop);
				gtk_text_buffer_insert(energyspk_buffer_execute, &iter_start, g_strdup_printf("Left = %d ch \t Right = %d ch\n\n", kindex[0], kindex[1]), -1);
				
			//	gtk_text_buffer_get_iter_at_line_offset(energyspk_buffer_execute, &iter_start, 1, 0);
				gtk_text_buffer_insert(energyspk_buffer_execute, &iter_start, g_strdup_printf("Area = %.1f (%.1f) ch\t Int = %.1f ch \n\nPick postion = %.1f (%.1f) ch \n\t\t\t\t %.1f (%.1f) KeV \n\nFWHM = %.1f (%.1f) ch\t \n\t\t %.1f KeV \n\t\t %.1f %%", u[0], u[1], u[8], u[2], u[3], a_en*u[2]+b_en, a_en*u[3], u[4], u[7], a_en*u[4], 100.0*u[4]/u[2]), -1);
			
				free(kindex);
				free(u);
			}
		}
		
	gflag.draw = 0; gflag.zoom = 0; gflag.down = 0;
}

static gboolean clicked_in_analyze(GtkWidget *widget, GdkEventButton *event,
    gpointer user_data)
{
	printf("darea was clicked x=%.2f, y=%.2f flag = %d\n", event->x, event->y, gflag.x);
	
	//global flag = 3 - поставлена одна граница, global flag = 1 - выставлено две границы
	
	if(event->button == 1) {
		gflag.button = 1;
		if(gflag.x == 3) {
			glob.x[1] = event->x;
			glob.y[1] = event->y;
			gflag.x = 1;
			gflag.zoom = 1;
			gtk_widget_queue_draw(widget);
		}
		else {
			glob.x[0] = event->x;
			glob.y[0] = event->y;
			gflag.x = 3;
		}
	}
	
	if(event->button == 3) {
		gflag.button = 3;
		if(gflag.exec == 3) {
			glob.x3[1] = event->x;
			glob.y3[1] = event->y;
			gflag.exec = 1;
			gflag.manual_range = 0;
			gtk_widget_queue_draw(widget);
		}
		else {
			glob.x3[0] = event->x;
			glob.y3[0] = event->y;
			gflag.exec = 3;
		}
	}
		
    return TRUE;
}

void statusbar_push(const gchar *text)
{
	gtk_statusbar_push(GTK_STATUSBAR(statusbar), sb_context_id, text);
}

void zoom_out( GtkWidget *widget,
               gpointer   data )
{
	//gflag.zoom = 1;
	GtkTextIter iter_start, iter_stop;
	
	gflag.x = 0; gflag.y = 0; gflag.zoom = -1; gflag.log_en = 0; gflag.exec = 0; gflag.manual_range = 0;
	
	gtk_text_buffer_get_iter_at_line_offset(energyspk_buffer_execute, &iter_start, 1, 0);
	gtk_text_buffer_get_end_iter(energyspk_buffer_execute, &iter_stop);
	gtk_text_buffer_delete(energyspk_buffer_execute, &iter_start, &iter_stop);
	
	gtk_button_set_label(GTK_BUTTON(button_log_en), "log");
	
	gtk_widget_queue_draw((GtkWidget *) data);
}

void vertical_up( GtkWidget *widget,
               gpointer   data )
{
	printf("vert up\n");

	gflag.zoom_eny = 1;
	
	gtk_widget_queue_draw((GtkWidget *) data);
}

void vertical_down( GtkWidget *widget,
               gpointer   data )
{
	printf("vert down\n");
	//gflag.y = 1;
	
	gflag.x = 0; gflag.y = 0; gflag.zoom = 0; gflag.down = 1;
	
	gtk_widget_queue_draw((GtkWidget *) data);
}

void horizontal_left( GtkWidget *widget,
               gpointer   data )
{
	printf("horizontal left\n");
	
	gflag.left_en = 1;
	
	gtk_widget_queue_draw((GtkWidget *) data);
}

void horizontal_right( GtkWidget *widget,
               gpointer   data )
{
	printf("horizontal right\n");
	
	gflag.right_en = 1;
	
	gtk_widget_queue_draw((GtkWidget *) data);
}


void show_execute( GtkWidget *widget,
               gpointer   data )
{	
	gflag.button = 3; gflag.x = 5; gflag.exec = 1;
	
	gtk_widget_queue_draw((GtkWidget *) data);
}

void log_scale_en( GtkWidget *widget,
               gpointer   data )
{
	printf("log_scale_en start\n");
	
	int k = 0;
	
	static number_itt = 0;
	
	if(number_itt == 0) {
		gtk_button_set_label(GTK_BUTTON(button_log_en), "lin");
		number_itt = 1;
		gflag.log_en = 1;
	}
	else {
		gtk_button_set_label(GTK_BUTTON(button_log_en), "log");
		number_itt = 0;
		gflag.log_en = 0;
	}
	
	gtk_widget_queue_draw((GtkWidget *) data);
}

void undo_en( GtkWidget *widget,
               gpointer   data )
{
	printf("undo en func\n");
	
	if (gflag.undo_en == 1) gflag.zoom=-1; // zoom X
	else if (gflag.undo_en == 2) gflag.zoom_eny=-1; // zoom Y
	else if (gflag.undo_en == 3) { // exec
		GtkTextIter iter_start, iter_stop;
	
		gtk_text_buffer_get_iter_at_line_offset(energyspk_buffer_execute, &iter_start, 1, 0);
		gtk_text_buffer_get_end_iter(energyspk_buffer_execute, &iter_stop);
		gtk_text_buffer_delete(energyspk_buffer_execute, &iter_start, &iter_stop);
		gflag.exec = 0; 
		if(gflag.zoom_eny == -1) gflag.zoom_eny = 0;
	}
	
	gtk_widget_queue_draw((GtkWidget *)data);
}

void callibration_en_cb(GtkWidget *dialog, gint response, GtkWidget **entries)
{
	int i;
	double calibr[4];
	
	switch (response)
	{
    case GTK_RESPONSE_ACCEPT: {
		for(i=0; i<4; i++)
			calibr[i] = atof( gtk_entry_get_text(GTK_ENTRY(entries[i])) );
		if(fabs(calibr[2]-calibr[3]) < EPS) {printf("callibration EN spk error\n"); break;}
		a_en = (calibr[0]-calibr[1])/(calibr[2]-calibr[3]);
		b_en = calibr[0] - a_en*calibr[2];
	}
       break;
    default:
       break;
	}
	gtk_widget_destroy (dialog);
	
	for(i=0; i<4; i++)
			printf("calibr[%d] = %.2f\t", i, calibr[i]);
	printf("\n y = %.2f*x + %.2f\n", a_en, b_en);
}

void callibration_en( GtkWidget *widget,
               gpointer   window )
{
	printf("main callibration en func\n");
	
	int i, j;
	GtkWidget *dialog, *content_area;
	GtkWidget *grid, *label;
	GtkWidget **entry;
	//E = a*CH+b;
	
	dialog = gtk_dialog_new_with_buttons ("Calibration EN spectrum",
                                         window,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         NULL);
    gtk_window_set_modal (GTK_WINDOW(dialog), FALSE);
	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	
	grid = gtk_grid_new();
	//gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
	gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
	
	entry = g_new(GtkWidget *, 4);
	for(i=0; i<4; i++)
		entry[i] = gtk_entry_new();
	
	gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Energy, KeV"), 1, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Channel"), 2, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), gtk_label_new("1 pick"), 0, 1, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), gtk_label_new("2 pick"), 0, 2, 1, 1);
	
	gtk_grid_attach(GTK_GRID(grid), entry[0], 1, 1, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), entry[1], 1, 2, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), entry[2], 2, 1, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), entry[3], 2, 2, 1, 1);
	
	gtk_container_add (GTK_CONTAINER (content_area), grid);
	gtk_widget_show_all (dialog);
	
	g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(callibration_en_cb), entry);

	printf("entry = %p\n", (GtkWidget *)entry);
}

void save_as_en_range( GtkWidget *button,
               gpointer   window )
{
	printf("save as en range func started\n");
	
	int i;
	
	for(i=0; i<4; i++)
		printf("gflag.cbe[%d] = %d\n", i, gflag.cbe[i]);
	
	if(strcmp(gtk_button_get_label(GTK_BUTTON(button)), "Save as LWindow") == 0) {
		printf("cbe0 = %d; range0 = %.2f, range1 = %.2f\n", gflag.cbe[0], gflag.exec_range[0], gflag.exec_range[1]);
		for(i=0; i<4; i++)
			if(gflag.cbe[i] == 1) {
				printf("1\n");
				infostruct.LD[2*i] = (int)gflag.exec_range[0]; infostruct.LD[2*i+1] = (int)gflag.exec_range[1];
			}	
	}
	if(strcmp(gtk_button_get_label(GTK_BUTTON(button)), "Save as RWindow") == 0) {
		printf("cbe0 = %d; range0 = %.2f, range1 = %.2f\n", gflag.cbe[0], gflag.exec_range[0], gflag.exec_range[1]);
		for(i=0; i<4; i++)
			if(gflag.cbe[i] == 1) {
				printf("2\n");
				infostruct.RD[2*i] = (int)gflag.exec_range[0]; infostruct.RD[2*i+1] = (int)gflag.exec_range[1];
			}	
	}
}

void ok_enrange_cb( GtkWidget *button,
               gpointer   window )
{
	printf("ok enrange func started\n");

	gflag.exec_range[0] = atof( gtk_entry_get_text(GTK_ENTRY(coord_xy_en.entry_lrange)) );
	gflag.exec_range[1] = atof( gtk_entry_get_text(GTK_ENTRY(coord_xy_en.entry_rrange)) );
	
	gflag.exec = 1;
	gflag.manual_range = 1;
	
	gtk_widget_queue_draw(window);
}

int get_line_end(const char *buffer, char reset_flag)
{
	int i = 0;
	static int j = 0;
	static long int ptr_to_endline = 0;
	
	if(reset_flag == 0) { ptr_to_endline = 0; return 0;}
	
	while(buffer[ptr_to_endline+i] != '\n') {
		i++;
	}
	ptr_to_endline += i+1;
	
	//printf("endline = %d; %d\n", i+1, j);
	j++;
	
	return (i+1);
}

void read_plot_spk( GtkWidget *widget, gpointer window )
{
	char end_of_file;
	
	int count=0;
	int in, i, j;
	int x[8];
	int N[8];
	char *s1 = (char *)malloc(64*sizeof(char));
	
	DIR	*d;
	struct dirent *dir;
	
	FILE *pFile;
	long lSize;
	unsigned char *buffer;
	size_t result;
	
	free(spectras);
	spectras = (dataspk *)malloc(sizeof(dataspk));
	for(i=0; i<12; i++)
		for(j=0; j<=MAX_CH-1; j++)
			spectras->timespk[i][j] = 0;
	for(i=0; i<4; i++)
		for(j=0; j<=MAX_CH-1; j++)
			spectras->energyspk[i][j] = 0;
	
	d = opendir(folder2spk);
	printf("000\n");
	if (d)
	{
		strcpy(infostruct.dir, folder2spk);
		GtkTextIter iter, end;
		gtk_text_buffer_get_iter_at_line(info_buffer, &iter, 0);
		gtk_text_buffer_get_iter_at_line_offset(info_buffer, &end, 1, 0);
		gtk_text_buffer_delete(info_buffer, &iter, &end);
		gtk_text_buffer_get_iter_at_line(info_buffer, &iter, 0);
		gtk_text_buffer_insert_with_tags_by_name(info_buffer, &iter, 
		g_strdup_printf("Dir: %s\n", infostruct.dir), -1, "bold", NULL);
		
		printf("123\n");
		while ((dir = readdir(d)) != NULL)
		{
			if (dir->d_type == DT_REG)
			{	
				sprintf(s1, "%s%s", folder2spk, dir->d_name);
				printf("%s\n", s1); 
	
				pFile = fopen ( s1, "r" );
				if (pFile==NULL) {fputs ("File error\n", stderr); exit(0);}
	
				fseek (pFile, 0, SEEK_END);
				lSize = ftell (pFile);
				rewind (pFile);

				buffer = (unsigned char *) malloc (sizeof(unsigned char)*lSize);
				if (buffer == NULL) {fputs ("Memory error\n", stderr); exit (0);}

				// copy the file into the buffer:
				result = fread (buffer, 1, lSize, pFile);
				if (result != lSize) {fputs ("Reading error\n", stderr); exit (0);}
	
				/* the whole file is now loaded in the memory buffer. */

				printf("lSize = %ld\n", lSize);
				/* the whole file is now loaded in the memory buffer. */
			
				for(i=0; i<=lSize-1; i+=8) {

					for(j=0; j<8; j++)
						x[j] = buffer[i+j];
						
					if(i==0) for(j=0; j<8; j++)
						printf("x[%d]=%d\n", j, x[j]);
					
					N[1] = (int)(x[0]+256*(x[1] & 0b00001111));
					N[7] = (int)(10*x[6]+x[7]);
				//	N[7] &= 0b00001111;
				
				//	N[7] = (int)(x[6]+256*(x[7] & 0b00001111));
				//	printf("N[7] = %d\n", N[7]);
				//	N[7] &= 0b00001111;
					
					switch(N[7]) {
						case 12 : { N[2] = (int)(x[2]+256*(x[3] & 0b00001111)); N[3] = (int)(x[4]+256*(x[5] & 0b00001111));
							spectras->energyspk[0][N[2]]++; spectras->energyspk[1][N[3]]++; 
							if (N[2] >= infostruct.LD[0] && N[2] <= infostruct.LD[1] && N[3] >= infostruct.RD[2] && N[3] <= infostruct.RD[3]) {
								spectras->timespk[1][N[1]]++; }
							else if (N[2] >= infostruct.RD[0] && N[2] <= infostruct.RD[1] && N[3] >= infostruct.LD[2] && N[3] <= infostruct.LD[3]) {
								spectras->timespk[0][N[1]]++; }
						break;
						};
						case 13 : { N[2] = (int)(x[2]+256*(x[3] & 0b00001111)); N[4] = (int)(x[4]+256*(x[5] & 0b00001111));
							spectras->energyspk[0][N[2]]++; spectras->energyspk[2][N[4]]++; 
							if (N[2] >= infostruct.LD[0] && N[2] <= infostruct.LD[1] && N[4] >= infostruct.RD[4] && N[4] <= infostruct.RD[5])
								spectras->timespk[3][N[1]]++;
							else if (N[2] >= infostruct.RD[4] && N[2] <= infostruct.RD[5] && N[4] >= infostruct.LD[0] && N[4] <= infostruct.LD[1])
								spectras->timespk[2][N[1]]++;
						break;
						};
						case 14 : { N[2] = (int)(x[2]+256*(x[3] & 0b00001111)); N[6] = (int)(x[4]+256*(x[5] & 0b00001111));
							spectras->energyspk[0][N[2]]++; spectras->energyspk[3][N[6]]++; 
							if (N[2] >= infostruct.LD[0] && N[2] <= infostruct.LD[1] && N[6] >= infostruct.RD[6] && N[6] <= infostruct.RD[7])
								spectras->timespk[5][N[1]]++;
							else if (N[2] >= infostruct.RD[6] && N[2] <= infostruct.RD[7] && N[6] >= infostruct.LD[0] && N[6] <= infostruct.LD[1])
								spectras->timespk[4][N[1]]++;
						break;
						};
						case 23 : { N[3] = (int)(x[2]+256*(x[3] & 0b00001111)); N[4] = (int)(x[4]+256*(x[5] & 0b00001111));
							spectras->energyspk[1][N[3]]++; spectras->energyspk[2][N[4]]++; 
							if (N[3] >= infostruct.LD[2] && N[3] <= infostruct.LD[3] && N[4] >= infostruct.RD[4] && N[4] <= infostruct.RD[5])
								spectras->timespk[7][N[1]]++;
							else if (N[3] >= infostruct.RD[4] && N[3] <= infostruct.RD[5] && N[4] >= infostruct.LD[2] && N[4] <= infostruct.LD[3])
								spectras->timespk[6][N[1]]++;
						break;
						};
						case 24 : { N[3] = (int)(x[2]+256*(x[3] & 0b00001111)); N[6] = (int)(x[4]+256*(x[5] & 0b00001111));
							spectras->energyspk[1][N[3]]++; spectras->energyspk[3][N[6]]++; 
							if (N[3] >= infostruct.LD[2] && N[3] <= infostruct.LD[3] && N[6] >= infostruct.RD[6] && N[6] <= infostruct.RD[7])
								spectras->timespk[9][N[1]]++;
							else if (N[3] >= infostruct.RD[6] && N[3] <= infostruct.RD[7] && N[6] >= infostruct.LD[2] && N[6] <= infostruct.LD[3])
								spectras->timespk[8][N[1]]++;
							break;
						};
						case 34 : { N[4] = (int)(x[2]+256*(x[3] & 0b00001111)); N[6] = (int)(x[4]+256*(x[5] & 0b00001111));
							spectras->energyspk[2][N[4]]++; spectras->energyspk[3][N[6]]++; 
							if (N[4] >= infostruct.LD[4] && N[4] <= infostruct.LD[5] && N[6] >= infostruct.RD[6] && N[6] <= infostruct.RD[7])
								spectras->timespk[11][N[1]]++;
							else if (N[4] >= infostruct.RD[6] && N[4] <= infostruct.RD[7] && N[6] >= infostruct.LD[2] && N[6] <= infostruct.LD[3])
								spectras->timespk[10][N[1]]++;
							break;
						};
						default: break;
					} 
		
				}
			}
		}

		closedir(d);
		free(s1);
	}
	
	if(glob.flag != 3)
		glob.flag=1;
	
	printf("Event-by-event-end\n");
	gtk_widget_queue_draw(window);
}

void get_online_data_cycle( gpointer data )
{
	printf("getdata cb was\n");
	int i, j;
	int fd_in;
	char *infostring;
	static double intensity0[4];
	static gboolean first_start = TRUE;
	
	if(first_start)
		infostring = (char *)malloc(300*sizeof(char));
	
	fd_in = open(SIMPLE_DEVICE, O_RDWR);
	if (fd_in < 0) {
        perror("error in device");
	}
	
	free(spectras);
	spectras = (dataspk *)malloc(sizeof(dataspk));
	for(i=0; i<12; i++)
		for(j=0; j<=MAX_CH-1; j++)
			spectras->timespk[i][j] = 0;
	for(i=0; i<4; i++)
		for(j=0; j<=MAX_CH-1; j++)
			spectras->energyspk[i][j] = 0;
	
	printf("free ok\n");
	
	read(fd_in, spectras, sizeof(dataspk));
	close(fd_in);
	
	for(i=1; i<=4000; i*=10)
		printf("spk[%d] = %d\t", i, spectras->energyspk[0][i]);
	
	if(!first_start)	
		for(i=0; i<4; i++)
			infostruct.intensity[i] = intensity0[i];
	else
		for(i=0; i<4; i++)
			intensity0[i] = infostruct.intensity[i];
	
	for(i=0; i<4; i++)
		for(j=0; j<=MAX_CH-1; j++)
			intensity0[i] += (double)spectras->energyspk[i][j];
	
	if(!first_start)
		for(i=0; i<4; i++)
			infostruct.intensity[i] = (intensity0[i]-infostruct.intensity[i])/(READ_ONLINE_PERIOD/1000.0);
	
//	infostring = g_strdup_printf(" <b>Dir:</b> %s \n <b>Start:</b> %s \n <b>Exps:</b> %d \n <b>Time:</b> %s \n <b>DTime:</b> %d \n <b>int1</b> - %.1e cnt/s. \n <b>int2</b> - %.1e cnt/s. \n <b>int3</b> - %.1e cnt/s. \n <b>int4</b> - %.1e cnt/s. \n", \
//							infostruct.dir, infostruct.start, infostruct.expos, convert_time_to_hms(infostruct.time), infostruct.dtime, infostruct.intensity[0], infostruct.intensity[1], infostruct.intensity[2], infostruct.intensity[3]);
//	gtk_label_set_markup(GTK_LABEL(label_info), infostring);
//	free(infostring);

	strncpy(infostruct.dir, SIMPLE_DEVICE, 60);
	GtkTextIter iter, end;
	gtk_text_buffer_get_iter_at_line(info_buffer, &iter, 6);
	gtk_text_buffer_get_iter_at_line_offset(info_buffer, &end, 10, 16);
	gtk_text_buffer_delete(info_buffer, &iter, &end);
	
	gtk_text_buffer_insert_with_tags_by_name(info_buffer, &iter, 
			g_strdup_printf("int #1: %.2e\n", infostruct.intensity[0]), -1, "bold", NULL);
	gtk_text_buffer_insert_with_tags_by_name(info_buffer, &iter, 
			g_strdup_printf("int #2: %.2e\n", infostruct.intensity[1]), -1, "bold", NULL);
	gtk_text_buffer_insert_with_tags_by_name(info_buffer, &iter, 
			g_strdup_printf("int #3: %.2e\n", infostruct.intensity[2]), -1, "bold", NULL);
	gtk_text_buffer_insert_with_tags_by_name(info_buffer, &iter, 
			g_strdup_printf("int #4: %.2e", infostruct.intensity[3]), -1, "bold", NULL);
	
	gtk_widget_queue_draw(data);
	
	glob.flag = 1;
	first_start = FALSE;
}

void get_data( GtkWidget *widget,
               gpointer   data )
{
	infostruct.tag_get_online_cycle = g_timeout_add(READ_ONLINE_PERIOD, (GSourceFunc) get_online_data_cycle, (gpointer) data);
}

void choose_spk( GtkWidget *widget,
               gpointer   data )
{ 
	GtkWidget *image = GTK_WIDGET(NULL);
	GtkWidget *toplevel = gtk_widget_get_toplevel (image);
	GtkFileFilter *filter = gtk_file_filter_new ();
	GtkWidget *dialog = gtk_file_chooser_dialog_new (("Open image"),
	                                                 GTK_WINDOW (toplevel),
	                                                 GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
	                                                 GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
	                                                 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	                                                 NULL);
	
	switch (gtk_dialog_run (GTK_DIALOG (dialog)))
	{
		case GTK_RESPONSE_ACCEPT:
		{
			folder2initialspk =
				gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
			printf("\nfolder_chosen = %s\n", folder2initialspk);
			strcat(folder2initialspk, "/");
		break;
		}
		default:
			break;
	}
	
	while(g_main_context_iteration(NULL, FALSE));
		gtk_widget_destroy (dialog);
	
	gchar text_to_sb[200];
	sprintf(text_to_sb, "Folder with data: %s. Now you can make BUFKA[1-4].SPK and TIME[1-12].SPK files and plot it", folder2initialspk);
	statusbar_push(text_to_sb);
}

void cbe1_clicked(GtkWidget *widget, gpointer window) {
	static char num_int = 0;
	
	if(gflag.cbe[0] == 2) 
		if(num_int == 1)
			num_int = 0;
	
	if(num_int == 1) { gflag.cbe[0] = 0; num_int = 0; gflag.x = 0; gflag.y = 0; gflag.zoom = 1; gflag.draw = 1; gflag.exec = 0;}
	else if (num_int == 0) { gflag.cbe[0] = 1; num_int = 1; gflag.x = 0; gflag.y = 0; gflag.zoom = 1; gflag.draw = 1;}

	gtk_widget_queue_draw(window);
}
void cbe2_clicked(GtkWidget *widget, gpointer window) {
	static char num_int = 0;
	
	if(gflag.cbe[1] == 2) 
		if(num_int == 1)
			num_int = 0;
	
	if(num_int == 1) { gflag.cbe[1] = 0; num_int = 0; gflag.x = 0; gflag.y = 0; gflag.zoom = 1; gflag.draw = 1; gflag.exec = 0;}
	else if (num_int == 0) { gflag.cbe[1] = 1; num_int = 1; gflag.x = 0; gflag.y = 0; gflag.zoom = 1; gflag.draw = 1;}
	
	gtk_widget_queue_draw(window);
}
void cbe3_clicked(GtkWidget *widget, gpointer window) {
	static char num_int = 0;
	
	if(gflag.cbe[2] == 2) 
		if(num_int == 1)
			num_int = 0;
	
	if(num_int == 1) { gflag.cbe[2] = 0; num_int = 0; gflag.x = 0; gflag.y = 0; gflag.zoom = 1; gflag.draw = 1; gflag.exec = 0;}
	else if (num_int == 0) { gflag.cbe[2] = 1; num_int = 1; gflag.x = 0; gflag.y = 0; gflag.zoom = 1; gflag.draw = 1;}
	
	gtk_widget_queue_draw(window);
}
void cbe4_clicked(GtkWidget *widget, gpointer window) {
	static char num_int = 0;
	
	if(gflag.cbe[3] == 2) 
		if(num_int == 1)
			num_int = 0;
	
	if(num_int == 1) { gflag.cbe[3] = 0; num_int = 0; gflag.x = 0; gflag.y = 0; gflag.zoom = 1; gflag.draw = 1; gflag.exec = 0;}
	else if (num_int == 0) { gflag.cbe[3] = 1; num_int = 1; gflag.x = 0; gflag.y = 0; gflag.zoom = 1; gflag.draw = 1;}
	
	gtk_widget_queue_draw(window);
}

void analyze_energy_spk( GtkWidget *widget,
               gpointer   data )
{
	GtkWidget *window;
	char s[60];
	GtkWidget *hbox, *vbox, *table, *hbox_entry, *hbox_entry_range;
	GtkWidget *darea;
	GtkWidget *cbe1, *cbe2, *cbe3, *cbe4;
	GdkColor color;
	GtkWidget *button0, *img_0, *button_up, *img_up, *button_down, *img_down, *button_left, *img_left, *button_right, *img_right, *button_plus, *img_plus, *button_minus, *button_execute, *button_undo, *img_undo;
	GtkWidget *button_calibration, *button_save_as_left_range, *button_save_as_right_range, *button_ok_range;
	GtkWidget *hseparator;
	GtkWidget *view_buffer; 
	GtkTextIter iter;
	
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	sprintf(s, "Energy analyze - %s", folder2readspk);
    gtk_window_set_title (GTK_WINDOW (window), s);
    gtk_widget_set_size_request (window, 640, 480);
    
    g_signal_connect (G_OBJECT (window), "destroy",
						G_CALLBACK (energy_analyze_destroy), window);
    
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    
    coord_xy_en.entry[0] = gtk_entry_new();
	coord_xy_en.entry[1] = gtk_entry_new();
    
    darea = gtk_drawing_area_new();
	gtk_widget_set_size_request (darea, 480, 480);
	g_signal_connect(G_OBJECT(darea), "draw", 
		G_CALLBACK(analyze_energy_draw_event), NULL);
	gtk_widget_add_events(darea, GDK_BUTTON_PRESS_MASK);
	gtk_widget_add_events(darea, GDK_POINTER_MOTION_MASK);
	gtk_widget_add_events(darea, GDK_POINTER_MOTION_HINT_MASK);
	g_signal_connect(darea, "button-press-event", 
      G_CALLBACK(clicked_in_analyze), (gpointer) window);
	//g_signal_connect (darea, "motion-notify-event",
	//	G_CALLBACK (show_motion_notify_in_energy), NULL);  
	gtk_box_pack_start (GTK_BOX (hbox), darea, TRUE, TRUE, 0);
		
	cbe1 = gtk_check_button_new_with_label("Energy SPK #1");
	gdk_color_parse ("blue", &color);
	gtk_widget_modify_fg (cbe1, GTK_STATE_NORMAL, &color);
	cbe2 = gtk_check_button_new_with_label("Energy SPK #2");
	gdk_color_parse ("red", &color);
	gtk_widget_modify_fg (cbe2, GTK_STATE_NORMAL, &color);
	cbe3 = gtk_check_button_new_with_label("Energy SPK #3");
	gdk_color_parse ("green", &color);
	gtk_widget_modify_fg (cbe3, GTK_STATE_NORMAL, &color);	
	cbe4 = gtk_check_button_new_with_label("Energy SPK #4");
	gdk_color_parse ("#FF00CD", &color);
	gtk_widget_modify_fg (cbe4, GTK_STATE_NORMAL, &color);
	
	g_signal_connect(G_OBJECT(cbe1), "clicked", 
          G_CALLBACK(cbe1_clicked), (gpointer) window);
	g_signal_connect(G_OBJECT(cbe2), "clicked", 
          G_CALLBACK(cbe2_clicked), (gpointer) window);
    g_signal_connect(G_OBJECT(cbe3), "clicked", 
          G_CALLBACK(cbe3_clicked), (gpointer) window);
    g_signal_connect(G_OBJECT(cbe4), "clicked", 
          G_CALLBACK(cbe4_clicked), (gpointer) window);
	
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start (GTK_BOX (vbox), cbe1, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), cbe2, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), cbe3, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), cbe4, FALSE, FALSE, 0);
	
	/*dcalc_label = gtk_label_new("Execute");
	gtk_misc_set_alignment(GTK_MISC(dcalc_label), 0.5, 0.5);
	gtk_widget_set_size_request(dcalc_label, 320, 100);*/
	
	view_buffer = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(view_buffer), FALSE);
	energyspk_buffer_execute = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view_buffer));
	gtk_misc_set_padding(GTK_MISC(energyspk_buffer_execute), 0, 20);
	gtk_widget_set_size_request(view_buffer, 400, 200);	
	gtk_text_buffer_create_tag(energyspk_buffer_execute, "bold", 
		"weight", PANGO_WEIGHT_BOLD, NULL);
	gtk_text_buffer_create_tag (energyspk_buffer_execute, "center",
        "justification", GTK_JUSTIFY_CENTER, NULL);
	gtk_text_buffer_get_iter_at_offset(energyspk_buffer_execute, &iter, 0);
	
	gtk_text_buffer_insert_with_tags_by_name(energyspk_buffer_execute, &iter, 
        "Execute results:\n", -1, "bold", "center", NULL);
	
	button0 = gtk_button_new();
	img_0 = gtk_image_new_from_file("./zoom-out.png");
	gtk_button_set_image(GTK_BUTTON(button0), img_0);
	g_signal_connect (G_OBJECT (button0), "clicked",
					G_CALLBACK (zoom_out), (gpointer) window);
	button_plus = gtk_button_new ();
	img_plus = gtk_image_new_from_file("./zoom-y.png");
	gtk_button_set_image(GTK_BUTTON(button_plus), img_plus);
	g_signal_connect (G_OBJECT (button_plus), "clicked",
					G_CALLBACK (vertical_up), (gpointer) window);
	button_up = gtk_button_new_from_stock (GTK_STOCK_GO_UP);
	g_signal_connect (G_OBJECT (button_up), "clicked",
					G_CALLBACK (vertical_up), (gpointer) window);
	button_down = gtk_button_new_from_stock (GTK_STOCK_GO_DOWN);
	g_signal_connect (G_OBJECT (button_down), "clicked",
					G_CALLBACK (vertical_down), (gpointer) window);
	button_left = gtk_button_new ();
	img_left = gtk_image_new_from_file("./go-left.png");
	gtk_button_set_image(GTK_BUTTON(button_left), img_left);
	g_signal_connect (G_OBJECT (button_left), "clicked",
					G_CALLBACK (horizontal_left), (gpointer) window);
	button_right = gtk_button_new ();
	img_right = gtk_image_new_from_file("./go-right.png");
	gtk_button_set_image(GTK_BUTTON(button_right), img_right);
	g_signal_connect (G_OBJECT (button_right), "clicked",
					G_CALLBACK (horizontal_right), (gpointer) window);
	button_execute = gtk_button_new_from_stock (GTK_STOCK_EXECUTE);
	g_signal_connect (G_OBJECT (button_execute), "clicked",
					G_CALLBACK (show_execute), (gpointer) window);
	button_log_en = gtk_button_new_with_label ("log");
	g_signal_connect (G_OBJECT (button_log_en), "clicked",
					G_CALLBACK (log_scale_en), (gpointer) window);
	button_undo = gtk_button_new ();
	img_undo = gtk_image_new_from_file("./edit-undo.png");
	gtk_button_set_image(GTK_BUTTON(button_undo), img_undo);
	g_signal_connect (G_OBJECT (button_undo), "clicked",
					G_CALLBACK (undo_en), (gpointer) window);
	
	button_calibration = gtk_button_new_with_label("Calibration");
	g_signal_connect (G_OBJECT (button_calibration), "clicked",
					G_CALLBACK (callibration_en), (gpointer) window);
	button_save_as_left_range = gtk_button_new_with_label("Save as LWindow");
	g_signal_connect (G_OBJECT (button_save_as_left_range), "clicked",
					G_CALLBACK (save_as_en_range), NULL);
	button_save_as_right_range = gtk_button_new_with_label("Save as RWindow");
	g_signal_connect (G_OBJECT (button_save_as_right_range), "clicked",
					G_CALLBACK (save_as_en_range), NULL);
	
	table = gtk_grid_new ();
	gtk_grid_attach(GTK_GRID(table), button0, 1, 4, 1, 1);
	gtk_grid_attach(GTK_GRID(table), button_plus, 1, 2, 1, 1);
//	gtk_grid_attach(GTK_GRID(table), button_up, 1, 1, 1, 1);
//	gtk_grid_attach(GTK_GRID(table), button_down, 1, 3, 1, 1);
	gtk_grid_attach(GTK_GRID(table), button_left, 0, 2, 1, 1);
	gtk_grid_attach(GTK_GRID(table), button_right, 2, 2, 1, 1);
	gtk_grid_attach(GTK_GRID(table), button_log_en, 0, 4, 1, 1);
	gtk_grid_attach(GTK_GRID(table), button_undo, 2, 4, 1, 1);
	gtk_grid_attach(GTK_GRID(table), button_calibration, 3, 2, 1, 2);
	gtk_grid_attach(GTK_GRID(table), button_save_as_left_range, 0, 5, 2, 1);
	gtk_grid_attach(GTK_GRID(table), button_save_as_right_range, 2, 5, 2, 1);

	gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
	
	hseparator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start (GTK_BOX (vbox), hseparator, FALSE, FALSE, 0);
	
	gtk_box_pack_start (GTK_BOX (vbox), view_buffer, FALSE, FALSE, 0);
	
	hbox_entry = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start (GTK_BOX (hbox_entry), gtk_label_new("CH:"), FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (hbox_entry), coord_xy_en.entry[0], FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (hbox_entry), gtk_label_new("cts:"), FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (hbox_entry), coord_xy_en.entry[1], FALSE, FALSE, 1);
	
	hbox_entry_range = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start (GTK_BOX (hbox_entry_range), gtk_label_new("Range in ch:"), FALSE, FALSE, 1);
	coord_xy_en.entry_lrange = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(coord_xy_en.entry_lrange), 4);
	gtk_entry_set_width_chars(GTK_ENTRY(coord_xy_en.entry_lrange), 6);
	coord_xy_en.entry_rrange = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(coord_xy_en.entry_rrange), 4);
	gtk_entry_set_width_chars(GTK_ENTRY(coord_xy_en.entry_rrange), 6);
	gtk_box_pack_start (GTK_BOX (hbox_entry_range), coord_xy_en.entry_lrange, FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (hbox_entry_range), coord_xy_en.entry_rrange, FALSE, FALSE, 1);
	button_ok_range = gtk_button_new_with_label("Ok");
	gtk_box_pack_start (GTK_BOX (hbox_entry_range), button_ok_range, FALSE, FALSE, 1);
	g_signal_connect (G_OBJECT (button_ok_range), "clicked",
				G_CALLBACK (ok_enrange_cb), (gpointer) window);
	
	gtk_widget_set_size_request(hbox_entry_range, 150, 20);
	
	gtk_box_pack_start (GTK_BOX (vbox), hbox_entry_range, FALSE, FALSE, 0);
	
	gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 2);
	
    gtk_container_add (GTK_CONTAINER (window), hbox);
    gtk_widget_show_all (window);
}

int read_ready_spk(GtkWidget *window)
{
	int i, j;
	int in;
	char name[60];
	
	free(spectras);
	spectras = (dataspk *)malloc(sizeof(dataspk));
	for(i=0; i<12; i++)
		for(j=0; j<=MAX_CH-1; j++)
			spectras->timespk[i][j] = 0;
	for(i=0; i<4; i++)
		for(j=0; j<=MAX_CH-1; j++)
			spectras->energyspk[i][j] = 0;
	
	for(i = 0; i <= NUM_TIME_SPK-1; i++) {
		sprintf(name, "%s%s%d%s", folder2readspk, "TIME", i+1, ".SPK");
		printf("%s\n", name);
		in = open(name, O_RDONLY);
		if (in < 0) {
			perror("error in device");
			return -1;
		}
		lseek(in, 512, 0);

		read(in, spectras->timespk[i], 4096*sizeof(int));

		close(in);
	}

	for(i = 0; i <= NUM_ENERGY_SPK-1; i++) {
		sprintf(name, "%s%s%d%s", folder2readspk, "BUFKA", i+1, ".SPK");
		printf("%s\n", name);
		in = open(name, O_RDONLY);
		if (in < 0) {
			perror("error in device");
			return -1;
		}
		lseek(in, 512, 0);

		read(in, spectras->energyspk[i], 4096*sizeof(int));

		close(in);
	}
	
	printf("1\n");
	gtk_widget_queue_draw(window);
	
	if(glob.flag != 3)
		glob.flag=1;
	
	printf("end--!!!\n");
	return 0;
}

void read_spk(GtkWidget *menuitem, gpointer window)
{	
	char s[60];
	GtkWidget *image = GTK_WIDGET(NULL);
	GtkWidget *toplevel = gtk_widget_get_toplevel (image);
	GtkFileFilter *filter = gtk_file_filter_new ();
	GtkWidget *dialog = gtk_file_chooser_dialog_new (("Open image"),
	                                                 GTK_WINDOW (toplevel),
	                                                 GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
	                                                 GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
	                                                 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	                                                 NULL);
	                                                             
	int i;
	char *infostring;
	
	/*gtk_file_filter_add_pattern (filter, "*.SPK");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);*/

	switch (gtk_dialog_run (GTK_DIALOG (dialog)))
	{
		case GTK_RESPONSE_ACCEPT:
		{
			folder2readspk =
				gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
			strcat(folder2readspk, "/");
			printf("\nfolder_chosen123= %s\n", folder2readspk);
			read_ready_spk(window);
			printf("end2\n");
			break;
		}
		default:
			break;
	}
	
	while(g_main_context_iteration(NULL, FALSE));
		gtk_widget_destroy (dialog);
	
	
	/*printf("end4\n");
	strcpy(infostruct.dir, folder2readspk);
	sprintf(infostring, " <b>Dir:</b> %s \n <b>Start:</b> %s \n <b>Exps:</b> %d \n <b>Time:</b> %d \n <b>DTime:</b> %d \n <b>int1</b> - %.0fcnt/s. \n <b>int2</b> - %.0fcnt/s. \n <b>int3</b> - %.0fcnt/s. \n <b>int4</b> - %.0fcnt/s. \n", \
							infostruct.dir, infostruct.start, infostruct.expos, infostruct.time, infostruct.dtime, infostruct.intensity[0], infostruct.intensity[1], infostruct.intensity[2], infostruct.intensity[3]);
	gtk_label_set_markup(GTK_LABEL(label_info), infostring);
*/

	strcpy(infostruct.dir, folder2readspk);
	GtkTextIter iter, end;
	gtk_text_buffer_get_iter_at_line(info_buffer, &iter, 1);
	gtk_text_buffer_get_iter_at_line_offset(info_buffer, &end, 2, 0);
	gtk_text_buffer_delete(info_buffer, &iter, &end);
	gtk_text_buffer_get_iter_at_line(info_buffer, &iter, 1);
	gtk_text_buffer_insert_with_tags_by_name(info_buffer, &iter, 
			g_strdup_printf("Dir: %s\n", infostruct.dir), -1, "bold", NULL);
	
	sprintf(s, "TDPAC BYK JINR - %s", folder2readspk);
	gtk_window_set_title (GTK_WINDOW (window), s);
}

void clock_func(gpointer data)
{
	printf("clock!!!\n");
	
	static gboolean first_start = TRUE;
	time_t curtime;
	struct tm *loctime;
	char *buffer = (char *)malloc(40*sizeof(char));
	GtkTextIter iter, end;

	curtime = time(NULL);
	loctime = localtime(&curtime);
	strftime(buffer, 40, "%R", loctime);
	
	gtk_text_buffer_get_iter_at_line(info_buffer, &iter, 0);
	if(!first_start) {
		gtk_text_buffer_get_iter_at_line_offset(info_buffer, &end, 1, 0);
		gtk_text_buffer_delete(info_buffer, &iter, &end);
	}
	gtk_text_buffer_insert_with_tags_by_name(info_buffer, &iter, 
			g_strdup_printf("Time: %s\n", buffer), -1, "bold", NULL);
			
	buffer = g_strdup_printf("%s", infostruct.start);
	
	if( compare_dates(infostruct.start) >= 0 || infostruct.status == 0) {
		printf("stoped time = %ld\n", compare_dates(buffer));
		strcpy(infostruct.start, "Stoped");
		gtk_text_buffer_get_iter_at_line(info_buffer, &iter, 2);
		gtk_text_buffer_get_iter_at_line_offset(info_buffer, &end, 3, 0);
		gtk_text_buffer_delete(info_buffer, &iter, &end);
		gtk_text_buffer_insert_with_tags_by_name(info_buffer, &iter, 
			g_strdup_printf("Start: %s\n", infostruct.start), -1, "bold", NULL);
		gtk_spinner_stop(GTK_SPINNER(spinner_get_data));
	}
	else {
		printf("started time = %ld\n", compare_dates(buffer));
		gtk_spinner_start(GTK_SPINNER(spinner_get_data));
	}
	
	free(buffer);
	first_start = FALSE;
}

void *doThreadClock(void *arg)
{
	printf("clock thread!!!\n");

	static gboolean first_start = TRUE;
	time_t curtime;
	struct tm *loctime;
	char *buffer = (char *)malloc(40*sizeof(char));
	static GtkTextIter iter, end;
	static GtkTextIter iter_status, end_status;

	curtime = time(NULL);
	loctime = localtime(&curtime);
	strftime(buffer, 40, "%R", loctime);
	
	gtk_label_set_text(GTK_LABEL(timer_label), g_strdup_printf("Time: %s", buffer));
	
	/*
	gtk_text_buffer_get_iter_at_line(info_buffer, &iter_status, 2);
	gtk_text_buffer_get_iter_at_line_offset(info_buffer, &end_status, 3, 0);
	if( compare_dates(infostruct.start) >= 0 || infostruct.status == 0) {
		printf("stoped time = %ld\n", compare_dates(buffer));
		strcpy(infostruct.start, "Stoped");
		printf("123");
		gtk_text_buffer_delete(info_buffer, &iter_status, &end_status);
		gtk_text_buffer_get_iter_at_line(info_buffer, &iter_status, 2);
		gtk_text_buffer_insert_with_tags_by_name(info_buffer, &iter_status, 
			g_strdup_printf("Start: %s\n", infostruct.start), -1, "bold", NULL);
		gtk_spinner_stop(GTK_SPINNER(spinner_get_data));
		printf("456");
	}
	else {
		printf("started time = %ld\n", compare_dates(buffer));
		gtk_spinner_start(GTK_SPINNER(spinner_get_data));
	}*/

	printf("789");
	if(first_start) {
		strftime(buffer, 40, "%S", loctime);
		sleep(60-atoi(buffer));
		printf("--\n");
	}
	else {
		sleep(60);
		printf("-\n");
	}
	
	free(buffer);

	first_start = FALSE;
	
	int err = pthread_create(&(tid), NULL, &doThreadClock, NULL);
    if (err != 0)
		printf("\ncan't create clock thread :[%s]", strerror(err));
	
	pthread_exit(NULL);
}

int main(int argc, char **argv)
{
	spectras0 = (dataspk *)malloc(sizeof(dataspk));
	int i, j;
	for(i=0; i<12; i++)
		for(j=0; j<=MAX_CH-1; j++)
			spectras0->timespk[i][j] = 0;
	for(i=0; i<4; i++)
		for(j=0; j<=MAX_CH-1; j++)
			spectras0->energyspk[i][j] = 0;
	
	GtkWidget *window;
	GtkWidget *main_box, *box;
	GtkWidget *menubar, *menu_file, *file, *menu_spectrum, *spectrum, *menu_aniz, *aniz, *menu_tools, *tools;
    GtkWidget *menu_get_data, *menu_read, *menu_event, *menu_save_as, *menu_zoom_out, *menu_quit, *menu_en_analyze, *menu_t_analyze, *menu_aniz_plot, *menu_choose_comp;
   // GtkToolItem *button, *button_get_data, *button_read, *separator1, *button_energy, *button_time, *separator2, *button_start, *button_pause, *button_stop, *button_clear;
   // GtkWidget *button_energy, *button_time;
    GtkWidget *hseparator;
    GtkWidget *grid_control;
    GtkWidget *scrolled_win;
    GtkWidget *button_mini, *image_mini;
    GtkWidget *en_label, *button_en_left, *img_en_left, *button_en_right, *img_en_right, *button_en_zoom_in, *img_en_zoom_in, *button_en_zoom_in_y, *img_en_zoom_in_y, *button_en_undo, *img_en_undo;
    GtkWidget *ent_separator, *label_separator, *label_separator1, *label_separator2, *label_separator3;
    GtkWidget *button_t_mini, *image_t_mini;
    GtkWidget *t_label, *button_t_left, *img_t_left, *button_t_right, *img_t_right, *button_t_zoom_in, *img_t_zoom_in, *button_t_zoom_in_y, *img_t_zoom_in_y, *button_t_undo, *img_t_undo;
    GtkWidget *button_start, *img_start, *button_stop, *img_stop, *button_pause, *img_pause, *button_clear, *img_clear, *button_view;
   
	GtkWidget *vbox_for_info;
   
    GtkWidget *viewinfo_buffer;
    GtkTextIter iter;
    
    char infostring[200];
    
	gtk_init (&argc, &argv);		

    /* Create a new window */
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

    gtk_window_set_title (GTK_WINDOW (window), "TDPAC BYK JINR");
    gtk_widget_set_size_request (window, 1200, 400);
    gtk_window_set_resizable (GTK_WINDOW(window), FALSE);

    /* It's a good idea to do this for all windows. */
    g_signal_connect (G_OBJECT (window), "destroy",
                        G_CALLBACK (gtk_main_quit), NULL);

    /* Sets the border width of the window. */
    gtk_container_set_border_width (GTK_CONTAINER (window), 1);
    gtk_widget_realize(window);
    
        /* This calls our box creating function */
    main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 1);
    
    gtk_container_add (GTK_CONTAINER (window), main_box);

	menubar = gtk_menu_bar_new();
	menu_file = gtk_menu_new();
	menu_spectrum = gtk_menu_new();
	menu_aniz = gtk_menu_new();
	menu_tools = gtk_menu_new();
	
	file = gtk_menu_item_new_with_label("File");
	spectrum = gtk_menu_item_new_with_label("Spectrum");
	aniz = gtk_menu_item_new_with_label("Anizotropy");
	tools = gtk_menu_item_new_with_label("Tools");
	
	menu_get_data = gtk_menu_item_new_with_label("Get online data");
	menu_read = gtk_menu_item_new_with_label("Read .spk");
	menu_event = gtk_menu_item_new_with_label("Read event-by-event");
	menu_save_as = gtk_menu_item_new_with_label("Save As");
	menu_quit = gtk_menu_item_new_with_label("Quit");
	
	menu_en_analyze = gtk_menu_item_new_with_label("Energy analyze");
	menu_t_analyze = gtk_menu_item_new_with_label("Time analyze");
	
	menu_choose_comp = gtk_menu_item_new_with_label("Choose compression");
	menu_aniz_plot = gtk_menu_item_new_with_label("Plot X90, X180 and anizotropy");
	
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(file), menu_file);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_file), menu_get_data);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_file), menu_read);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_file), menu_event);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_file), menu_save_as);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_file), menu_quit);
	gtk_menu_shell_append(GTK_MENU_SHELL(menubar), file);
	
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(spectrum), menu_spectrum);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_spectrum), menu_en_analyze);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_spectrum), menu_t_analyze);
	gtk_menu_shell_append(GTK_MENU_SHELL(menubar), spectrum);
	
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(aniz), menu_aniz);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_aniz), menu_choose_comp);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_aniz), menu_aniz_plot);
	gtk_menu_shell_append(GTK_MENU_SHELL(menubar), aniz);
	
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(tools), menu_tools);
	gtk_menu_shell_append(GTK_MENU_SHELL(menubar), tools);

	g_signal_connect (G_OBJECT (menu_get_data), "activate",
		G_CALLBACK (get_data), (gpointer) window);
    g_signal_connect (G_OBJECT (menu_read), "activate",
		G_CALLBACK (read_spk), (gpointer) window);
	g_signal_connect (G_OBJECT (menu_event), "activate",
		G_CALLBACK (read_plot_spk), (gpointer) window); //event-by-event
	g_signal_connect (G_OBJECT (menu_save_as), "activate",
		G_CALLBACK (save_as_spk), (gpointer) window);
	g_signal_connect(G_OBJECT (menu_quit), "activate",
        G_CALLBACK (gtk_main_quit_and_save_conf), NULL);
        
	g_signal_connect(G_OBJECT (menu_en_analyze), "activate",
        G_CALLBACK (analyze_energy_spk), NULL);
	g_signal_connect(G_OBJECT (menu_t_analyze), "activate",
        G_CALLBACK (analyze_time_spk), NULL);

	g_signal_connect(G_OBJECT (menu_choose_comp), "activate",
        G_CALLBACK (choose_aniz_comp), (gpointer) window);
	g_signal_connect(G_OBJECT (menu_aniz_plot), "activate",
        G_CALLBACK (plot_aniz), NULL);

	gtk_box_pack_start(GTK_BOX(main_box), menubar, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(main_box), box, TRUE, FALSE, 0);
	
	printf("123");
	
	/* Pack and show all our widgets */
   // gtk_widget_show(main_box);
	
	//main_table = gtk_grid_new ();
	
	hbox_en = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	hbox_t[0] = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	hbox_t[1] = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	
	plot_mini_spk();
	
	button_mini = gtk_button_new();
	image_mini = gtk_image_new_from_file("./zoom-out.png");
	gtk_button_set_image(GTK_BUTTON(button_mini), image_mini);
	g_signal_connect (G_OBJECT (button_mini), "clicked",
                        G_CALLBACK (minimize_func), (gpointer) window);
	
	label_separator = gtk_label_new("");
	gtk_widget_set_size_request(label_separator, 60, 12);
	en_label = gtk_label_new("En SPK");
	button_en_left = gtk_button_new();
	img_en_left = gtk_image_new_from_file("./go-left.png");
	gtk_button_set_image(GTK_BUTTON(button_en_left), img_en_left);
	g_signal_connect (G_OBJECT (button_en_left), "clicked",
                        G_CALLBACK (main_left_en_func), (gpointer) window);
    button_en_log = gtk_button_new_with_label("log");
    g_signal_connect (G_OBJECT (button_en_log), "clicked",
                        G_CALLBACK (main_log_en_func), (gpointer) window);
	button_en_right = gtk_button_new();
	img_en_right = gtk_image_new_from_file("./go-right.png");
	gtk_button_set_image(GTK_BUTTON(button_en_right), img_en_right);
	g_signal_connect (G_OBJECT (button_en_right), "clicked",
                        G_CALLBACK (main_right_en_func), (gpointer) window);
	button_en_zoom_in = gtk_button_new();
	img_en_zoom_in = gtk_image_new_from_file("./zoom-x.png");
	gtk_button_set_image(GTK_BUTTON(button_en_zoom_in), img_en_zoom_in);
	g_signal_connect (G_OBJECT (button_en_zoom_in), "clicked",
                        G_CALLBACK (main_zoom_in_en_func), (gpointer) window);
    button_en_zoom_in_y = gtk_button_new();
    img_en_zoom_in_y = gtk_image_new_from_file("./zoom-y.png");
    gtk_button_set_image(GTK_BUTTON(button_en_zoom_in_y), img_en_zoom_in_y);
    g_signal_connect (G_OBJECT (button_en_zoom_in_y), "clicked",
                        G_CALLBACK (main_zoom_in_en_y_func), (gpointer) window);
    button_en_undo = gtk_button_new();
    img_en_undo = gtk_image_new_from_file("./edit-undo.png");
    gtk_button_set_image(GTK_BUTTON(button_en_undo), img_en_undo);
    g_signal_connect (G_OBJECT (button_en_undo), "clicked",
                        G_CALLBACK (main_undo_en_func), (gpointer) window);
	
	grid_control = gtk_grid_new ();
//	gtk_grid_insert_column(GTK_GRID(grid_control), 0);
	gtk_grid_set_column_spacing(GTK_GRID(grid_control), 1);
	
	gtk_grid_attach(GTK_GRID(grid_control), label_separator, 1, 0, 3, 3);
	gtk_grid_attach(GTK_GRID(grid_control), en_label, 5, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid_control), button_en_zoom_in_y, 5, 1, 1, 1);
	gtk_grid_attach(GTK_GRID(grid_control), button_en_left, 4, 2, 1, 1);
	gtk_grid_attach(GTK_GRID(grid_control), button_en_log, 4, 1, 1, 1);
	gtk_grid_attach(GTK_GRID(grid_control), button_en_right, 6, 2, 1, 1);
	gtk_grid_attach(GTK_GRID(grid_control), button_en_zoom_in, 5, 2, 1, 1);
	
	gtk_grid_attach(GTK_GRID(grid_control), button_mini, 5, 3, 1, 1);
	gtk_grid_attach(GTK_GRID(grid_control), button_en_undo, 4, 3, 1, 1);
	
	ent_separator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
	
	button_t_mini = gtk_button_new();
	image_t_mini = gtk_image_new_from_file("./zoom-out.png");
	gtk_button_set_image(GTK_BUTTON(button_t_mini), image_t_mini);
	g_signal_connect (G_OBJECT (button_t_mini), "clicked",
                        G_CALLBACK (minimize_t_func), (gpointer) window);
	t_label = gtk_label_new("Time SPK");
	button_t_left = gtk_button_new();
	img_t_left = gtk_image_new_from_file("./go-left.png");
	gtk_button_set_image(GTK_BUTTON(button_t_left), img_t_left);
	g_signal_connect (G_OBJECT (button_t_left), "clicked",
						G_CALLBACK (main_left_t_func), (gpointer) window);
	button_t_log = gtk_button_new_with_label("log");
    g_signal_connect (G_OBJECT (button_t_log), "clicked",
                        G_CALLBACK (main_log_t_func), (gpointer) window);
	button_t_right = gtk_button_new();
	img_t_right = gtk_image_new_from_file("./go-right.png");
	gtk_button_set_image(GTK_BUTTON(button_t_right), img_t_right);
	g_signal_connect (G_OBJECT (button_t_right), "clicked",
						G_CALLBACK (main_right_t_func), (gpointer) window);
	button_t_zoom_in = gtk_button_new();
	img_t_zoom_in = gtk_image_new_from_file("./zoom-x.png");
	gtk_button_set_image(GTK_BUTTON(button_t_zoom_in), img_t_zoom_in);
	g_signal_connect (G_OBJECT (button_t_zoom_in), "clicked",
                        G_CALLBACK (main_zoom_in_t_func), (gpointer) window);
    button_t_zoom_in_y = gtk_button_new();
    img_t_zoom_in_y = gtk_image_new_from_file("./zoom-y.png");
    gtk_button_set_image(GTK_BUTTON(button_t_zoom_in_y), img_t_zoom_in_y);
    g_signal_connect (G_OBJECT (button_t_zoom_in_y), "clicked",
                        G_CALLBACK (main_zoom_in_t_y_func), (gpointer) window);
    button_t_undo = gtk_button_new();
    img_t_undo = gtk_image_new_from_file("./edit-undo.png");
    gtk_button_set_image(GTK_BUTTON(button_t_undo), img_t_undo);
    g_signal_connect (G_OBJECT (button_t_undo), "clicked",
                        G_CALLBACK (main_undo_t_func), (gpointer) window);
	
	label_separator1 = gtk_label_new("");
	gtk_widget_set_size_request(label_separator1, 5, 12);
	label_separator2 = gtk_label_new("");
	gtk_widget_set_size_request(label_separator2, 5, 12);
	
	gtk_grid_attach(GTK_GRID(grid_control), label_separator1, 8, 0, 1, 3);
	gtk_grid_attach(GTK_GRID(grid_control), ent_separator, 9, 0, 1, 4);
	gtk_grid_attach(GTK_GRID(grid_control), label_separator2, 10, 0, 1, 3);
	
	gtk_grid_attach(GTK_GRID(grid_control), t_label, 10, 0, 5, 1);
	gtk_grid_attach(GTK_GRID(grid_control), button_t_zoom_in_y, 12, 1, 2, 1);
	gtk_grid_attach(GTK_GRID(grid_control), button_t_left, 11, 2, 1, 1);
	gtk_grid_attach(GTK_GRID(grid_control), button_t_log, 11, 1, 1, 1);
	gtk_grid_attach(GTK_GRID(grid_control), button_t_right, 14, 2, 1, 1);
	gtk_grid_attach(GTK_GRID(grid_control), button_t_zoom_in, 12, 2, 2, 1);
	gtk_grid_attach(GTK_GRID(grid_control), button_t_mini, 12, 3, 2, 1);
	gtk_grid_attach(GTK_GRID(grid_control), button_t_undo, 11, 3, 1, 1);
	
	button_start = gtk_button_new();
	img_start = gtk_image_new_from_file("./start.png");
	gtk_button_set_image(GTK_BUTTON(button_start), img_start);
	g_signal_connect (G_OBJECT (button_start), "clicked",
                        G_CALLBACK (start_expos), NULL);
	button_stop = gtk_button_new();
	img_stop = gtk_image_new_from_file("./stop.png");
	gtk_button_set_image(GTK_BUTTON(button_stop), img_stop);
	g_signal_connect (G_OBJECT (button_stop), "clicked",
                        G_CALLBACK (stop_expos), (gpointer) window);
	button_pause = gtk_button_new();
	img_pause = gtk_image_new_from_file("./pause.png");
	gtk_button_set_image(GTK_BUTTON(button_pause), img_pause);
	button_clear = gtk_button_new();
	img_clear = gtk_image_new_from_file("./clear.png");
	gtk_button_set_image(GTK_BUTTON(button_clear), img_clear);
	g_signal_connect (G_OBJECT (button_clear), "clicked",
                        G_CALLBACK (clear_expos), NULL);
	button_view = gtk_button_new_with_label("View");
	g_signal_connect (G_OBJECT (button_view), "clicked",
                        G_CALLBACK (view_expos), (gpointer) window);
	
	spinner_get_data = gtk_spinner_new();
	gtk_spinner_stop(GTK_SPINNER(spinner_get_data));
	
	label_separator3 = gtk_label_new("");
	gtk_widget_set_size_request(label_separator3, 0, 10);
	
	gtk_grid_attach(GTK_GRID(grid_control), label_separator3, 4, 4, 1, 1);
	gtk_grid_attach(GTK_GRID(grid_control), button_start, 4, 5, 1, 1);
	gtk_grid_attach(GTK_GRID(grid_control), button_stop, 5, 5, 1, 1);
	gtk_grid_attach(GTK_GRID(grid_control), button_pause, 6, 5, 1, 1);
	gtk_grid_attach(GTK_GRID(grid_control), button_clear, 4, 6, 1, 1);
	gtk_grid_attach(GTK_GRID(grid_control), button_view, 5, 6, 2, 1);
	gtk_grid_attach(GTK_GRID(grid_control), spinner_get_data, 7, 6, 1, 1);
	
	read_conf_file(); //true Read d.conf file
	
	timer_label = gtk_label_new(NULL);
	
	viewinfo_buffer = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(viewinfo_buffer), FALSE);
	info_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(viewinfo_buffer));
	gtk_misc_set_padding(GTK_MISC(energyspk_buffer_execute), 0, 20);
	gtk_widget_set_size_request(viewinfo_buffer, 300, 200);	
	gtk_text_buffer_create_tag(info_buffer, "bold", 
		"weight", PANGO_WEIGHT_BOLD, NULL);
	gtk_text_buffer_create_tag (info_buffer, "center",
        "justification", GTK_JUSTIFY_CENTER, NULL);
    gtk_text_buffer_create_tag(info_buffer, "red bg", 
		"background", "#770011", NULL);
	gtk_text_buffer_get_iter_at_line(info_buffer, &iter, 1);
	
	label_info = gtk_label_new(NULL);
	if (getcwd(infostruct.dir, sizeof(infostruct.dir)) != NULL) {
		//sprintf(infostring, " <b>Dir:</b> %s \n <b>Start:</b> %s \n <b>Exps:</b> %d \n <b>Time:</b> %s \n <b>DTime:</b> %d \n <b>int1</b> - %.0fcnt/s. \n <b>int2</b> - %.0fcnt/s. \n <b>int3</b> - %.0fcnt/s. \n <b>int4</b> - %.0fcnt/s. \n", \
		//					infostruct.dir, infostruct.start, infostruct.expos, convert_time_to_hms(infostruct.time), infostruct.dtime, infostruct.intensity[0], infostruct.intensity[1], infostruct.intensity[2], infostruct.intensity[3]);
		//gtk_label_set_markup(GTK_LABEL(label_info), infostring);
		gtk_text_buffer_insert_with_tags_by_name(info_buffer, &iter, 
			g_strdup_printf("Dir: %s\n", infostruct.dir), -1, "bold", NULL);
		gtk_text_buffer_insert_with_tags_by_name(info_buffer, &iter, 
			g_strdup_printf("Start: %s\n", infostruct.start), -1, "bold", NULL);
		gtk_text_buffer_insert_with_tags_by_name(info_buffer, &iter, 
			g_strdup_printf("Exposition: %d\n", infostruct.expos), -1, "bold", NULL);
		gtk_text_buffer_insert_with_tags_by_name(info_buffer, &iter, 
			g_strdup_printf("Accumulation: %s\n", convert_time_to_hms(infostruct.time)), -1, "bold", NULL);
		gtk_text_buffer_insert_with_tags_by_name(info_buffer, &iter, 
			g_strdup_printf("DTime: %d\n", infostruct.dtime), -1, "bold", NULL);
		gtk_text_buffer_insert_with_tags_by_name(info_buffer, &iter, 
			g_strdup_printf("int #1: %.2e\n", infostruct.intensity[0]), -1, "bold", NULL);
		gtk_text_buffer_insert_with_tags_by_name(info_buffer, &iter, 
			g_strdup_printf("int #2: %.2e\n", infostruct.intensity[1]), -1, "bold", NULL);
		gtk_text_buffer_insert_with_tags_by_name(info_buffer, &iter, 
			g_strdup_printf("int #3: %.2e\n", infostruct.intensity[2]), -1, "bold", NULL);
		gtk_text_buffer_insert_with_tags_by_name(info_buffer, &iter, 
			g_strdup_printf("int #4: %.2e", infostruct.intensity[3]), -1, "bold", NULL);
	}
	else
		perror("getcwd() error");
	
	//g_timeout_add(6e4, (GSourceFunc) clock_func, NULL);	
	//clock_func(NULL);
	
	int err = pthread_create(&(tid), NULL, &doThreadClock, NULL);
    if (err != 0)
		printf("\ncan't create clock thread :[%s]", strerror(err));
	
	vbox_for_info = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start(GTK_BOX(vbox_for_info), timer_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox_for_info), viewinfo_buffer, FALSE, FALSE, 0);
	
	//gtk_grid_attach(GTK_GRID(main_table), grid_control, 4, 0, 2, 1);
	gtk_box_pack_end(GTK_BOX(hbox_en), vbox_for_info, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox_en), grid_control, FALSE, FALSE, 3);

	
	hseparator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	
	gtk_box_pack_start (GTK_BOX (box), hbox_en, TRUE, TRUE, 0);
//	gtk_box_pack_start (GTK_BOX (box), hseparator, FALSE, FALSE, 5);
	gtk_box_pack_start (GTK_BOX (box), hbox_t[0], TRUE, TRUE, 5);
	gtk_box_pack_start (GTK_BOX (box), hbox_t[1], TRUE, TRUE, 0);

	gtk_box_set_homogeneous(GTK_BOX(box), TRUE);

    statusbar = gtk_statusbar_new();
    sb_context_id = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusbar), "Message to user");
	statusbar_push("Hello! It's program for working with TDPAC JINR");

	gtk_box_pack_end (GTK_BOX (main_box), statusbar, FALSE, FALSE, 0);
	
	gtk_box_pack_end (GTK_BOX (main_box), hseparator, FALSE, FALSE, 0);

    gtk_widget_show_all (window);

    /* Rest in gtk_main and wait for the fun to begin! */
    gtk_main ();
	
	return 0;
}
