/*----------------------------------------------------------------------------/
/ TJpgDec - Tiny JPEG Decompressor R0.03                      (C)ChaN, 2021
/-----------------------------------------------------------------------------/
/ The TJpgDec is a generic JPEG decompressor module for tiny embedded systems.
/ This is a free software that opened for education, research and commercial
/  developments under license policy of following terms.
/
/  Copyright (C) 2021, ChaN, all right reserved.
/
/ * The TJpgDec module is a free software and there is NO WARRANTY.
/ * No restriction on use. You can use, modify and redistribute it for
/   personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
/ * Redistributions of source code must retain the above copyright notice.
/
/-----------------------------------------------------------------------------/
/ Oct 04, 2011 R0.01  First release.
/ Feb 19, 2012 R0.01a Fixed decompression fails when scan starts with an escape seq.
/ Sep 03, 2012 R0.01b Added JD_TBLCLIP option.
/ Mar 16, 2019 R0.01c Supprted stdint.h.
/ Jul 01, 2020 R0.01d Fixed wrong integer type usage.
/ May 08, 2021 R0.02  Supprted grayscale image. Separated configuration options.
/ Jun 11, 2021 R0.02a Some performance improvement.
/ Jul 01, 2021 R0.03  Added JD_FASTDECODE option.
/                     Some performance improvement.
/-----------------------------------------------------------------------------/
/ original source is here : http://elm-chan.org/fsw/tjpgd/00index.html
/
/ Modified for LGFX  by lovyan03, 2023
/----------------------------------------------------------------------------*/

#pragma GCC optimize ("O3")

#include "tjpgdClass.h"

#include <sdkconfig.h>
#include <string.h> // for memcpy memset
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#if JD_FASTDECODE == 2
#define HUFF_BIT	8	/* Bit length to apply fast huffman decode */
#define HUFF_LEN	(1 << HUFF_BIT)
#define HUFF_MASK	(HUFF_LEN - 1)
#endif


/*-----------------------------------------------*/
/* Zigzag-order to raster-order conversion table */
/*-----------------------------------------------*/

static const uint8_t Zig[64] = {	/* Zigzag-order to raster-order conversion table */
	 0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
	12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
};



/*-------------------------------------------------*/
/* Input scale factor of Arai algorithm            */
/* (scaled up 16 bits for fixed point operations)  */
/*-------------------------------------------------*/

static const uint16_t Ipsf[64] = {	/* See also aa_idct.png */
	(uint16_t)(1.00000*8192), (uint16_t)(1.38704*8192), (uint16_t)(1.30656*8192), (uint16_t)(1.17588*8192), (uint16_t)(1.00000*8192), (uint16_t)(0.78570*8192), (uint16_t)(0.54120*8192), (uint16_t)(0.27590*8192),
	(uint16_t)(1.38704*8192), (uint16_t)(1.92388*8192), (uint16_t)(1.81226*8192), (uint16_t)(1.63099*8192), (uint16_t)(1.38704*8192), (uint16_t)(1.08979*8192), (uint16_t)(0.75066*8192), (uint16_t)(0.38268*8192),
	(uint16_t)(1.30656*8192), (uint16_t)(1.81226*8192), (uint16_t)(1.70711*8192), (uint16_t)(1.53636*8192), (uint16_t)(1.30656*8192), (uint16_t)(1.02656*8192), (uint16_t)(0.70711*8192), (uint16_t)(0.36048*8192),
	(uint16_t)(1.17588*8192), (uint16_t)(1.63099*8192), (uint16_t)(1.53636*8192), (uint16_t)(1.38268*8192), (uint16_t)(1.17588*8192), (uint16_t)(0.92388*8192), (uint16_t)(0.63638*8192), (uint16_t)(0.32442*8192),
	(uint16_t)(1.00000*8192), (uint16_t)(1.38704*8192), (uint16_t)(1.30656*8192), (uint16_t)(1.17588*8192), (uint16_t)(1.00000*8192), (uint16_t)(0.78570*8192), (uint16_t)(0.54120*8192), (uint16_t)(0.27590*8192),
	(uint16_t)(0.78570*8192), (uint16_t)(1.08979*8192), (uint16_t)(1.02656*8192), (uint16_t)(0.92388*8192), (uint16_t)(0.78570*8192), (uint16_t)(0.61732*8192), (uint16_t)(0.42522*8192), (uint16_t)(0.21677*8192),
	(uint16_t)(0.54120*8192), (uint16_t)(0.75066*8192), (uint16_t)(0.70711*8192), (uint16_t)(0.63638*8192), (uint16_t)(0.54120*8192), (uint16_t)(0.42522*8192), (uint16_t)(0.29290*8192), (uint16_t)(0.14932*8192),
	(uint16_t)(0.27590*8192), (uint16_t)(0.38268*8192), (uint16_t)(0.36048*8192), (uint16_t)(0.32442*8192), (uint16_t)(0.27590*8192), (uint16_t)(0.21678*8192), (uint16_t)(0.14932*8192), (uint16_t)(0.07612*8192)
};



/*---------------------------------------------*/
/* Conversion table for fast clipping process  */
/*---------------------------------------------*/

#if JD_TBLCLIP

#define BYTECLIP(v) Clip8[(unsigned int)(v) & 0x3FF]

static const uint8_t Clip8[1024] = {
	/* 0..255 */
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
	64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
	96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
	128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
	160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191,
	192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223,
	224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255,
	/* 256..511 */
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	/* -512..-257 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* -256..-1 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

#else	/* JD_TBLCLIP */

static uint8_t BYTECLIP (int val)
{
	return (val < 0) ? 0 : (val > 255) ? 255 : val;
}

#endif


/*-----------------------------------------------------------------------*/
/* Allocate a memory block from memory pool                              */
/*-----------------------------------------------------------------------*/

static void* alloc_pool (	/* Pointer to allocated memory block (NULL:no memory available) */
    TJpgD* jd,		/* Pointer to the decompressor object */
    size_t ndata		/* Number of bytes to allocate */
)
{
	uint8_t *rp = 0;


	ndata = (ndata + 3) & ~3;			/* Align block size to the word boundary */

	/* The first part is used as a buffer for reading data, so the necessary area is allocated from the tail. */
	if (jd->sz_pool >= ndata) {
		rp = &(jd->inbuf[jd->sz_pool -= ndata]);			/* Get start of available memory pool */
	}

	return (void*)rp;	/* Return allocated memory block (NULL:no memory to allocate) */
}




/*-----------------------------------------------------------------------*/
/* data load       */
/*-----------------------------------------------------------------------*/

static size_t read_data ( TJpgD* jd, size_t buflen)
{
	uint8_t *dp = jd->dptr;
	uint8_t *dpend = jd->dpend;
	size_t dc;
	if ((dc = (dpend - dp)) < 256) {
		uint8_t *inbuf = jd->inbuf;
		if (dpend)
		{	/* If an EOI marker has already been found, exit */
			uint8_t* last = dpend - 2;
			if (last[0] == 0xFF && last[1] == 0xD9) {
// printf("%08x  EOI exists   dc:%d\n", (uintptr_t)last, dc);
				return dc;
			}
			{
#if JD_FASTDECODE > 0
				buflen -= 4;
				memcpy(inbuf, dp-4, dc+4);
				inbuf += 4;
#else
				if (dc != 0)
				{
					memcpy(inbuf, dp, dc);
				}
#endif
			}
		}
		dp = &(inbuf[dc]);
		int reqlen = TJPGD_SZBUF;//buflen - dc;
		int res = jd->infunc(jd, dp, reqlen);
//printf("read_data:req:%d - %d = %d  :  res:%d \n",buflen,dc,buflen-dc,res);
		if (res >= 0) {
			dc += res;
			dpend = &dp[res];
		}
		jd->dptr = inbuf;
		jd->dpend = dpend;
	}

	return dc;
}

/*-----------------------------------------------------------------------*/
/* Create de-quantization and prescaling tables with a DQT segment       */
/*-----------------------------------------------------------------------*/

static int create_qt_tbl (	/* 0:OK, !0:Failed */
    TJpgD* jd,				/* Pointer to the decompressor object */
	const uint8_t* data,	/* Pointer to the quantizer tables */
	size_t ndata			/* Size of input data */
)
{
	unsigned int i, zi;
	uint8_t d;
	int32_t *pb;


	while (ndata) {	/* Process all tables in the segment */
		if (ndata < 65) return TJpgD::JDR_FMT1;	/* Err: table size is unaligned */
		ndata -= 65;
		d = *data++;							/* Get table property */
		if (d & 0xF0) return TJpgD::JDR_FMT1;			/* Err: not 8-bit resolution */
		i = d & 3;								/* Get table ID */
		pb = (int32_t*)alloc_pool(jd, 64 * sizeof (int32_t));/* Allocate a memory block for the table */
		if (!pb) return TJpgD::JDR_MEM1;				/* Err: not enough memory */
		jd->qttbl[i] = pb;						/* Register the table */
		for (i = 0; i < 64; i++) {				/* Load the table */
			zi = Zig[i];						/* Zigzag-order to raster-order conversion */
			pb[zi] = (int32_t)((uint32_t)*data++ * Ipsf[zi]);	/* Apply scale factor of Arai algorithm to the de-quantizers */
		}
	}

	return TJpgD::JDR_OK;
}

/*-----------------------------------------------------------------------*/
/* Create huffman code tables with a DHT segment                         */
/*-----------------------------------------------------------------------*/
uint32_t prof0, prof1, prof2, prof3, prof4, prof5, prof6, prof7;

// 5044
static int create_huffman_tbl (	/* 0:OK, !0:Failed */
    TJpgD* jd,					/* Pointer to the decompressor object */
	const uint8_t* data,		/* Pointer to the packed huffman tables */
	int_fast16_t ndata				/* Size of input data */
)
{
	unsigned int i, j, b, cls, num;
	size_t np;
	uint8_t d, *pb, *pd;
	uint16_t hc, *ph;

	while (ndata) {	/* Process all tables in the segment */
		ndata -= 17;
		if (ndata < 0) return TJpgD::JDR_FMT1;	/* Err: wrong data size */
		d = *data++;						/* Get table number and class */
		if (d & 0xEE) return TJpgD::JDR_FMT1;		/* Err: invalid class/number */
		cls = d >> 4; num = d & 0x0F;		/* class = dc(0)/ac(1), table number = 0/1 */
		pb = (uint8_t*)alloc_pool(jd, 16);			/* Allocate a memory block for the bit distribution table */
		if (!pb) return TJpgD::JDR_MEM1;			/* Err: not enough memory */
		jd->huffbits[num][cls] = pb;
		for (np = i = 0; i < 16; i++) {		/* Load number of patterns for 1 to 16-bit code */
			np += (pb[i] = *data++);		/* Get sum of code words for each code */
		}
		ph = (uint16_t*)alloc_pool(jd, np * sizeof(uint16_t));/* Allocate a memory block for the code word table */
		if (!ph) return TJpgD::JDR_MEM1;			/* Err: not enough memory */
		jd->huffcode[num][cls] = ph;
		hc = 0;
		for (j = i = 0; i < 16; i++) {		/* Re-build huffman code word table */
			b = pb[i];
			while (b--) ph[j++] = hc++;
			hc <<= 1;
		}

		ndata -= np;
		if (ndata < 0) return TJpgD::JDR_FMT1;	/* Err: wrong data size */
		pd = (uint8_t*)alloc_pool(jd, np);			/* Allocate a memory block for the decoded data */
		if (!pd) return TJpgD::JDR_MEM1;			/* Err: not enough memory */
		jd->huffdata[num][cls] = pd;
		if (cls) {
			memcpy(pd, data, np);
			data += np;
		} else {
			for (i = 0; i < np; i++) {			/* Load decoded data corresponds to each code word */
				d = *data++;
				if (d > 11) return TJpgD::JDR_FMT1;
				pd[i] = d;
			}
		}
#if JD_FASTDECODE == 2
		{	/* Create fast huffman decode table */
			unsigned int span, td, ti;
			uint16_t *tbl_ac = 0;
			uint8_t *tbl_dc = 0;

			if (cls) {
				tbl_ac = (uint16_t*)alloc_pool(jd, HUFF_LEN * sizeof (uint16_t));	/* LUT for AC elements */
				if (!tbl_ac) return TJpgD::JDR_MEM1;		/* Err: not enough memory */
				jd->hufflut_ac[num] = tbl_ac;
				memset(tbl_ac, 0xFF, HUFF_LEN * sizeof (uint16_t));		/* Default value (0xFFFF: may be long code) */
			} else {
				tbl_dc = (uint8_t*)alloc_pool(jd, HUFF_LEN * sizeof (uint8_t));	/* LUT for AC elements */
				if (!tbl_dc) return TJpgD::JDR_MEM1;		/* Err: not enough memory */
				jd->hufflut_dc[num] = tbl_dc;
				memset(tbl_dc, 0xFF, HUFF_LEN * sizeof (uint8_t));		/* Default value (0xFF: may be long code) */
			}
			ph = jd->huffcode[num][cls];
			for (i = b = 0; b < HUFF_BIT; b++) {	/* Create LUT */
				for (j = pb[b]; j; j--) {
					ti = ph[i] << (HUFF_BIT - 1 - b) & HUFF_MASK;	/* Index of input pattern for the code */
					if (cls) {
						td = pd[i++] | ((b + 1) << 8);	/* b15..b8: code length, b7..b0: zero run and data length */
						for (span = 1 << (HUFF_BIT - 1 - b); span; span--, tbl_ac[ti++] = (uint16_t)td) ;
					} else {
						td = pd[i++] | ((b + 1) << 4);	/* b7..b4: code length, b3..b0: data length */
						for (span = 1 << (HUFF_BIT - 1 - b); span; span--, tbl_dc[ti++] = (uint8_t)td) ;
					}
				}
			}
			jd->longofs[num][cls] = i;	/* Code table offset for long code */
		}
#endif
	}

	return TJpgD::JDR_OK;
}








/*-----------------------------------------------------------------------*/
/* Extract a huffman decoded data from input stream                      */
/*-----------------------------------------------------------------------*/

static int huffext (	/* >=0: decoded data, <0: error code */
	TJpgD* jd,			/* Pointer to the decompressor object */
	unsigned int id,	/* Table ID (0:Y, 1:C) */
	unsigned int cls	/* Table class (0:DC, 1:AC) */
)
{
#if JD_FASTDECODE == 0
	unsigned int flg = 0;
	uint8_t bm, nd, bl;
	const uint8_t *hb = jd->huffbits[id][cls];	/* Bit distribution table */
	const uint16_t *hc = jd->huffcode[id][cls];	/* Code word table */
	const uint8_t *hd = jd->huffdata[id][cls];	/* Data table */


	bm = jd->dbit;	/* Bit mask to extract */
	d = 0; bl = 16;	/* Max code length */
	do {
		if (!bm) {		/* Next byte? */
			if (!dc) {	/* No input data is available, re-fill input buffer */
				dp = jd->inbuf;	/* Top of input buffer */
				dc = jd->infunc(jd->device, dp, JD_SZBUF);
				if (!dc) return 0 - (int)JDR_INP;	/* Err: read error or wrong stream termination */
			} else {
				dp++;	/* Next data ptr */
			}
			dc--;		/* Decrement number of available bytes */
			if (flg) {		/* In flag sequence? */
				flg = 0;	/* Exit flag sequence */
				if (*dp != 0) return 0 - (int)JDR_FMT1;	/* Err: unexpected flag is detected (may be collapted data) */
				*dp = 0xFF;				/* The flag is a data 0xFF */
			} else {
				if (*dp == 0xFF) {		/* Is start of flag sequence? */
					flg = 1; continue;	/* Enter flag sequence, get trailing byte */
				}
			}
			bm = 0x80;		/* Read from MSB */
		}
		d <<= 1;			/* Get a bit */
		if (*dp & bm) d++;
		bm >>= 1;

		for (nd = *hb++; nd; nd--) {	/* Search the code word in this bit length */
			if (d == *hc++) {	/* Matched? */
				jd->dbit = bm; jd->dctr = dc; jd->dptr = dp;
				return *hd;		/* Return the decoded data */
			}
			hd++;
		}
		bl--;
	} while (bl);

#elif JD_FASTDECODE == 1
	unsigned int wbit = jd->dbit;
	uint8_t* dp = jd->dptr;
	uint_fast32_t w = 0;
	uint_fast8_t i = 0;

	/* Incremental serch for all codes */
	const uint8_t *hb = jd->huffbits[id][cls];	/* Bit distribution table */
	const uint16_t *hc = jd->huffcode[id][cls];	/* Code word table */
	const uint8_t *hd = jd->huffdata[id][cls];	/* Data table */
	int loop = 3;
	if (!wbit) { goto huffext_in; }
	w = *(dp-1) & ((1UL << wbit) - 1);
	if (wbit == 1)
	{
		goto huffext_in;
	}

	do {
		do {
			uint_fast8_t nc = *hb++;
			--wbit;
			if (nc) {
				nc += i;
				uint_fast16_t d = w >> wbit;
				do {	/* Search the code word in this bit length */
					if (d == hc[i]) {		/* Matched? */
						jd->dbit = wbit;
						return hd[i];					/* Return the decoded data */
					}
				} while (++i != nc);
			}
		} while (wbit);

huffext_in:
		{
			uint_fast8_t d = *dp++;
			wbit += 8;
			w = (w << 8) + d;	/* Shift 8 bits in the working register */
			if (d == 0xFF) {
				*dp++ = d;
			}
			jd->dptr = dp;
		}
	} while (--loop);
	return 0 - (int)JDR_FMT1;	/* Err: code not found (may be collapted data) */

#elif JD_FASTDECODE == 2

	const uint8_t* hb, * hd;
	const uint16_t* hc;
	unsigned int nc, bl, wbit = jd->dbit & 31;
	uint_fast32_t w = jd->wreg & ((1UL << wbit) - 1);
	uint_fast16_t d;
	if (wbit < 16) {	/* Prepare 16 bits into the working register */
		uint8_t* dp = jd->dptr;
		do {
			d = *dp++;
			w = w << 8 | d;	/* Shift 8 bits in the working register */
			wbit += 8;
			if (d == 0xFF) {
				uint_fast8_t marker = *dp++;
				if (marker != 0) {
					jd->marker = marker;
					w = w << 8 | d;
					wbit += 8;
				}
			}
		} while (wbit < 16);
		jd->dptr = dp;
		jd->wreg = w;
	}

	/* Table serch for the short codes */
	d = (unsigned int)(w >> (wbit - HUFF_BIT));	/* Short code as table index */
	if (cls) {	/* AC element */
		d = jd->hufflut_ac[id][d];	/* Table decode */
		if (d != 0xFFFF) {	/* It is done if hit in short code */
			jd->dbit = wbit - (d >> 8);	/* Snip the code length */
			return d & 0xFF;	/* b7..0: zero run and following data bits */
		}
	}
	else {	/* DC element */
		d = jd->hufflut_dc[id][d];	/* Table decode */
		if (d != 0xFF) {	/* It is done if hit in short code */
			jd->dbit = wbit - (d >> 4);	/* Snip the code length  */
			return d & 0xF;	/* b3..0: following data bits */
		}
	}

	/* Incremental serch for the codes longer than HUFF_BIT */
	hb = jd->huffbits[id][cls] + HUFF_BIT;				/* Bit distribution table */
	hc = jd->huffcode[id][cls] + jd->longofs[id][cls];	/* Code word table */
	hd = jd->huffdata[id][cls] + jd->longofs[id][cls];	/* Data table */
	bl = HUFF_BIT;
	wbit -= HUFF_BIT;
	int i = 0;
	do {	/* Incremental search */
		nc = *hb++;
		--wbit;
		if (nc) {
			nc += i;
			d = w >> wbit;
			do {	/* Search the code word in this bit length */
				if (d == hc[i]) {		/* Matched? */
					jd->dbit = wbit;	/* Snip the huffman code */
					return hd[i];			/* Return the decoded data */
				}
			} while (++i != nc);
		}
	} while (++bl < 16);

	return TJpgD::JDR_FMT1;	/* Err: code not found (may be collapted data) */
#endif
}




/*-----------------------------------------------------------------------*/
/* Extract N bits from input stream                                      */
/*-----------------------------------------------------------------------*/

static int bitext (	/* >=0: extracted data, <0: error code */
	TJpgD* jd,			/* Pointer to the decompressor object */
	unsigned int nbit	/* Number of bits to extract (1 to 16) */
)
{
	// size_t dc = jd->dctr;
	uint8_t *dp = jd->dptr;
	unsigned int d;

#if JD_FASTDECODE == 0
	unsigned int flg = 0;
	uint8_t mbit = jd->dbit;

	d = 0;
	do {
		if (!mbit) {			/* Next byte? */
			if (!dc) {			/* No input data is available, re-fill input buffer */
				dp = jd->inbuf;	/* Top of input buffer */
				dc = jd->infunc(jd->device, dp, JD_SZBUF);
				if (!dc) return 0 - (int)JDR_INP;	/* Err: read error or wrong stream termination */
			} else {
				dp++;			/* Next data ptr */
			}
			dc--;				/* Decrement number of available bytes */
			if (flg) {			/* In flag sequence? */
				flg = 0;		/* Exit flag sequence */
				if (*dp != 0) return 0 - (int)JDR_FMT1;	/* Err: unexpected flag is detected (may be collapted data) */
				*dp = 0xFF;		/* The flag is a data 0xFF */
			} else {
				if (*dp == 0xFF) {		/* Is start of flag sequence? */
					flg = 1; continue;	/* Enter flag sequence */
				}
			}
			mbit = 0x80;		/* Read from MSB */
		}
		d <<= 1;	/* Get a bit */
		if (*dp & mbit) d |= 1;
		mbit >>= 1;
		nbit--;
	} while (nbit);

	jd->dbit = mbit; jd->dctr = dc; jd->dptr = dp;
	return (int)d;

#elif JD_FASTDECODE == 1
	unsigned int wbit = jd->dbit;
	uint_fast32_t w = 0;
	if (wbit) {
		w = *(dp - 1) & ((1UL << wbit) - 1);
		if (wbit >= nbit) {
bitext_end:
			wbit -= nbit;
			jd->dbit = wbit;
			return (int)(w >> wbit);
		}
	}
	/* Prepare nbit bits into the working register */
	do {
		d = *dp++;
		wbit += 8;
		w = (w << 8) + d;	/* Shift 8 bits in the working register */
		if (d == 0xFF) {
			*dp++ = d;
		}
	} while (wbit < nbit);
	jd->dptr = dp;
	goto bitext_end;
#else

	unsigned int wbit = jd->dbit;
	uint_fast32_t w = 0;
	if (wbit) {
		w = jd->wreg & ((1UL << wbit) - 1);
		if (wbit >= nbit) {
bitext_end:
			wbit -= nbit;
			jd->dbit = wbit;
			jd->wreg = w;
			return (int)(w >> wbit);
		}
	}

	{	/* Prepare nbit bits into the working register */
		do {
			d = *dp;
			wbit += 8;
			w = (w << 8) + d;	/* Get 8 bits into the working register */
			dp += (d == 0xFF) ? 2 : 1;
		} while (wbit < nbit);
	}
	jd->dptr = dp;
	goto bitext_end;

#endif
}




/*-----------------------------------------------------------------------*/
/* Process restart interval                                              */
/*-----------------------------------------------------------------------*/

static TJpgD::JRESULT restart (
	TJpgD* jd,		/* Pointer to the decompressor object */
	uint16_t rstn	/* Expected restert sequense number */
)
{
	unsigned int i;
	uint8_t *dp = jd->dptr;
	// uint8_t *dpend = jd->dpend;
	// size_t dc = jd->dctr;

#if JD_FASTDECODE == 0
	uint16_t d = 0;

	/* Get two bytes from the input stream */
	for (i = 0; i < 2; i++) {
		if (!dc) {	/* No input data is available, re-fill input buffer */
			dp = jd->inbuf;
			dc = jd->infunc(jd->device, dp, JD_SZBUF);
			if (!dc) return TJpgD::JDR_INP;
		} else {
			dp++;
		}
		dc--;
		d = d << 8 | *dp;	/* Get a byte */
	}
	jd->dptr = dp; jd->dctr = dc; jd->dbit = 0;

	/* Check the marker */
	if ((d & 0xFFD8) != 0xFFD0 || (d & 7) != (rstn & 7)) {
		return TJpgD::JDR_FMT1;	/* Err: expected RSTn marker is not detected (may be collapted data) */
	}

#else
	uint_fast16_t marker;


	if (jd->marker) {	/* Generate a maker if it has been detected */
		marker = 0xFF00 | jd->marker;
		jd->marker = 0;
	} else {
		marker = 0;
		for (i = 0; i < 2; i++) {	/* Get a restart marker */
			marker = (marker << 8) | *dp++;	/* Get a byte */
		}
		jd->dptr = dp;
	}

	/* Check the marker */
	if ((marker & 0xFFD8) != 0xFFD0 || (marker & 7) != (rstn & 7)) {
		return TJpgD::JDR_FMT1;	/* Err: expected RSTn marker was not detected (may be collapted data) */
	}

	jd->dbit = 0;			/* Discard stuff bits */
#endif

	jd->dcv[2] = jd->dcv[1] = jd->dcv[0] = 0;	/* Reset DC offset */
	return TJpgD::JDR_OK;
}




/*-----------------------------------------------------------------------*/
/* Apply Inverse-DCT in Arai Algorithm (see also aa_idct.png)            */
/*-----------------------------------------------------------------------*/

#if defined (CONFIG_IDF_TARGET_ARCH_XTENSA)
__attribute__((noinline,noclone))
void block_idct (
	int32_t* src,	/* Input block data (de-quantized and pre-scaled for Arai Algorithm) */
	jd_yuv_t* dst	/* Pointer to the destination to store the block as byte array */
)
{

    // 関数が呼び出された直後のレジスタの値
    // a0 : リターンアドレス     (使用しない)
    // a1 : スタックポインタ     (変更不可)
    // a2 : src                  (ループ内で加算しながら利用する)
    // a3 : dst                  (ループ内で加算しながら利用する)
    __asm__ (
	"movi       a4 ,10703           \n"	// a4:M4  = (int32_t)(2.61313*4096)
	"movi       a5 , 7568           \n"	// a5:M5  = (int32_t)(1.84776*4096)
	"movi       a6 , 4433           \n"	// a6:M2  = (int32_t)(1.08239*4096)
	"movi       a7 , 5792           \n"	// a7:M13 = (int32_t)(1.41421*4096)
    "movi       a15, 8              \n"

    "loop       a15, .LOOP_IDCT_COL  \n" // 8回ループ
	"l32i       a8 , a2 , 3 * 32    \n" // int32_t a8  = src[8 * 3];
	"l32i       a9 , a2 , 5 * 32    \n" // int32_t a9  = src[8 * 5];
	"l32i       a10, a2 , 7 * 32    \n" // int32_t a10 = src[8 * 7];
	"l32i       a11, a2 , 1 * 32    \n" // int32_t a11 = src[8 * 1];

	"add        a8 , a9 , a8        \n" // a8  =  a9      + a8;
	"subx2      a9 , a9 , a8        \n" // a9  = (a9 <<1) - a8;
	"add        a10, a11, a10       \n" // a10 =  a11     + a10;
	"subx2      a11, a11, a10       \n" // a11 = (a11<<1) - a10;

	"add        a8 , a10, a8        \n" // a8  = a10 + a8;
	"subx2      a10, a10, a8        \n" // a10 = (a10 << 1) - a8;
    "mull       a10, a10, a7        \n" // a10 *= M13;

	"add        a13, a11, a9        \n" // int32_t a13 = a11 + a9;
    "mull       a13, a13, a5        \n" // a13 *= M5;
    "mull       a9 , a9 , a4        \n" // a9  = a9  * M4;
    "mull       a11, a11, a6        \n" // a11 = a11 * M2;

    "slli       a8 , a8 , 12        \n" // a8  <<= 12

	"sub        a9 , a13, a9        \n" // a9  = a13 - a9;
	"sub        a11, a13, a11       \n" // a11 = a13 - a11;
	"sub        a9 , a9 , a8        \n" // a9  -= a8;
	"sub        a10, a10, a9        \n" // a10 -= a9;
	"sub        a11, a11, a10       \n" // a11 -= a10;

	"l32i       a13, a2 , 0 * 32    \n" // a13 = src[8 * 0];
	"l32i       a12, a2 , 4 * 32    \n" // a12 = src[8 * 4];
	"l32i       a15, a2 , 6 * 32    \n" // a15 = src[8 * 6];
	"l32i       a14, a2 , 2 * 32    \n" // a14 = src[8 * 2];

	"add        a12, a13, a12       \n" // a12 =  a13    + a12;
	"subx2      a13, a13, a12       \n" // a13 = (a13<<1)- a12;
	"add        a15, a14, a15       \n" // a15 =  a14    + a15;
	"subx2      a14, a14, a15       \n" // a14 = (a14<<1)- a15;

    "mull       a14, a14, a7        \n" // a14 *= M13;
    "slli       a12, a12, 12        \n" // a12 <<= 12
    "slli       a13, a13, 12        \n" // a13 <<= 12
    "slli       a15, a15, 12        \n" // a15 <<= 12
	"sub        a14, a14, a15       \n" // a14 =  a14     - a15;
	"add        a15, a12, a15       \n" // a15 =  a12     + a15;
	"subx2      a12, a12, a15       \n" // a12 = (a12<<1) - a15;
	"add        a14, a13, a14       \n" // a14 =  a13     + a14;
	"subx2      a13, a13, a14       \n" // a13 = (a13<<1) - a14;

	"add        a8 , a15, a8        \n" // a8  =  a15     + a8;
	"add        a9 , a14, a9        \n" // a9  =  a14     + a9;
	"add        a10, a13, a10       \n" // a10 =  a13     + a10;
	"add        a11, a12, a11       \n" // a11 =  a12     + a11;
	"subx2      a15, a15, a8        \n" // a15 = (a15<<1) - a8;
	"subx2      a14, a14, a9        \n" // a14 = (a14<<1) - a9;
	"subx2      a13, a13, a10       \n" // a13 = (a13<<1) - a10;
	"subx2      a12, a12, a11       \n" // a12 = (a12<<1) - a11;

	"s32i       a8 , a2 , 0 * 32    \n" // src[8 * 0] = a8;
	"s32i       a9 , a2 , 1 * 32    \n" // src[8 * 1] = a9;
	"s32i       a10, a2 , 2 * 32    \n" // src[8 * 2] = a10;
	"s32i       a11, a2 , 3 * 32    \n" // src[8 * 3] = a11;
	"s32i       a12, a2 , 4 * 32    \n" // src[8 * 4] = a12;
	"s32i       a13, a2 , 5 * 32    \n" // src[8 * 5] = a13;
	"s32i       a14, a2 , 6 * 32    \n" // src[8 * 6] = a14;
	"s32i       a15, a2 , 7 * 32    \n" // src[8 * 7] = a15;

    "addi       a2 , a2 , 4         \n"
    ".LOOP_IDCT_COL:                \n"
    "addi       a2 , a2 , -32       \n"

/////////////////////////////////////////////////////

    "movi       a15, 8              \n"
    "loop       a15, .LOOP_IDCT_ROW  \n"  // 8回ループ
	"l32i       a8 , a2 , 3 * 4     \n" // int32_t a8  = src[3];
	"l32i       a9 , a2 , 5 * 4     \n" // int32_t a9  = src[5];
	"l32i       a10, a2 , 7 * 4     \n" // int32_t a10 = src[7];
	"l32i       a11, a2 , 1 * 4     \n" // int32_t a11 = src[1];

	"add        a8 , a9 , a8        \n" // a8  =  a9      + a8; 
	"subx2      a9 , a9 , a8        \n" // a9  = (a9 <<1) - a8; 
	"add        a10, a11, a10       \n" // a10 =  a11     + a10; 
	"subx2      a11, a11, a10       \n" // a11 = (a11<<1) - a10; 

	"add        a8 , a10, a8        \n" // a8  = a10 + a8;
	"subx2      a10, a10, a8        \n" // a10 = (a10 << 1) - a8;
    "srai       a10, a10, 12        \n" // a10 = a10 >> 12;
    "mull       a10, a10, a7        \n" // a10 *= M13;

	"add        a13, a11, a9        \n" // int32_t a13 = a11 + a9;
    "srai       a13, a13, 12        \n" // a13 >>= 12
    "srai       a9 , a9 , 12        \n" // a9  >>= 12
    "srai       a11, a11, 12        \n" // a11 >>= 12
    "mull       a13, a13, a5        \n" // a13 *= M5;
    "mull       a9 , a9 , a4        \n" // a9  = a9  * M4;
    "mull       a11, a11, a6        \n" // a11 = a11 * M2;

	"sub        a9 , a13, a9        \n" // a9  = a13 - a9;
	"sub        a11, a13, a11       \n" // a11 = a13 - a11;
	"sub        a9 , a9 , a8        \n" // a9  -= a8;
	"sub        a10, a10, a9        \n" // a10 -= a9;
	"sub        a11, a11, a10       \n" // a11 -= a10;

	"l32i       a13, a2 , 0 * 4     \n" // a13 = src[0];
	"l32i       a12, a2 , 4 * 4     \n" // a12 = src[4];
	"movi       a14, 128 << 20      \n"

	"add        a13, a13, a14       \n"
	"l32i       a15, a2 , 6 * 4     \n" // a15 = src[6];
	"l32i       a14, a2 , 2 * 4     \n" // a14 = src[2];

	"add        a12, a13, a12       \n" // a12 =  a13    + a12;
	"subx2      a13, a13, a12       \n" // a13 = (a13<<1)- a12;
	"add        a15, a14, a15       \n" // a15 =  a14    + a15;
	"subx2      a14, a14, a15       \n" // a14 = (a14<<1)- a15;

    "srai       a14, a14, 12        \n" // a14 >>= 12;
    "mull       a14, a14, a7        \n" // a14 *= M13;
	"sub        a14, a14, a15       \n" // a14 =  a14     - a15;
	"add        a15, a12, a15       \n" // a15 =  a12     + a15;
	"add        a14, a13, a14       \n" // a14 =  a13     + a14;
	"subx2      a12, a12, a15       \n" // a12 = (a12<<1) - a15;
	"subx2      a13, a13, a14       \n" // a13 = (a13<<1) - a14;

	"add        a8 , a15, a8        \n" // a8  =  a15     + a8;
	"add        a9 , a14, a9        \n" // a9  =  a14     + a9;
	"add        a10, a13, a10       \n" // a10 =  a13     + a10;
	"add        a11, a12, a11       \n" // a11 =  a12     + a11;
	"subx2      a15, a15, a8        \n" // a15 = (a15<<1) - a8;
	"subx2      a14, a14, a9        \n" // a14 = (a14<<1) - a9;
	"subx2      a13, a13, a10       \n" // a13 = (a13<<1) - a10;
	"subx2      a12, a12, a11       \n" // a12 = (a12<<1) - a11;

    "srai       a8 , a8 , 20        \n" // a8  = a8  >> 20;
    "srai       a9 , a9 , 20        \n" // a9  = a9  >> 20;
    "srai       a10, a10, 20        \n" // a10 = a10 >> 20;
    "srai       a11, a11, 20        \n" // a11 = a11 >> 20;
    "srai       a12, a12, 20        \n" // a12 = a12 >> 20;
    "srai       a13, a13, 20        \n" // a13 = a13 >> 20;
    "srai       a14, a14, 20        \n" // a14 = a14 >> 20;
    "srai       a15, a15, 20        \n" // a15 = a15 >> 20;

	"s16i       a8 , a3 , 0 * 2     \n" // dst[0] = a8;
	"s16i       a9 , a3 , 1 * 2     \n" // dst[1] = a9;
	"s16i       a10, a3 , 2 * 2     \n" // dst[2] = a10;
	"s16i       a11, a3 , 3 * 2     \n" // dst[3] = a11;
	"s16i       a12, a3 , 4 * 2     \n" // dst[4] = a12;
	"s16i       a13, a3 , 5 * 2     \n" // dst[5] = a13;
	"s16i       a14, a3 , 6 * 2     \n" // dst[6] = a14;
	"s16i       a15, a3 , 7 * 2     \n" // dst[7] = a15;

    "addi       a2 , a2 , 32        \n"
    "addi       a3 , a3 , 16        \n"
    ".LOOP_IDCT_ROW:                \n"    
	);
}
#else
void block_idct (
	int32_t* src,	/* Input block data (de-quantized and pre-scaled for Arai Algorithm) */
	jd_yuv_t* dst	/* Pointer to the destination to store the block as byte array */
)
{

	const int32_t M13 = (int32_t)(1.41421*4096), M2 = (int32_t)(1.08239*4096), M4 = (int32_t)(2.61313*4096), M5 = (int32_t)(1.84776*4096);

/// 元のコードでは固定小数の掛算をした箇所で>>12シフトし、最後に>>8シフトして記録する構成だった。
/// これを変更し、掛算をしなかったレジスタを<<12シフトし、最後に>>20シフトして記録するとした。

	/* Process columns */
	for (int i = 0; i < 8; i++) {
		int32_t a8  = src[8 * 3];
		int32_t a9  = src[8 * 5];
		int32_t a10 = src[8 * 7];
		int32_t a11 = src[8 * 1];

		/* Process the odd elements */
		a8  =  a9      + a8;
		a9  = (a9 <<1) - a8;
		a10 =  a11     + a10;
		a11 = (a11<<1) - a10;

		a8  = a10 + a8;
		a10 = (a10 << 1) - a8;
		a10 *= M13;

		int32_t a13 = a11 + a9;
		a13 *= M5;
		a9   = a9  * M4;
		a11  = a11 * M2;

		// 掛算をしたレジスタを>>12シフトするのをやめ、代わりに掛けてないレジスタを<<12シフトする
		a8 <<= 12;
//		a10 >>= 12;
//		a13 >>= 12;
//		a9  >>= 12;
//		a11 >>= 12;
		a9   = a13 - a9;
		a11  = a13 - a11;
		a9  -= a8;
		a10 -= a9;
		a11 -= a10;

		/* Process the even elements */
		        a13 = src[8 * 0];
		int32_t a12 = src[8 * 4];
		int32_t a15 = src[8 * 6];
		int32_t a14 = src[8 * 2];

		a12 =  a13    + a12;
		a13 = (a13<<1)- a12;
		a15 =  a14    + a15;
		a14 = (a14<<1)- a15;

		a14 *= M13;
// 掛算をしたレジスタを>>12シフトするのをやめ、代わりに掛けてないレジスタを<<12シフトする
//		a14 >>= 12;
		a12 <<= 12;
		a13 <<= 12;
		a15 <<= 12;
		a14 =  a14     - a15;
		a15 =  a12     + a15;
		a12 = (a12<<1) - a15;
		a14 =  a13     + a14;
		a13 = (a13<<1) - a14;

		/* Write-back transformed values */
		a8  =  a15     + a8;
		a9  =  a14     + a9;
		a10 =  a13     + a10;
		a11 =  a12     + a11;
		a15 = (a15<<1) - a8;
		a14 = (a14<<1) - a9;
		a13 = (a13<<1) - a10;
		a12 = (a12<<1) - a11;

// ここで保存される値はすべて <<12 シフトされた状態になっている
		src[8 * 0] = a8;
		src[8 * 1] = a9;
		src[8 * 2] = a10;
		src[8 * 3] = a11;
		src[8 * 4] = a12;
		src[8 * 5] = a13;
		src[8 * 6] = a14;
		src[8 * 7] = a15;

		src++;	/* Next column */
	}

	/* Process rows */
	src -= 8;
	for (int i = 0; i < 8; i++) {
		int32_t a8 = src[3];
		int32_t a9 = src[5];
		int32_t a10 = src[7];
		int32_t a11 = src[1];

		/* Process the odd elements */
		a8  =  a9      + a8;
		a9  = (a9 <<1) - a8;
		a10 =  a11     + a10;
		a11 = (a11<<1) - a10;

		a8  = a10 + a8;
		a10 = (a10 << 1) - a8;
		a10 >>= 12;
		a10 *= M13;

//		a8 <<= 12;

		int32_t a13 = a11 + a9;
		a13 >>= 12;
		a9  >>= 12;
		a11 >>= 12;
		a13 *= M5;
		a9   = a9  * M4;
		a11  = a11 * M2;
		a9   = a13 - a9;
		a11  = a13 - a11;
		a9  -= a8;
		a10 -= a9;
		a11 -= a10;

		/* Process the even elements */
		        a13 = src[0];
		int32_t a12 = src[4];
		int32_t a15 = src[6];
		int32_t a14 = src[2];
		a13 += 128L << 20;

		a12 =  a13    + a12;
		a13 = (a13<<1)- a12;
		a15 =  a14    + a15;
		a14 = (a14<<1)- a15;

//		a12 <<= 12;
//		a13 <<= 12;
//		a15 <<= 12;
		a14 >>= 12;
		a14 *= M13;
		a14 =  a14     - a15;
		a15 =  a12     + a15;
		a12 = (a12<<1) - a15;
		a14 =  a13     + a14;
		a13 = (a13<<1) - a14;

		/* Write-back transformed values */
		a8  =  a15     + a8;
		a9  =  a14     + a9;
		a10 =  a13     + a10;
		a11 =  a12     + a11;
		a15 = (a15<<1) - a8;
		a14 = (a14<<1) - a9;
		a13 = (a13<<1) - a10;
		a12 = (a12<<1) - a11;

		 a8  >>= 20;
		 a9  >>= 20;
		 a10 >>= 20;
		 a11 >>= 20;
		 a12 >>= 20;
		 a13 >>= 20;
		 a14 >>= 20;
		 a15 >>= 20;

		/* Descale the transformed values 8 bits and output a row */
#if JD_FASTDECODE >= 1
		dst[0] = a8 ;
		dst[1] = a9 ;
		dst[2] = a10;
		dst[3] = a11;
		dst[4] = a12;
		dst[5] = a13;
		dst[6] = a14;
		dst[7] = a15;
#else
		dst[0] = BYTECLIP(a8 );
		dst[1] = BYTECLIP(a9 );
		dst[2] = BYTECLIP(a10);
		dst[3] = BYTECLIP(a11);
		dst[4] = BYTECLIP(a12);
		dst[5] = BYTECLIP(a13);
		dst[6] = BYTECLIP(a14);
		dst[7] = BYTECLIP(a15);
#endif

		dst += 8; src += 8;	/* Next row */
	}
}
#endif




/*-----------------------------------------------------------------------*/
/* Load all blocks in the MCU into working buffer                        */
/*-----------------------------------------------------------------------*/
static TJpgD::JRESULT mcu_load (
    TJpgD* jd,		/* Pointer to the decompressor object */
    jd_yuv_t* bp,		/* mcubuf */
    int32_t* tmp	/* Block working buffer for de-quantize and IDCT */
                                )
{
	int d, e;
	unsigned int blk, nby, i, bc, z, id, cmp;
	const int32_t *dqf;


    nby = jd->msx * jd->msy;	/* Number of Y blocks (1, 2 or 4) */

	for (blk = 0; blk < nby + 2; blk++) {	/* Get nby Y blocks and two C blocks */
		cmp = (blk < nby) ? 0 : blk - nby + 1;	/* Component number 0:Y, 1:Cb, 2:Cr */

		if (cmp && jd->ncomp != 3) {		/* Clear C blocks if not exist (monochrome image) */
			for (i = 0; i < 64; bp[i++] = 128) ;

		} else {							/* Load Y/C blocks from input stream */
			read_data ( jd, jd->sz_pool );
			id = cmp ? 1 : 0;						/* Huffman table ID of this component */
			/* Extract a DC element from input stream */
			d = huffext(jd, id, 0);					/* Extract a huffman coded data (bit length) */
			if (d < 0) return (TJpgD::JRESULT)(0 - d);		/* Err: invalid code or input */
			bc = (unsigned int)d;
			d = jd->dcv[cmp];						/* DC value of previous block */
			if (bc) {								/* If there is any difference from previous block */
				e = bitext(jd, bc);					/* Extract data bits */
				if (e < 0) return (TJpgD::JRESULT)(0 - e);	/* Err: input */
				bc = 1 << (bc - 1);					/* MSB position */
				if (!(e & bc)) e -= (bc << 1) - 1;	/* Restore negative value if needed */
				d += e;								/* Get current value */
				jd->dcv[cmp] = (int16_t)d;			/* Save current DC value for next block */
			}
			dqf = jd->qttbl[jd->qtid[cmp]];			/* De-quantizer table ID for this component */
			tmp[0] = d * dqf[0] >> 8;				/* De-quantize, apply scale factor of Arai algorithm and descale 8 bits */

			/* Extract following 63 AC elements from input stream */
			memset(&tmp[1], 0, 63 * sizeof (int32_t));	/* Initialize all AC elements */
			z = 1;		/* Top of the AC elements (in zigzag-order) */
			do {
				d = huffext(jd, id, 1);				/* Extract a huffman coded value (zero runs and bit length) */
				if (d == 0) break;					/* EOB? */
				if (d < 0) return (TJpgD::JRESULT)(0 - d);	/* Err: invalid code or input error */
				bc = (unsigned int)d;
				z += bc >> 4;						/* Skip leading zero run */
				if (z >= 64)
					return TJpgD::JDR_FMT1;		/* Too long zero run */
				if (bc &= 0x0F) {					/* Bit length? */
					d = bitext(jd, bc);				/* Extract data bits */
					if (d < 0) return (TJpgD::JRESULT)(0 - d);	/* Err: input device */
					bc = 1 << (bc - 1);				/* MSB position */
					if (!(d & bc)) d -= (bc << 1) - 1;	/* Restore negative value if needed */
					i = Zig[z];						/* Get raster-order index */
					tmp[i] = d * dqf[i] >> 8;		/* De-quantize, apply scale factor of Arai algorithm and descale 8 bits */
				}
			} while (++z < 64);		/* Next AC element */

			if (JD_FORMAT != 2 || !cmp) {	/* C components may not be processed if in grayscale output */
				if (z == 1 || (JD_USE_SCALE && jd->scale == 3)) {	/* If no AC element or scale ratio is 1/8, IDCT can be ommited and the block is filled with DC value */
					d = (jd_yuv_t)((*tmp / 256) + 128);
					if (JD_FASTDECODE >= 1) {
						for (i = 0; i < 64; bp[i++] = d) ;
					} else {
						memset(bp, d, 64);
					}
				} else {
					block_idct(tmp, bp);	/* Apply IDCT and store the block to the MCU buffer */
				}
			}
		}

		bp += 64;				/* Next block */
	}

    return TJpgD::JDR_OK;	/* All blocks have been loaded successfully */
}




/*-----------------------------------------------------------------------*/
/* Output an MCU: Convert YCrCb to RGB and output it in RGB form         */
/*-----------------------------------------------------------------------*/

static TJpgD::JRESULT mcu_output (
    TJpgD* jd,		/* Pointer to the decompressor object */
    jd_yuv_t* mcubuf,
    uint8_t* workbuf,
    uint32_t (*outfunc)(TJpgD*, void*, TJpgD::JRECT*),	/* RGB output function */
    uint_fast16_t x,		/* MCU position in the image (left of the MCU) */
    uint_fast16_t y		/* MCU position in the image (top of the MCU) */
                                  )
{
    uint_fast16_t ix, iy, mx, my, rx, ry;
    jd_yuv_t *py, *pc;
    TJpgD::JRECT rect;

    mx = jd->msx * 8; my = jd->msy * 8;					/* MCU size (pixel) */
    rx = (x + mx <= jd->width) ? mx : jd->width - x;	/* Output rectangular size (it may be clipped at right/bottom end) */
    ry = (y + my <= jd->height) ? my : jd->height - y;

    rect.left = x; rect.right = x + rx - 1;				/* Rectangular area in the frame buffer */
    rect.top = y; rect.bottom = y + ry - 1;

    static constexpr float frr = 1.402;
    static constexpr float fgr = 0.71414;
    static constexpr float fgb = 0.34414;
    static constexpr float fbb = 1.772;

    /* Build an RGB MCU from discrete comopnents */
//    const int8_t* btbase = Bayer[jd->bayer];
//    const int8_t* btbl;
    uint_fast8_t ixshift = (mx == 16);
    uint_fast8_t iyshift = (my == 16);
    iy = 0;
    uint8_t* prgb = workbuf;
    do {
//        btbl = &btbase[(iy & 3) << 3];
        py = &mcubuf[((iy & 8) + iy) << 3];
        pc = &mcubuf[((mx << iyshift) + (iy >> iyshift)) << 3];
        ix = 0;
        do {
            do {
                float cb = (pc[ 0] - 128); 	/* Get Cb/Cr component and restore right level */
                float cr = (pc[64] - 128);
                ++pc;

                /* Convert CbCr to RGB */
                int32_t gg = fgb * cb + fgr * cr;
                int32_t rr = frr * cr;
                int32_t bb = fbb * cb;
                // int32_t yy = btbl[0] + py[0];			/* Get Y component */
                int32_t yy = py[0];			/* Get Y component */
                prgb[0] = BYTECLIP(yy + rr);
                prgb[1] = BYTECLIP(yy - gg);
                prgb[2] = BYTECLIP(yy + bb);
                if (ixshift) {
                    // yy = btbl[1] + py[1];			/* Get Y component */
                    yy = py[1];			/* Get Y component */
                    prgb[3] = BYTECLIP(yy + rr);
                    prgb[4] = BYTECLIP(yy - gg);
                    prgb[5] = BYTECLIP(yy + bb);
                }
                prgb += 3 << ixshift;
                // btbl += 1 << ixshift;
                py += 1 << ixshift;
                ix += 1 << ixshift;
            } while (ix & 7);
            // btbl -= 8;
            py += 64 - 8;	/* Jump to next block if double block heigt */
        } while (ix != mx);
    } while (++iy != my);

    if (rx < mx) {
        uint8_t *s, *d;
        s = d = (uint8_t*)workbuf;
        rx *= 3;
        mx *= 3;
        for (size_t y = 1; y < ry; ++y)
        {
            memcpy(d += rx, s += mx, rx);	/* Copy effective pixels */
        }
    }
    /* Output the RGB rectangular */
    return outfunc(jd, workbuf, &rect) ? TJpgD::JDR_OK : TJpgD::JDR_INTR; 
}

/*-----------------------------------------------------------------------*/
/* Process restart interval                                              */
/*-----------------------------------------------------------------------*/

static TJpgD::JRESULT restart (
    TJpgD* jd,		/* Pointer to the decompressor object */
    uint_fast16_t rstn	/* Expected restert sequense number */
                               )
{
    uint_fast16_t d;
    uint8_t *dp, *dpend;


    /* Discard padding bits and get two bytes from the input stream */
    dp = jd->dptr; dpend = jd->dpend;
    d = 0;
    for (size_t i = 0; i < 2; i++) {
        if (++dp == dpend) {	/* No input data is available, re-fill input buffer */
            dp = jd->inbuf;
            jd->dpend = dpend = dp + jd->infunc(jd, dp, TJPGD_SZBUF);
            if (dp == dpend) return TJpgD::JDR_INP;
        }
        d = (d << 8) | *dp;	/* Get a byte */
    }
    jd->dptr = dp; jd->dbit = 0;

    /* Check the marker */
    if ((d & 0xFFD8) != 0xFFD0 || (d & 7) != (rstn & 7)) {
        return TJpgD::JDR_FMT1;	/* Err: expected RSTn marker is not detected (may be collapted data) */
    }

    /* Reset DC offset */
    jd->dcv[2] = jd->dcv[1] = jd->dcv[0] = 0;

    return TJpgD::JDR_OK;
}




/*-----------------------------------------------------------------------*/
/* Analyze the JPEG image and Initialize decompressor object             */
/*-----------------------------------------------------------------------*/

#define	LDB_WORD(ptr)		(uint16_t)(((uint16_t)*((uint8_t*)(ptr))<<8)|(uint16_t)*(uint8_t*)((ptr)+1))


//#define JD_INNER_BUFFER_SIZE (1024 * 8)

TJpgD::JRESULT TJpgD::prepare (
    uint32_t (*infunc)(TJpgD*, uint8_t*, uint32_t),	/* JPEG strem input function */
    void* dev			/* I/O device identifier for the session */
                               )
{
    uint8_t *seg;
    uint_fast8_t b;
    uint16_t marker = 0;
    uint_fast16_t i, len;
    TJpgD::JRESULT rc;

    static constexpr uint_fast16_t sz_pool = 5760;
    static uint8_t pool[sz_pool];

    seg = pool;		/* Work memroy */
	this->inbuf = pool;
	this->dptr = nullptr;
	this->dpend = nullptr;
    this->sz_pool = sz_pool;	/* Size of given work memory */
    this->infunc = infunc;	/* Stream input function */
    this->device = dev;		/* I/O device identifier */
    this->nrst = 0;			/* No restart interval (default) */

	size_t dctr;
	do {	/* Find SOI marker */
		dctr = read_data(this, TJPGD_SZBUF + 64);
		if (0 == dctr)
			return JDR_INP; /* Err: SOI was not detected */
		marker = marker << 8 | this->dptr[0];
		this->dptr++;
		--dctr;
	} while (marker != 0xFFD8);
	seg = this->dptr;
	len = 0;

	for (;;) {				/* Parse JPEG segments */
		/* Skip segment data (null pointer specifies to remove data from the stream) */
		if (dctr < len) {
			do {
				seg += dctr;
				len -= dctr;
				this->dptr = seg;
				dctr = read_data(this, TJPGD_SZBUF + 64);
				seg = this->dptr;
			} while (dctr < len);
		}
		seg += len;
		this->dptr = seg;
		if (seg > this->dpend)
		{ return JDR_INP; }
		do {	/* Get a JPEG marker */
			dctr = read_data( this, TJPGD_SZBUF + 64 );
			if (dctr < 4)
				return JDR_INP;
			marker = marker << 8 | this->dptr[0];
			this->dptr++;
			dctr--;
		} while ((marker & 0xFF) == 0xFF);
		seg = this->dptr;
		len = LDB_WORD(seg);	/* Length field */
		if (len <= 2 || (marker >> 8) != 0xFF)
			return (TJpgD::JRESULT)marker;//JDR_FMT1;
		len -= 2;			/* Segent content size */
		seg += 2;
		dctr -= 2;

		switch (marker & 0xFF) {
        case 0xC0:	/* SOF0 (baseline JPEG) */
            width = LDB_WORD(seg+3);		/* Image width in unit of pixel */
            height = LDB_WORD(seg+1);		/* Image height in unit of pixel */
			ncomp = seg[5];					/* Number of color components */
			if (ncomp != 3 && ncomp != 1) return JDR_FMT3;	/* Err: Supports only Grayscale and Y/Cb/Cr */

			/* Check each image component */
			for (i = 0; i < this->ncomp; i++) {
				b = seg[7 + 3 * i];							/* Get sampling factor */
				if (i == 0) {	/* Y component */
					if (b != 0x11 && b != 0x22 && b != 0x21) {	/* Check sampling factor */
						return JDR_FMT3;					/* Err: Supports only 4:4:4, 4:2:0 or 4:2:2 */
					}
					this->msx = b >> 4; this->msy = b & 15;		/* Size of MCU [blocks] */
				} else {		/* Cb/Cr component */
					if (b != 0x11) return JDR_FMT3;			/* Err: Sampling factor of Cb/Cr must be 1 */
				}
				this->qtid[i] = seg[8 + 3 * i];				/* Get dequantizer table ID for this component */
				if (this->qtid[i] > 3) return JDR_FMT3;		/* Err: Invalid ID */
			}
            break;

        case 0xDD:	/* DRI */
            /* Get restart interval (MCUs) */
            nrst = LDB_WORD(seg);
            break;

        case 0xC4:	/* DHT */
            /* Create huffman tables */
            rc = (TJpgD::JRESULT)create_huffman_tbl(this, seg, len);
            if (rc) return rc;
            break;

        case 0xDB:	/* DQT */
            /* Create de-quantizer tables */
            rc = (TJpgD::JRESULT)create_qt_tbl(this, seg, len);
            if (rc) return rc;
            break;

        case 0xDA:	/* SOS */
            if (!width || !height) return (TJpgD::JRESULT)16;//TJpgD::JDR_FMT1;	/* Err: Invalid image size */

            if (seg[0] != ncomp) return TJpgD::JDR_FMT3;	/* Err: Supports only three color or grayscale components format */

            /* Check if all tables corresponding to each components have been loaded */
            for (i = 0; i < ncomp; i++) {
                b = seg[2 + 2 * i];	/* Get huffman table ID */
                if (b != 0x00 && b != 0x11)	return TJpgD::JDR_FMT3;	/* Err: Different table number for DC/AC element */
                b = i ? 1 : 0;
                if (!huffbits[b][0] || !huffbits[b][1]) {	/* Check dc/ac huffman table for this component */
                    return (TJpgD::JRESULT)17;//TJpgD::JDR_FMT1;					/* Err: Nnot loaded */
                }
                if (!qttbl[qtid[i]]) {			/* Check dequantizer table for this component */
                    return (TJpgD::JRESULT)18;//TJpgD::JDR_FMT1;					/* Err: Not loaded */
                }
            }
			seg += len;
			dptr = seg;
			if (seg > dpend)
			{ return JDR_INP; }

            /* Allocate working buffer for MCU and RGB */
            if (!msy || !msx) return (TJpgD::JRESULT)19;//TJpgD::JDR_FMT1;					/* Err: SOF0 has not been loaded */
            dbit = 0;
            // dpend = dptr + dctr;
            // --dptr;

            return TJpgD::JDR_OK;		/* Initialization succeeded. Ready to decompress the JPEG image. */

        case 0xC1:	/* SOF1 */
        case 0xC2:	/* SOF2 */
        case 0xC3:	/* SOF3 */
        case 0xC5:	/* SOF5 */
        case 0xC6:	/* SOF6 */
        case 0xC7:	/* SOF7 */
        case 0xC9:	/* SOF9 */
        case 0xCA:	/* SOF10 */
        case 0xCB:	/* SOF11 */
        case 0xCD:	/* SOF13 */
        case 0xCE:	/* SOF14 */
        case 0xCF:	/* SOF15 */
        case 0xD9:	/* EOI */
            return TJpgD::JDR_FMT3;	/* Unsuppoted JPEG standard (may be progressive JPEG) */

        default:	/* Unknown segment (comment, exif or etc..) */
            break;
        }
    }
}




/*-----------------------------------------------------------------------*/
/* Start to decompress the JPEG picture                                  */
/*-----------------------------------------------------------------------*/

TJpgD::JRESULT TJpgD::decomp (
    uint32_t (*outfunc)(TJpgD*, void*, TJpgD::JRECT*),	/* RGB output function */
    uint32_t (*linefunc)(TJpgD*,uint32_t,uint32_t),
    uint32_t lineskip						/* linefunc skip number */
                              )
{
    uint16_t x, y, mx, my;
    uint16_t rst, rsc;
    TJpgD::JRESULT rc;
    uint8_t workbuf[768];
    jd_yuv_t mcubuf[384];
    uint8_t yidx = 0;

    // bayer = (bayer + 1) & 7;

    mx = msx * 8; my = msy * 8;			/* Size of the MCU (pixel) */
    uint16_t lasty = ((height - 1) / my) * my;

    dcv[2] = dcv[1] = dcv[0] = 0;	/* Initialize DC values */
    rst = rsc = 0;

    rc = TJpgD::JDR_OK;
    for (y = 0; y < height; y += my) {		/* Vertical loop of MCUs */
        for (x = 0; x < width; x += mx) {	/* Horizontal loop of MCUs */
            if (nrst && rst++ == nrst) {	/* Process restart interval if enabled */
                rc = restart(this, rsc++);
                if (rc != TJpgD::JDR_OK) return rc;
                rst = 1;
            }
            rc = mcu_load(this, mcubuf, (int32_t*)workbuf);		/* Load an MCU (decompress huffman coded stream and apply IDCT) */
            if (rc != TJpgD::JDR_OK) return rc;
            rc = mcu_output(this, mcubuf, (uint8_t*)workbuf, outfunc, x, y);	/* Output the MCU (color space conversion, scaling and output) */
            if (rc != TJpgD::JDR_OK) return rc;
        }
        if (linefunc && (yidx == lineskip || y == lasty)) {
            linefunc(this, y - yidx * my, yidx * my + ((height < y + my) ? height - y : my));
            yidx = 0;
        } else {
            ++yidx;
        }
    }

    return rc;
}

typedef struct {
    jd_yuv_t* mcubuf = NULL;
    uint_fast16_t x = 0;
    uint_fast16_t y = 0;
    uint_fast8_t h = 0;
    volatile uint_fast8_t queue = false;
} queue_t;

typedef struct {
    TJpgD* jd;
    uint32_t (*outfunc)(TJpgD*, void*, TJpgD::JRECT*);
    uint32_t (*linefunc)(TJpgD*,uint32_t,uint32_t);
    QueueHandle_t sem;
    TaskHandle_t task;
} param_task_output;

//static constexpr uint_fast8_t queue_max = 20;
static constexpr uint_fast8_t queue_max = 24;
static param_task_output param;
static jd_yuv_t mcubufs[queue_max + 1][384];
static queue_t qwrites[queue_max];
static queue_t qline;
static uint_fast8_t qidx = 0;
static uint_fast8_t mcuidx = 0;

static void task_output(void* arg)
{
    uint8_t workbuf[768];
    param_task_output* p = (param_task_output*)arg;
    queue_t* q;
    //Serial.println("task_output start");
    for (;;) {
        if (!xQueueReceive(p->sem, &q, portMAX_DELAY)) continue;
        if (!q) break;
        //Serial.printf("task work: X=%d,Y=%d\r\n",q->x,q->y);
        if (q->h == 0) {
            mcu_output(p->jd, q->mcubuf, workbuf, p->outfunc, q->x, q->y);
        } else {
            p->linefunc(p->jd, q->y, q->h);
        }
        q->queue = false;
        //Serial.println("task work done");
    }
    vQueueDelete(p->sem);
    //Serial.println("task_output end");
    vTaskDelete(NULL);
}

void TJpgD::multitask_begin ()
{
    param.sem = xQueueCreate(queue_max + 1, sizeof(queue_t*));
    xTaskCreatePinnedToCore(task_output, "task_output", 4096, &param, 1, &param.task, 0/*core0*/);
}

void TJpgD::multitask_end ()
{
    queue_t* q = NULL;
#if 0
    xQueueSend(param.sem, &q, 0);
    vTaskDelay(10);
#else
    xQueueSend(param.sem, &q, portMAX_DELAY);
#endif
}

TJpgD::JRESULT TJpgD::decomp_multitask (
    uint32_t (*outfunc)(TJpgD*, void*, TJpgD::JRECT*),	/* RGB output function */
    uint32_t (*linefunc)(TJpgD*,uint32_t,uint32_t),
    uint32_t lineskip						/* linefunc skip number */
                                        )
{
    uint_fast16_t x, y, mx, my;
    uint_fast16_t rst, rsc;
    TJpgD::JRESULT rc;
    uint8_t workbuf[768];
    uint_fast16_t yidx = 0;

    if (ncomp == 1) { /* Erase Cr/Cb for Grayscale */
        jd_yuv_t* b = (jd_yuv_t*)mcubufs;
        size_t end = sizeof(mcubufs) / sizeof(jd_yuv_t);
        do { *b++ = 128; } while (--end);
    }

//    bayer = (bayer + 1) & 7;

    param.jd = this;
    param.outfunc = outfunc;
    param.linefunc = linefunc;
    queue_t* q = &qwrites[qidx];
    queue_t* ql = &qline;
    queue_t* qtmp = NULL;

    mx = msx * 8; my = msy * 8;			/* Size of the MCU (pixel) */

    dcv[2] = dcv[1] = dcv[0] = 0;	/* Initialize DC values */
    rst = rsc = 0;
    uint_fast16_t lasty = ((height - 1) / my) * my;

    rc = TJpgD::JDR_OK;
    y = 0;
    do {		/* Vertical loop of MCUs */
        x = 0;
        do {	/* Horizontal loop of MCUs */
            if (nrst && rst++ == nrst) {	/* Process restart interval if enabled */
                rc = restart(this, rsc++);
                if (rc != TJpgD::JDR_OK) break;
                rst = 1;
            }
            rc = mcu_load(this, mcubufs[mcuidx], (int32_t*)workbuf);
            if (rc != TJpgD::JDR_OK) break;
            if (!q->queue) {
                //mcubufs[mcuidx][0] = 0;
                //mcubufs[mcuidx][1] = 0;
                q->mcubuf  = mcubufs[mcuidx];
                q->x = x;
                q->y = y;
                q->queue = true;
                xQueueSend(param.sem, &q, 0);
                mcuidx = (1 + mcuidx) % (queue_max + 1);
                qidx = (1 + qidx) % queue_max;
                q = &qwrites[qidx];
            } else {
                while (ql->queue) taskYIELD();
                //mcubufs[mcuidx][0] = 0xFF;
                //mcubufs[mcuidx][1] = 0xFF;
                rc = mcu_output(this, mcubufs[mcuidx], workbuf, outfunc, x, y);
            }
        } while ((x += mx) < width && rc == TJpgD::JDR_OK);
        if (rc != TJpgD::JDR_OK) { break; }
        if (linefunc && (yidx == lineskip || y == lasty)) {
            while (ql->queue) taskYIELD();
            while (xQueueReceive(param.sem, &qtmp, 0)) {
                //qtmp->mcubuf[0] = 0xFF;
                //qtmp->mcubuf[1] = 0xFF;
                rc = mcu_output(this, qtmp->mcubuf, workbuf, outfunc, qtmp->x, qtmp->y);
                qtmp->queue = false;
            }
            ql->h = (y == lasty) ? (yidx * my + height - y) : ((lineskip + 1) * my);
            ql->y = y - yidx * my;
            ql->queue = true;
            xQueueSend(param.sem, &ql, 0);
            yidx = 0;
        } else {
            ++yidx;
        }
    } while ((y += my) < height);
    return rc;
}
