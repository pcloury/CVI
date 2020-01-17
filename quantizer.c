#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "quantizer.h"

// tables de dequantification (32 tables de resolution 16 bits)
// les index ont un offset de 1<<15, i.e. l'index 32768 represente le zero
short int dequant_table[32][65536];


// fonction de quantification
// intervalle d'entree : [-1.0, 1.0]
// intervalle de sortie : max [-32767, 32767], min [-1, 1]
short int quantif( double val, int iband )
{

int pas_iband[32];
pas_iband[0]=32760;
//pas_iband[1]=16384;
for  (int i=1; i<32 ; i++){
	pas_iband[i] = 32760-6610*log2(i);
}
short int retval;
if	( val == 0.0 )
	return 0;
if	( val < 0.0 )			// symetrie assuree
	return( -quantif( -val, iband));
if	( val > 1.0 )
	val = 1.0;
// ici l'intervalle d'entr�e est r�duit � ]0.0, 1.0]

float pas = (log2((val + 1))) * pas_iband[iband] + 5;
//double pas = (1 - exp(-(val * 4))) * pas_iband[iband] + 5;


retval = (short int)round(val * pas);	// quantifieur HiFi 16 bits

// quantificateur absur0]=32767de pour couper une bande
//	retval = (short int)(val > 0.999)?(1):(0);
return retval;

}

// fonction de dequantification
double dequantif( short int code, int iband )
{
int i;
i = code + (1<<15);	// offset 16 bits signe --> index
i &= 0xFFFF;		// securite
return ((double)dequant_table[iband][i]) / 32767.0;
}

// fonction de preparation des tables de dequantification
void prep_dequant()
{
int iband, i;
short int code, oldcode;
double val, first, last, middle;
double inc = 1.0 / 32768.0;

for	( iband = 0; iband < 32; ++iband )
	{
	val = -1.0; oldcode = -(1<<15);
	while	( val <= 1.0 )
		{	// on cherche l'intervalle [ first : last ] ou le code est constant
		first = val;
		code = quantif( val, iband );
		// checks de securite
		if	( code < oldcode )
			{
			printf("ERREUR : fonction de quantification non croissante\n");
			exit(1);
			}
		oldcode = code;
		do	{
			last = val;
			val += inc;
			} while ( ( code == quantif( val, iband ) ) && ( val < 1.0 ) );
		middle = ( first + last ) / 2;
		i = code + (1<<15);	// offset 16 bits signe --> index
		i &= 0xFFFF;		// securite
		dequant_table[iband][i] = (short int)round( middle * 32767.0 );
		}
	}
// verification de la dequantif
double reval, err, minerr, maxerr;
for	( iband = 0; iband < 32; ++iband )
	{
	val = -1.0; maxerr = 0.0; minerr = 1.0;
	while	( val < 1.0 )
		{
		code = quantif( val, iband );
		reval = dequantif( code, iband );
		err = fabs( reval - val );
		if	( err > maxerr )
			maxerr = err;
		if	( err < minerr )
			minerr = err;
		val += inc;
		}
	printf("subband %2d : %g < err < %g (vs %g)\n", iband, minerr, maxerr, inc );
	}
}

// production d'un fichier javascript pour graphiques 
int plot_js( int iband )
{
FILE * fil;
double x;

fil = fopen("PLOT/plot_a44.js", "w" );

if	( fil == NULL )
	return 1;

fprintf( fil, "var plot_q = [\n" );
for	( x = -1.0; x <= 1.0; x += 0.001 )
	{
	fprintf( fil, "[%g,%d],", x, quantif( x, iband ) );
	}
fprintf( fil, "];\n");
fprintf( fil, "var plot_d = [\n" );
for	( x = -1.0; x <= 1.0; x += 0.001 )
	{
	fprintf( fil, "[%g,%g],", x, dequantif( quantif( x, iband ), iband ) );
	}
fprintf( fil, "];\n");

fclose( fil );
return 0;
}
