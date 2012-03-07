/* ____________________________________________________________________________
*
*   Filename   : bch.c
*______________________________________________________________________________
*
*   History:
*   24 Apr 2008 : Creation
*____________________________________________________________________________*/

/*_______________________________ Include Files _____________________________*/
#include <common.h>
#include <asm/arch/omap4_hal.h>
/*________________________________ Local Types ______________________________*/

/*______________________________ Local Constants ____________________________*/

#define mm_max  8		/* Dimension of Galois Field */
#define nn_max  256		/* Length of codeword, n = 2**m - 1 */
#define tt_max  2		/* Number of errors that can be corrected */
#define kk_max  256		/* Length of information bit, kk = nn - rr  */
#define rr_max  32		/* Number of parity checks, rr = deg[g(x)] */
#define parallel_max  8		/* Number of parallel encoding/syndrome computations */

/*______________________________ Local Variables ____________________________*/
int i, j;
int gen_roots[nn_max + 1], gen_roots_true[nn_max + 1] ;	// Roots of generator polynomial
int iii, jjj, Temp ;
int bb_temp[rr_max] ;
int loop_count ;
int mask ;	// Register states

int mm, nn, kk, tt, rr;		// BCH code parameters
int nn_shorten, kk_shorten;	// Shortened BCH code
int Parallel ;			// Parallel processing
int p[mm_max + 1], alpha_to[nn_max], index_of[nn_max] ;	// Galois field
int gg[rr_max] ;		// Generator polynomial
int bb[rr_max] ;		// Parity checks
int T_G[rr_max][rr_max], T_G_R[rr_max][rr_max];		// Parallel lookahead table
int T_G_R_Temp[rr_max][rr_max] ;
int data[kk_max], data_p[parallel_max][kk_max];	// Information data and received data
/*______________________________ Local Functions Declarations _______________*/

/*______________________________ Global Functions ___________________________*/

U32 hexStringtoInteger(const char* hexString, U32* result)
{
	int iL;
	int temp;
	int factor;
	int length;

	length = strlen(hexString);
	if ((length == 0) || (length>8))
		return 1;
	*result = 0;
	factor = 1;
	/* Convert the characters to hex number */
	for(iL=length-1; iL>=0 ;iL--)
	{
		if (*(hexString+iL)>=97){
			temp = ( *(hexString+iL) - 97) + 10;
		}else if ( *(hexString+iL) >= 65){
				temp = (*(hexString+iL) - 65) + 10;
		}else{
				temp = *(hexString+iL) - 48;
		}
		*result += (temp * factor);
		factor *= 16;
	}
	return 0;
}

U32 cpfrom_bit_reverse32(U32 value)
{
	U32 value_i;
	U32 value_o;
	U32 itr;

	value_i=value;
	value_o=value_i & 1;

	for(itr=0; itr<31; itr++)
	{
		value_o = value_o<<1;
		value_i = value_i>>1;
		value_o = value_o | (value_i & 1);
	}
	return value_o;
}

U32 cpfrom_byte_reverse32(U32 value)
{
	U32 value_i;
	U32 value_o;
	U32 itr;

	value_i=value;
	value_o=value_i & 0xFF;

	for(itr=0; itr<3; itr++)
	{
		value_o = value_o<<8;
		value_i = value_i>>8;
		value_o = value_o | (value_i & 0xFF);
	}
	return value_o;
}

U32 bch_enc(U8 index, U32 in_v[])
{
	U32 out_v;

	if (index == 1){
		kk_shorten = 32;
		mm = 6;
	}
	else if (index == 4){
		kk_shorten = 128;
		mm = 8;
	}
	else
		return 0xFFFFFFFF;

	tt = 2;
	Parallel = 8;

//	nn = (int)pow(2, mm) - 1 ;
	nn = 1;                                         /*  nn = (int)pow(2, mm) - 1 ;  */
	for (j = 0 ; j < mm ; j++)                      /*  nn = (int)pow(2, mm) - 1 ;  */
		nn = nn * 2;                            /*  nn = (int)pow(2, mm) - 1 ;  */
	nn = nn - 1;                                    /*  nn = (int)pow(2, mm) - 1 ;  */

// INPUT
	for (j = 0 ; j < index ; j++)
	{	for (i = 0 ; i < 32 ; i++)
		{	if (0x1 & (in_v[j] >> i))
				data[32*(j+1)-i-1] = 1 ;
			else
				data[32*(j+1)-i-1] = 0 ;
		}
	}

// generate the Galois Field GF(2**mm)
	// Primitive polynomials
	for (i = 1; i < mm; i++)
		p[i] = 0;
	p[0] = p[mm] = 1;
	if (mm == 6)		p[1] = 1;
	else if (mm == 8)	p[4] = p[5] = p[6] = 1;

	// Galois field implementation with shift registers
	mask = 1 ;
	alpha_to[mm] = 0 ;
	for (i = 0; i < mm; i++)
	{ 	alpha_to[i] = mask ;
		index_of[alpha_to[i]] = i ;
		if (p[i] != 0)
			alpha_to[mm] ^= mask ;

		mask <<= 1 ;
	}

	index_of[alpha_to[mm]] = mm ;
	mask >>= 1 ;
	for (i = mm + 1; i < nn; i++)
	{ 	if (alpha_to[i-1] >= mask)
			alpha_to[i] = alpha_to[mm] ^ ((alpha_to[i-1] ^ mask) << 1) ;
		else alpha_to[i] = alpha_to[i-1] << 1 ;

		index_of[alpha_to[i]] = i ;
	}
	index_of[0] = -1 ;

// Compute the generator polynomial and lookahead matrix for BCH code
	// Initialization of gen_roots
	for (i = 0; i <= nn; i++)
	{	gen_roots_true[i] = 0;
		gen_roots[i] = 0;
	}

	// Cyclotomic cosets of gen_roots
	for (i = 1; i <= 2*tt ; i++)
	{
		for (j = 0; j < mm; j++)
		{
//			Temp = ((int)pow(2, j) * i) % nn;
			Temp = 1;                                       /*  Temp = ((int)pow(2, j) * i);  */
			for(jjj=0; jjj<j; jjj++)                        /*  Temp = ((int)pow(2, j) * i);  */
				Temp = Temp * 2;                        /*  Temp = ((int)pow(2, j) * i);  */
//			Temp = fmod((double)(Temp * i), (double)nn);
			Temp = (Temp * i) % nn;                         /*  Temp = ((int)pow(2, j) * i) % nn;  */
			gen_roots_true[Temp] = 1;
		}
	}

	rr = 0;		// Count the number of parity check bits
	for (i = 0; i < nn; i++)
	{	if (gen_roots_true[i] == 1)
		{	rr++;
			gen_roots[rr] = i;
		}
	}
	kk = nn - rr;

	// Compute generator polynomial based on its roots
	gg[0] = 2 ;	// g(x) = (X + alpha) initially
	gg[1] = 1 ;
	for (i = 2; i <= rr; i++)
	{ 	gg[i] = 1 ;
		for (j = i - 1; j > 0; j--)
		{
		if (gg[j] != 0)
//			gg[j] = gg[j-1]^ alpha_to[fmod((double)(index_of[gg[j]] + index_of[alpha_to[gen_roots[i]]]), (double)nn)] ;
			gg[j] = gg[j-1]^ alpha_to[(index_of[gg[j]] + index_of[alpha_to[gen_roots[i]]]) % nn] ;
		else
			gg[j] = gg[j-1] ;
		}
//		gg[0] = alpha_to[fmod((double)(index_of[gg[0]] + index_of[alpha_to[gen_roots[i]]]), (double)nn)] ;
		gg[0] = alpha_to[(index_of[gg[0]] + index_of[alpha_to[gen_roots[i]]]) % nn] ;
	}

	// for parallel encoding and syndrome computation
	// Max parallalism is rr
	if (Parallel > rr)
		Parallel = rr ;

	// Construct parallel lookahead matrix T_g, and T_g**r from gg(x)
	for (i = 0; i < rr; i++)
	{	for (j = 0; j < rr; j++)
			T_G[i][j] = 0;
	}
	for (i = 1; i < rr; i++)
		T_G[i][i-1] = 1 ;
	for (i = 0; i < rr; i++)
		T_G[i][rr - 1] = gg[i] ;
	for (i = 0; i < rr; i++)
	{	for (j = 0; j < rr; j++)
			T_G_R[i][j] = T_G[i][j];
	}

	// Compute T_G**R Matrix
	for (iii = 1; iii < Parallel; iii++)
	{	for (i = 0; i < rr; i++)
		{	for (j = 0; j < rr; j++)
			{	Temp = 0;
				for (jjj = 0; jjj < rr; jjj++)
					Temp = Temp ^ T_G_R[i][jjj] * T_G[jjj][j];
				T_G_R_Temp[i][j] = Temp;
			}
		}
		for (i = 0; i < rr; i++)
		{	for (j = 0; j < rr; j++)
				T_G_R[i][j] = T_G_R_Temp[i][j];
		}
	}

	nn_shorten = kk_shorten + rr ;

// Parallel BCH Encode
	// Determine the number of loops required for parallelism.
//	loop_count = ceil(kk_shorten / (double)Parallel) ;
	loop_count = kk_shorten / Parallel ;

	// Serial to parallel data conversion
	for (i = 0; i < Parallel; i++)
	{	for (j = 0; j < loop_count; j++)
		{	if (i + j * Parallel < kk_shorten)
				data_p[i][j] = data[i + j * Parallel];
			else
				data_p[i][j] = 0;
		}
	}

	// Initialize the parity bits.
	for (i = 0; i < rr; i++)
		bb[i] = 0;

	// Compute parity checks
	// S(t) = T_G_R [ S(t-1) + M(t) ]
	for (iii = loop_count - 1; iii >= 0; iii--)
	{
		for (i = 0; i < rr; i++)
			bb_temp[i] = bb[i] ;
		for (i = Parallel - 1; i >= 0; i--)
			bb_temp[rr - Parallel + i] = bb_temp[rr - Parallel + i] ^ data_p[i][iii];

		for (i = 0; i < rr; i++)
		{	Temp = 0;
			for (j = 0; j < rr; j++)
				Temp = Temp ^ (bb_temp[j] * T_G_R[i][j]);
			bb[i] = Temp;
		}
	}

// OUTPUT
	out_v = 0;
	for (i = 0 ; i < rr ; i++)
		out_v = ( out_v << 1) | bb[i] ;
	return out_v;
}
/*------------------------------- End OF File --------------------------------*/
