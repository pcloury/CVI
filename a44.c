/* programme pour experimenter le decoupage en bandes sur fichier WAVE */
/*   */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <math.h>

#include "kit_01w.h"
#include "wav_head.h"
#include "quantizer.h"

// le chip est la plus petite quantite de son qu'on puisse traiter
// 1 chip = 32 samples audio = 1 sample par sous-bande
// pour des raisons d'efficacite, le traitement se fera par trames
// de CHIPSperFRAME chips chacune
#define CHIPSperFRAME 64

// taille de buffer pour une trame en 16 bits stereo
#define BYTESperFRAME (32*4*CHIPSperFRAME)

// fonction pour l'encodage d'une trame
// sbuf (16 bits stereo) --> dbuf(32 sous bandes quantifiees) 
void encode_frame( short int * sbuf, short int * dbuf )
{
short int srcL[32];		// donnees audio 16 bits LEFT
short int srcR[32];		// donnees audio 16 bits RIGHT
static double zbuf[512];	// buffer pour filtre polyphase
double sbandL[32];		// echantillons filtres par sous bandes
double sbandR[32];		// echantillons filtres par sous bandes

int ichip;			// index du chip dans la trame
int  sind;			// index courant dans sbuf[]
int  dind;			// index courant dans dbuf[]
int  isam;			// index de samples dans un chip d'un canal (mono)
int iband;			// index de sous-bande (0 a 32)

// boucle des chips
for	( ichip = 0; ichip < CHIPSperFRAME; ichip++ )
	{
	// extraire les canaux L et R qui sont entrelaces
	sind = ichip * 32 * 2;
	for	( isam = 0; isam < 32; ++isam )
		{
		srcL[isam] = sbuf[sind++];
		srcR[isam] = sbuf[sind++];
		}
	// filtrer en sous-bandes avec decimation (code ISO)
	window_subband( srcL, zbuf, 0 );		// left
	filter_subband( zbuf, sbandL );
	window_subband( srcR, zbuf, 1 );		// right
	filter_subband( zbuf, sbandR );
	// traiter les sous-bandes : quantifier et ordonner par bandes
	// dans dbuf on place la bande 0 pour toute la trame suivie de la bande 1 etc...
	// dans chaque bande les echantillons L et R sont entrelaces
	dind = ichip * 2;
	for	( iband = 0; iband < 32; ++iband )
		{
		dbuf[dind]   = quantif( sbandL[iband], iband );
		dbuf[dind+1] = quantif( sbandR[iband], iband );
		dind += ( CHIPSperFRAME * 2 );
		}
	}
}

// fonction pour le decodage d'une trame
// sbuf (32 sous bandes quantifiees) --> dbuf (16 bits stereo)
void decode_frame( short int * sbuf, short int * dbuf )
{
short int dstL[32];		// donnees audio 16 bits LEFT
short int dstR[32];		// donnees audio 16 bits RIGHT
double sbandL[32];		// echantillons filtres par sous bandes
double sbandR[32];		// echantillons filtres par sous bandes

int ichip;			// index du chip dans la trame
int  sind;			// index courant dans sbuf[]
int  dind;			// index courant dans dbuf[]
int  isam;			// index de samples dans un chip d'un canal (mono)
int iband;			// index de sous-bande (0 a 32)

// boucle des chips
for	( ichip = 0; ichip < CHIPSperFRAME; ichip++ )
	{
	// extraire les donnees d'un chip et les dequantifier
	// dans dbuf on trouve la bande 0 pour toute la trame suivie de la bande 1 etc...
	// dans chaque bande les echantillons L et R sont entrelaces
	sind = ichip * 2;
	for	( iband = 0; iband < 32; ++iband )
		{
		sbandL[iband] = dequantif( sbuf[sind],   iband );
		sbandR[iband] = dequantif( sbuf[sind+1], iband );
		sind += ( CHIPSperFRAME * 2 );
		}
	// reconstituer le signal temporel
	SubBandSynthesis ( sbandL, 0, dstL );
	SubBandSynthesis ( sbandR, 1, dstR );
	// entrelacer les canaux L et R
	dind = ichip * 32 * 2;
	for	( isam = 0; isam < 32; ++isam )
		{
		dbuf[dind++] = dstL[isam];
		dbuf[dind++] = dstR[isam];
		}
	}
}
	
// fonction pour encoder le contenu d'un fichier wav
// le fichier source est suppose etre ouvert et le traitement du header deja fait
// avec les parametres dans la structure s
// le fichier destination est suppose ouvert, handle dhand
void encode_wav( wavpars *s, int dhand )
{
short int sbuf[BYTESperFRAME];	// source
short int dbuf[BYTESperFRAME];	// destination
int qbytes;			// nombre de bytes restant a traiter 
int rdcont, rdcont2, wrcont;	// bytes par operation de lecture ou ecriture

qbytes = s->wavsize * s->chan * ((s->resol)/8);

// boucle de traitement des trames
while	( qbytes > 0L )		// tant qu'il en reste
	{
	if	( qbytes < (long)BYTESperFRAME )
		rdcont = (int)qbytes;
	else	rdcont = BYTESperFRAME;
	// lire une trame dans le fichier source
	rdcont2 = read( s->hand, sbuf, rdcont );
	if	( rdcont2 < rdcont )
		gasp("fin inattendue fichier source");
	// completer avec des zeros la derniere trame
	if	( rdcont < BYTESperFRAME )
		{
		int sind;
		for	( sind = rdcont; sind < BYTESperFRAME; sind+=2 )
			sbuf[sind] = 0;
		}
	// encoder la trame
	encode_frame( sbuf, dbuf );
	// ecrire la trame dans le fichier destination
	wrcont = write( dhand, dbuf, BYTESperFRAME );
	if	( wrcont < BYTESperFRAME )
		gasp("erreur ecriture disque (plein?)");
	qbytes -= rdcont;
	plot_js( 1 );
	}
}

// fonction pour decoder l'audio a44 et l'ecrire dans un fichier wav
// le fichier source est suppose ouvert, handle shand
// le fichier destination est suppose etre ouvert et la creation du header deja faite
// avec les parametres dans la structure d
void decode_wav( wavpars *d, int shand )
{
short int sbuf[BYTESperFRAME];	// source
short int dbuf[BYTESperFRAME];	// destination
int qbytes;			// nombre de bytes restant a traiter 
int rdcont, wrcont;		// quantites traitees en bytes

qbytes = d->wavsize * d->chan * ((d->resol)/8);

// boucle de traitement des trames
while	( qbytes > 0L )		// tant qu'il en reste
	{
	rdcont = read( shand, sbuf, BYTESperFRAME );
	if	( rdcont < BYTESperFRAME )
		gasp("fin inattendue fichier source");
	// decoder la trame
	decode_frame( sbuf, dbuf );
	// ecrire la trame dans le fichier destination
	wrcont = write( d->hand, dbuf, BYTESperFRAME );
	if	( wrcont < BYTESperFRAME )
		gasp("erreur ecriture disque (plein?)");
	qbytes -= BYTESperFRAME;
	}
}

void usage()
{
fprintf( stderr, "\nUsage : a44 source.wav dest.a44 c\n\n");
fprintf( stderr, "ou    : a44 source.a44 dest.wav d\n\n");
fprintf( stderr, "\n");
exit(1);
}

int main( int argc, char ** argv )
{
int shand, dhand;
char snam[256];
char dnam[256];
char option;

prep_dequant();
if ( argc != 4 ) usage();

sprintf( snam, "%s", argv[1] );
sprintf( dnam, "%s", argv[2] );
option = argv[3][0];

printf("ouverture %s en lecture\n", snam );
shand = open( snam, O_RDONLY | O_BINARY );
if ( shand == -1 ) gasp("not found");
printf("ouverture %s en ecriture\n", dnam );
dhand = open( dnam, O_RDWR | O_BINARY | O_CREAT | O_TRUNC, 0666 );

if ( dhand == -1 ) gasp("pb ouverture pour ecrire");

// encodage
if	( option == 'c' )
	{
	wavpars s;
	s.hand = shand;
	WAVreadHeader( &s );
	if (
	   ( s.chan != 2 )   ||
	   ( s.resol != 16 )
	   )
	   gasp("programme seulement pour fichiers stereo 16 bits");
	// printf("block = %d, bpsec = %d, size = %d\n", s.block, s.bpsec, s.wavsize );
	encode_wav( &s, dhand );
	}

if	( option == 'd' )
	{
	// preparer data pour header WAV
	wavpars d;
	struct stat statbuf;
	if	( fstat( shand, &statbuf ) )
		gasp("echec stat sur fichier source");
	d.wavsize = (int)statbuf.st_size;	// taille fichier a42
	d.wavsize /= 4;		// longueur en echantillons (par canal)
	d.type = 1;		// pcm 16 bits
	d.chan = 2;
	d.freq = 44100;    	// frequence d'echantillonnage
	d.bpsec = d.freq * 4;	// bytes par seconde
	d.block = 4;
	d.resol = 16;		// en bits
	d.hand = dhand;

	printf("%d echantillons par canal, duree %gs\n", d.wavsize, (double)d.wavsize / (double)d.freq );
	// action
	WAVwriteHeader( &d );
	decode_wav( &d, shand );
	}

close( dhand );
close( shand );
return 0;
}

