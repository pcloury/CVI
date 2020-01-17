/* encoder */
void window_subband(	short *buffer, double *z, int k );

void filter_subband( double *z, double *s );

/* decoder */

int SubBandSynthesis ( double * bandPtr, int channel, short * samples );
