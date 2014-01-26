#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include <cairo.h>
#include <gtk/gtk.h>
#include <math.h>

#define SIMPLE_DEVICE "/dev/simple"

#define MAX_CH 4096
#define NUM_TIME_SPK	12
#define NUM_ENERGY_SPK	4

#define Y_SCALE	0.5

#define EPS 0.00001
#define X2PIX	4096/480
#define onetwothree(x)	((x)<150.0) ? 0.0 : ( ((x)>150.0 && (x)<350.0) ? 1.0 : 2.0)

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
} gflag;

static char *folder2spk = "jinrspk2/";
static char *folder2initialspk = "./";
static char *folder2readspk ="./";

int LD[8] = {401, 660, 235, 440, 267, 439, 292, 563};
int RD[8] = {744, 1001, 514, 792, 518, 708, 678, 911};

GtkWidget *dcalc_label;
GtkWidget *statusbar;
guint sb_context_id;
GtkWidget *table;

float u[7];
double globscalex;

static void do_drawing(cairo_t *, gpointer);
static void analyze_energy_drawing(cairo_t *);

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
	return 0;
/* IF flag == 1 END */	
	}	

return -12;	
}

static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr, 
    gpointer user_data)
{
	printf("on draw event start\n");
	printf("ud = %d\n", *((int *)user_data));
	
	do_drawing(cr, user_data);

	return FALSE;
}

static gboolean analyze_energy_draw_event(GtkWidget *widget, cairo_t *cr, 
    gpointer user_data)
{
	printf("analyze energy on draw event start 111\n");
	
	analyze_energy_drawing(cr);

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
		for(j=0; j<=MAX_CH-1; j++)
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

void left_coord(cairo_t *cr, int yrow)
{
	printf("left coord glob.x = %f\n", glob.x[0]);
	
	int xc, i;
	
	xc = (int)glob.x[0];
	
	for(i=0; i<=5; i++) {
		if(xc>=220.0*i && xc<=220.0*i+160.0) xc = glob.x[0]-220*i;
		else if(xc<=220*(i+1) && xc>=220.0*i+160.0) xc = 0;
	}
		
	cairo_set_source_rgb(cr, 0.1, 0.1, 0.1); 
	cairo_select_font_face(cr, "FreeMono",
			CAIRO_FONT_SLANT_NORMAL,
			CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 6);
	cairo_move_to(cr, glob.x[0]+2.0, (yrow-1)*200.0+100.0);
	cairo_show_text(cr, concat_str("", (4096*xc/160)));
}

void right_coord(cairo_t *cr, int yrow)
{
	printf("left coord glob.x = %f\n", glob.x[1]);
	
	int xc, i;
	
	xc = (int)glob.x[1];
	printf("xc vefore = %d\n", xc);
	
	for(i=0; i<=5; i++) {
		if(xc>=220*i && xc<=220*i+160) {xc = (int)glob.x[1]-220*i; break;}
		else if(xc>=160*(i+1) && xc<=220*(i+1)) {xc = 0;}
	}	
		
	cairo_set_source_rgb(cr, 0.1, 0.1, 0.1); 
	cairo_select_font_face(cr, "FreeMono",
			CAIRO_FONT_SLANT_NORMAL,
			CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 6);
	cairo_move_to(cr, glob.x[1]-20.0, (yrow-1)*200.0+80.0);
	cairo_show_text(cr, concat_str("", (4096*xc/160)));
}

void cairo_pt(cairo_t *cr, double x0, double y0, double trx, double try, double sclx, double scly)
{
	printf("x0 = %.2f ", x0);
	
	cairo_set_line_width (cr, 1.0*scly);
	
	cairo_arc (cr, x0, y0, 1., 0., 2 * M_PI);
	
	cairo_fill(cr);
}

void add_bottics(cairo_t *cr, double x, int x_label, double scale, double scaley)
{
	cairo_set_line_width (cr, 2.0/scale);
	printf("x = %f label = %d\n", x, x_label);
	cairo_move_to(cr, x, 470.0+5.0/scaley);
	cairo_line_to(cr, x, 470.0-5.0/scaley);
	
	if(gflag.y==2) cairo_move_to(cr, x, 450.0);
	else cairo_move_to(cr, x, 465.0);
	
	cairo_save(cr);
	
	cairo_scale(cr, 1.0/scale, 1.0);

	if(gflag.y==2) {
		cairo_set_font_size(cr, 11);
		
		cairo_save(cr);
		cairo_scale(cr, 1.0, 1.0/scaley);	

		cairo_set_source_rgb(cr, 0.1, 0.1, 0.1); 
		cairo_select_font_face(cr, "Garuda",
			CAIRO_FONT_SLANT_NORMAL,
			CAIRO_FONT_WEIGHT_BOLD);

		cairo_show_text(cr, concat_str("", x_label));
		cairo_restore(cr);
	}
	else {
		cairo_set_font_size(cr, 11);	

		cairo_set_source_rgb(cr, 0.1, 0.1, 0.1); 
		cairo_select_font_face(cr, "Garuda",
			CAIRO_FONT_SLANT_NORMAL,
			CAIRO_FONT_WEIGHT_BOLD);

		cairo_show_text(cr, concat_str("", x_label));
	}
	cairo_stroke(cr);
	cairo_restore(cr);
}

void add_bottics_y(cairo_t *cr, double x, int x_label, double scalex, double scaley)
{
	printf("!!!!!!!!!! flag.x = %d flag.y = %d \n", gflag.x, gflag.y);
	cairo_set_line_width (cr, 1.0/scalex);
	cairo_save(cr);
	
	cairo_scale(cr, 1.0, 1.0/scaley);
	cairo_translate(cr, 0.0, -470.0*(1.0-scaley));
	
	if(gflag.x==2) {
		cairo_set_font_size(cr, 11);
	//	gflag.x = 0;
	
		cairo_save(cr);
		cairo_move_to(cr, x, 466.0);
		cairo_scale(cr, 1.0/scalex, 1.0);
		cairo_translate(cr, 0.0, 0.0);
	
		cairo_set_source_rgb(cr, 0.1, 0.1, 0.1); 
		cairo_select_font_face(cr, "Garuda",
			CAIRO_FONT_SLANT_NORMAL,
			CAIRO_FONT_WEIGHT_BOLD);

		cairo_show_text(cr, concat_str("", x_label));
		cairo_restore(cr);
	}
	else {
		cairo_set_font_size(cr, 11);
		
		cairo_move_to(cr, x, 466.0);
	
		cairo_set_source_rgb(cr, 0.1, 0.1, 0.1); 
		cairo_select_font_face(cr, "Garuda",
			CAIRO_FONT_SLANT_NORMAL,
			CAIRO_FONT_WEIGHT_BOLD);

		cairo_show_text(cr, concat_str("", x_label));
	}
	
	printf("x = %f label = %d scalex = %f scaley = %f\n", x, x_label, scalex, scaley);
	cairo_move_to(cr, x, 475.0);
	cairo_line_to(cr, x, 465.0);

	cairo_stroke(cr);
	cairo_restore(cr);
}

void add_rghtics_y(cairo_t *cr, double y, double x, double y_label, double scalex, double scaley, double transx)
{
	cairo_set_line_width (cr, 1.0);
	cairo_save(cr);
	
	cairo_scale(cr, 1.0, 1.0/scaley);
	cairo_translate(cr, 0.0, -470.0*(1.0-scaley));
	
//	printf("rtics y = %.2f gflagx = %d x = %.2f label = %.2f scalex = %.2f scaley = %.2f transx = %.2f\n", y, gflag.x, x, y_label, scalex, scaley, transx);

	if(gflag.x==2) {
		cairo_move_to(cr, x, 470.0-y);
		cairo_line_to(cr, x-10.0/scalex, 470.0-y);

		cairo_save(cr);
		cairo_move_to(cr, x-40.0/scalex, 465.0-y);
		cairo_scale(cr, 1.0/scalex, 1.0);
		cairo_translate(cr, 0.0, 0.0);
	
		cairo_set_font_size(cr, 11/scalex);
	
		cairo_set_source_rgb(cr, 0.1, 0.1, 0.1); 
		cairo_select_font_face(cr, "Garuda",
			CAIRO_FONT_SLANT_NORMAL,
			CAIRO_FONT_WEIGHT_BOLD);
		cairo_set_font_size(cr, 11);

		if(y_label<5000)
			cairo_show_text(cr, concat_str("", (int)y_label));
		else 
			cairo_show_text(cr, concat_str_inv("K", y_label/1000.0));
			
		cairo_restore(cr);
	}
	else {
		cairo_move_to(cr, x, 470.0-y);
		cairo_line_to(cr, x-10.0, 470.0-y);
		
		cairo_set_font_size(cr, 11);
		
		cairo_move_to(cr, x-40.0, 465.0-y);
	
		cairo_set_source_rgb(cr, 0.1, 0.1, 0.1); 
		cairo_select_font_face(cr, "Garuda",
			CAIRO_FONT_SLANT_NORMAL,
			CAIRO_FONT_WEIGHT_BOLD);

		if(y_label<5000)
			cairo_show_text(cr, concat_str("", (int)y_label));
		else 
			cairo_show_text(cr, concat_str_inv("K", y_label/1000.0));
	}

	cairo_stroke(cr);
	cairo_restore(cr); 
}

static void do_drawing(cairo_t *cr, gpointer user_data)
{
	printf("do draw global flag = %d\n", glob.flag);
	int i, j;
	double scalex=1.0; 
	double scaley=1.0;
	double transx, transy;
	
	if(glob.flag == 2) {
		scalex = 900.0/(glob.x[1]-glob.x[0]);
		scaley = 550.0/150.0;
		transx = -1.0*900.0*glob.x[0]/(glob.x[1]-glob.x[0]);
	//	transy = -1.0*150.0;
		transy = onetwothree(glob.y[0])*(-1.0)*200.0*scaley;
		printf("transy = %f, globy = %f, one more time = %f\n", transy, glob.y[0], onetwothree(glob.y[0]));
		
		cairo_translate(cr, transx, transy);
		cairo_scale(cr, scalex, scaley);
		
		cairo_set_source_rgba (cr, 0.0, 250.1, 0.0, 1.0);
		cairo_set_line_width (cr, 0.5);
		if(glob.y[0]<=150) {
			cairo_move_to(cr, glob.x[0], 150.0);
			cairo_line_to(cr, glob.x[0], 0.0);
			cairo_move_to(cr, glob.x[1], 150.0);
			cairo_line_to(cr, glob.x[1], 0.0);
			
			left_coord(cr, 1);
			right_coord(cr, 1);
		}
		else if(glob.y[0]<=350) {
			cairo_move_to(cr, glob.x[0], 350.0);
			cairo_line_to(cr, glob.x[0], 200.0);
			cairo_move_to(cr, glob.x[1], 350.0);
			cairo_line_to(cr, glob.x[1], 200.0);
			
			left_coord(cr, 2);
			right_coord(cr, 2);
		}
		else if(glob.y[0]<=550) {
			cairo_move_to(cr, glob.x[0], 550.0);
			cairo_line_to(cr, glob.x[0], 400.0);
			cairo_move_to(cr, glob.x[1], 550.0);
			cairo_line_to(cr, glob.x[1], 400.0);
			
			left_coord(cr, 3);
			right_coord(cr, 3);
		}
		
		cairo_stroke(cr);
		sleep(0.2);
		
		glob.flag = 1;
	}
	
	cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.5);
	cairo_set_line_width (cr, 0.8);
	for(i=0; i<4; i++) {
		cairo_move_to(cr, 220.0*i, 150.0);
		cairo_line_to(cr, 160.0+220.0*i, 150.0);
	/*	cairo_move_to(cr, 1.0+250.0*i, 150.0);
		cairo_line_to(cr, 1.0+250.0*i, 1.0);*/
		cairo_stroke(cr);
		cairo_set_source_rgb(cr, 0.1, 0.1, 0.1); 
		cairo_select_font_face(cr, "Purisa",
			CAIRO_FONT_SLANT_NORMAL,
			CAIRO_FONT_WEIGHT_BOLD);
		cairo_set_font_size(cr, 16);
		cairo_move_to(cr, 100.0+220*i, 30.0);
		cairo_show_text(cr, concat_str("E", i+1));
		cairo_stroke(cr);
	}
	
	cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.5);
	cairo_set_line_width (cr, 0.8);
	for(i=0; i<6; i++) {
		cairo_move_to(cr, 220.0*i, 350.0);
		cairo_line_to(cr, 160.0+220.0*i, 350.0);
		cairo_stroke(cr);
		cairo_set_source_rgb(cr, 0.1, 0.1, 0.1); 
		cairo_select_font_face(cr, "Purisa",
			CAIRO_FONT_SLANT_NORMAL,
			CAIRO_FONT_WEIGHT_BOLD);
		cairo_set_font_size(cr, 14);
		cairo_move_to(cr, 100.0+220*i, 230.0);
		cairo_show_text(cr, concat_str("T", i+1));
		cairo_stroke(cr);
	}
	cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.5);
	cairo_set_line_width (cr, 0.8);
	for(i=0; i<6; i++) {
		cairo_move_to(cr, 220.0*i, 550.0);
		cairo_line_to(cr, 160.0+220.0*i, 550.0);
		cairo_stroke(cr);
		cairo_set_source_rgb(cr, 0.1, 0.1, 0.1); 
		cairo_select_font_face(cr, "Purisa",
			CAIRO_FONT_SLANT_NORMAL,
			CAIRO_FONT_WEIGHT_BOLD);
		cairo_set_font_size(cr, 14);
		cairo_move_to(cr, 100.0+220*i, 430.0);
		cairo_show_text(cr, concat_str("T", i+7));
		cairo_stroke(cr);
	}

	if(glob.flag == 1) {
		int max_t = max_time();
		double co_t = (double)(max_t/150.0)+1.0;
		int max_e = max_energy();
		double co_e = (double)(max_e/150.0)+1.0;
	//	printf("max_t = %d; co_t = %.2f\n", max_t, co_t);
		
		cairo_set_source_rgba (cr, 0.0, 0.0, 255.0, 1.0);
		cairo_set_line_width (cr, 1.0);
		for(i=0; i<4; i++) {
			for(j=0; j<=MAX_CH-1; j+=5)
				//cairo_arc(cr, (double)(0.039*j+i*220), 150.0-(double)spectras->energyspk[i][j], 0.05, 0, 2*M_PI);
			//	cairo_rectangle(cr, (double)(0.039*j+i*220), 150.0-(double)spectras->energyspk[i][j], 0.1/scaley, 0.05/scaley);
			//	cairo_pt(cr, (double)(0.039*j+i*220), 150.0-(double)spectras->energyspk[i][j]/co_e);
				cairo_arc(cr, (double)(0.039*j+i*220), (150.0-(double)spectras->energyspk[i][j]/co_e), 0.05, 0, 2*M_PI);
			cairo_stroke(cr);
		}

		cairo_set_source_rgba (cr, 255.0, 0.0, 0.0, 1.0);
		cairo_set_line_width (cr, 1.0);
		for(i=0; i<6; i++) {
			for(j=0; j<=MAX_CH-1; j+=5)
			/*	if(spectras->timespk[i][j]>150.0) cairo_arc(cr, (double)(0.039*j+i*220), 350.0, 0.05, 0, 2*M_PI);
				else cairo_arc(cr, (double)(0.039*j+i*220), (350.0-10.0*(double)spectras->timespk[i][j]), 0.05, 0, 2*M_PI); */
			/*	if(spectras->timespk[i][j]>150.0) cairo_pt(cr, (double)(0.039*j+i*220), 200.0, scalex);
				else cairo_pt(cr, (double)(0.039*j+i*220), (350.0-(double)spectras->timespk[i][j]), scalex); */
			//	cairo_pt(cr, (double)(0.039*j+i*220.0), (350.0-(double)spectras->timespk[i][j]/co_t));
				cairo_arc(cr, (double)(0.039*j+i*220), (350.0-(double)spectras->timespk[i][j]/co_t), 0.05, 0, 2*M_PI);
			cairo_stroke(cr);
		} 
		
		cairo_set_source_rgba (cr, 255.0, 0.0, 0.0, 1.0);
		cairo_set_line_width (cr, 1.0);
		for(i=0; i<6; i++) {
			for(j=0; j<=MAX_CH-1; j+=5) {
			//	if((double)spectras->timespk[i+6][j]>0.0 && (i+6 == 7)) printf("in spk %d %d = %f\n", i+6, j, (550.0-1.0*150.0*(double)spectras->timespk[i+6][j]));
			/*	if(550.0-150.0/co_t*(double)spectras->timespk[i+6][j]>150.0) cairo_arc(cr, (double)(0.039*j+i*220), 550.0, 1.5, 0, 2*M_PI);
				else cairo_arc(cr, (double)(0.039*j+i*220), 550.0-150.0/co_t*(double)spectras->timespk[i+6][j], 1.5, 0, 2*M_PI); */
			/*	if(spectras->timespk[i+6][j]>150.0) cairo_arc(cr, (double)(0.039*j+i*220), 400.0, 0.05, 0, 2*M_PI);
				else cairo_arc(cr, (double)(0.039*j+i*220), (550.0-(double)spectras->timespk[i+6][j]), 0.05, 0, 2*M_PI); */
				cairo_arc(cr, (double)(0.039*j+i*220), (550.0-(double)spectras->timespk[i+6][j]/co_t), 0.05, 0, 2*M_PI);
			}
			cairo_stroke(cr);
		} 
		
	}	
	cairo_stroke(cr);

}

static void analyze_energy_drawing(cairo_t *cr)
{
	int i, j;
	static double scalex; 
	static double scaley;
	static double transx, transy;
	static char num_it=0;
	static double x0, x1;
//	double x_max, x_min;
	
	printf("analyze energy gflag.x = %d gflag.y = %d x[0] = %.2f x[1] = %.2f\t x3[0] = %.2f x3[1] = %.2f\n", gflag.x, gflag.y, glob.x[0], glob.x[1], glob.x3[0], glob.x3[1]);
	
	cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 1.0);
	cairo_set_line_width (cr, 1.0);

	cairo_rectangle(cr, 0.0, 0.0, 480.0, 470.0);
	cairo_stroke(cr);
	
//	printf("aed before flag = %d, flage = %d\n", glob.flag, glob.flage);
	
	if(gflag.x == 1) {
	
		if(num_it==0) {
		//	printf("1 scalex = %f transx = %f\n", scalex, transx);
			/*printf("x1' = %.2f, x2' = %.2f\t", scalex*glob.x[0]+transx, scalex*glob.x[1]+transx);
			printf("x1' = %.2f, x2' = %.2f\n", glob.x[0], glob.x[1]); */
		
			scalex = 480.0/(glob.x[1]-glob.x[0]);
	//		scaley = 1.0;1
			transx = -1.0*480.0*(glob.x[0])/(glob.x[1]-glob.x[0]);
	//		transy = 470.0*(1.0-scaley);
	//		printf("scalex = %f transx = %f\n", scalex, transx);
		
			x0 = glob.x[0]; x1 = glob.x[1];
		
			if( (x0 < EPS) && (x1 < EPS) ) {x0 = 0.0; x1 = 4096.0;} 
			
			num_it++;
			
			globscalex = scalex;
		}
		else {
		//	printf("2 scalex = %f transx = %f\n", scalex, transx);
		//	printf("size - %.2f\n", x1-x0);
			x0 = (glob.x[0]-transx)/scalex; x1 = (glob.x[1]-transx)/scalex;
		
			scalex = 480.0/(x1-x0);
		//	scaley = 1.0;1
			transx = -1.0*480.0*(x0)/(x1-x0);
		//	transy = 470.0*(1.0-scaley);
		//	printf("scalex = %f transx = %f\n", scalex, transx);
		
			globscalex = scalex;
		}
	}
	
	if(gflag.y == 1) {
		//	scalex = 1.0;
			scaley *= Y_SCALE;
		//	transx = 0.0;
			transy = 470.0*(1.0-scaley);
			
		//	cairo_translate(cr, transx, transy);
		//	cairo_scale(cr, scalex, scaley);
		
			gflag.draw = 1;
			
			printf("up\n");
	}
	
	if(gflag.x == 1) {
		cairo_move_to(cr, 0.0, 480.0);
		cairo_set_source_rgb(cr, 0.1, 0.1, 0.1); 
		cairo_select_font_face(cr, "Garuda",
			CAIRO_FONT_SLANT_NORMAL,
			CAIRO_FONT_WEIGHT_BOLD);
		cairo_set_font_size(cr, 11);
		cairo_show_text(cr, concat_str("", x0*X2PIX));
		cairo_move_to(cr, 0.0, 470.0);
		cairo_line_to(cr, 0.0, 475.0);
		cairo_stroke(cr);
		
		cairo_move_to(cr, 227.5, 480.0);
		cairo_set_source_rgb(cr, 0.1, 0.1, 0.1); 
		cairo_select_font_face(cr, "Garuda",
			CAIRO_FONT_SLANT_NORMAL,
			CAIRO_FONT_WEIGHT_BOLD);
		cairo_set_font_size(cr, 11);
		cairo_show_text(cr, concat_str("", x0*X2PIX + X2PIX*(x1-x0)/2));
		cairo_move_to(cr, 240.0, 470.0);
		cairo_line_to(cr, 240.0, 475.0);
		cairo_move_to(cr, 240.0, 0.0);
		cairo_line_to(cr, 240.0, 5.0);
		cairo_stroke(cr);
		
		cairo_move_to(cr, 445.0, 480.0);
		cairo_set_source_rgb(cr, 0.1, 0.1, 0.1); 
		cairo_select_font_face(cr, "Garuda",
			CAIRO_FONT_SLANT_NORMAL,
			CAIRO_FONT_WEIGHT_BOLD);
		cairo_set_font_size(cr, 11);
		cairo_show_text(cr, concat_str("", x1*X2PIX));
		cairo_move_to(cr, 480.0, 470.0);
		cairo_line_to(cr, 480.0, 475.0);
		cairo_stroke(cr);
		
	//	printf("size cell = %f\n", (x1-x0)*4096/480);
		
		cairo_translate(cr, transx, transy);
		cairo_scale(cr, scalex, scaley); 
		
		double tics[4];
		
		if((x1-x0)*4096/480 < 90) {
			for(i=0; i<=3; i++) {
			//	printf("x0 = %f, tics_%d = %f\n", x0*4096/480, i, tics[i]);
				tics[i] = 25*((int)(x0*4096.0/480.0)/25+i+1)*480/4096;
				add_bottics(cr, ceil(tics[i]), 25*((int)(x0*4096.0/480.0)/25+i+1), scalex, scaley);
			}
		}
		else if((x1-x0)*4096/480 < 200) {
		for(i=0; i<=3; i++) {
			//	printf("x0 = %f, tics_%d = %f\n", x0*4096/480, i, tics[i]);
				tics[i] = 50*((int)(x0*4096.0/480.0)/50+i+1)*480/4096;
				add_bottics(cr, ceil(tics[i]), 50*((int)(x0*4096.0/480.0)/50+i+1), scalex, scaley);
			}
		}
		else if((x1-x0)*4096/480 < 500) {
		for(i=0; i<=3; i++) {
			//	printf("x0 = %f, tics_%d = %f\n", x0*4096/480, i, tics[i]);
				tics[i] = 100*((int)(x0*4096.0/480.0)/100+i+1)*480/4096;
				add_bottics(cr, ceil(tics[i]), 100*((int)(x0*4096.0/480.0)/100+i+1), scalex, scaley);
			}
		}
		else if((x1-x0)*4096/480 < 1000) {
		for(i=0; i<=3; i++) {
			//	printf("x0 = %f, tics_%d = %f\n", x0*4096/480, i, tics[i]);
				tics[i] = 250*((int)(x0*4096.0/480.0)/250+i+1)*480/4096;
				add_bottics(cr, ceil(tics[i]), 250*((int)(x0*4096.0/480.0)/250+i+1), scalex, scaley);
			}
		}
		else if((x1-x0)*4096/480 < 3000) {
		for(i=0; i<=3; i++) {
			//	printf("x0 = %f, tics_%d = %f\n", x0*4096/480, i, tics[i]);
				tics[i] = 500*((int)(x0*4096.0/480.0)/500+i+1)*480/4096;
				add_bottics(cr, ceil(tics[i]), 500*((int)(x0*4096.0/480.0)/500+i+1), scalex, scaley);
			}
		}
		else if((x1-x0)*4096/480 < 4000) {
		for(i=0; i<=3; i++) {
			//	printf("x0 = %f, tics_%d = %f\n", x0*4096/480, i, tics[i]);
				tics[i] = 1000*((int)(x0*4096.0/480.0)/1000+i+1)*480/4096;
				add_bottics(cr, ceil(tics[i]), 1000*((int)(x0*4096.0/480.0)/1000+i+1), scalex, scaley);
			}
		}
		
		gflag.x = 2;
		gflag.draw = 1;
		
		add_rghtics_y(cr, 100.0, x1, 100.0/(scaley), scalex, scaley, transx);
		add_rghtics_y(cr, 200.0, x1, 200.0/(scaley), scalex, scaley, transx);
		add_rghtics_y(cr, 300.0, x1, 300.0/(scaley), scalex, scaley, transx);
		add_rghtics_y(cr, 400.0, x1, 400.0/(scaley), scalex, scaley, transx);
	}
	else if(gflag.y == 1) {
		printf("TYYYYYYYYYYYYYY\n");
		cairo_translate(cr, transx, transy);
		cairo_scale(cr, scalex, scaley); 
		
		double tics[4];
		
		if((x1-x0)*4096/480 < 90) {
			for(i=0; i<=3; i++) {
				tics[i] = 25*((int)(x0*4096.0/480.0)/25+i+1)*480/4096;
				add_bottics_y(cr, ceil(tics[i]), 25*((int)(x0*4096.0/480.0)/25+i+1), scalex, scaley);
			}
		}
		else if((x1-x0)*4096/480 < 200) {
		for(i=0; i<=3; i++) {
				tics[i] = 50*((int)(x0*4096.0/480.0)/50+i+1)*480/4096;
				add_bottics_y(cr, ceil(tics[i]), 50*((int)(x0*4096.0/480.0)/50+i+1), scalex, scaley);
			}
		}
		else if((x1-x0)*4096/480 < 500) {
		for(i=0; i<=3; i++) {
				tics[i] = 100*((int)(x0*4096.0/480.0)/100+i+1)*480/4096;
				add_bottics_y(cr, ceil(tics[i]), 100*((int)(x0*4096.0/480.0)/100+i+1), scalex, scaley);
			}
		}
		else if((x1-x0)*4096/480 < 1000) {
		for(i=0; i<=3; i++) {
				tics[i] = 250*((int)(x0*4096.0/480.0)/250+i+1)*480/4096;
				add_bottics_y(cr, ceil(tics[i]), 250*((int)(x0*4096.0/480.0)/250+i+1), scalex, scaley);
			}
		}
		else if((x1-x0)*4096/480 < 3000) {
		for(i=0; i<=3; i++) {
				tics[i] = 500*((int)(x0*4096.0/480.0)/500+i+1)*480/4096;
				add_bottics_y(cr, ceil(tics[i]), 500*((int)(x0*4096.0/480.0)/500+i+1), scalex, scaley);
			}
		}
		else if((x1-x0)*4096/480 < 4000) {
		for(i=0; i<=3; i++) {
				tics[i] = 1000*((int)(x0*4096.0/480.0)/1000+i+1)*480/4096;
				add_bottics_y(cr, ceil(tics[i]), 1000*((int)(x0*4096.0/480.0)/1000+i+1), scalex, scaley);
			}
		}
		else {
			add_bottics_y(cr, 0.0, 0, scalex, scaley);
			add_bottics_y(cr, 1000.0*480/4096, 1000, scalex, scaley);
			add_bottics_y(cr, 2000.0*480/4096, 2000, scalex, scaley);
			add_bottics_y(cr, 3000.0*480/4096, 3000, scalex, scaley);
			add_bottics_y(cr, 4000.0*480/4096, 4000, scalex, scaley);
		}
		
		if(scalex == 1) {
			add_rghtics_y(cr, 100.0, 480, 100.0/(scaley), scalex, scaley, transx);
			add_rghtics_y(cr, 200.0, 480, 200.0/(scaley), scalex, scaley, transx);
			add_rghtics_y(cr, 300.0, 480, 300.0/(scaley), scalex, scaley, transx);
			add_rghtics_y(cr, 400.0, 480, 400.0/(scaley), scalex, scaley, transx);
		}	
		else {
			add_rghtics_y(cr, 100.0, x1, 100.0/(scaley), scalex, scaley, transx);
			add_rghtics_y(cr, 200.0, x1, 200.0/(scaley), scalex, scaley, transx);
			add_rghtics_y(cr, 300.0, x1, 300.0/(scaley), scalex, scaley, transx);
			add_rghtics_y(cr, 400.0, x1, 400.0/(scaley), scalex, scaley, transx);
		}
		
		gflag.y = 2;
		gflag.draw = 1;
	}
	else if(gflag.x != 5) {
		
		add_bottics(cr, 0.0, 0, 1.0, 1.0);
		add_bottics(cr, 1000.0*480/4096, 1000, 1.0, 1.0);
		add_bottics(cr, 2000.0*480/4096, 2000, 1.0, 1.0);
		add_bottics(cr, 3000.0*480/4096, 3000, 1.0, 1.0);
		add_bottics(cr, 4000.0*480/4096, 4000, 1.0, 1.0);	
		
		add_rghtics_y(cr, 100.0, 480.0, 100.0, 1.0, 1.0, transx);
		add_rghtics_y(cr, 200.0, 480.0, 200.0, 1.0, 1.0, transx);
		add_rghtics_y(cr, 300.0, 480.0, 300.0, 1.0, 1.0, transx);
		add_rghtics_y(cr, 400.0, 480.0, 400.0, 1.0, 1.0, transx);
		
		/*cairo_move_to(cr, 4000.0*480/4096, 475.0);
		cairo_line_to(cr, 4000.0*480/4096, 465.0);
		cairo_move_to(cr, 4000.0*480/4096, 0.0);
		cairo_line_to(cr, 4000.0*480/4096, 5.0);
		cairo_move_to(cr, 4000*480/4096-35.0, 480.0);
		cairo_set_source_rgb(cr, 0.1, 0.1, 0.1); 
		cairo_select_font_face(cr, "Garuda",
			CAIRO_FONT_SLANT_NORMAL,
			CAIRO_FONT_WEIGHT_BOLD);
		cairo_set_font_size(cr, 11);
		cairo_show_text(cr, concat_str("", 4000));
		cairo_stroke(cr);*/
		
		gflag.draw = 1;
	}
	
	if(gflag.x == 5) {
		cairo_translate(cr, transx, transy);
		cairo_scale(cr, scalex, scaley); 
		
		double tics[4];
		
		if((x1-x0)*4096/480 < 90) {
			for(i=0; i<=3; i++) {
			//	printf("x0 = %f, tics_%d = %f\n", x0*4096/480, i, tics[i]);
				tics[i] = 25*((int)(x0*4096.0/480.0)/25+i+1)*480/4096;
				add_bottics(cr, ceil(tics[i]), 25*((int)(x0*4096.0/480.0)/25+i+1), scalex, scaley);
			}
		}
		else if((x1-x0)*4096/480 < 200) {
		for(i=0; i<=3; i++) {
			//	printf("x0 = %f, tics_%d = %f\n", x0*4096/480, i, tics[i]);
				tics[i] = 50*((int)(x0*4096.0/480.0)/50+i+1)*480/4096;
				add_bottics(cr, ceil(tics[i]), 50*((int)(x0*4096.0/480.0)/50+i+1), scalex, scaley);
			}
		}
		else if((x1-x0)*4096/480 < 500) {
		for(i=0; i<=3; i++) {
			//	printf("x0 = %f, tics_%d = %f\n", x0*4096/480, i, tics[i]);
				tics[i] = 100*((int)(x0*4096.0/480.0)/100+i+1)*480/4096;
				add_bottics(cr, ceil(tics[i]), 100*((int)(x0*4096.0/480.0)/100+i+1), scalex, scaley);
			}
		}
		else if((x1-x0)*4096/480 < 1000) {
		for(i=0; i<=3; i++) {
			//	printf("x0 = %f, tics_%d = %f\n", x0*4096/480, i, tics[i]);
				tics[i] = 250*((int)(x0*4096.0/480.0)/250+i+1)*480/4096;
				add_bottics(cr, ceil(tics[i]), 250*((int)(x0*4096.0/480.0)/250+i+1), scalex, scaley);
			}
		}
		else if((x1-x0)*4096/480 < 3000) {
		for(i=0; i<=3; i++) {
			//	printf("x0 = %f, tics_%d = %f\n", x0*4096/480, i, tics[i]);
				tics[i] = 500*((int)(x0*4096.0/480.0)/500+i+1)*480/4096;
				add_bottics(cr, ceil(tics[i]), 500*((int)(x0*4096.0/480.0)/500+i+1), scalex, scaley);
			}
		}
		else if((x1-x0)*4096/480 < 4000) {
		for(i=0; i<=3; i++) {
			//	printf("x0 = %f, tics_%d = %f\n", x0*4096/480, i, tics[i]);
				tics[i] = 1000*((int)(x0*4096.0/480.0)/1000+i+1)*480/4096;
				add_bottics(cr, ceil(tics[i]), 1000*((int)(x0*4096.0/480.0)/1000+i+1), scalex, scaley);
			}
		}
		
		gflag.x = 2;
		gflag.draw = 1;
		
		add_rghtics_y(cr, 100.0, x1, 100.0/(scaley), scalex, scaley, transx);
		add_rghtics_y(cr, 200.0, x1, 200.0/(scaley), scalex, scaley, transx);
		add_rghtics_y(cr, 300.0, x1, 300.0/(scaley), scalex, scaley, transx);
		add_rghtics_y(cr, 400.0, x1, 400.0/(scaley), scalex, scaley, transx);
	}
	
	if(gflag.zoom == 1) {
		scalex = 1.0;
		scaley = 1.0;
		transx = 0.0;
		transy = 0.0;
		
		cairo_translate(cr, transx, transy);
		cairo_scale(cr, scalex, scaley);
		
		gflag.draw = 1; num_it=0;
		
		x0 = 0; x1 = 4096;
	}
	
	
	if(gflag.down == 1) {
			printf("scalex = %f transx = %f scaley = %f transy = %f\n", scalex, transx, scaley, transy);
			transy += 50.0;
			
			cairo_translate(cr, transx, transy);
			cairo_scale(cr, scalex, scaley);
			
			gflag.draw = 1;
	}
	
	//printf("aed after flag = %d, flage = %d, cbe.flag[0] = %d\n", gflag, glob.flage, glob_cbe.flag[0]);
	
	if(gflag.draw == 1) {
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
				cairo_set_line_width (cr, 2.0/sqrt(scalex*scalex+4.0*scaley*scaley));
				//printf("line width = %.2f\n", 2.0*scaley/sqrt(scalex*scalex));
				for(j=0; j<=MAX_CH-1; j+=1)
					cairo_arc(cr, (double)(0.117*j), 470.0-(double)spectras->energyspk[i][j], 0.05, 0, 2*M_PI);
				//	{cairo_move_to(cr, (double)(0.117*j), 470.0-(double)spectras->energyspk[i][j]); cairo_line_to(cr, (double)(0.117*j+1), 470.0-(double)spectras->energyspk[i][j]); cairo_stroke(cr);}
				//	cairo_pt(cr, (double)(0.117*j), 470.0-(double)spectras->energyspk[i][j], transx, transy, scalex, scaley);
				//	cairo_pt(cr, (double)(0.117*j), 470.0-(double)spectras->energyspk[i][j], scalex, scaley);
					//cairo_arc(cr, (double)(0.117*j), 470.0-(double)spectras->energyspk[i][j], 0.05, 0, 2*M_PI);
					//cairo_arc(cr, (double)(scalex*240+transx), 230, 50, 0, 2*M_PI);
					//{cairo_move_to(cr, (scalex*220+transx), 230.0); cairo_line_to(cr, (scalex*260+transx), 230.0);}
				cairo_stroke(cr);
				
				if(gflag.exec == 1) {
					double y_background;
					cairo_set_source_rgb (cr, -0.066, 0.82, 0.0);
					cairo_set_line_width (cr, 1.0/sqrt(0.1*scalex*scalex+4.0*scaley*scaley));
					cairo_arc(cr, 0.117*(4096.0*(glob.x[0]+glob.x3[0]/globscalex)/480.0), 470.0-u[5]*4096.0*(glob.x[0]+glob.x3[0]/globscalex)/480.0-u[6], 0.05, 0, 2*M_PI);
					cairo_arc(cr, 0.117*(4096.0*(glob.x[0]+glob.x3[1]/globscalex)/480.0), 470.0-u[5]*4096.0*(glob.x[0]+glob.x3[1]/globscalex)/480.0-u[6], 0.05, 0, 2*M_PI);
					for(j=(4096*(glob.x[0]+glob.x3[0]/globscalex)/480.0); j<(4096*(glob.x[0]+glob.x3[1]/globscalex)/480.0); j+=1) {
						if(470.0-spectras->energyspk[i][j] > 470.0-u[5]*j-u[6]) y_background = 470.0-u[5]*j-u[6];
						else y_background = 470.0-spectras->energyspk[i][j];
						cairo_arc(cr, 0.117*j, y_background, 0.05, 0, 2*M_PI);
					//	printf("j=%d y=%d\n", j, spectras->energyspk[i][j]);
					}
					cairo_fill(cr);
					printf("LEFT Y_1 = %.2f\n", 470.0-u[5]*4096.0*(glob.x[0]+glob.x3[1]/globscalex)/480.0-u[6]);
					
					cairo_set_source_rgb (cr, .0, .0, .0);
					for(j=0; j<=10/scaley; j++) {
						cairo_arc(cr, 0.117*u[2], 470.0-0.1*j*spectras->energyspk[i][(int)u[2]], 0.05, 0, 2*M_PI);
					//	printf("10/scaley = %d j = %d u[2] = %.2f exec = %.2f\n", 10/scaley, j, u[2], 470.0-0.1*j*(double)u[2]);
					}
					cairo_stroke(cr);
					
					for(j=0; j<=100; j++)
						cairo_arc(cr, 0.117*(4096.0*(glob.x[0]+glob.x3[0]/globscalex+0.01*j*(glob.x3[1]-glob.x3[0])/globscalex)/480.0), 470.0-u[5]*(4096.0*(glob.x[0]+glob.x3[0]/globscalex+0.01*j*(glob.x3[1]-glob.x3[0])/globscalex)/480.0)-u[6], 0.01, 0, 2*M_PI);
					//cairo_arc(cr, 0.117*(4096.0*(glob.x[0]+glob.x3[1]/globscalex)/480.0), 470.0-u[5]*(4096.0*(glob.x[0]+glob.x3[1]/globscalex)/480.0)-u[6], 0.01, 0, 2*M_PI);
					//printf("x = %.2f\n", 4096.0*(glob.x[0]+glob.x3[0]/globscalex)/480.0+j);
					printf("LEFT Y_2 = %.2f\n",  470.0-u[5]*(4096.0*(glob.x[0]+glob.x3[0]/globscalex+(glob.x3[1]-glob.x3[0])/globscalex)/480.0)-u[6]);
					cairo_stroke(cr);
					
					gflag.exec = 0;
				}
			}
/*		if(gflag.x==2) ;
		else gflag.x = 0;
		
		if(gflag.y==2) ;
		else gflag.y = 0;*/
		
		gflag.draw = 0; gflag.zoom = 0; gflag.down = 0;
		printf("1\n");
	}
	
	if(gflag.button == 3) {
		cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
		cairo_set_line_width (cr, 1.0/sqrt(scalex*scalex+4.0*scaley*scaley));
		for(j=0; j<=10/scaley; j++) {
			cairo_arc(cr, (glob.x3[0]-transx)/scalex, 470.0-j*47.0, 0.05, 0, 2*M_PI); 
		}
		cairo_stroke(cr);
		for(j=0; j<=10/scaley; j++) {
			cairo_arc(cr, (glob.x3[1]-transx)/scalex, 470.0-j*47.0, 0.05, 0, 2*M_PI); 
		}
		cairo_stroke(cr);
		gflag.button = 0;
		
		/*if(gflag.exec == 1) {
			double pick_pos = u[2];
			printf("flagexec!!!!!!!!!1 pick = %.2f\n", u[2]);
			for(j=0; j<=10/scaley; j++) {
				cairo_arc(cr, (0.117*pick_pos-transx)/scalex, 470.0-j*47.0, 0.05, 0, 2*M_PI); 
			}
			cairo_stroke(cr);
			gflag.exec = 0;
		}*/
	}
}

static gboolean clicked(GtkWidget *widget, GdkEventButton *event,
    gpointer user_data)
{
//	printf("darea was clicked x=%.2f, y=%.2f\n", event->x, event->y);
	
	//global flag = 3 - поставлена одна граница, global flag = 2 - выставлено две границы
	
	if(glob.flag == 3) {
		glob.x[1] = event->x;
		glob.y[1] = event->y;
		glob.flag = 2;
		gtk_widget_queue_draw(widget);
	}
	if(glob.flag == 1) {
		glob.x[0] = event->x;
		glob.y[0] = event->y;
		glob.flag = 3;
	}
		
    return TRUE;
}

static gboolean clicked_in_analyze(GtkWidget *widget, GdkEventButton *event,
    gpointer user_data)
{
	printf("darea was clicked x=%.2f, y=%.2f flag = %d\n", event->x, event->y, gflag.x);
	
	//global flag = 3 - поставлена одна граница, global flag = 2 - выставлено две границы
	
	if(event->button == 1) {
		gflag.button = 1;
		if(gflag.x == 3) {
			glob.x[1] = event->x;
			glob.y[1] = event->y;
			gflag.x = 1;
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
		if(gflag.x == 7) {
			glob.x3[1] = event->x;
			glob.y3[1] = event->y;
			gflag.x = 5;
			gtk_widget_queue_draw(widget);
		}
		else {
			glob.x3[0] = event->x;
			glob.y3[0] = event->y;
			gflag.x = 7;
		}
	}
	
		
    return TRUE;
}

void statusbar_push(const gchar *text)
{
	gtk_statusbar_push(GTK_STATUSBAR(statusbar), sb_context_id, text);
}

void plot_spk( GtkWidget *widget,
               gpointer   data )
{ 
	printf("6\n");
	
	read_plot_spk((GtkWidget *) data);
}

void zoom_out( GtkWidget *widget,
               gpointer   data )
{
	//gflag.zoom = 1;
	
	gflag.x = 0; gflag.y = 0; gflag.zoom = 1;
	
	gtk_label_set_text (GTK_LABEL(dcalc_label), "DCalc");
	
	gtk_widget_queue_draw((GtkWidget *) data);
}

void vertical_up( GtkWidget *widget,
               gpointer   data )
{
	printf("vert up\n");
	//gflag.y = 1;
	
//	gflag.x = 0; 
	gflag.y = 1; 
	gflag.zoom = 0;
	
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

void show_execute( GtkWidget *widget,
               gpointer   data )
{
	int i;
	
	printf("lch = %f rch = %f\n", 4096.0*(glob.x[0]+glob.x3[0]/globscalex)/480.0, 4096.0*(glob.x[0]+glob.x3[1]/globscalex)/480.0);

	for(i=0; i<4; i++)
		if(gflag.cbe[i]==1) {
					switch(i) {
						case 0: DenisCalc(spectras->energyspk[0], NULL, 4096.0*(glob.x[0]+glob.x3[0]/globscalex)/480.0, 4096.0*(glob.x[0]+glob.x3[1]/globscalex)/480.0, 1, 0, 0, u);
							break;
						case 1: DenisCalc(spectras->energyspk[1], NULL, 4096.0*(glob.x[0]+glob.x3[0]/globscalex)/480.0, 4096.0*(glob.x[0]+glob.x3[1]/globscalex)/480.0, 1, 0, 0, u);
							break;
						case 2: DenisCalc(spectras->energyspk[2], NULL, 4096.0*(glob.x[0]+glob.x3[0]/globscalex)/480.0, 4096.0*(glob.x[0]+glob.x3[1]/globscalex)/480.0, 1, 0, 0, u);
							break;
						case 3: DenisCalc(spectras->energyspk[3], NULL, 4096.0*(glob.x[0]+glob.x3[0]/globscalex)/480.0, 4096.0*(glob.x[0]+glob.x3[1]/globscalex)/480.0, 1, 0, 0, u);
							break;
						default: break;
					}
		}
	//DenisCalc(, NULL, lch, rch, 1, 0, 0, u);

	for(i=0; i<=4; i++)
		printf("u[%d] = %e;\n", i, u[i]); 
		
	gchar *text = g_strdup_printf ("\t\t\tDCalc\n\nS = %.2f \ndS = %.2f \nPick postion = %.2f \nError in pick position = %.2f \nFWHM = %.2f", u[0], u[1], u[2], u[3], u[4]);
	gtk_label_set_text (GTK_LABEL (dcalc_label), text);
	gtk_misc_set_alignment(GTK_MISC(dcalc_label), 0.5, 0.0);
	g_free ((gpointer) text);
	
	gflag.button = 3; gflag.x = 5; gflag.exec = 1;
	
	gtk_widget_queue_draw((GtkWidget *) data);
}

int read_plot_spk(GtkWidget *widget)
{
	glob.x[0] = glob.x[1] = 0.0;
	glob.y[0] = glob.y[1] = 0.0;
	
	int count=0;
	int in, i, j;
	char y[8];
	int x[8], N[8];
	int t;
	char s1[64], s2[64];
	
	sprintf(s1, "\"%sDATA2.BIN\"", folder2initialspk);
	sprintf(s2, "%s%s", folder2initialspk, folder2spk);
	printf("%s %s\n", s1, s2); 
	
	in = open("/home/das/job/test/DATA2.BIN", O_RDONLY);
	if(in<0) {
		perror("error in device");
		exit(1);
	}
/*	for(i=0; i<=1000000; i++)
		if ( read(in, &count, sizeof(int)) ==0) {printf("i=%d\n", i); return -1;}
	
	printf("i=%d\n", i); return 0; */
	free(spectras);
	spectras = (dataspk *)malloc(sizeof(dataspk));
	for(i=0; i<12; i++)
		for(j=0; j<=MAX_CH-1; j++)
			spectras->timespk[i][j] = 0;
	for(i=0; i<4; i++)
		for(j=0; j<=MAX_CH-1; j++)
			spectras->energyspk[i][j] = 0;

	for(i=0; i<=7; i++)
		x[i] = y[i] = 0;
		
	for(j=0; j<=14000; j++) {
		
		N[1] = x[0]+256*(0b00001111&x[1]);
		N[7] = 10*x[6]+x[7];
//		printf("N[7] = %d\n", N[7]);
		
		switch(N[7]) {
		case 12 : {N[2] = x[2]+256*(0b00001111&x[3]); N[3] = x[4]+256*(0b00001111&x[5]);
					spectras->energyspk[0][N[2]]++; spectras->energyspk[1][N[3]]++; 
					if (N[2] >= 200 && N[2] <= 470 && N[3] >= 715 && N[3] <= 1100) {
						spectras->timespk[1][N[1]]++; }
					else if (N[2] >= 390 && N[2] <= 630 && N[3] >= 500 && N[3] <= 830) {
						spectras->timespk[0][N[1]]++; }
					break;
					};
		case 13 : {N[2] = x[2]+256*(0b00001111&x[3]); N[4] = x[4]+256*(0b00001111&x[5]);
					spectras->energyspk[0][N[2]]++; spectras->energyspk[2][N[4]]++; 
					if (N[2] >= LD[0] && N[2] <= LD[1] && N[4] >= RD[4] && N[4] <= RD[5])
						spectras->timespk[3][N[1]]++;
					else if (N[2] >= LD[4] && N[2] <= LD[5] && N[4] >= RD[0] && N[4] <= RD[1])
						spectras->timespk[2][N[1]]++;
					break;
					};
		case 14 : {N[2] = x[2]+256*(0b00001111&x[3]); N[6] = x[4]+256*(0b00001111&x[5]);
					spectras->energyspk[0][N[2]]++; spectras->energyspk[3][N[6]]++; 
					if (N[2] >= LD[0] && N[2] <= LD[1] && N[6] >= RD[6] && N[6] <= RD[7])
						spectras->timespk[5][N[1]]++;
					else if (N[2] >= LD[6] && N[2] <= LD[7] && N[6] >= RD[0] && N[6] <= RD[1])
						spectras->timespk[4][N[1]]++;
					break;
					};
		case 23 : {N[3] = x[2]+256*(0b00001111&x[3]); N[4] = x[4]+256*(0b00001111&x[5]);
					spectras->energyspk[1][N[3]]++; spectras->energyspk[2][N[4]]++; 
					if (N[3] >= LD[2] && N[3] <= LD[3] && N[4] >= RD[4] && N[4] <= RD[5])
						spectras->timespk[7][N[1]]++;
					else if (N[3] >= LD[4] && N[3] <= LD[5] && N[4] >= RD[2] && N[4] <= RD[3])
						spectras->timespk[6][N[1]]++;
					break;
					};
		case 24 : {N[3] = x[2]+256*(0b00001111&x[3]); N[6] = x[4]+256*(0b00001111&x[5]);
					spectras->energyspk[1][N[3]]++; spectras->energyspk[3][N[6]]++; 
					if (N[3] >= LD[2] && N[3] <= LD[3] && N[6] >= RD[6] && N[6] <= RD[7])
						spectras->timespk[9][N[1]]++;
					else if (N[3] >= LD[6] && N[3] <= LD[7] && N[6] >= RD[2] && N[6] <= RD[3])
						spectras->timespk[8][N[1]]++;
					break;
					};
		case 34 : {N[4] = x[2]+256*(0b00001111&x[3]); N[6] = x[4]+256*(0b00001111&x[5]);
					spectras->energyspk[2][N[4]]++; spectras->energyspk[3][N[6]]++; 
					if (N[4] >= LD[4] && N[4] <= LD[5] && N[6] >= RD[6] && N[6] <= RD[7])
						spectras->timespk[11][N[1]]++;
					else if (N[4] >= LD[6] && N[4] <= LD[7] && N[6] >= RD[2] && N[6] <= RD[3])
						spectras->timespk[10][N[1]]++;
					break;
					};
		default: break;
		} 
		
		for(i=0; i<=7; i++) {
			if(read(in, &x[i], 1)==0) { printf("j=%d\n", j); close(in); break; /* j=200000; */}
		//	printf("y[%d] = %d  ", i, x[i]);
		}
	//	printf("\n");
	}			
	close(in);
	
	int fd_out[16];
	char name[20];
	char ItoA[3];
	printf("01\n");
	for(i=0; i<=11; i++) {
		strcpy(name, s2);
		strcat(name, "TIME");
		itoa(i+1, ItoA);
		strcat(name, ItoA);
		strcat(name, ".SPK");
		fd_out[i] = open(name, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR);
		
		if (fd_out[i] < 0) {
			perror("error in device gtk_widget_queue_draw()");
			exit(1);
		}
		lseek(fd_out[i], 512, 0);
		
		write(fd_out[i], spectras->timespk[i], MAX_CH*sizeof(int));
		close(fd_out[i]);
	}
	printf("02\n");
	for(i=12; i<=15; i++) {
		strcpy(name, s2);
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
		
		write(fd_out[i], spectras->energyspk[i-12], MAX_CH*sizeof(int));
		close(fd_out[i]);
	}
	printf("3-\n");
	
	gtk_widget_queue_draw(widget);
	
	printf("4\n");
	
	if(glob.flag != 3)
		glob.flag=1;
}

void get_data( GtkWidget *widget,
               gpointer   data )
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
	
	free(spectras);
	spectras = (dataspk *)malloc(sizeof(dataspk));
	for(i=0; i<12; i++)
		for(j=0; j<=MAX_CH-1; j++)
			spectras->timespk[i][j] = 0;
	for(i=0; i<4; i++)
		for(j=0; j<=MAX_CH-1; j++)
			spectras->energyspk[i][j] = 0;
	
	dataspk *truedata = (dataspk *)malloc(sizeof(dataspk));
	
	read(fd_in, truedata, sizeof(dataspk));
	close(fd_in);
	
	for(i=0; i<=11; i++) {
		strcpy(name, folder2spk);
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
		
		write(fd_out[i], truedata->timespk[i], MAX_CH*sizeof(int));
		close(fd_out[i]);
	}
	
	for(i=12; i<=15; i++) {
		strcpy(name, folder2spk);
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
		
		write(fd_out[i], truedata->energyspk[i-12], MAX_CH*sizeof(int));
		close(fd_out[i]);
	}
	
	printf("Get data ok!\n");
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
	if(num_int == 1) { gflag.cbe[0] = 0; num_int = 0; gflag.x = 0; gflag.y = 0; gflag.zoom = 1; gflag.draw = 1;}
	else if (num_int == 0) { gflag.cbe[0] = 1; num_int = 1; gflag.x = 0; gflag.y = 0; gflag.zoom = 1; gflag.draw = 1;}
	
	gtk_widget_queue_draw(window);
}
void cbe2_clicked(GtkWidget *widget, gpointer window) {
	static char num_int = 0;
	if(num_int == 1) { gflag.cbe[1] = 0; num_int = 0; gflag.x = 0; gflag.y = 0; gflag.zoom = 1; gflag.draw = 1;}
	else if (num_int == 0) { gflag.cbe[1] = 1; num_int = 1; gflag.x = 0; gflag.y = 0; gflag.zoom = 1; gflag.draw = 1;}
	
	gtk_widget_queue_draw(window);
}
void cbe3_clicked(GtkWidget *widget, gpointer window) {
	static char num_int = 0;
	if(num_int == 1) { gflag.cbe[2] = 0; num_int = 0; gflag.x = 0; gflag.y = 0; gflag.zoom = 1; gflag.draw = 1;}
	else if (num_int == 0) { gflag.cbe[2] = 1; num_int = 1; gflag.x = 0; gflag.y = 0; gflag.zoom = 1; gflag.draw = 1;}
	
	gtk_widget_queue_draw(window);
}
void cbe4_clicked(GtkWidget *widget, gpointer window) {
	static char num_int = 0;
	if(num_int == 1) { gflag.cbe[3] = 0; num_int = 0; gflag.x = 0; gflag.y = 0; gflag.zoom = 1; gflag.draw = 1;}
	else if (num_int == 0) { gflag.cbe[3] = 1; num_int = 1; gflag.x = 0; gflag.y = 0; gflag.zoom = 1; gflag.draw = 1;}
	
	gtk_widget_queue_draw(window);
}

void analyze_energy_spk( GtkWidget *widget,
               gpointer   data )
{
	GtkWidget *window;
	GtkWidget *hbox, *vbox, *table;
	GtkWidget *darea;
	GtkWidget *cbe1, *cbe2, *cbe3, *cbe4;
	GdkColor color;
	GtkWidget *button0, *button_up, *button_down, *button_left, *button_right, *button_plus, *button_minus, *button_execute;
	GtkWidget *hseparator;
	
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

    gtk_window_set_title (GTK_WINDOW (window), "Energy analyze JINR");
    gtk_widget_set_size_request (window, 640, 480);
    
    g_signal_connect (G_OBJECT (window), "destroy",
						G_CALLBACK (gtk_widget_destroy), window);
    
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    
    darea = gtk_drawing_area_new();
	gtk_widget_set_size_request (darea, 480, 480);
	g_signal_connect(G_OBJECT(darea), "draw", 
		G_CALLBACK(analyze_energy_draw_event), NULL);
	gtk_widget_add_events(darea, GDK_BUTTON_PRESS_MASK);
	g_signal_connect(darea, "button-press-event", 
      G_CALLBACK(clicked_in_analyze), (gpointer) window);
	gtk_box_pack_start (GTK_BOX (hbox), darea, FALSE, FALSE, 0);
		
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
	
	dcalc_label = gtk_label_new("DCalc");
	gtk_misc_set_alignment(GTK_MISC(dcalc_label), 0.5, 0.0);
	
	button0 = gtk_button_new_from_stock (GTK_STOCK_ZOOM_100);
	g_signal_connect (G_OBJECT (button0), "clicked",
					G_CALLBACK (zoom_out), (gpointer) window);
	button_plus = gtk_button_new_from_stock (GTK_STOCK_ZOOM_IN);
	g_signal_connect (G_OBJECT (button_plus), "clicked",
					G_CALLBACK (vertical_up), (gpointer) window);
	button_up = gtk_button_new_from_stock (GTK_STOCK_GO_UP);
	g_signal_connect (G_OBJECT (button_up), "clicked",
					G_CALLBACK (vertical_up), (gpointer) window);
	button_down = gtk_button_new_from_stock (GTK_STOCK_GO_DOWN);
	g_signal_connect (G_OBJECT (button_down), "clicked",
					G_CALLBACK (vertical_down), (gpointer) window);
	button_left = gtk_button_new_from_stock (GTK_STOCK_GO_BACK);
	button_right = gtk_button_new_from_stock (GTK_STOCK_GO_FORWARD);
	button_execute = gtk_button_new_from_stock (GTK_STOCK_EXECUTE);
	g_signal_connect (G_OBJECT (button_execute), "clicked",
					G_CALLBACK (show_execute), (gpointer) window);
	
	table = gtk_table_new (5, 3, TRUE);
	gtk_table_attach_defaults (GTK_TABLE(table), button0, 1, 2, 4, 5);
	gtk_table_attach_defaults (GTK_TABLE(table), button_plus, 1, 2, 2, 3);
	gtk_table_attach_defaults (GTK_TABLE(table), button_up, 1, 2, 1, 2);
	gtk_table_attach_defaults (GTK_TABLE(table), button_down, 1, 2, 3, 4);
	gtk_table_attach_defaults (GTK_TABLE(table), button_left, 0, 1, 2, 3);
	gtk_table_attach_defaults (GTK_TABLE(table), button_right, 2, 3, 2, 3);
	gtk_table_attach_defaults (GTK_TABLE(table), button_execute, 0, 1, 4, 5);
	
	gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
	
	hseparator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start (GTK_BOX (vbox), hseparator, FALSE, FALSE, 0);
	
	gtk_box_pack_start (GTK_BOX (vbox), dcalc_label, FALSE, FALSE, 0);
	
	gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);
	
    gtk_container_add (GTK_CONTAINER (window), hbox);
    gtk_widget_show_all (window);
}

void analyze_time_spk( GtkWidget *widget,
               gpointer   data )
{
}

int read_ready_spk(GtkWidget *widget)
{
	int i, j;
	int in;
	char name[30];
	
	free(spectras);
	spectras = (dataspk *)malloc(sizeof(dataspk));
	for(i=0; i<12; i++)
		for(j=0; j<=MAX_CH-1; j++)
			spectras->timespk[i][j] = 0;
	for(i=0; i<4; i++)
		for(j=0; j<=MAX_CH-1; j++)
			spectras->energyspk[i][j] = 0;
	
	printf("rds\n");
	
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
	
	gtk_widget_queue_draw(widget);
	
	if(glob.flag != 3)
		glob.flag=1;
	
	printf("end\n");
	return 0;
}

void read_spk(GtkWidget *menuitem, gpointer user_data)
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
	                                                             
	int i;
	
	gtk_file_filter_add_pattern (filter, "*.SPK");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog),
	                             filter);

	switch (gtk_dialog_run (GTK_DIALOG (dialog)))
	{
		case GTK_RESPONSE_ACCEPT:
		{
			folder2readspk =
				gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
			strcat(folder2readspk, "/");
			printf("\nfolder_chosen = %s\n", folder2readspk);
			read_ready_spk(user_data);
			break;
		}
		default:
			break;
	}
	
	while(g_main_context_iteration(NULL, FALSE));
	gtk_widget_destroy (dialog);
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
	GtkWidget *box, *hbox;
    GtkWidget *button, *button_get_data, *button_read, *button_plot, *button_zoom_out;
    GtkWidget *button_energy, *button_time;
    GtkWidget *hseparator;
    GtkWidget *darea;
    GtkWidget *scrolled_win;
    int spk_num[1];
    
	gtk_init (&argc, &argv);		

    /* Create a new window */
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

    gtk_window_set_title (GTK_WINDOW (window), "Event by event JINR");
    gtk_widget_set_size_request (window, 1300, 600);

    /* It's a good idea to do this for all windows. */
    g_signal_connect (G_OBJECT (window), "destroy",
                        G_CALLBACK (gtk_main_quit), NULL);

    /* Sets the border width of the window. */
    gtk_container_set_border_width (GTK_CONTAINER (window), 10);
    gtk_widget_realize(window);

    /* Create a new button */
    button_get_data = gtk_button_new_with_label ("Взять накопленные данные online");
    button = gtk_button_new_with_label ("Выбрать директорию со спектрами event by event");
    button_read = gtk_button_new_with_label ("Прочитать файлы .SPK");
    button_plot = gtk_button_new_with_label ("Построить спектры из выбранной директории");

    /* Connect the "clicked" signal of the button to our callback */
    g_signal_connect (G_OBJECT (button), "clicked",
                        G_CALLBACK (get_data), (gpointer) "choose button");
    g_signal_connect (G_OBJECT (button), "clicked",
                        G_CALLBACK (choose_spk), (gpointer) "choose button");
    g_signal_connect (G_OBJECT (button_read), "clicked",
                        G_CALLBACK (read_spk), (gpointer) window);
    g_signal_connect (G_OBJECT (button_plot), "clicked",
                        G_CALLBACK (plot_spk), (gpointer) window);
                        
    button_energy = gtk_button_new_with_label ("Анализ энергетических спектров");
    button_time = gtk_button_new_with_label ("Анализ временных спектров");
    
    g_signal_connect (G_OBJECT (button_energy), "clicked",
                        G_CALLBACK (analyze_energy_spk), NULL);
    g_signal_connect (G_OBJECT (button_time), "clicked",
                        G_CALLBACK (analyze_time_spk), NULL);

    /* This calls our box creating function */
    box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	
    /* Pack and show all our widgets */
    gtk_widget_show(box);

	gtk_box_pack_start (GTK_BOX (box), button_get_data, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (box), button_plot, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (box), button_read, FALSE, FALSE, 0);
    
    gtk_box_pack_start (GTK_BOX (hbox), button_energy, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), button_time, FALSE, FALSE, 0);
    
    gtk_box_pack_start (GTK_BOX (box), hbox, FALSE, FALSE, 0);

    gtk_container_add (GTK_CONTAINER (window), box);
    printf("3\n");
    table = gtk_table_new(3, 6, TRUE);
    
    gtk_box_pack_start (GTK_BOX (box), table, FALSE, FALSE, 0);

	darea = gtk_drawing_area_new();
	gtk_widget_set_size_request (darea, 1000, 550);
	g_signal_connect(G_OBJECT(darea), "draw", 
		G_CALLBACK(on_draw_event), (gpointer) spk_num);
	gtk_widget_add_events(darea, GDK_BUTTON_PRESS_MASK);
	g_signal_connect(darea, "button-press-event", 
      G_CALLBACK(clicked), (gpointer) window);
	gtk_box_pack_start (GTK_BOX (box), darea, FALSE, FALSE, 0);
	
	/*button_zoom_out = gtk_button_new_with_label ("Снять увеличение");
	g_signal_connect (G_OBJECT (button_zoom_out), "clicked",
                        G_CALLBACK (zoom_out), (gpointer) window);
	gtk_box_pack_start (GTK_BOX (box), button_zoom_out, FALSE, FALSE, 0);*/
	
/*	scrolled_win = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_win), darea);
	gtk_box_pack_start (GTK_BOX (box), scrolled_win, FALSE, FALSE, 0); */

    statusbar = gtk_statusbar_new();
    sb_context_id = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusbar), "Message to user");
	statusbar_push("Hello! It's program for working with TDPAC JINR Event by event. Please, choose a folder with data.");
printf("4\n");
	gtk_box_pack_end (GTK_BOX (box), statusbar, FALSE, FALSE, 0);
	
	hseparator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	
	gtk_box_pack_end (GTK_BOX (box), hseparator, FALSE, FALSE, 0);
printf("5\n");
    gtk_widget_show_all (window);

    /* Rest in gtk_main and wait for the fun to begin! */
    gtk_main ();
	
	return 0;
}
