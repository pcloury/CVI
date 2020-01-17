/* API de kit_01.c (extrait du code de reference ISO-MPEG)

Les fonctions d'encodage et de decodage ne sont pas presentees dans le meme style...
on le sait - c'est historique sans doute ;-)

====== fonctions d'encodage (filtrage en 32 sous-bandes) =============================

void window_subband( short *buffer, double *z, int k );

- cette fonction prend 32 echantillons de 16 bits dans buffer[]
- elle les introduit dans un buffer circulaire interne de 512 echantillons (par canal)
- elle applique un fenetrage (de Hanning) sur ces 512 echantillons
- elle restitue le resultat dans z (valeurs normalisees sur l'intervalle (-1.0, +1.0) )
On doit lui indiquer le canal k (0 ou 1) a cause du buffer interne.

void filter_subband( double *z, double *s );

- cette fonction prend 512 echantillons resultant du fenetrage dans z
- elle restitue 32 echantillons (chip) a raison de 1 par sous-bande

Ces 2 fonctions s'utilisent toujours en sequence comme suit :
	short int s16_chip_buf[32];	// donnees WAV 16 bits
	static double zbuf[512];	// buffer pour filtre polyphase
	double d32_chip_buf[32];	// echantillons filtres par sous bandes

	window_subband( s16_chip_buf, zbuf, k );
	filter_subband( zbuf, d32_chip_buf );

====== fonction de decodage (reconstitution du signal a partir des 32 sous-bandes) ===

int SubBandSynthesis ( double * bandPtr, int channel, short * samples );

- cette fonction prend 32 echantillons normalises dans bandPtr[]
  a raison de 1 par sous-bande
- elle restitue 32 echantilons de 16 bits dans samples[]
- on doit lui indiquer le canal (0 ou 1) dans channel a cause du buffer interne.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define HAN_SIZE 512
#define SBLIMIT 32
#define SCALE  32768.0

#define PI   3.14159265358979
#define PI4  PI/4
#define PI64 PI/64

// fonctions de window_data.c
void read_ana_window( double *C );
void read_syn_window( double *D );

void * mem_alloc( long unsigned int block, char *item )
{
void	  *ptr;
ptr = (void *) malloc(block);
if   (ptr != NULL)
     {
     memset(ptr, 0, block);
     }
else {
     printf("Unable to allocate %s\n", item);
     exit(0);
     }
return(ptr);
}

void create_ana_filter(double (*filter)[64])
{
   register int i,k;
 
   for (i = 0; i < 32; i++)
      for (k = 0; k < 64; k++) {
          if ((filter[i][k] = 1e9*cos((double)((2*i+1)*(16-k)*PI64))) >= 0)
             modf(filter[i][k]+0.5, &filter[i][k]);
          else
             modf(filter[i][k]-0.5, &filter[i][k]);
          filter[i][k] *= 1e-9;
   }
}

/************************************************************************
 JLN --> lecture fichier enwindow remplacee par code compile window_data.c
 ************************************************************************/
/*
void read_ana_window(double *ana_win)
{
    int i,j[4];
    FILE *fp;
    double f[4];
    char t[150];
 
    if (!(fp = fopen("enwindow", "r") ) ) {
       printf("Please check analysis window table 'enwindow'\n");
       exit(1);
    }
    for (i=0;i<512;i+=4) {
       fgets(t, 80, fp); 
       sscanf(t,"C[%d] = %lf C[%d] = %lf C[%d] = %lf C[%d] = %lf\n",
              j, f,j+1,f+1,j+2,f+2,j+3,f+3);
       if (i==j[0]) {
          ana_win[i] = f[0];
          ana_win[i+1] = f[1];
          ana_win[i+2] = f[2];
          ana_win[i+3] = f[3];
       }
       else {
          printf("Check index in analysis window table\n");
          exit(1);
       }
       fgets(t,80,fp); 
    }
    fclose(fp);
}
*/
/************************************************************************
 JLN --> window_subband() prend maintenant des shorts dans 1 buffer fixe
 ************************************************************************/
 
void
window_subband(
	short *buffer,
	double *z,
	int k
) {
typedef double XX[2][HAN_SIZE];
static XX *x;
int i, j;
static int off[2]= {0,0};
static char init = 0;
static double *c;

if (!init)
   {
   c = (double *) mem_alloc(sizeof(double) * HAN_SIZE, "window");
   read_ana_window(c);
   x = (XX *) mem_alloc(sizeof(XX),"x");

   for (i = 0; i < 2; i++)
       for (j = 0; j < HAN_SIZE; j++)
           (*x)[i][j] = 0;
   init = 1;
   }

   for (i=0;i<32;i++)
       (*x)[k][31-i+off[k]] = ( (double) buffer[i] ) / SCALE;
   for (i=0;i<HAN_SIZE;i++) 
       z[i] = (*x)[k][(i+off[k])&(HAN_SIZE-1)] * c[i];

   off[k] += 480; /*offset is modulo (HAN_SIZE-1) faux HAN_SIZE dixit JLN */
   off[k] &= HAN_SIZE-1;

}
 
void filter_subband(double *z, double *s)
                                
{
   double y[64];
   int i, k;
static char init = 0;
   typedef double MM[SBLIMIT][64];
static MM  *m;
   double sum1;
  
   if (!init) {
       m = (MM  *) mem_alloc(sizeof(MM), "filter");
       create_ana_filter(*m);
       init = 1;
   }
   /* Window */
   for (i=0; i<64; i++)
   {
      for (k=0, sum1 = 0.0; k<8; k++)
         sum1 += z[i+64*k];
      y[i] = sum1;
   }

   /* Filter */
   for (i=0;i<SBLIMIT;i++)
   {
       for (k=0, sum1=0.0 ;k<64;k++)
          sum1 += (*m)[i][k] * y[k];
       s[i] = sum1;
   }
}

/* The following are the subband synthesis routines. They apply
 * to both layer I and layer II stereo or mono.
 * create in synthesis filter */

void create_syn_filter(double filter[64][SBLIMIT])
{
   register int i,k;

   for (i=0; i<64; i++)
      for (k=0; k<32; k++) {
		 if ((filter[i][k] = 1e9*cos((double)((PI64*i+PI4)*(2*k+1)))) >= 0)
			modf(filter[i][k]+0.5, &filter[i][k]);
         else
            modf(filter[i][k]-0.5, &filter[i][k]);
		 filter[i][k] *= 1e-9;
	  }
}


/* read in synthesis window */
/************************************************************************
 JLN --> lecture fichier dewindow remplacee par code compile window_data.c
 ************************************************************************/
/*
void read_syn_window(double window[HAN_SIZE])
{
   int i,j[4];
   FILE *fp;
   double f[4];
   char t[150];

   if (!(fp = fopen("dewindow", "r") )) {
	  printf("Please check synthesis window table 'dewindow'\n");
	  exit(1);
   }
   for (i=0;i<512;i+=4) {
		fgets(t,150, fp);
	  sscanf(t,"D[%d] = %lf D[%d] = %lf D[%d] = %lf D[%d] = %lf\n",
			 j, f,j+1,f+1,j+2,f+2,j+3,f+3);
	  if (i==j[0]) {
		 window[i] = f[0];
		 window[i+1] = f[1];
		 window[i+2] = f[2];
		 window[i+3] = f[3];
	  }
	  else {
		 printf("Check index in synthesis window table\n");
			exit(1);
	  }
	  fgets(t,80,fp);
   }
   fclose(fp);
}
*/

int SubBandSynthesis ( double * bandPtr, int channel, short * samples )
{
    register int i,j,k;
    register double *bufOffsetPtr, sum;
    static int init = 1;
    typedef double NN[64][32];
    static NN  *filter;
    typedef double BB[2][2*HAN_SIZE];
    static BB  *buf;
    static int bufOffset[2] = {64,64};
    static double  *window;
    int clip = 0;               /* count & return how many samples clipped */

    if (init) {
        buf = (BB  *) mem_alloc(sizeof(BB),"BB");
        filter = (NN  *) mem_alloc(sizeof(NN), "NN");
        create_syn_filter(*filter);
        window = (double  *) mem_alloc(sizeof(double) * HAN_SIZE, "WIN");
        read_syn_window(window);
        init = 0;
    }
    bufOffset[channel] = (bufOffset[channel] - 64) & 0x3ff;
    bufOffsetPtr = &((*buf)[channel][bufOffset[channel]]);

    for (i=0; i<64; i++) {
        sum = 0;
        for (k=0; k<32; k++)
            sum += bandPtr[k] * (*filter)[i][k];
        bufOffsetPtr[i] = sum;
    }
    /*  S(i,j) = D(j+32i) * U(j+32i+((i+1)>>1)*64)  */
    /*  samples(i,j) = MWindow(j+32i) * bufPtr(j+32i+((i+1)>>1)*64)  */
    for (j=0; j<32; j++) {
        sum = 0;
        for (i=0; i<16; i++) {
            k = j + (i<<5);
            sum += window[k] * (*buf) [channel] [( (k + ( ((i+1)>>1) <<6) ) +
                                                  bufOffset[channel]) & 0x3ff];
        }

      {
      long foo = (long) ( (sum > 0.0) ? sum * SCALE + 0.5 : sum * SCALE - 0.5 );

      if (foo >= (long) SCALE)      {samples[j] = (short) (SCALE-1); ++clip;}
      else if (foo < (long) -SCALE) {samples[j] = (short) -SCALE;  ++clip;}
      else                           samples[j] = (short) foo;
      }
    }
    return(clip);
}
