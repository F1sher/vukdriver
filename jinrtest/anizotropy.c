#include <stdio.h>
#include <stdlib.h>
#include <cairo.h>
#include <gtk/gtk.h>
#include <math.h>

#define sqr(x)	((x)*(x))
#define ln(x)	log(x)

#define MAX_CH 4096
#define BG_START	500
#define BG_STOP	600

#define EPS	0.000001

extern int COMP;
extern cairo_surface_t *pt_surface();

typedef struct {
	int timespk[12][MAX_CH];
	int energyspk[4][MAX_CH];
} dataspk;

extern char *folder2readspk;
extern dataspk *spectras;

extern double *coincidence[2];
static int nucleus = 0;

extern double zero_R;
extern double *aniz;

struct {
  int flag;
  double x[2];
} flag_da_X;

struct {
  int flag;
  double x[2];
} flag_R;

double max_bubble(double *x, int nums)
{
	int i;
	double z;
	
	for(i=1, z = fabs(x[0]); i<=nums-1; i++)
		if(fabs(x[i]) > z)
			z = fabs(x[i]);
	
	return z;
}

int maxnum_bubble_int(int *x, int nums)
{
	int i, max_num = 100;
	
	for(i=100; i<=nums-1200; i++)
		if( (x[i] > x[max_num]) && (x[i] <= 2*(x[i-1]+x[i+1])) )
			max_num = i;
	
	return max_num;
}

double center_of_pick(int *x, int left_border, int right_border)
{
	int i;
	double xc, s;
	
	xc = s = 0.0;
	
	for(i=left_border; i<=right_border; i++) {
		xc += (double)i*x[i];
		s += (double)x[i];
	}
	
	if(s<EPS) xc = 0;
	else xc = xc/s;
	
	return xc;
}

void swap_spk(int *x, int xc, int nums)
{
	int i, p;
	
	for(i=0; (i<=xc) && (xc+i<=nums-1) && (xc-i>=0); i++) {
		p = x[xc+i];
		x[xc+i] = x[xc-i];
		x[xc-i] = p;
	}
	
	printf("i in swap spk = %d\n", i);
}

int anisotropy(double *res, double **y, double **F, double *compX90, double *compX180, double *R, double *dRcomp)
{
	//n array - time spk, e array - energy spk
	int i, j, k=0, z1, z2, z3, p;
	int in;
	int c, max_el[6], cent[6];
	int **n = (int **)calloc(12, sizeof(int *)); 
	int **m = (int **)calloc(6, sizeof(int *)); 
	double u, v1, v2;
	int max_el_num[6];
	double t = 0.0, K[4], K_min;
	int S_start[4], S_stop[4];
	char name[7];
	char ItoA[3];
	
	double *xc, *s, *shift;

	/**************** ВЫДЕЛЕНИЕ ПАМЯТИ ****************/
	for(i=0; i<12; i++)
		n[i] = (int *)calloc(MAX_CH, sizeof(int));
	
	for(i=0; i<6; i++)
		m[i] = (int *)calloc(MAX_CH, sizeof(int));
		
	xc = (double *)calloc(6, sizeof(double));
	s = (double *)calloc(6, sizeof(double));
	shift = (double *)calloc(6, sizeof(double));
	/****************  ****************/

	for(i=0; i<12; i++)
		for(j=0; j<=MAX_CH-1; j++)
			n[i][j] = spectras->timespk[i][j];

	//Iterr #1 shift
	for(i=0; i<=MAX_CH-1; i++) {
		m[0][i] = n[0][i]+n[1][i]; //D1-2
		m[1][i] = n[2][i]+n[3][i]; //D1-3
		m[2][i] = n[4][i]+n[5][i]; //D1-4
		m[3][i] = n[6][i]+n[7][i]; //D2-3
		m[4][i] = n[8][i]+n[9][i]; //D2-4
		m[5][i] = n[10][i]+n[11][i]; //D3-4
	}
	
	for(i=0; i<6; i++) {
		max_el_num[i] = maxnum_bubble_int(m[i], MAX_CH);
		max_el[i] = m[i][max_el_num[i]];
		printf("max el num = (%d, %d)\n", max_el_num[i], m[i][max_el_num[i]]);
	}
	
	for(i=0; i<6; i++) {
		xc[i] = center_of_pick(m[i], max_el_num[i]-100, max_el_num[i]+99);
		printf("Xc[%d] = %.2f\n", i, xc[i]);
	}
	
	for(i=0; i<6; i++)
		shift[i] = xc[0]-xc[i];
		
	for(i=0; i<6; i++) {
		res[i] = xc[i];
		res[i+6] = shift[i];
		res[i+2*6] = max_el[i];
	}
	
	FILE *pFile;
	for(i=0; i<6; i++) {
		pFile = fopen (g_strdup_printf("../0SUMtest%d", i),"w+");
		for(j=0; j<=MAX_CH-1; j++)
			fprintf (pFile, "%d %d\n", j, m[i][j]);
		fclose(pFile);
	}
	
	//SWAP 1 - Ti, 0 - In
	for(i=nucleus; i<12; i+=2) {
		printf("xc[%d] = %d\n", i/2, (int)xc[i/2]);
		swap_spk(n[i], (int)round(xc[i/2]), MAX_CH);
	}

	for(i=0; i<=MAX_CH-1; i++) {
		m[0][i] = n[0][i]+n[1][i]; //D1-2
		m[1][i] = n[2][i]+n[3][i]; //D1-3
		m[2][i] = n[4][i]+n[5][i]; //D1-4
		m[3][i] = n[6][i]+n[7][i]; //D2-3
		m[4][i] = n[8][i]+n[9][i]; //D2-4
		m[5][i] = n[10][i]+n[11][i]; //D3-4
	}

	for(i=0; i<6; i++) {
		pFile = fopen (g_strdup_printf("../1SUMtest%d", i),"w+");
		for(j=0; j<=MAX_CH-1; j++)
			fprintf (pFile, "%d %d\n", j, m[i][j]);
		fclose(pFile);
	}
	
	for(i=0; i<6; i++) {
		max_el_num[i] = maxnum_bubble_int(m[i], MAX_CH);
		max_el[i] = m[i][max_el_num[i]];
		printf("max el num = (%d, %d)\n", max_el_num[i], m[i][max_el_num[i]]);
	}
	
	for(i=0; i<6; i++) {
		xc[i] = center_of_pick(m[i], max_el_num[i]-100, max_el_num[i]+99);
		printf("Xc[%d] = %.2f\n", i, xc[i]);
	}
	
	for(i=0; i<6; i++) {
		shift[i] = xc[0]-xc[i];
		printf("shift[%d] = %.2f\n", i, shift[i]);
	}

	for(i=2; i<12; i++) {
		p = i/2;
		k = (int)round(shift[p]);
		printf("k = %d p = %d\n", k, p);
		if(k>0)
			for(j=MAX_CH-1; j>=k; j--)
				n[i][j] = n[i][j-k];
		else if(k<0)
			for(j=0; j<=MAX_CH-1-k; j++)
				n[i][j] = n[i][j-k];
	}
			
	for(i=0; i<=MAX_CH-1; i++) {
		m[0][i] = n[0][i]+n[1][i]; //D1-2
		m[1][i] = n[2][i]+n[3][i]; //D1-3
		m[2][i] = n[4][i]+n[5][i]; //D1-4
		m[3][i] = n[6][i]+n[7][i]; //D2-3
		m[4][i] = n[8][i]+n[9][i]; //D2-4
		m[5][i] = n[10][i]+n[11][i]; //D3-4
	}
		
	for(i=0; i<6; i++) {
		max_el_num[i] = maxnum_bubble_int(m[i], MAX_CH);
		max_el[i] = m[i][max_el_num[i]];
		printf("max el num = (%d, %d)\n", max_el_num[i], m[i][max_el_num[i]]);
	}
	
	for(i=0; i<6; i++) {
		xc[i] = center_of_pick(m[i], max_el_num[i]-100, max_el_num[i]+99);
		printf("Xc[%d] = %.2f\n", i, xc[i]);
	}
	
	for(i=0; i<6; i++) {
		shift[i] = xc[0]-xc[i];
		printf("shift[%d] = %.2f\n", i, shift[i]);
	}
	
	for(i=0; i<=MAX_CH-1; i++) {
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
	
	for (i=0; i<=4-1; i++)
		K[i]=(double)S_start[i]*S_stop[i];
	
	K_min = K[0];	
	for(i=1; i<=4-1; i++) {
		if (K[i]<K_min) K_min=K[i];
	}
	
	for(i=0; i<=4-1; i++) {
		K[i] = K[i]/K_min;
		printf("K[%d] = %.2f\n", i, K[i]);
	}
	
	for(j=0; j<=MAX_CH-1; j++) {
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
/*	double alpha[12], beta[12];
	double sxlny[12], sx[12], slny[12], ssqrx[12], ssqrlny[12];
	
	for(i=0; i<=12-1; i++)
		for(j=BG_START; j<BG_STOP; j++) {
			sxlny[i] += j*ln(y[i][j]);
			sx[i] += (double)j;
			slny[i] += ln(y[i][j]);
			ssqrx[i] += sqr((double)j);
			ssqrlny[i] += sqr(ln(y[i][j]));
		}
	
	for(i=0; i<=12-1; i++) {
		beta[i] = ( (BG_STOP-BG_START)*sxlny[i] - sx[i]*slny[i] ) / ( (BG_STOP-BG_START)*ssqrx[i]-sqr(sx[i]) ); 
		alpha[i] = exp( (slny[i]-beta[i]*sx[i])/(BG_STOP-BG_START) );
		printf("\n alpha[%d] = %f; beta[%d] = %.20f\n", i, alpha[i], i, beta[i]);
	}

	for(i=0; i<=12-1; i++) {
		res[3*6+i] = alpha[i];
		res[3*6+12+i] = beta[i];
	}
		
	for(i=0; i<=12-1; i++)
		for(j=0; j<=MAX_CH-1; j++)
			F[i][j] = alpha[i]*exp( beta[i]*j ); */
	
	//y_background = const	
	for(i=0; i<12; i++)
		for(j=BG_START; j<BG_STOP; j++)
			F[i][0] += y[i][j];
			
	for(i=0; i<12; i++)
		F[i][0] = F[i][0] / (double)(BG_STOP-BG_START);
	
	for(i=0; i<12; i++)
		for(j=1; j<=MAX_CH-1; j++)
			F[i][j] = F[i][0];
			
	for(i=0; i<12; i++) {
		pFile = fopen (g_strdup_printf("../Y%d", i),"w+");
		for(j=0; j<=MAX_CH-1; j++)
			fprintf (pFile, "%d %.2f\n", j, y[i][j]);
		fclose(pFile);
	}
	for(i=0; i<12; i++) {
		pFile = fopen (g_strdup_printf("../F%d", i),"w+");
		for(j=0; j<=MAX_CH-1; j++)
			fprintf (pFile, "%d %.2f\n", j, F[i][j]);
		fclose(pFile);
	}
			
	//y_background = A + B*exp(x_j)
	/*double sy[12], sex[12], syex[12], se2x[12];
	double alpha[12], beta[12]; 
	
	for(i=0; i<=12-1; i++)
		for(j=BG_START; j<BG_STOP; j++) {
			sy[i] += y[i][j];
			sex[i] += exp ((double)j);
			syex[i] += y[i][j]*exp(j);
			se2x[i] += exp(2.0*j);
		}
	 printf("%.2f %.2f\n", sy[0], sex[0]);
	for(i=0; i<=12-1; i++) {
		beta[i] = ( (BG_STOP-BG_START)*syex[i]-sy[i]*sex[i] ) / ( (BG_STOP-BG_START)*se2x[i] - sqr(sex[i]) );
		alpha[i] = (sy[i] - beta[i]*sex[i])/(BG_STOP-BG_START);
		printf("\n alpha[%d] = %f; beta[%d] = %e\n", i, alpha[i], i, beta[i]);
	}
	 
	for(i=0; i<=12-1; i++)
		for(j=0; j<=MAX_CH-1; j++)
			F[i][j] = alpha[i] + beta[i]*exp(j*TAU/3.6);
	*/
	
	printf("before background y[11][550] = %.1f\n", y[11][550]);
	printf("F[11][550] = %.1f\n", F[11][550]);
	for(i=0; i<=12-1; i++)
		for(j=0; j<=MAX_CH-1; j++)
			if((v1=(y[i][j] - F[i][j]))<EPS)
				y[i][j] = 1.0;
			else
				y[i][j] = v1;
	
	printf("afterbackground y[11][550] = %.1f; background = %.1f\n", y[11][550], F[11][550]);
	
	for(i=0; i<=12-1; i++)
		printf("y[%d][550] = %.1f\n", i, y[i][550]);
	
	double *X90, *X180;
	X90 = (double *)calloc(MAX_CH, sizeof(double));
	X180 = (double *)calloc(MAX_CH, sizeof(double));
	
	for(j=0; j<=MAX_CH-1; j++) {
		v1 = y[0][j]*y[1][j]*y[4][j]*y[5][j]*y[6][j]*y[7][j]*y[10][j]*y[11][j];
	//	X90[j] = pow(y[0][j], 1.0/8.0)*pow(y[1][j],1.0/8.0)*pow(y[4][j],1.0/8.0)*pow(y[5][j],1.0/8.0)*pow(y[6][j],1.0/8.0)*pow(y[7][j],1.0/8.0)*pow(y[10][j],1.0/8.0)*pow(y[11][j],1.0/8.0);
		if(v1<EPS) X90[j] = 0.0;
		else X90[j] = pow(v1, 1.0/8.0);
		if(X90[j]<EPS) X90[j] = 0.0;
		if (isnan(X90[j])) printf("Error! X90[%d] is NaN!\n", j);
		
		v2 = y[2][j]*y[3][j]*y[8][j]*y[9][j];
		if(v2<=EPS) X180[j] = 0.0;
		else X180[j] = pow(v2, 1.0/4.0);
		if (isnan(X180[j])) printf("Error! X180[%d] is NaN!\n", j);
	}

	for(i=0; i<=MAX_CH/COMP-1; i++)
		compX90[i] = compX180[i] = 0.0;

	for(p=0; p<=MAX_CH/COMP-1; p++)
		for(j=0; j<=COMP-1 && p*COMP+j<=MAX_CH-1; j++) {
			compX90[p] += X90[p*COMP+j];
			compX180[p] += X180[p*COMP+j];
			if(p==108) printf("x90[%d] = %E\n", p*COMP+j, X90[p*COMP+j]);
		}

	for(j=0; j<=MAX_CH/COMP-1; j++) {
		if ((compX180[j]+2.0*compX90[j]) <= EPS) { R[j] = 0.0; printf("anis=0!\n"); }
		else R[j] = 2.0*(compX180[j]-compX90[j])/(compX180[j]+2.0*compX90[j]);
		if (isnan(R[j])) printf("Error! R[%d] is NaN! compX90[%d] = %E; comX180[%d] = %E\n", j, j, compX90[j], j, compX180[j]);
		if (isinf(R[j])) printf("Error! R[%d] is Inf! and = %E\n", j, R[j]);
	}
	
	double **errSum = (double **)malloc(MAX_CH*sizeof(double *));
	errSum[0] = (double *)calloc(MAX_CH, sizeof(double));
	errSum[1] = (double *)calloc(MAX_CH, sizeof(double));
	
	for(j=0; j<=MAX_CH-1; j++) {
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

	double dR[MAX_CH];
	for(j=0; j<=MAX_CH-1; j++) {
		v1 = sqr(X90[j]+2.0*X180[j])*pow((errSum[0][j]+errSum[1][j]), 0.5);
		if (v1 <= EPS) return -1;
		dR[j] = 6.0*X90[j]*X180[j]/v1;
	}
	
	for(p=0; p<=MAX_CH/COMP-1; p++)
		for(j=0; j<=COMP-1 && p*COMP+j<=MAX_CH-1; j++) {
			dRcomp[p] += sqr(dR[p*COMP+j]);
		}
		
	for(p=0; p<=MAX_CH/COMP-1; p++)
		dRcomp[p] = pow(dRcomp[p], 1.0/2.0);*/

//	for(i=0; i<12; i++)
//		free(n[i]);
	free(n);
	
//	for(i=0; i<6; i++)
//		free(m[i]);
	free(m);
	
	free(X90);
	free(X180);

	printf("anisotropy function end\n");
	return 0;
}


static gboolean clicked_in_da_X(GtkWidget *widget, GdkEventButton *event,
    gpointer user_data)
{	
	//global flag = 3 - поставлена одна граница, global flag = 1 - выставлено две границы
	
	if(event->button == 1) {
		if(flag_da_X.flag == 3) {
			flag_da_X.x[1] = event->x;
			flag_da_X.flag = 1;
			gtk_widget_queue_draw(widget);
		}
		else {
			flag_da_X.x[0] = event->x;
			flag_da_X.flag = 3;
		}
	}
	
	if(event->button == 3) {
		flag_da_X.flag = -1;
		gtk_widget_queue_draw(widget);
	}
		
	printf("da_X was clicked x=%.2f, y=%.2f flag = %d\n", event->x, event->y, flag_da_X.flag);	
		
    return TRUE;
}

void draw_X(GtkWidget *widget, cairo_t *cr, 
    gpointer X_data)
{
	int width = gtk_widget_get_allocated_width (widget);
	int height = gtk_widget_get_allocated_height (widget);
	
	int i, j;
	double max_X = max_bubble(coincidence[0], MAX_CH/COMP);
	if(max_bubble(coincidence[1], MAX_CH/COMP) > max_X) max_X = max_bubble(coincidence[1], MAX_CH/COMP);
	
	static int x_start = 0, x_stop = 0;
	
	char coord[8][10];
	char coord_y[8][10];
	int tics[8]; 
	long int tics_y[8];
	int tics_range[8] = {50, 100, 200, 500, 1000, 2000, 3000, 5000};
	long int tics_range_y[8] = {1e3, 5e3, 10e3, 20e3, 90e3, 200e3, 500e3, 1e6};
	int tics_points[8] = {10, 25, 50, 100, 250, 400, 500, 1000};
	int tics_points_y[8] = {250, 1e3, 2e3, 5e3, 10e3, 40e3, 100e3, 200e3};
	int x_range;
	long int y_range;
	
	int bot_shift = 40;
	int left_shift = 40;
		
	cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0);
	cairo_set_line_width (cr, 1.0);
	cairo_rectangle(cr, 0.0, 0.0, width, height);
	cairo_fill(cr);
	
	cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 1.0);
	cairo_rectangle(cr, 0.0, 0.0, width-left_shift, height-bot_shift);
	cairo_stroke(cr);
	
	if(flag_da_X.flag == 1) {x_start = (int)(flag_da_X.x[0]/(width-left_shift)*MAX_CH/COMP); x_stop = (int)(flag_da_X.x[1]/(width-left_shift)*MAX_CH/COMP);}
	else {x_start = 0; x_stop = MAX_CH/COMP;}
	
	if(flag_da_X.flag == -1) {x_start = 0; x_stop = MAX_CH/COMP;}
	
	printf("x_start = %d, x_stop = %d, flag = %d\n", x_start, x_stop, flag_da_X.flag);
	
	x_range = x_stop-x_start;
	for(j=0; j<8; j++)
		if (x_range < tics_range[j]) {
			for(i=0; i<=5; i++) {
				tics[i] = tics_points[j]*((int)(x_start/tics_points[j])+i);
				if((width-left_shift)/(x_stop-x_start)*(tics[i]-x_start)>width-left_shift) break;
				sprintf(coord[i], "%d", tics[i]);
				cairo_move_to(cr, (double)(width-left_shift)/(x_stop-x_start)*(tics[i]-x_start), height-bot_shift+5.0);
				cairo_line_to(cr, (double)(width-left_shift)/(x_stop-x_start)*(tics[i]-x_start), height-bot_shift-5.0);
				cairo_stroke(cr);
				cairo_move_to(cr, (double)(width-left_shift)/(x_stop-x_start)*(tics[i]-x_start), height-20.0);
				cairo_show_text(cr, coord[i]);
			}
			j = 8;
		}
	
	y_range = (long int)max_X;
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
	
	/*sprintf(coord_y[0], "%.1f", 0.0);
	sprintf(coord_y[2], "%.1fK", max_X/1000.0);
			
	cairo_move_to(cr, width-5, height-10.0);
	cairo_line_to(cr, width-15.0, height-10.0);
	cairo_stroke(cr);
	cairo_move_to(cr, width-30.0, height-15.0);
	cairo_show_text(cr, coord_y[0]);
	cairo_move_to(cr, width-30.0, 10.0);
	cairo_show_text(cr, coord_y[2]);
	cairo_stroke(cr);*/
	
	cairo_set_font_size(cr, 16.0);
	for(i=0; i<=1; i++) {
		if(i==0)
			cairo_set_source_rgb (cr, 0.0, 1.0, 0.0);
		else
			cairo_set_source_rgb (cr, 0.0, 0.0, 1.0);
		for(j=x_start; j<=x_stop-1; j+=1)
			cairo_mask_surface(cr, pt_surface(), (double)(width-left_shift)/(x_stop-x_start)*(j-x_start),  height-bot_shift - 0.5*(height-bot_shift)*coincidence[i][j]/max_X-3.0);
	
		cairo_set_line_width (cr, 0.5);
		cairo_move_to(cr, 0.0, height-bot_shift);
		for(j=x_start; j<=x_stop-1; j+=1)
			cairo_arc(cr, (double)(width-left_shift)/(x_stop-x_start)*(j-x_start)+3.0,  height-bot_shift - 0.5*(height-bot_shift)*coincidence[i][j]/max_X, 0.05, 0, 2*M_PI);
		cairo_stroke(cr);
		
		cairo_move_to(cr, 30.0, 15.0+15.0*i);
		if(i)
			cairo_show_text(cr, "X180");
		else
			cairo_show_text(cr, "X90");
		cairo_move_to(cr, 10.0, 10.0+15.0*i);
		cairo_line_to(cr, 30.0, 10.0+15.0*i);
		cairo_stroke(cr);
	}
	
	cairo_set_line_width (cr, 1.0);
	cairo_set_source_rgba (cr, 1.0, 0.0, 0.0, 1.0);
	cairo_move_to(cr, (double)(width-left_shift)/(x_stop-x_start)*(zero_R/COMP-x_start)+3.0, height-bot_shift);
	cairo_line_to(cr, (double)(width-left_shift)/(x_stop-x_start)*(zero_R/COMP-x_start)+3.0, 0.0);
	cairo_stroke(cr);
} 

static gboolean clicked_in_anizotropy(GtkWidget *widget, GdkEventButton *event,
    gpointer user_data)
{	
	//global flag = 3 - поставлена одна граница, global flag = 1 - выставлено две границы
	
	if(event->button == 1) {
		if(flag_R.flag == 3) {
			flag_R.x[1] = event->x;
			flag_R.flag = 1;
			gtk_widget_queue_draw(widget);
		}
		else {
			flag_R.x[0] = event->x;
			flag_R.flag = 3;
		}
	}
	
	if(event->button == 3) {
		flag_R.flag = -1;
		gtk_widget_queue_draw(widget);
	}
		
	printf("da_X was clicked x=%.2f, y=%.2f flag = %d\n", event->x, event->y, flag_R.flag);	
		
    return TRUE;
}

void draw_anizotropy(GtkWidget *widget, cairo_t *cr, 
    gpointer R)
{
	int width = gtk_widget_get_allocated_width (widget);
	int height = gtk_widget_get_allocated_height (widget);
	
	int i, j;
	double max_R = max_bubble((double *)R, MAX_CH/COMP);
	
	static int x_start, x_stop;
	
	char coord[8][10];
	char coord_y[8][10];
	int tics[8]; 
	double tics_y[2];
	double tics_y_step;
	int tics_range[8] = {50, 100, 200, 500, 1000, 2000, 3000, 5000};
	int tics_points[8] = {10, 25, 50, 100, 250, 400, 500, 1000};
	int x_range;
	
	int bot_shift = 40;
	int left_shift = 40;
	
	cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0);
	cairo_set_line_width (cr, 1.0);
	cairo_rectangle(cr, 0.0, 0.0, width, height);
	cairo_fill(cr);
	
	cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 1.0);
	cairo_rectangle(cr, 0.0, 0.0, width-left_shift, height-bot_shift);
	cairo_stroke(cr);
	
	if(flag_R.flag == 1) {x_start = (int)(flag_R.x[0]/(width-left_shift)*MAX_CH/COMP); x_stop = (int)(flag_R.x[1]/(width-left_shift)*MAX_CH/COMP);}
	else {x_start = 0; x_stop = MAX_CH/COMP;}
	
	x_range = x_stop-x_start;
	for(j=0; j<8; j++)
		if (x_range < tics_range[j]) {
			for(i=0; i<=5; i++) {
				tics[i] = tics_points[j]*((int)(x_start/tics_points[j])+i);
				if((width-left_shift)/(x_stop-x_start)*(tics[i]-x_start)>width-left_shift) break;
				sprintf(coord[i], "%d", tics[i]);
				cairo_move_to(cr, (double)(width-left_shift)/(x_stop-x_start)*(tics[i]-x_start), height-bot_shift+5.0);
				cairo_line_to(cr, (double)(width-left_shift)/(x_stop-x_start)*(tics[i]-x_start), height-bot_shift-5.0);
				cairo_stroke(cr);
				cairo_move_to(cr, (double)(width-left_shift)/(x_stop-x_start)*(tics[i]-x_start), height-20.0);
				cairo_show_text(cr, coord[i]);
			}
			j = 8;
		}
	
	printf("max_R = %.2f\n", max_R);
	
	cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
	cairo_set_line_width (cr, 1.0);
	cairo_move_to(cr, (double)(width-left_shift-5.0), height-bot_shift - 0.5*(height-bot_shift));
	cairo_line_to(cr, (double)(width-left_shift+5.0), height-bot_shift - 0.5*(height-bot_shift));
	cairo_stroke(cr);
	cairo_move_to(cr, (double)(width-left_shift+10.0), height-bot_shift - 0.5*(height-bot_shift)+5.0);
	cairo_show_text(cr, g_strdup_printf("%.2f", 0.0));
	
	
	if(max_R<0.2) tics_y_step = 0.05;
	else tics_y_step = 0.1;
	while( (tics_y[0] <= max_R-0.1) ) {
		tics_y[0] += tics_y_step;
		tics_y[1] = -tics_y[0];
		for(i=0; i<2; i++) {
			cairo_move_to(cr, (double)(width-left_shift-5.0), height-bot_shift - 0.5*(height-bot_shift)*(1.0 + tics_y[i]/max_R));
			cairo_line_to(cr, (double)(width-left_shift+5.0), height-bot_shift - 0.5*(height-bot_shift)*(1.0 + tics_y[i]/max_R));
			cairo_stroke(cr);
			cairo_move_to(cr, (double)(width-left_shift+10.0), height-bot_shift - 0.5*(height-bot_shift)*(1.0 + tics_y[i]/max_R)+5.0);
			cairo_show_text(cr, g_strdup_printf("%.2f", tics_y[i]));
		}
	}
	
	for(j=x_start; j<=x_stop-1; j+=1) {
		cairo_mask_surface(cr, pt_surface(), (double)(width-left_shift)/(x_stop-x_start)*(j-x_start), height-bot_shift - 0.5*(height-bot_shift)*(1.0 + ((double *) R)[j]/max_R)-3.0);
		//printf("R[%d] = %e\n", j, ( (double *) R)[j]);
	}
	cairo_stroke(cr);
	
	cairo_set_line_width (cr, 0.5);
	cairo_set_source_rgba (cr, 0.0, 1.0, 0.0, 1.0);
	cairo_move_to(cr, 0.0, height-bot_shift);
	for(j=x_start; j<=x_stop-1; j+=1) {
		cairo_arc(cr, (double)(width-left_shift)/(x_stop-x_start)*(j-x_start)+3.0, height-bot_shift - 0.5*(height-bot_shift)*(1.0 + ((double *) R)[j]/max_R), 0.05, 0, 2*M_PI);
	}
	cairo_stroke(cr);
	
	cairo_set_line_width (cr, 1.0);
	cairo_set_source_rgba (cr, 1.0, 0.0, 0.0, 1.0);
	cairo_move_to(cr, (double)(width-left_shift)/(x_stop-x_start)*(zero_R/COMP-x_start)+3.0, height-bot_shift);
	cairo_line_to(cr, (double)(width-left_shift)/(x_stop-x_start)*(zero_R/COMP-x_start)+3.0, 0.0);
	cairo_stroke(cr);
}

void plot_aniz(GtkWidget *widget, gpointer data)
{	
	GtkWidget *window;
	GtkWidget *notebook, *vbox_aniz, *vbox_X, *hbox_comp, *hbox_Xgraph;
	GtkWidget *label_aniz, *da_aniz, *label_X, *da_X;
	int i;
	char s[60];
	
	double res[18], *y[12], *F[12], *compX90, *compX180, *R, *dRcomp;
	struct coincidence *X;
	
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	sprintf(s, "Anisotropy - %s", folder2readspk);
    gtk_window_set_title (GTK_WINDOW (window), s);
    gtk_widget_set_size_request (window, 640, 480);
    
    notebook = gtk_notebook_new();
    gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), GTK_POS_TOP);
    gtk_container_add (GTK_CONTAINER (window), notebook);
    
    vbox_X = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
    
    for(i=0; i<=12-1; i++) {
		y[i] = g_new0(double, MAX_CH);
		F[i] = g_new0(double, MAX_CH);
	}
	compX90 = g_new0(double, MAX_CH/COMP);
	compX180 = g_new0(double, MAX_CH/COMP);
	R = g_new0(double, MAX_CH/COMP);
	dRcomp = g_new0(double, MAX_CH/COMP);
	
    anisotropy(res, y, F, compX90, compX180, R, dRcomp);
    
/*    zero_R = 0.0;
    for(i=0; i<6; i++) {
		zero_R += res[i];
		printf("res[%d] = %.2f\n", i, res[i]);
	}
	zero_R = zero_R/6.0;*/
	zero_R = res[0];
	printf("zero level = %.2f\n", zero_R);
    
	coincidence[0] = compX90;
	coincidence[1] = compX180;
	aniz = R;
    
    label_X = gtk_label_new("X90 and X180");
    da_X = gtk_drawing_area_new();
    g_signal_connect(G_OBJECT(da_X), "draw", 
			G_CALLBACK(draw_X), NULL);
	gtk_widget_add_events(da_X, GDK_BUTTON_PRESS_MASK);
	g_signal_connect(da_X, "button-press-event", 
      G_CALLBACK(clicked_in_da_X), (gpointer) window);
	
    gtk_box_pack_start (GTK_BOX (vbox_X), da_X, TRUE, TRUE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK (notebook), vbox_X, label_X);
    
    vbox_aniz = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    label_aniz = gtk_label_new("Anisotropy");
    da_aniz = gtk_drawing_area_new();
    g_signal_connect(G_OBJECT(da_aniz), "draw", 
			G_CALLBACK(draw_anizotropy), (gpointer)R);
	gtk_widget_add_events(da_aniz, GDK_BUTTON_PRESS_MASK);
	g_signal_connect(da_aniz, "button-press-event", 
			G_CALLBACK(clicked_in_anizotropy), (gpointer) window);
    
    gtk_box_pack_start (GTK_BOX (vbox_aniz), da_aniz, TRUE, TRUE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK (notebook), vbox_aniz, label_aniz);
    
    gtk_widget_show_all(window);
}

void choose_aniz_comp(GtkWidget *widget, gpointer window)
{
	GtkWidget *dialog, *content_area;
  	GtkWidget *label_comp, *entry_comp;
  	GtkWidget *hbox_comp;
  	GtkWidget *radio_button_In, *radio_button_Ti;
  	GtkWidget *hbox_nucl;
	GtkWidget *vbox;

   /* Create the widgets */
   dialog = gtk_dialog_new_with_buttons ("COMP for anisotropy and X90 and X180",
                                         window,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         NULL);
	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    
	entry_comp = gtk_entry_new();
	gtk_entry_set_text( GTK_ENTRY(entry_comp), g_strdup_printf("%d", COMP) );
	gtk_entry_set_max_length(GTK_ENTRY(entry_comp), 2);
	gtk_entry_set_width_chars(GTK_ENTRY(entry_comp), 8);

	hbox_comp = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start (GTK_BOX (hbox_comp), gtk_label_new("Choose compression level: "), FALSE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (hbox_comp), entry_comp, FALSE, TRUE, 0);
   
    radio_button_In = gtk_radio_button_new_with_label( NULL, "111In" );
	radio_button_Ti = gtk_radio_button_new_with_label_from_widget( GTK_RADIO_BUTTON(radio_button_In), "44Ti" );
   
	hbox_nucl = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start (GTK_BOX (hbox_nucl), gtk_label_new("Choose nucleus: "), FALSE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (hbox_nucl), radio_button_In, FALSE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (hbox_nucl), radio_button_Ti, FALSE, TRUE, 0);
   
	if(nucleus == 1)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button_Ti), TRUE);
	else
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button_In), TRUE);
   
   	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
	gtk_box_pack_start (GTK_BOX (vbox), hbox_comp, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox_nucl, FALSE, TRUE, 0);
   
	gtk_container_add (GTK_CONTAINER (content_area), vbox);
	gtk_widget_show_all (dialog);
	
	gint result = gtk_dialog_run (GTK_DIALOG (dialog));
	switch (result)
	{
    case GTK_RESPONSE_ACCEPT:
		COMP = (int)atof( gtk_entry_get_text(GTK_ENTRY(entry_comp)) );
       
		if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(radio_button_Ti) ) ) {
			nucleus = 1;
			g_print( "Ti is chosen\n" );
		}
		else {
			nucleus = 0;
			g_print( "In is chosen\n" );
		}
		
		break;
	default:
       break;
	}
	gtk_widget_destroy (dialog);
}
