// extrait du code de reference WAV2WAVc.zip
/* WAV est un cas de fichier RIFF.
 * le fichier RIFF commence par "RIFF" (4 caracteres) suivi de la longueur
 * du reste du fichier en long (4 bytes ordre Intel)
 * puis on trouve "WAVE" suivi de chucks.
 * chaque chuck commence par 4 caracteres (minuscules) suivis de la longueur
 * du reste du chuck en long (4 bytes ordre Intel)
 * le chuck "fmt " contient les parametres, sa longueur n'est pas tres fixe,
 * il peut y avoir un chuck "fact" contenant le nombre de samples
 * le chuck "data" contient les samples,
 * tout autre doit etre ignore.
 * ATTENTION : en float 32 : valeurs samples sur [-1.0 , 1.0]

 header mini :
	     _
 "RIFF"    4  |
 filesize  4  } == 12 bytes de pre-header
 "WAVE"    4 _|
 "fmt "    4     le chuck fmt coute  au moins 24 bytes en tout
 chucksize 4 _   == 16 bytes utile
 type      2  |				1 pour sl 16		3 pour float
 chan      2  |	
 freq      4  } == 16 bytes		(par exemple 44100 = 0xAC44)
 bpsec     4  |		bytes/s		block * freq	
 block     2  |		bytes/frame	4			8
 resol     2 _|		bits/sample	16			32
 "data"    4
 chucksize 4    == 8 bytes de post-header
 --------------
          44 au total

 RIFF filesize = real filesize - 8
 chucksize     = real filesize - 44
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "wav_head.h"


unsigned int readlong( unsigned char *buf )
{
unsigned int l;
l  = (unsigned int)buf[3] << 24 ;
l += (unsigned int)buf[2] << 16 ;
l += (unsigned int)buf[1] << 8 ;
l += buf[0];
return(l);
}
unsigned int readshort( unsigned char *buf )
{
unsigned int s;
s = buf[0] + ( buf[1] << 8 );
return(s);
}

#define QBUF 4096

void WAVreadHeader( wavpars *s )
{
unsigned char buf[QBUF]; unsigned int filesize, chucksize, factsize;
read( s->hand, buf, 4 );
if ( strncmp( (char *)buf, "RIFF", 4 ) != 0 ) gasp("manque en-tete RIFF");
read( s->hand, buf, 4 ); filesize = readlong( buf );
read( s->hand, buf, 4 );
if ( strncmp( (char *)buf, "WAVE", 4 ) != 0 ) gasp("manque en-tete WAVE");
s->type = 0x10000;
factsize = 0;
chucksize = 0;
while	( read( s->hand, buf, 8 ) == 8 )	// boucle des chucks preliminaires
	{
	chucksize = readlong( buf + 4 );
	if	( strncmp( (char *)buf, "data", 4 ) == 0 )
		break;	// c'est le bon chuck, on le lira plus tard...
	buf[4] = 0;
	printf("chuck %s : %d bytes\n", buf, chucksize );
	if	( chucksize > QBUF ) gasp("chuck trop gros");
	if	( strncmp( (char *)buf, "fmt ", 4 ) == 0 )		// chuck fmt
		{
		if ( chucksize > QBUF ) gasp("chuck fmt trop gros");
		read( s->hand, buf, (int)chucksize );
		s->type = readshort( buf );
		if	( ( s->type == 1 ) || ( s->type == 3 ) || ( s->type == 0xFFFE ) )
			{
			s->chan = readshort( buf + 2 );
			s->freq = readlong( buf + 4 );
			s->bpsec = readlong( buf + 8 );
			s->block = readshort( buf + 12 );
			s->resol = readshort( buf + 14 );
			s->wavsize = 0;
			if	( s->type == 0xFFFE )		// WAVE_FORMAT_EXTENSIBLE
				{
				printf("WAVE_FORMAT_EXTENSIBLE\n");
				if	( s->chan > 2 )
					gasp("plus que 2 canaux : non supporte\n");
				int extsize = readshort( buf + 16 );
				if	( extsize == 22 )
					{
					int validbits = readshort( buf + 18 );
					if	( validbits != s->resol )
						gasp("validbits = %d vs %d\n", validbits, s->resol );
					int ext_type = readshort( buf + 20 );
					if	( ( ext_type != 1 ) && ( ext_type != 3 ) )
						gasp("ext format %d non supporte", ext_type );
					s->type = ext_type;
					}
				else	gasp("fmt extension toot short %d\n", extsize );
				}
			}
		else	gasp("format %d inconnu", s->type );
		}
	else if	( strncmp( (char *)buf, "fact", 4 ) == 0 )		// chuck fact
		{
		read( s->hand, buf, chucksize );
		factsize = readlong( buf );
		}
	else	read( s->hand, buf, chucksize );				// autre chuck
	}
if	( s->type == 0x10000 ) gasp("pas de chuck fmt");
if	( strncmp( (char *)buf, "data", 4 ) != 0 ) gasp("pas de chuck data");
					// on est bien arrive dans les data !
s->wavsize = chucksize / ( s->chan * ((s->resol)>>3) );

printf("%u canaux, %u Hz, %u bytes/s, %u bits\n",
       s->chan, s->freq, s->bpsec, s->resol );
printf("%u echantillons selon data chuck\n", s->wavsize );
if	( factsize != 0 )
	{
	printf("%u echantillons selon fact chuck\n", factsize );
	if ( s->wavsize != factsize ) gasp("longueurs incoherentes");
	}
printf("fichier %u bytes, chuck data %u bytes\n", filesize, chucksize );
	{
	double durs, dmn, ds; int mn;
	durs = s->wavsize;  durs /= s->freq;
	dmn = durs / 60.0;  mn = (int)dmn; ds = durs - 60.0 * mn;
	printf("duree %.3f s soit %d mn %.3f s\n", durs, mn, ds );
	}
}

void writelong( unsigned char *buf, unsigned int l )
{
buf[0] = (unsigned char)l; l >>= 8;
buf[1] = (unsigned char)l; l >>= 8;
buf[2] = (unsigned char)l; l >>= 8;
buf[3] = (unsigned char)l;
}
void writeshort( unsigned char *buf, unsigned int s )
{
buf[0] = (unsigned char)s; s >>= 8;
buf[1] = (unsigned char)s;
}
void gulp()
{ gasp("erreur ecriture fichier");
}

void WAVwriteHeader( wavpars *d )
{
unsigned char buf[16]; long filesize, chucksize;

d->block = d->chan * ((d->resol)>>3);
d->bpsec = d->freq * d->block;
chucksize = d->wavsize * d->block;
filesize = chucksize + 36;

if ( write( d->hand, "RIFF", 4 ) != 4 ) gulp();
writelong( buf, filesize );
if ( write( d->hand, buf, 4 ) != 4 ) gulp();

if ( write( d->hand, "WAVE", 4 ) != 4 ) gulp();
if ( write( d->hand, "fmt ", 4 ) != 4 ) gulp();
writelong( buf, 16 );
if ( write( d->hand, buf, 4 ) != 4 ) gulp();

writeshort( buf,      d->type  );
writeshort( buf + 2,  d->chan  );
writelong(  buf + 4,  d->freq  );
writelong(  buf + 8,  d->bpsec );
writeshort( buf + 12, d->block );
writeshort( buf + 14, d->resol );
if ( write( d->hand, buf, 16 ) != 16 ) gulp();

if ( write( d->hand, "data", 4 ) != 4 ) gulp();
writelong( buf, chucksize );
if ( write( d->hand, buf, 4 ) != 4 ) gulp();
}

/* --------------------------------------- traitement erreur fatale */

void gasp( const char *fmt, ... )
{
  char lbuf[2048];
  va_list  argptr;
  va_start( argptr, fmt );
  vsprintf( lbuf, fmt, argptr );
  va_end( argptr );
  printf("STOP : %s\n", lbuf ); exit(1);
}
