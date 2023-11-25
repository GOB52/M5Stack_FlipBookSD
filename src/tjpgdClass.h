/*----------------------------------------------------------------------------/
/ TJpgDec - Tiny JPEG Decompressor R0.03 include file         (C)ChaN, 2021
/-----------------------------------------------------------------------------/
/ original source is here : http://elm-chan.org/fsw/tjpgd/00index.html
/
/ Modified for LGFX  by lovyan03, 2023
/----------------------------------------------------------------------------*/

#ifndef _TJPGDEC_H_
#define _TJPGDEC_H_

/*---------------------------------------------------------------------------*/
#define	TJPGD_SZBUF		512
/* Specifies size of stream input buffer */

#define JD_FORMAT		0
/* Specifies output pixel format.
/  0: RGB888 (24-bit/pix)
/  1: RGB565 (16-bit/pix)
/  2: Grayscale (8-bit/pix)
*/

#define	JD_USE_SCALE	1
/* Switches output descaling feature.
/  0: Disable
/  1: Enable
*/

#define JD_TBLCLIP		0
/* Use table conversion for saturation arithmetic. A bit faster, but increases 1 KB of code size.
/  0: Disable
/  1: Enable
*/

#define JD_FASTDECODE	2
/* Optimization level
/  0: Basic optimization. Suitable for 8/16-bit MCUs.
/  1: + 32-bit barrel shifter. Suitable for 32-bit MCUs.
/  2: + Table conversion for huffman decoding (wants 6 << HUFF_BIT bytes of RAM)
*/
/*---------------------------------------------------------------------------*/

#include <string.h>

#if __has_include (<stdint.h>)
#include <stdint.h>
#elif defined(_WIN32)	/* Main development platform */
typedef unsigned char	uint8_t;
typedef unsigned short	uint16_t;
typedef short			int16_t;
typedef unsigned long	uint32_t;
typedef long			int32_t;
#else				/* Embedded platform */
#include <stdint.h>
#endif

#if JD_FASTDECODE >= 1
typedef int16_t jd_yuv_t;
#else
typedef uint8_t jd_yuv_t;
#endif


/* Decompressor object structure */
typedef struct TJpgD TJpgD;
struct TJpgD {
    /* Error code */
    typedef enum {
        JDR_OK = 0,	/* 0: Succeeded */
        JDR_INTR,	/* 1: Interrupted by output function */	
        JDR_INP,	/* 2: Device error or wrong termination of input stream */
        JDR_MEM1,	/* 3: Insufficient memory pool for the image */
        JDR_MEM2,	/* 4: Insufficient stream input buffer */
        JDR_PAR,	/* 5: Parameter error */
        JDR_FMT1,	/* 6: Data format error (may be damaged data) */
        JDR_FMT2,	/* 7: Right format but not supported */
        JDR_FMT3	/* 8: Not supported JPEG standard */
    } JRESULT;

    /* Rectangular structure */
    typedef struct {
        int_fast16_t left, right, top, bottom;
    } JRECT;

	uint8_t* dptr;				/* Current data read ptr */
	uint8_t* dpend;				/* Current data end ptr */
	uint8_t* inbuf;				/* Bit stream input buffer */
	uint8_t dbit;				/* Number of bits availavble in wreg or reading bit mask */
	uint8_t scale;				/* Output scaling ratio */
	uint8_t msx, msy;			/* MCU size in unit of block (width, height) */
	uint8_t qtid[3];			/* Quantization table ID of each component, Y, Cb, Cr */
	uint8_t ncomp;				/* Number of color components 1:grayscale, 3:color */
	int16_t dcv[3];				/* Previous DC element of each component */
	uint16_t nrst;				/* Restart inverval */
	uint16_t width, height;		/* Size of the input image (pixel) */
	uint8_t* huffbits[2][2];	/* Huffman bit distribution tables [id][dcac] */
	uint16_t* huffcode[2][2];	/* Huffman code word tables [id][dcac] */
	uint8_t* huffdata[2][2];	/* Huffman decoded data tables [id][dcac] */
	int32_t* qttbl[4];			/* Dequantizer tables [id] */
#if JD_FASTDECODE >= 1
	uint32_t wreg;				/* Working shift register */
	uint8_t marker;				/* Detected marker (0:None) */
#if JD_FASTDECODE == 2
	uint8_t longofs[2][2];		/* Table offset of long code [id][dcac] */
	uint16_t* hufflut_ac[2];	/* Fast huffman decode tables for AC short code [id] */
	uint8_t* hufflut_dc[2];		/* Fast huffman decode tables for DC short code [id] */
#endif
#endif
//	voi·* workbuf;				/* Working buffer for IDCT and RGB output */
//	jd_yuv_t* mcubuf;			/* Working buffer for the MCU */
	size_t sz_pool;				/* Size of momory pool (bytes available) */
    uint32_t (*infunc)(TJpgD*, uint8_t*, uint32_t);	/* Pointer to jpeg stream input function */
	void* device;				/* Pointer to I/O device identifiler for the session */

    JRESULT prepare (uint32_t(*)(TJpgD*,uint8_t*,uint32_t), void*);
    JRESULT decomp (uint32_t(*)(TJpgD*,void*,JRECT*), uint32_t(*)(TJpgD*,uint32_t,uint32_t) = 0, uint32_t = 0);
    JRESULT decomp_multitask (uint32_t(*)(TJpgD*,void*,JRECT*), uint32_t(*)(TJpgD*,uint32_t,uint32_t) = 0, uint32_t = 0);
    static void multitask_begin ();
    static void multitask_end ();
};
#endif /* _TJPGDEC */
