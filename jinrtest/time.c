#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <cairo.h>
#include <gtk/gtk.h>
#include <math.h>

#define MAX_CH 4096
#define EPS 0.00001

#define ZOOM_T_X	100
#define ZOOM_T_Y	0.3
#define LR_T_X	200	

struct {
  char cbt[6];
  char rbt[6];
  char flag;
  double x[2];
  double y[2];
  double x3[2];
  double y3[2];
  
  char zoom;
  char left_t;
  char right_t;
  char zoom_ty;
  char log_t;
  char undo_t;
  char exec_t;
  char sum_t;
} time_gflag;

struct {
	int x_start;
	int x_stop;
	int x_left;
	int width;
	
	double y_comp;
	
	GtkWidget *entry[2]; // 0 - entry X, 1 - entry Y
} coord_xy;

int DenisCalc(const int N[], const unsigned int F[], int LCH, int RCH, char flag, float time_N, float time_F, float u[]);
extern cairo_surface_t *pt_surface();
int max_time();

GtkWidget *button_log_t;
GtkTextBuffer *timespk_buffer_execute;

double tpick[2], latency_ns;
double latency_t;

typedef struct {
	int timespk[12][MAX_CH];
	int energyspk[4][MAX_CH];
} dataspk;

extern char *folder2readspk;
extern dataspk *spectras;

const char TIMESPK_LABEL[][6] = {"D1-D2", "D2-D1", 
							"D1-D3", "D3-D1", 
							"D1-D4", "D4-D1",
							"D2-D3", "D3-D2",
							"D2-D4", "D4-D2",
							"D3-D4", "D4-D3"};
const char DETECTORS_LABEL[][6] = {"D1", "D2", "D3", "D4"};

void time_analyze_destroy(GtkWidget *widget, gpointer window)
{
	int i;
	
	gtk_widget_destroy((GtkWidget *)window);
	for(i=0; i<6; i++) {
		time_gflag.cbt[i] = 2;
		time_gflag.rbt[i] = 0;
	}
		
	time_gflag.zoom = -1; time_gflag.log_t = 0; time_gflag.exec_t = -1;
}

static gboolean analyze_time_draw_cb(GtkWidget *widget, cairo_t *cr, 
    gpointer user_data)
{
	int i, j;
	int width = gtk_widget_get_allocated_width (widget);
	int height =  gtk_widget_get_allocated_height (widget);
	int left_shift = 40;
	int bot_shift = 20;
	int tics_range[8] = {50, 100, 200, 500, 1000, 2000, 3000, 5000};
	long int tics_range_y[8] = {1e3, 5e3, 10e3, 20e3, 90e3, 200e3, 500e3, 1e6};
	int tics_points[8] = {10, 25, 50, 100, 250, 400, 500, 1000};
	int tics_points_y[8] = {250, 1e3, 2e3, 5e3, 10e3, 40e3, 100e3, 200e3};
	int tics[6];
	long int tics_y[8];
	char sum_label[12];
	char coord[6][10], coord_y[8][10];
	long int y_range;
	int x_start, x_stop, x_range;
	static int x_left=0, x_right=0;
	static double y_comp = 1.0;
	double max_Y = 1.0;
	static int graph_nums_inverse, graph_nums_reverse;
	
	cairo_rectangle(cr, 0.0, 0.0, width, height);
	cairo_set_source_rgba (cr, 0.0/255.0, 0.0/255.0, 0.0/255.0, 1.0);
	cairo_fill(cr);
	
	cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0);
	cairo_set_line_width (cr, 0.5);
	cairo_rectangle(cr, 0.0, 0.0, width-left_shift, height-bot_shift);
	cairo_stroke(cr);
	
	if(time_gflag.flag == 1) {x_start = (int) (time_gflag.x[0]/(width-left_shift)*MAX_CH); x_stop = (int) (time_gflag.x[1]/(width-left_shift)*MAX_CH);}
	else {x_start = 0; x_stop = MAX_CH;}
	
	x_left += LR_T_X*time_gflag.left_t;
	if(time_gflag.left_t == 1) time_gflag.left_t = 0;
	x_right += LR_T_X*time_gflag.right_t;
	if(time_gflag.right_t == 1) time_gflag.right_t = 0;
	
	y_comp *= 1.0+time_gflag.zoom_ty*ZOOM_T_Y;
	
	if(time_gflag.zoom == 1) time_gflag.undo_t = 1;
	if(time_gflag.zoom_ty == 1) time_gflag.undo_t = 2;
	if(time_gflag.exec_t == 1 ) time_gflag.undo_t = 3;
	
	if(time_gflag.zoom_ty == 1) time_gflag.zoom_ty = 0;
	
	if(time_gflag.zoom == -1) {x_start = 0; x_stop = MAX_CH; x_left = 0; x_right = 0; y_comp = 1.0; time_gflag.exec_t = 0; time_gflag.zoom = 0;}
			
	if( (x_stop+x_left-x_right>MAX_CH) || (x_start+x_left-x_right < 0) ) {printf("out of range\n"); x_start = 0; x_stop = MAX_CH; x_left = 0; x_right = 0;}
	
	printf("x_stop = %d; x_start = %d; time_gflag.flag = %d\n", x_start, x_stop, time_gflag.flag);
	
	coord_xy.x_start = x_start;
	coord_xy.x_stop = x_stop;
	coord_xy.x_left = x_left;
	coord_xy.width = width;
	coord_xy.y_comp = y_comp;
	
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
	
	if(time_gflag.log_t != 1) {
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
		max_Y = max_time();

		for(i=0; i<8; i++) {
			if(lg_tic<=1000)
				sprintf(coord_y[i], "%.0f", lg_tic);
			else
				sprintf(coord_y[i], "%.0fK", lg_tic/1e3);
			cairo_move_to(cr, (double)(width-left_shift-5.0), height-bot_shift-(double)(height-bot_shift)*log10(lg_tic)/log10(max_Y));
			cairo_line_to(cr, (double)(width-left_shift+5.0), height-bot_shift-(double)(height-bot_shift)*log10(lg_tic)/log10(max_Y));
			cairo_stroke(cr);
			cairo_move_to(cr, (double)(width-left_shift+10.0), height-bot_shift-(double)(height-bot_shift)*log10(lg_tic)/log10(max_Y));
			cairo_show_text(cr, coord_y[i]);
			
			lg_tic *= 10.0;
		}
	}
		
	cairo_set_font_size(cr, 16.0);
	cairo_set_line_width (cr, 1.0);
	graph_nums_inverse = graph_nums_reverse = -1;
	for(i=0; i<6; i++)
		if(time_gflag.cbt[i]==1) {
			if(time_gflag.sum_t == 1)
			{
			graph_nums_inverse = 5;
			graph_nums_reverse = 5;
			switch(i) {
				case 0: { cairo_set_source_rgb (cr, 0.0, 0.0, 102.0/255.0);  }
						break;
				case 1: { cairo_set_source_rgb (cr, 142.0/255.0, 0.0, 0.0);  }
						break;
				case 2: { cairo_set_source_rgb (cr, 0.0, 153.0/255.0, 0.0);  }
						break;
				case 3: { cairo_set_source_rgb (cr, 153.0/255.0, 0.0, 122.0/255.0);  }
						break;
				case 4: { cairo_set_source_rgb (cr, 139.0/255.0, 119.0/255.0, 78.0/255.0);  }
						break;
				case 5: { cairo_set_source_rgb (cr, 31.0/255.0, 122.0/255.0, 122.0/255.0);  }
						break;
				default: break;
			}
			cairo_move_to(cr, 30.0, 15.0);
			sprintf(sum_label, "%s+%s", TIMESPK_LABEL[i*2], TIMESPK_LABEL[i*2+1]);
			cairo_show_text(cr, sum_label);
			cairo_move_to(cr, 10.0, 10.0);
			cairo_line_to(cr, 30.0, 10.0);
			cairo_stroke(cr);
			
			if(time_gflag.log_t != 1)
				for(j=x_start+x_left-x_right; j<=x_stop+x_left-x_right-1; j+=1) {
					cairo_mask_surface(cr, pt_surface(), (double)(width-left_shift)/(x_stop-x_start)*(j-x_start-x_left+x_right), (height-bot_shift-(double)(spectras->timespk[i*2][j]+spectras->timespk[i*2+1][j])/y_comp-3.0));
				}
			else
				for(j=x_start+x_left-x_right; j<=x_stop+x_left-x_right-1; j+=1) {
					cairo_mask_surface(cr, pt_surface(), (double)(width-left_shift)/(x_stop-x_start)*(j-x_start-x_left+x_right), height-bot_shift-(height-bot_shift)*log10( (double)(spectras->timespk[i*2][j]+spectras->timespk[i*2+1][j]) )/log10(max_Y)-3.0);
			}
			
			if(time_gflag.rbt[i] == 0) time_gflag.rbt[i] = 3;
			}
			
			if(time_gflag.rbt[i] == 0 || time_gflag.rbt[i] == 1)
			{
			graph_nums_inverse++;
			switch(i) {
				case 0: { cairo_set_source_rgb (cr, 0.0, 0.0, 102.0/255.0);  }
						break;
				case 1: { cairo_set_source_rgb (cr, 142.0/255.0, 0.0, 0.0);  }
						break;
				case 2: { cairo_set_source_rgb (cr, 0.0, 153.0/255.0, 0.0);  }
						break;
				case 3: { cairo_set_source_rgb (cr, 153.0/255.0, 0.0, 122.0/255.0);  }
						break;
				case 4: { cairo_set_source_rgb (cr, 139.0/255.0, 119.0/255.0, 78.0/255.0);  }
						break;
				case 5: { cairo_set_source_rgb (cr, 31.0/255.0, 122.0/255.0, 122.0/255.0);  }
						break;
				default: break;
			}
			cairo_move_to(cr, 30.0, 15.0+15.0*graph_nums_inverse);
			cairo_show_text(cr, TIMESPK_LABEL[i*2]);
			cairo_move_to(cr, 10.0, 10.0+15.0*graph_nums_inverse);
			cairo_line_to(cr, 30.0, 10.0+15.0*graph_nums_inverse);
			cairo_stroke(cr);
			printf("x_start = %d; x_stop = %d\n", x_start, x_stop);
			
			if(time_gflag.log_t != 1)
				for(j=x_start+x_left-x_right; j<=x_stop+x_left-x_right-1; j+=1) {
					cairo_mask_surface(cr, pt_surface(), (double)(width-left_shift)/(x_stop-x_start)*(j-x_start-x_left+x_right), (height-bot_shift-(double)spectras->timespk[i*2][j]/y_comp-3.0));
				}
			else
				for(j=x_start+x_left-x_right; j<=x_stop+x_left-x_right-1; j+=1)
					cairo_mask_surface(cr, pt_surface(), (double)(width-left_shift)/(x_stop-x_start)*(j-x_start-x_left+x_right), height-bot_shift-(height-bot_shift)*log10( (double)spectras->timespk[i*2][j] )/log10(max_Y)-3.0);
			}
			
			if(time_gflag.rbt[i] == 0 || time_gflag.rbt[i] == 2)
			{
			graph_nums_reverse++;
			switch(i) {
				case 0: { cairo_set_source_rgb (cr, 153.0/255.0, 153.0/255.0, 194.0/255.0);  }
						break;
				case 1: { cairo_set_source_rgb (cr, 208.0/255.0, 148.0/255.0, 148/255.0);  }
						break;
				case 2: { cairo_set_source_rgb (cr, 184.0/255.0, 226.0/255.0, 184.0/255.0);  }
						break;
				case 3: { cairo_set_source_rgb (cr, 214.0/255.0, 153.0/255.0, 202.0/255.0);  }
						break;
				case 4: { cairo_set_source_rgb (cr, 197.0/255.0, 187.0/255.0, 166.0/255.0);  }
						break;
				case 5: { cairo_set_source_rgb (cr, 143.0/255.0, 188.0/255.0, 188.0/255.0);  }
						break;
				default: break;
			}
			cairo_move_to(cr, 130.0, 15.0+15.0*graph_nums_reverse);
			cairo_show_text(cr, TIMESPK_LABEL[i*2+1]);
			cairo_move_to(cr, 110.0, 10.0+15.0*graph_nums_reverse);
			cairo_line_to(cr, 130.0, 10.0+15.0*graph_nums_reverse);
			cairo_stroke(cr);
			if(time_gflag.log_t != 1)
				for(j=x_start+x_left-x_right; j<=x_stop+x_left-x_right-1; j+=1)		
					cairo_mask_surface(cr, pt_surface(), (double)(width-left_shift)/(x_stop-x_start)*(j-x_start-x_left+x_right), (height-bot_shift-(double)spectras->timespk[i*2+1][j]/y_comp-3.0));
			else
				for(j=x_start+x_left-x_right; j<=x_stop+x_left-x_right-1; j+=1)
					cairo_mask_surface(cr, pt_surface(), (double)(width-left_shift)/(x_stop-x_start)*(j-x_start-x_left+x_right), height-bot_shift-(height-bot_shift)*log10( (double)spectras->timespk[i*2+1][j] )/log10(max_Y)-3.0);
			}
			
			if(time_gflag.exec_t == 1 && graph_nums_reverse + graph_nums_inverse != -1 && graph_nums_reverse + graph_nums_inverse != 10) {
				time_gflag.exec_t = -1;
				time_gflag.x3[0] = time_gflag.x3[1] = 0.0;
			}
			else if (time_gflag.exec_t == 1 && graph_nums_reverse + graph_nums_inverse == -1 || graph_nums_reverse + graph_nums_inverse == 10) {
				if(fabs(time_gflag.x3[0] - time_gflag.x3[1] ) < EPS) break;
				
				int kindex[3];
				GtkTextIter iter_start, iter_stop;
				float u[7];
				int m;
				
				cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0);
				
				kindex[0] = x_left+x_start+MAX_CH*(time_gflag.x3[0])/((width-left_shift)*MAX_CH/(x_stop-x_start));
				kindex[1] = x_left+x_start+MAX_CH*(time_gflag.x3[1])/((width-left_shift)*MAX_CH/(x_stop-x_start));
				
				if(graph_nums_inverse == 0)
					m = 2*i;
				else if (graph_nums_reverse == 0)
					m = 2*i+1;
				else if (graph_nums_reverse + graph_nums_inverse == 10)
					m = 2*i;
				printf("m = %d\n", m);	
				
				int sum_spectras[MAX_CH];
				if(time_gflag.sum_t != 1)
					DenisCalc(spectras->timespk[m], NULL, kindex[0], kindex[1], 1, 0, 0, u);
				else {
					for(j=0; j<=MAX_CH; j++)
						sum_spectras[j] = spectras->timespk[m][j] + spectras->timespk[m+1][j];
					
					printf("k0 = %d; k1 = %d\n", kindex[0], kindex[1]);
					for(j=0; j<=10; j++)	
						printf("sum_spectras[%d] = %d\t", j, sum_spectras[j]);
					
					DenisCalc(sum_spectras, NULL, kindex[0], kindex[1], 1, 0, 0, u);
				}
				
				kindex[2] = u[2];

				for(j=0; j<=6; j++)
					printf("u[%d] = %e;\n", j, u[j]); 
				
				//u5 = k u6 = b y=kx+b
				
				kindex[0] = x_left+x_start+MAX_CH*(time_gflag.x3[0])/((width-left_shift)*MAX_CH/(x_stop-x_start));
				kindex[1] = x_left+x_start+MAX_CH*(time_gflag.x3[1])/((width-left_shift)*MAX_CH/(x_stop-x_start));
				if(time_gflag.sum_t != 1) {
					printf("kindex0 = %d kindex1 = %d\n", kindex[0], kindex[1]);
					cairo_set_line_width (cr, 0.2);
					cairo_set_source_rgb(cr, 180.0/255.0, 59.0/255.0, 91.0/255.0);
					cairo_move_to(cr, (double)(width-left_shift)/(x_stop-x_start)*(kindex[0]-x_start-x_left+x_right), height-bot_shift-(double)spectras->timespk[m][kindex[0]]/y_comp);
					for(j=kindex[0]; j<kindex[1] && j<MAX_CH; j++)
						if((double)spectras->timespk[m][j]>=u[5]*j+u[6])
							cairo_line_to(cr, (double)(width-left_shift)/(x_stop-x_start)*(j-x_start-x_left+x_right)+2.5, height-bot_shift-(double)spectras->timespk[m][j]/y_comp-2.5);
						else
							cairo_line_to(cr, (double)(width-left_shift)/(x_stop-x_start)*(j-x_start-x_left+x_right), height-bot_shift-(double)(u[5]*j+u[6])/y_comp);
					cairo_close_path(cr);
					cairo_fill(cr);
				
					printf("0timespk\n");
					cairo_set_line_width (cr, 1.5);
					cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0);
				
					cairo_move_to(cr, (double)(width-left_shift)/(x_stop-x_start)*(kindex[0]-x_start-x_left+x_right), height-bot_shift);
					cairo_line_to(cr, (double)(width-left_shift)/(x_stop-x_start)*(kindex[0]-x_start-x_left+x_right), height-bot_shift-(double)spectras->timespk[m][kindex[0]]/(y_comp));
					cairo_stroke(cr);
				
					cairo_move_to(cr, (double)(width-left_shift)/(x_stop-x_start)*(kindex[1]-x_start-x_left+x_right), height-bot_shift);
					cairo_line_to(cr, (double)(width-left_shift)/(x_stop-x_start)*(kindex[1]-x_start-x_left+x_right), height-bot_shift-(double)spectras->timespk[m][kindex[1]]/(y_comp));
					cairo_stroke(cr);
					
					cairo_set_source_rgb(cr, 255.0/255.0, 251.0/255.0, 0.0/255.0);
					cairo_move_to(cr, (double)(width-left_shift)/(x_stop-x_start)*(kindex[2]-x_start-x_left+x_right), height-bot_shift);
					cairo_line_to(cr, (double)(width-left_shift)/(x_stop-x_start)*(kindex[2]-x_start-x_left+x_right), height-bot_shift-(double)spectras->timespk[m][kindex[2]]/(y_comp));
					cairo_stroke(cr);
					printf("1timespk\n");
				}
				else {
					cairo_set_line_width (cr, 0.2);
					cairo_set_source_rgb(cr, 180.0/255.0, 59.0/255.0, 91.0/255.0);
					cairo_move_to(cr, (double)(width-left_shift)/(x_stop-x_start)*(kindex[0]-x_start-x_left+x_right), height-bot_shift-(double)sum_spectras[kindex[0]]/y_comp);
					for(j=kindex[0]; j<kindex[1]; j++)
						if((double)sum_spectras[j]>=u[5]*j+u[6])
							cairo_line_to(cr, (double)(width-left_shift)/(x_stop-x_start)*(j-x_start-x_left+x_right)+2.5, height-bot_shift-(double)sum_spectras[j]/y_comp-2.5);
						else
							cairo_line_to(cr, (double)(width-left_shift)/(x_stop-x_start)*(j-x_start-x_left+x_right)+2.5, height-bot_shift-(double)(u[5]*j+u[6])/y_comp-2.5);
					cairo_close_path(cr);
					cairo_fill(cr);
				
					cairo_set_line_width (cr, 1.5);
					cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0);
				
					cairo_move_to(cr, (double)(width-left_shift)/(x_stop-x_start)*(kindex[0]-x_start-x_left+x_right), height-bot_shift);
					cairo_line_to(cr, (double)(width-left_shift)/(x_stop-x_start)*(kindex[0]-x_start-x_left+x_right), height-bot_shift-(double)sum_spectras[kindex[0]]/(y_comp));
					cairo_stroke(cr);
				
					cairo_move_to(cr, (double)(width-left_shift)/(x_stop-x_start)*(kindex[1]-x_start-x_left+x_right), height-bot_shift);
					cairo_line_to(cr, (double)(width-left_shift)/(x_stop-x_start)*(kindex[1]-x_start-x_left+x_right), height-bot_shift-(double)sum_spectras[kindex[1]]/(y_comp));
					cairo_stroke(cr);
					
					cairo_set_source_rgb(cr, 255.0/255.0, 251.0/255.0, 0.0/255.0);
					cairo_move_to(cr, (double)(width-left_shift)/(x_stop-x_start)*(kindex[2]-x_start-x_left+x_right), height-bot_shift);
					cairo_line_to(cr, (double)(width-left_shift)/(x_stop-x_start)*(kindex[2]-x_start-x_left+x_right), height-bot_shift-(double)sum_spectras[kindex[2]]/(y_comp));
					cairo_stroke(cr);
				}
				
				time_gflag.x3[0] = time_gflag.x3[1] = 0.0;
				
				gtk_text_buffer_get_iter_at_line_offset(timespk_buffer_execute, &iter_start, 1, 0);
				gtk_text_buffer_get_end_iter(timespk_buffer_execute, &iter_stop);
				gtk_text_buffer_delete(timespk_buffer_execute, &iter_start, &iter_stop);
				gtk_text_buffer_insert(timespk_buffer_execute, &iter_start, g_strdup_printf("LCH = %d \t RCH = %d\n\n", kindex[0], kindex[1]), -1);
				gtk_text_buffer_insert(timespk_buffer_execute, &iter_start, g_strdup_printf("S = %.1f (%.1f) ch\t S_WOB = %.1f ch\n\nPick postion = %.1f (%.1f) ch \n\t\t\t\t %.1f (%.1f) ns \n\nFWHM = %.1f (%.1f) ch\t\t %.1f s\nPrecise = %.1f %%", u[0], u[1], u[8], u[2], u[3], latency_t*u[2], latency_t*u[3], u[4], u[7], latency_t*u[4], 100.0*u[4]/u[2]), -1);
			}
		}
}

static gboolean clicked_in_time_analyze(GtkWidget *widget, GdkEventButton *event,
    gpointer user_data)
{
	printf("clicked_in_time_analyze x=%.2f, y=%.2f flag = %d\n", event->x, event->y, time_gflag.flag);
	
	//global flag = 3 - поставлена одна граница, global flag = 1 - выставлено две границы
	
	if(event->button == 1) {
		if(time_gflag.flag == 3) {
			time_gflag.x[1] = event->x;
			time_gflag.y[1] = event->y;
			time_gflag.flag = 1;
			gtk_widget_queue_draw(widget);
		}
		else {
			time_gflag.x[0] = event->x;
			time_gflag.y[0] = event->y;
			time_gflag.flag = 3;
		}
	}
	
	if(event->button == 3) {
		if(time_gflag.exec_t == 3) {
			time_gflag.x3[1] = event->x;
			time_gflag.y3[1] = event->y;
			time_gflag.exec_t = 1;
			gtk_widget_queue_draw(widget);
		}
		else {
			time_gflag.x3[0] = event->x;
			time_gflag.y3[0] = event->y;
			time_gflag.exec_t = 3;
		}
	}
	
	return TRUE;
}

void timespk_zoom_out( GtkWidget *widget,
               gpointer   window )
{
	int i;
	GtkTextIter iter_start, iter_stop;
	
	printf("zoom out clicked\n");
	time_gflag.zoom = -1;
	time_gflag.flag = 0;
	time_gflag.log_t = 0;
	time_gflag.sum_t = 0;
	gtk_button_set_label(GTK_BUTTON(button_log_t), "log");
	
	for(i=0; i<6; i++)
		if(time_gflag.rbt[i] == 3) time_gflag.rbt[i] = 0;
		
	gtk_text_buffer_get_iter_at_line_offset(timespk_buffer_execute, &iter_start, 1, 0);
	gtk_text_buffer_get_end_iter(timespk_buffer_execute, &iter_stop);
	gtk_text_buffer_delete(timespk_buffer_execute, &iter_start, &iter_stop);
	
	gtk_widget_queue_draw((GtkWidget *) window);
}

void timespk_comp( GtkWidget *widget,
               gpointer  window )
{
	time_gflag.zoom_ty = 1;
	time_gflag.undo_t = 2;
	
	gtk_widget_queue_draw((GtkWidget *) window);
}         

void timespk_horizontal_left( GtkWidget *widget,
               gpointer   window )
{
	printf("horizontal left\n");
	time_gflag.left_t = 1;
	
	gtk_widget_queue_draw((GtkWidget *) window);
}

void timespk_horizontal_right( GtkWidget *widget,
               gpointer   window )
{
	printf("horizontal right\n");
	time_gflag.right_t = 1;
	
	gtk_widget_queue_draw((GtkWidget *) window);
}

void timespk_log_scale( GtkWidget *widget,
               gpointer   window )
{	
	static number_itt = 0;
	
	if(number_itt == 0) {
		gtk_button_set_label(GTK_BUTTON(button_log_t), "lin");
		number_itt = 1;
		time_gflag.log_t = 1;
	}
	else {
		gtk_button_set_label(GTK_BUTTON(button_log_t), "log");
		number_itt = 0;
		time_gflag.log_t = 0;
	}
	
	gtk_widget_queue_draw((GtkWidget *) window);
}

void timespk_undo( GtkWidget *widget,
               gpointer   window )
{
	printf("undo t func\n");
	
	if (time_gflag.undo_t == 1) time_gflag.zoom = -1; // zoom X
	else if (time_gflag.undo_t == 2) time_gflag.zoom_ty = -1; // zoom Y
	else if (time_gflag.undo_t == 3) { // exec
		GtkTextIter iter_start, iter_stop;
	
		gtk_text_buffer_get_iter_at_line_offset(timespk_buffer_execute, &iter_start, 1, 0);
		gtk_text_buffer_get_end_iter(timespk_buffer_execute, &iter_stop);
		gtk_text_buffer_delete(timespk_buffer_execute, &iter_start, &iter_stop);
		time_gflag.exec_t = 0; 
		if(time_gflag.zoom_ty == -1) time_gflag.undo_t == 2;
	}
	
	gtk_widget_queue_draw((GtkWidget *)window);
}

void timespk_show_execute( GtkWidget *widget,
               gpointer   data )
{
}

void timespk_sum( GtkWidget *widget,
               gpointer   window )
{
	time_gflag.sum_t = 1;
	
	time_gflag.zoom = -1;
	time_gflag.flag = 0;
	time_gflag.log_t = 0;
	gtk_button_set_label(GTK_BUTTON(button_log_t), "log");
	
	gtk_widget_queue_draw((GtkWidget *) window);
}

 
void cbt_cb_clicked(int spk_num, gpointer window) 
{
	static char num_int[6] = {0, 0, 0, 0, 0, 0};
	
	time_gflag.exec_t = -1;
	
	if(time_gflag.cbt[spk_num] == 2) 
		if(num_int[spk_num] == 1)
			num_int[spk_num] = 0;
	
	if(num_int[spk_num] == 1) { time_gflag.cbt[spk_num] = 0; num_int[spk_num] = 0;}
	else if (num_int[spk_num] == 0) { time_gflag.cbt[spk_num] = 1; num_int[spk_num] = 1; timespk_zoom_out(NULL, window);}

	gtk_widget_queue_draw(window);
}

void cbt0_clicked( GtkWidget *widget,
               gpointer   window )
{
	cbt_cb_clicked(0, window); 
}
void cbt1_clicked( GtkWidget *widget,
               gpointer   window )
{
	cbt_cb_clicked(1, window);
}    
void cbt2_clicked( GtkWidget *widget,
               gpointer   window )
{
	cbt_cb_clicked(2, window);
}    
void cbt3_clicked( GtkWidget *widget,
               gpointer   window )
{
	cbt_cb_clicked(3, window);
}    
void cbt4_clicked( GtkWidget *widget,
               gpointer   window )
{
	cbt_cb_clicked(4, window);
}    
void cbt5_clicked( GtkWidget *widget,
               gpointer   window )
{
	cbt_cb_clicked(5, window);
}           

void radio_button0_toggle (GtkToggleButton *togglebutton, gpointer window)
{
	printf("radio button_toggled\n");
	
	int i = 0;
	char toggle_label[6];
	
	time_gflag.exec_t = -1;
	
	if(gtk_toggle_button_get_active(togglebutton)) {
		strcpy( toggle_label, gtk_button_get_label(GTK_BUTTON(togglebutton)) );
		if(strncmp(toggle_label, TIMESPK_LABEL[2*i], 6) == 0) time_gflag.rbt[i] = 0;
		else 
			switch(i) {
				case 0: {if(strncmp(toggle_label, DETECTORS_LABEL[0], 6) == 0) time_gflag.rbt[i] = 1; 
						else time_gflag.rbt[i] = 2; }
					break;
				case 1: {if(strncmp(toggle_label, DETECTORS_LABEL[0], 6) == 0) time_gflag.rbt[i] = 1; 
						else time_gflag.rbt[i] = 2; }
					break;
				case 2: {if(strncmp(toggle_label, DETECTORS_LABEL[0], 6) == 0) time_gflag.rbt[i] = 1; 
						else time_gflag.rbt[i] = 2; }
					break;
				case 3: {if(strncmp(toggle_label, DETECTORS_LABEL[1], 6) == 0) time_gflag.rbt[i] = 1; 
						else time_gflag.rbt[i] = 2; }
					break;
				case 4: {if(strncmp(toggle_label, DETECTORS_LABEL[1], 6) == 0) time_gflag.rbt[i] = 1; 
						else time_gflag.rbt[i] = 2; }
					break;
				case 5: {if(strncmp(toggle_label, DETECTORS_LABEL[2], 6) == 0) time_gflag.rbt[i] = 1; 
						else time_gflag.rbt[i] = 2; }
					break;
				default: break;
			}
		gtk_widget_queue_draw(window);	
		printf("cbt[%d] = %d; rbt[%d] = %d\n", i, time_gflag.cbt[i], i, time_gflag.rbt[i]);
	}
}

void radio_button1_toggle (GtkToggleButton *togglebutton, gpointer window)
{
	printf("radio button_toggled\n");
	
	int i = 1;
	char toggle_label[6];
	
	time_gflag.exec_t = -1;
	
	if(gtk_toggle_button_get_active(togglebutton)) {
		strcpy( toggle_label, gtk_button_get_label(GTK_BUTTON(togglebutton)) );
		if(strncmp(toggle_label, TIMESPK_LABEL[2*i], 6) == 0) time_gflag.rbt[i] = 0;
		else 
			switch(i) {
				case 0: {if(strncmp(toggle_label, DETECTORS_LABEL[0], 6) == 0) time_gflag.rbt[i] = 1; 
						else time_gflag.rbt[i] = 2; }
					break;
				case 1: {if(strncmp(toggle_label, DETECTORS_LABEL[0], 6) == 0) time_gflag.rbt[i] = 1; 
						else time_gflag.rbt[i] = 2; }
					break;
				case 2: {if(strncmp(toggle_label, DETECTORS_LABEL[0], 6) == 0) time_gflag.rbt[i] = 1; 
						else time_gflag.rbt[i] = 2; }
					break;
				case 3: {if(strncmp(toggle_label, DETECTORS_LABEL[1], 6) == 0) time_gflag.rbt[i] = 1; 
						else time_gflag.rbt[i] = 2; }
					break;
				case 4: {if(strncmp(toggle_label, DETECTORS_LABEL[1], 6) == 0) time_gflag.rbt[i] = 1; 
						else time_gflag.rbt[i] = 2; }
					break;
				case 5: {if(strncmp(toggle_label, DETECTORS_LABEL[2], 6) == 0) time_gflag.rbt[i] = 1; 
						else time_gflag.rbt[i] = 2; }
					break;
				default: break;
			}
		gtk_widget_queue_draw(window);	
		printf("cbt[%d] = %d; rbt[%d] = %d\n", i, time_gflag.cbt[i], i, time_gflag.rbt[i]);
	}
}

void radio_button2_toggle (GtkToggleButton *togglebutton, gpointer window)
{
	printf("radio button_toggled\n");
	
	int i = 2;
	char toggle_label[6];
	
	time_gflag.exec_t = -1;
	
	if(gtk_toggle_button_get_active(togglebutton)) {
		strcpy( toggle_label, gtk_button_get_label(GTK_BUTTON(togglebutton)) );
		if(strncmp(toggle_label, TIMESPK_LABEL[2*i], 6) == 0) time_gflag.rbt[i] = 0;
		else 
			switch(i) {
				case 0: {if(strncmp(toggle_label, DETECTORS_LABEL[0], 6) == 0) time_gflag.rbt[i] = 1; 
						else time_gflag.rbt[i] = 2; }
					break;
				case 1: {if(strncmp(toggle_label, DETECTORS_LABEL[0], 6) == 0) time_gflag.rbt[i] = 1; 
						else time_gflag.rbt[i] = 2; }
					break;
				case 2: {if(strncmp(toggle_label, DETECTORS_LABEL[0], 6) == 0) time_gflag.rbt[i] = 1; 
						else time_gflag.rbt[i] = 2; }
					break;
				case 3: {if(strncmp(toggle_label, DETECTORS_LABEL[1], 6) == 0) time_gflag.rbt[i] = 1; 
						else time_gflag.rbt[i] = 2; }
					break;
				case 4: {if(strncmp(toggle_label, DETECTORS_LABEL[1], 6) == 0) time_gflag.rbt[i] = 1; 
						else time_gflag.rbt[i] = 2; }
					break;
				case 5: {if(strncmp(toggle_label, DETECTORS_LABEL[2], 6) == 0) time_gflag.rbt[i] = 1; 
						else time_gflag.rbt[i] = 2; }
					break;
				default: break;
			}
		gtk_widget_queue_draw(window);	
		printf("cbt[%d] = %d; rbt[%d] = %d\n", i, time_gflag.cbt[i], i, time_gflag.rbt[i]);
	}
}

void radio_button3_toggle (GtkToggleButton *togglebutton, gpointer window)
{
	printf("radio button_toggled\n");
	
	int i = 3;
	char toggle_label[6];
	
	time_gflag.exec_t = -1;
	
	if(gtk_toggle_button_get_active(togglebutton)) {
		strcpy( toggle_label, gtk_button_get_label(GTK_BUTTON(togglebutton)) );
		if(strncmp(toggle_label, TIMESPK_LABEL[2*i], 6) == 0) time_gflag.rbt[i] = 0;
		else 
			switch(i) {
				case 0: {if(strncmp(toggle_label, DETECTORS_LABEL[0], 6) == 0) time_gflag.rbt[i] = 1; 
						else time_gflag.rbt[i] = 2; }
					break;
				case 1: {if(strncmp(toggle_label, DETECTORS_LABEL[0], 6) == 0) time_gflag.rbt[i] = 1; 
						else time_gflag.rbt[i] = 2; }
					break;
				case 2: {if(strncmp(toggle_label, DETECTORS_LABEL[0], 6) == 0) time_gflag.rbt[i] = 1; 
						else time_gflag.rbt[i] = 2; }
					break;
				case 3: {if(strncmp(toggle_label, DETECTORS_LABEL[1], 6) == 0) time_gflag.rbt[i] = 1; 
						else time_gflag.rbt[i] = 2; }
					break;
				case 4: {if(strncmp(toggle_label, DETECTORS_LABEL[1], 6) == 0) time_gflag.rbt[i] = 1; 
						else time_gflag.rbt[i] = 2; }
					break;
				case 5: {if(strncmp(toggle_label, DETECTORS_LABEL[2], 6) == 0) time_gflag.rbt[i] = 1; 
						else time_gflag.rbt[i] = 2; }
					break;
				default: break;
			}
		gtk_widget_queue_draw(window);	
		printf("cbt[%d] = %d; rbt[%d] = %d\n", i, time_gflag.cbt[i], i, time_gflag.rbt[i]);
	}
}

void radio_button4_toggle (GtkToggleButton *togglebutton, gpointer window)
{
	printf("radio button_toggled\n");
	
	int i = 4;
	char toggle_label[6];
	
	time_gflag.exec_t = -1;
	
	if(gtk_toggle_button_get_active(togglebutton)) {
		strcpy( toggle_label, gtk_button_get_label(GTK_BUTTON(togglebutton)) );
		if(strncmp(toggle_label, TIMESPK_LABEL[2*i], 6) == 0) time_gflag.rbt[i] = 0;
		else 
			switch(i) {
				case 0: {if(strncmp(toggle_label, DETECTORS_LABEL[0], 6) == 0) time_gflag.rbt[i] = 1; 
						else time_gflag.rbt[i] = 2; }
					break;
				case 1: {if(strncmp(toggle_label, DETECTORS_LABEL[0], 6) == 0) time_gflag.rbt[i] = 1; 
						else time_gflag.rbt[i] = 2; }
					break;
				case 2: {if(strncmp(toggle_label, DETECTORS_LABEL[0], 6) == 0) time_gflag.rbt[i] = 1; 
						else time_gflag.rbt[i] = 2; }
					break;
				case 3: {if(strncmp(toggle_label, DETECTORS_LABEL[1], 6) == 0) time_gflag.rbt[i] = 1; 
						else time_gflag.rbt[i] = 2; }
					break;
				case 4: {if(strncmp(toggle_label, DETECTORS_LABEL[1], 6) == 0) time_gflag.rbt[i] = 1; 
						else time_gflag.rbt[i] = 2; }
					break;
				case 5: {if(strncmp(toggle_label, DETECTORS_LABEL[2], 6) == 0) time_gflag.rbt[i] = 1; 
						else time_gflag.rbt[i] = 2; }
					break;
				default: break;
			}
		gtk_widget_queue_draw(window);	
		printf("cbt[%d] = %d; rbt[%d] = %d\n", i, time_gflag.cbt[i], i, time_gflag.rbt[i]);
	}
}

void radio_button5_toggle (GtkToggleButton *togglebutton, gpointer window)
{
	printf("radio button_toggled\n");
	
	int i = 5;
	char toggle_label[6];
	
	time_gflag.exec_t = -1;
	
	if(gtk_toggle_button_get_active(togglebutton)) {
		strcpy( toggle_label, gtk_button_get_label(GTK_BUTTON(togglebutton)) );
		if(strncmp(toggle_label, TIMESPK_LABEL[2*i], 6) == 0) time_gflag.rbt[i] = 0;
		else 
			switch(i) {
				case 0: {if(strncmp(toggle_label, DETECTORS_LABEL[0], 6) == 0) time_gflag.rbt[i] = 1; 
						else time_gflag.rbt[i] = 2; }
					break;
				case 1: {if(strncmp(toggle_label, DETECTORS_LABEL[0], 6) == 0) time_gflag.rbt[i] = 1; 
						else time_gflag.rbt[i] = 2; }
					break;
				case 2: {if(strncmp(toggle_label, DETECTORS_LABEL[0], 6) == 0) time_gflag.rbt[i] = 1; 
						else time_gflag.rbt[i] = 2; }
					break;
				case 3: {if(strncmp(toggle_label, DETECTORS_LABEL[1], 6) == 0) time_gflag.rbt[i] = 1; 
						else time_gflag.rbt[i] = 2; }
					break;
				case 4: {if(strncmp(toggle_label, DETECTORS_LABEL[1], 6) == 0) time_gflag.rbt[i] = 1; 
						else time_gflag.rbt[i] = 2; }
					break;
				case 5: {if(strncmp(toggle_label, DETECTORS_LABEL[2], 6) == 0) time_gflag.rbt[i] = 1; 
						else time_gflag.rbt[i] = 2; }
					break;
				default: break;
			}
		gtk_widget_queue_draw(window);	
		printf("cbt[%d] = %d; rbt[%d] = %d\n", i, time_gflag.cbt[i], i, time_gflag.rbt[i]);
	}
}

static gboolean show_motion_notify_in_time(GtkWidget *widget, GdkEventMotion *event, gpointer entry_xy)
{
	int i, x_start, x_stop, x_left, width;
	double bot_shift = 20.0;
	double left_shift = 40.0;
	gchar *text;
	
	printf("x = %.1f, y = %.1f\n", event->x, event->y);

	x_start = coord_xy.x_start;
	x_stop = coord_xy.x_stop;
	x_left = coord_xy.x_left;
	width = coord_xy.width;

	for(i=0; i<6; i++)
		if(time_gflag.cbt[i] == 1) {
			int x_coord = x_left+x_start+MAX_CH*(event->x)/((width-left_shift)*MAX_CH/(x_stop-x_start));
			if(x_coord > 4095 || x_coord < 0) x_coord = 0;
			text = g_strdup_printf ("%d", x_coord);
			gtk_entry_set_text (GTK_ENTRY (coord_xy.entry[0]), text);
			g_free ((gpointer) text);
			if(time_gflag.rbt[i] == 1) {
				text = g_strdup_printf ("%d", spectras->timespk[2*i][x_coord]);
				gtk_entry_set_text (GTK_ENTRY (coord_xy.entry[1]), text);
				g_free ((gpointer) text);
			}
			else if(time_gflag.rbt[i] == 2) {
				text = g_strdup_printf ("%d", spectras->timespk[2*i+1][x_coord]);
				gtk_entry_set_text (GTK_ENTRY (coord_xy.entry[1]), text);
				g_free ((gpointer) text);
			}
			else if(time_gflag.rbt[i] == 3) {
				text = g_strdup_printf ("%d", spectras->timespk[2*i][x_coord]+spectras->timespk[2*i+1][x_coord]);
				gtk_entry_set_text (GTK_ENTRY (coord_xy.entry[1]), text);
				g_free ((gpointer) text);
			}
		}
		
}

void callibration_t_cb(GtkWidget *dialog, gint response, GtkWidget **entry)
{
	
	switch (response)
	{
    case GTK_RESPONSE_ACCEPT: {
		tpick[0] = atof( gtk_entry_get_text(GTK_ENTRY(entry[0])) );
		tpick[1] = atof( gtk_entry_get_text(GTK_ENTRY(entry[1])) );
		latency_ns = atof( gtk_entry_get_text(GTK_ENTRY(entry[2])) );
		
		if(fabs(tpick[0]-tpick[1])>EPS) latency_t = latency_ns/(tpick[1]-tpick[0]);
		//latency_t = atof( gtk_entry_get_text(GTK_ENTRY(entries[2])) )/( atof( gtk_entry_get_text(GTK_ENTRY(entries[1])) ) - atof( gtk_entry_get_text(GTK_ENTRY(entries[0])) ) );
	}
       break;
    default:
       break;
	}
	
	printf("larencty t = %.2f\n", latency_t);
	gtk_widget_destroy (dialog);
}

void timespk_callibration(GtkWidget *widget, gpointer   window)
{
	printf("main callibration t func\n");
	
	int i, j;
	GtkWidget *dialog, *content_area;
	GtkWidget *grid, *label;
	GtkWidget **entry;
	
	dialog = gtk_dialog_new_with_buttons ("Calibration Time spectrum",
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
	
	entry = g_new(GtkWidget *, 3);
	for(i=0; i<3; i++)
		entry[i] = gtk_entry_new();
	
	gtk_grid_attach(GTK_GRID(grid), gtk_label_new("T pick #1, ch"), 1, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), gtk_label_new("T pick #2, ch"), 2, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), gtk_label_new("latency, ns"), 3, 0, 1, 1);
	
	gtk_grid_attach(GTK_GRID(grid), entry[0], 1, 1, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), entry[1], 2, 1, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), entry[2], 3, 1, 1, 1);
	
	gtk_entry_set_text(GTK_ENTRY(entry[0]), g_strdup_printf("%.1f", tpick[0]));
	gtk_entry_set_text(GTK_ENTRY(entry[1]), g_strdup_printf("%.1f", tpick[1]));
	gtk_entry_set_text(GTK_ENTRY(entry[2]), g_strdup_printf("%.0f", latency_ns));
	
	gtk_container_add (GTK_CONTAINER (content_area), grid);
	
	gtk_widget_show_all (dialog);
	
	g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(callibration_t_cb), entry);
}

void analyze_time_spk( GtkWidget *widget,
               gpointer   data )
{
	int i, j, det1, det2;
	GtkWidget *window;
	char s[60];
	GtkWidget *hbox, *vbox, *grid;
	GtkWidget *darea;
	GtkWidget *cbt[6], *radio_button[6][3], *hbox_cbt[6], *hbox_entry;
	GdkColor color;
	GtkWidget *button0, *img_0, *button_up, *img_up, *button_down, *img_down, *button_left, *img_left, *button_right, *img_right, *button_plus, *img_plus, *button_minus, *button_execute, *button_undo, *img_undo;
	GtkWidget *button_calibration, *button_sum;
	GtkWidget *hseparator;
	GtkWidget *view_buffer; 
	GtkTextIter iter;
	
	void (*radio_func[6])(GtkToggleButton *, gpointer) = {radio_button0_toggle, radio_button1_toggle, radio_button2_toggle, radio_button3_toggle, radio_button4_toggle, radio_button5_toggle};
	
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	sprintf(s, "Time analyze - %s", folder2readspk);
    gtk_window_set_title (GTK_WINDOW (window), s);
    gtk_widget_set_size_request (window, 800, 600);
    
    g_signal_connect (G_OBJECT (window), "destroy",
						G_CALLBACK (time_analyze_destroy), window);
    
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    
    coord_xy.entry[0] = gtk_entry_new();
	coord_xy.entry[1] = gtk_entry_new();
    
    darea = gtk_drawing_area_new();
	gtk_widget_set_size_request (darea, 640, 400);
	g_signal_connect(G_OBJECT(darea), "draw", 
		G_CALLBACK(analyze_time_draw_cb), NULL);
	gtk_widget_add_events(darea, GDK_BUTTON_PRESS_MASK);
	gtk_widget_add_events(darea, GDK_POINTER_MOTION_MASK);
	gtk_widget_add_events(darea, GDK_POINTER_MOTION_HINT_MASK);
	g_signal_connect(darea, "button-press-event", 
      G_CALLBACK(clicked_in_time_analyze), (gpointer) window);
     g_signal_connect (darea, "motion-notify-event",
			     G_CALLBACK (show_motion_notify_in_time), NULL);  
	gtk_box_pack_start (GTK_BOX (hbox), darea, TRUE, TRUE, 0);
		
	cbt[0] = gtk_check_button_new_with_label("Time SPK #1 (D1-D2)");
	gdk_color_parse ("blue", &color);
	gtk_widget_modify_fg (cbt[0], GTK_STATE_NORMAL, &color);
	g_signal_connect(G_OBJECT(cbt[0]), "clicked", 
			G_CALLBACK(cbt0_clicked), (gpointer) window);
	cbt[1] = gtk_check_button_new_with_label("Time SPK #2 (D1-D3)");
	gdk_color_parse ("red", &color);
	gtk_widget_modify_fg (cbt[1], GTK_STATE_NORMAL, &color);
	g_signal_connect(G_OBJECT(cbt[1]), "clicked", 
			G_CALLBACK(cbt1_clicked), (gpointer) window);
	cbt[2] = gtk_check_button_new_with_label("Time SPK #3 (D1-D4)");
	gdk_color_parse ("green", &color);
	gtk_widget_modify_fg (cbt[2], GTK_STATE_NORMAL, &color);
	g_signal_connect(G_OBJECT(cbt[2]), "clicked", 
			G_CALLBACK(cbt2_clicked), (gpointer) window);	
	cbt[3] = gtk_check_button_new_with_label("Time SPK #4 (D2-D3)");
	gdk_color_parse ("#FF00CD", &color);
	gtk_widget_modify_fg (cbt[3], GTK_STATE_NORMAL, &color);
	g_signal_connect(G_OBJECT(cbt[3]), "clicked", 
			G_CALLBACK(cbt3_clicked), (gpointer) window);
	cbt[4] = gtk_check_button_new_with_label("Time SPK #5 (D2-D4)");
	gdk_color_parse ("#C6AA70", &color);
	gtk_widget_modify_fg (cbt[4], GTK_STATE_NORMAL, &color);
	g_signal_connect(G_OBJECT(cbt[4]), "clicked", 
			G_CALLBACK(cbt4_clicked), (gpointer) window);
	cbt[5] = gtk_check_button_new_with_label("Time SPK #6 (D3-D4)");
	gdk_color_parse ("#33CCCC", &color);
	gtk_widget_modify_fg (cbt[5], GTK_STATE_NORMAL, &color);
	g_signal_connect(G_OBJECT(cbt[5]), "clicked", 
			G_CALLBACK(cbt5_clicked), (gpointer) window);
	
	for(i=0; i<6; i++) {
		radio_button[i][0] = gtk_radio_button_new_with_label(NULL, TIMESPK_LABEL[2*i]);
		switch(i) {
			case 0: {det1 = 0; det2 = 1;}
					break;
			case 1: {det1 = 0; det2 = 2;}
					break;
			case 2: {det1 = 0; det2 = 3;}
					break;
			case 3: {det1 = 1; det2 = 2;}
					break;
			case 4: {det1 = 1; det2 = 3;}
					break;
			case 5: {det1 = 2; det2 = 3;}
					break;
			default: break;
		}
		radio_button[i][1] = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (radio_button[i][0]), DETECTORS_LABEL[det1]);
		radio_button[i][2] = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (radio_button[i][0]), DETECTORS_LABEL[det2]);
		for(j=0; j<3; j++)
			g_signal_connect (radio_button[i][j], "toggled",
				G_CALLBACK (*radio_func[i]), (gpointer) window);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(radio_button[i][0]), TRUE);
		
		hbox_cbt[i] = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	}
	
	for(i=0; i<6; i++) {
		gtk_box_pack_start (GTK_BOX (hbox_cbt[i]), cbt[i], FALSE, FALSE, 0);
		for(j=0; j<3; j++)
			gtk_box_pack_start (GTK_BOX (hbox_cbt[i]), radio_button[i][j], FALSE, FALSE, 0);
	}
	
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	for(i=0; i<6; i++)
		gtk_box_pack_start (GTK_BOX (vbox), hbox_cbt[i], FALSE, FALSE, 1);
	
	view_buffer = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(view_buffer), FALSE);
	timespk_buffer_execute = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view_buffer));
	gtk_misc_set_padding(GTK_MISC(timespk_buffer_execute), 0, 20);
	gtk_widget_set_size_request(view_buffer, 320, 300);	
	gtk_text_buffer_create_tag(timespk_buffer_execute, "bold", 
		"weight", PANGO_WEIGHT_BOLD, NULL);
	gtk_text_buffer_create_tag (timespk_buffer_execute, "center",
        "justification", GTK_JUSTIFY_CENTER, NULL);
	gtk_text_buffer_get_iter_at_offset(timespk_buffer_execute, &iter, 0);
	
	gtk_text_buffer_insert_with_tags_by_name(timespk_buffer_execute, &iter, 
        "Execute results:\n", -1, "bold", "center", NULL);
	
	button0 = gtk_button_new();
	img_0 = gtk_image_new_from_file("./zoom-out.png");
	gtk_button_set_image(GTK_BUTTON(button0), img_0);
	g_signal_connect (G_OBJECT (button0), "clicked",
					G_CALLBACK (timespk_zoom_out), (gpointer) window);
	button_plus = gtk_button_new ();
	img_plus = gtk_image_new_from_file("./zoom-y.png");
	gtk_button_set_image(GTK_BUTTON(button_plus), img_plus);
	g_signal_connect (G_OBJECT (button_plus), "clicked",
					G_CALLBACK (timespk_comp), (gpointer) window);
	
	button_left = gtk_button_new ();
	img_left = gtk_image_new_from_file("./go-left.png");
	gtk_button_set_image(GTK_BUTTON(button_left), img_left);
	g_signal_connect (G_OBJECT (button_left), "clicked",
					G_CALLBACK (timespk_horizontal_left), (gpointer) window);
	button_right = gtk_button_new ();
	img_right = gtk_image_new_from_file("./go-right.png");
	gtk_button_set_image(GTK_BUTTON(button_right), img_right);
	g_signal_connect (G_OBJECT (button_right), "clicked",
					G_CALLBACK (timespk_horizontal_right), (gpointer) window);
	button_execute = gtk_button_new_from_stock (GTK_STOCK_EXECUTE);
	g_signal_connect (G_OBJECT (button_execute), "clicked",
					G_CALLBACK (timespk_show_execute), (gpointer) window);
	button_log_t = gtk_button_new_with_label ("log");
	g_signal_connect (G_OBJECT (button_log_t), "clicked",
					G_CALLBACK (timespk_log_scale), (gpointer) window);
	button_undo = gtk_button_new ();
	img_undo = gtk_image_new_from_file("./edit-undo.png");
	gtk_button_set_image(GTK_BUTTON(button_undo), img_undo);
	g_signal_connect (G_OBJECT (button_undo), "clicked",
					G_CALLBACK (timespk_undo), (gpointer) window);
	
	button_calibration = gtk_button_new_with_label("Calibration");
	g_signal_connect (G_OBJECT (button_calibration), "clicked",
					G_CALLBACK (timespk_callibration), (gpointer) window);
	button_sum = gtk_button_new_with_label("Sum");
	g_signal_connect (G_OBJECT (button_sum), "clicked",
					G_CALLBACK (timespk_sum), (gpointer) window);
	
	grid = gtk_grid_new ();
	gtk_grid_attach(GTK_GRID(grid), button0, 1, 4, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), button_plus, 1, 2, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), button_left, 0, 2, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), button_right, 2, 2, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), button_log_t, 0, 4, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), button_undo, 2, 4, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), button_calibration, 4, 2, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), button_sum, 4, 4, 1, 1);

	gtk_box_pack_start (GTK_BOX (vbox), grid, FALSE, FALSE, 0);
	
	hseparator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start (GTK_BOX (vbox), hseparator, FALSE, FALSE, 5);
	
	gtk_box_pack_start (GTK_BOX (vbox), view_buffer, FALSE, FALSE, 0);
	
	hbox_entry = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start (GTK_BOX (hbox_entry), gtk_label_new("ch:"), FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (hbox_entry), coord_xy.entry[0], FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (hbox_entry), gtk_label_new("cts:"), FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (hbox_entry), coord_xy.entry[1], FALSE, FALSE, 1);
	
	gtk_box_pack_start (GTK_BOX (vbox), hbox_entry, FALSE, FALSE, 5);
	
	gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 2);
	
    gtk_container_add (GTK_CONTAINER (window), hbox);
    gtk_widget_show_all (window);
}
