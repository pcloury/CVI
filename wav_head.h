// plus d'explications dans wav_head.c
typedef struct {
int hand;		// file handle	
unsigned int type;	// 1 = signed short int, 3 = float
unsigned int chan;
unsigned int freq;	// frequence d'echantillonnage
unsigned int bpsec;	// bytes par seconde
unsigned int block;	// bytes par frame
unsigned int resol;	// bits par sample
unsigned int wavsize;	// longueur en echantillons (par canal)
			// = nombre de frames
} wavpars;

#ifndef O_BINARY
#define O_BINARY 0
#endif

void WAVreadHeader( wavpars *s );

void WAVwriteHeader( wavpars *d );

void gasp( const char *fmt, ... );  /* traitement erreur fatale */
