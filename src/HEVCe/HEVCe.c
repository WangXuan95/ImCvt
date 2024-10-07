// Copyright : github.com/WangXuan95
//
// A light-weight H.265/HEVC intra frame encoder for monochrome (grayscale) still image compression.
// This code may be the most understandable HEVC implementation in the world.
//
//
// definations of abbr. :
//
//   pix : pixel
//  coef : coefficient
//
//   hor : horizontal
//   ver : vertical
//    sz : size
//   pos : position
//
//  orig : original
//  pred : predicted
//  quat : quantized
//  rcon : reconstructed
//
//   img : image
//   CTU : code tree unit (32x32)
//   CU  : code unit (8x8, 16x16, or 32x32)
//   blk : block
//   CG  : coefficient group (4x4)
//   pix : pixel
//
//   bla  : border on left-above  (which contains 1 pixel)
//   blb  : border on left  and left-below  (which contains 2*CU-size pixels)
//   bar  : border on above and above-right (which contains 2*CU-size pixels)
//          Note: If the prefix "u" is added to the above abbr., it means "unfiltered", and the prefix "f" means "filtered". For example: ublb, fblb
//  pmode : prediction mode (0~34)
//
//  rdcost: RD-cost (Rate-Distortion Cost)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////





///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// data type definations. This HEVC encoder will only use these data types
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef  unsigned char  BOOL;          // unsigned integer, 8-bit, used as boolean: 0=false 1=true
typedef  unsigned char  UI8;           // unsigned integer, 8-bit
typedef            int  I32;           // signed integer, must be at least 32 bits 





///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// common used definations and functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef    NULL
#define    NULL                 0
#endif

#define    MAX_YSZ              8192                               // max image height
#define    MAX_XSZ              8192                               // max image width

#define    CTU_SZ               32                                 // CTU        : 32x32
#define    MIN_CU_SZ            8                                  // minimal CU : 8x8
#define    MIN_TU_SZ            4                                  // minimal TU : 4x4

#define    GETnTU(i)            ((i) / MIN_TU_SZ)
#define    nTUinCTU             GETnTU(CTU_SZ)                     // number of rows/colums of minimal TU in a CTU , =8
#define    nTUinROW             GETnTU(MAX_XSZ)                    // number of columns of minimal TU in image row

#define    CG_SZ                4                                  // coefficient group (CG) size
#define    CG_SZxSZ             (CG_SZ*CG_SZ)

#define    GETnCG(i)            ((i) / CG_SZ)                      // input a pixel vertical or horizontal position, output its CG's vertical or horizontal position
#define    GETn_inCG(i)         ((i) % CG_SZ)                      // input a pixel vertical or horizontal position, output its vertical or horizontal position in the CG

#define    GETi_inCG(i)         ((i) % CG_SZxSZ)                   // input a pixel scan index, output its scan index in the CG

typedef enum {
    SCAN_TYPE_DIAG = 0,                                            // CG scan order : diag
    SCAN_TYPE_HOR  = 1,                                            // CG scan order : horizontal
    SCAN_TYPE_VER  = 2                                             // CG scan order : vertical
} ScanType ;

typedef enum {
    CH_Y = 0,                                                      // Y : luma channel
    CH_U = 1,                                                      // U : chroma-blue channel (Cb)
    CH_V = 2                                                       // V : chroma-red  channel (Cr)
} ChannelType ;

#define    PMODE_PLANAR         0                                  // planar prediction mode
#define    PMODE_DC             1                                  // DC prediction mode
#define    PMODE_DEG45          2                                  // angular prediction mode : up right   ( 45 degree)
#define    PMODE_HOR            10                                 // angular prediction mode : right      ( 90 degree)
#define    PMODE_DEG135         18                                 // angular prediction mode : down right (135 degree)
#define    PMODE_VER            26                                 // angular prediction mode : down       (180 degree)
#define    PMODE_DEG225         34                                 // angular prediction mode : down left  (225 degree)
#define    PMODE_COUNT          35                                 // there are total 35 prediction modes (0~34)

#define    I32_MAX_VALUE        ((I32)(0x7fffffff))

#define    PIX_MIN_VALUE        ((UI8)(  0))
#define    PIX_MAX_VALUE        ((UI8)(255))
#define    PIX_MIDDLE_VALUE     ((UI8)(128))

#define    COEF_MIN_VALUE       ((I32)(-32768))
#define    COEF_MAX_VALUE       ((I32)( 32767))

#define    ABS(x)               ( ((x) < 0) ? (-(x)) : (x) )                                                 // get absolute value
#define    MAX(x, y)            ( ((x)<(y)) ? (y) : (x) )                                                    // get the minimum value of x, y
#define    MIN(x, y)            ( ((x)<(y)) ? (x) : (y) )                                                    // get the maximum value of x, y
#define    CLIP(x, min, max)    ( MIN(MAX((x), (min)), (max)) )                                              // clip x between min~max
#define    PIX_CLIP(x)          ( (UI8)CLIP((x),  PIX_MIN_VALUE,  PIX_MAX_VALUE) )                           // clip x between 0~255 , and convert it to UI8 type (pixel type)
#define    COEF_CLIP(x)         ( (I32)CLIP((x), COEF_MIN_VALUE, COEF_MAX_VALUE) )                           // clip x between -32768~32767 (HEVC-specified coefficient range)


#define GET2D(ptr, ysz, xsz, y, x) ( *( (ptr) + (xsz)*CLIP((y),0,(ysz)-1) + CLIP((x),0,(xsz)-1) ) )          // regard a 1-D array (ptr) as a 2-D array, and get value from position (y,x)


#define BLK_SET(sz, value, dst) {                                               \
    I32 i, j;                                                                   \
    for (i=0; i<(sz); i++)                                                      \
        for (j=0; j<(sz); j++)                                                  \
            (dst)[i][j] = (value);                                              \
}


#define BLK_COPY(sz, src, dst) {                                                \
    I32 i, j;                                                                   \
    for (i=0; i<(sz); i++)                                                      \
        for (j=0; j<(sz); j++)                                                  \
            (dst)[i][j] = (src)[i][j];                                          \
}


#define BLK_SUB(sz, src1, src2, dst) {                                          \
    I32 i, j;                                                                   \
    for (i=0; i<(sz); i++)                                                      \
        for (j=0; j<(sz); j++)                                                  \
            (dst)[i][j] = ((I32)(src1)[i][j]) - (src2)[i][j];                   \
}


#define BLK_ADD_CLIP_TO_PIX(sz, src1, src2, dst) {                              \
    I32 i, j;                                                                   \
    for (i=0; i<(sz); i++)                                                      \
        for (j=0; j<(sz); j++)                                                  \
            (dst)[i][j] = PIX_CLIP( (src1)[i][j] + (src2)[i][j] );              \
}


BOOL blkNotAllZero (const I32 sz, const I32 src [][CTU_SZ]) {
    I32 i, j;
    for (i=0; i<sz; i++)
        for (j=0; j<sz; j++)
            if (src[i][j] != 0)
                return 1;
    return 0;
}


// calculate SSE (sum of squared error) as distortion, the result will be save on result
#define CALC_BLK_SSE(sz, src1, src2, result) {                                  \
    I32 i, j, diff;                                                             \
    (result) = 0;                                                               \
    for (i=0; i<(sz); i++) {                                                    \
        for (j=0; j<(sz); j++) {                                                \
            diff = ABS( (I32)(src1)[i][j] - (src2)[i][j] ) ;                    \
            (result) += diff * diff;                                            \
        }                                                                       \
    }                                                                           \
}


I32 calcRDcost (I32 qpd6, I32 dist, I32 bits) {                                                  // calculate RD-cost, avoid overflow from 32-bit integer
    static const I32 RDCOST_WEIGHT_DIST [] = {11, 11, 11,  5,  1};
    static const I32 RDCOST_WEIGHT_BITS [] = { 1,  4, 16, 29, 23};
    const I32  weight1 = RDCOST_WEIGHT_DIST[qpd6];
    const I32  weight2 = RDCOST_WEIGHT_BITS[qpd6];
    const I32  cost1 = (I32_MAX_VALUE / weight1 <= dist) ? I32_MAX_VALUE : weight1 * dist;       // avoiding multiply overflow
    const I32  cost2 = (I32_MAX_VALUE / weight2 <= bits) ? I32_MAX_VALUE : weight2 * bits;       // avoiding multiply overflow
    return             (I32_MAX_VALUE - cost1 <= cost2)  ? I32_MAX_VALUE : cost1 + cost2;        // avoiding add overflow
}





///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// prediction 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// description : get border pixels (ubla, ublb, ubar) from the reconstructed image for prediction.
void getBorder (
    const I32  sz,                                            // block size
    const BOOL bll_exist,
    const BOOL blb_exist,
    const BOOL baa_exist,
    const BOOL bar_exist,
    const UI8  blk_rcon [][1+CTU_SZ*2],
          UI8  ubla  [1],
          UI8  ublb  [CTU_SZ*2],
          UI8  ubar  [CTU_SZ*2],
          UI8  fbla  [1],
          UI8  fblb  [CTU_SZ*2],
          UI8  fbar  [CTU_SZ*2]
) {
    I32 i;
    
    if      (bll_exist && baa_exist)                         // 1st, construct border on left-above pixel
        ubla[0] = blk_rcon[-1][-1];
    else if (bll_exist)
        ubla[0] = blk_rcon[ 0][-1];
    else if (baa_exist)
        ubla[0] = blk_rcon[-1][ 0];
    else
        ubla[0] = PIX_MIDDLE_VALUE;
    
    for (i=0; i<sz; i++)                                     // 2nd, construct border on left pixels
        if (bll_exist)
            ublb[i] = blk_rcon[i][-1];
        else
            ublb[i] = ubla[0];
    
    for (i=sz; i<sz*2; i++)                                  // 3rd, construct border on left-below pixels
        if (blb_exist)
            ublb[i] = blk_rcon[i][-1];
        else
            ublb[i] = ublb[sz-1];
    
    for (i=0; i<sz; i++)                                     // 4th, construct border on above pixels
        if (baa_exist)
            ubar[i] = blk_rcon[-1][i];
        else
            ubar[i] = ubla[0];
    
    for (i=sz; i<sz*2; i++)                                  // 5th, construct border on above-right pixels
        if (bar_exist)
            ubar[i] = blk_rcon[-1][i];
        else
            ubar[i] = ubar[sz-1];
    
    // filter border ---------------------------------------------------------------------------------------
    fbla[0] = ( 2 + ublb[0] + ubar[0] + 2*ubla[0] ) >> 2;
    fblb[0] = ( 2 + 2*ublb[0] + ublb[1] + ubla[0] ) >> 2;
    fbar[0] = ( 2 + 2*ubar[0] + ubar[1] + ubla[0] ) >> 2;
    
    for (i=1; i<sz*2-1; i++) {
        fblb[i] = ( 2 + 2*ublb[i] + ublb[i-1] + ublb[i+1] ) >> 2;
        fbar[i] = ( 2 + 2*ubar[i] + ubar[i-1] + ubar[i+1] ) >> 2;
    }
    
    fblb[sz*2-1] = ublb[sz*2-1];
    fbar[sz*2-1] = ubar[sz*2-1];
}



// description : do prediction, getting the predicted block
void predict (
    const I32  sz,
    const ChannelType  ch,
    const I32  pmode,
    const UI8  ubla,
    const UI8  ublb  [CTU_SZ*2],
    const UI8  ubar  [CTU_SZ*2],
    const UI8  fbla,
    const UI8  fblb  [CTU_SZ*2],
    const UI8  fbar  [CTU_SZ*2],
          UI8  dst   [CTU_SZ][CTU_SZ]                                                 // the predict result block will be put here
) {
    static const BOOL WHETHER_FILTER_BORDER_FOR_Y_TABLE [][35] = {
      { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },      // sz = 4x4   , pmode = 0~34
      { 1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 },      // sz = 8x8   , pmode = 0~34
      { 1,0,1,1,1,1,1,1,1,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,1,1,1,1,1,1,1 },      // sz = 16x16 , pmode = 0~34
      { 0 },
      { 1,0,1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1 }       // sz = 32x32 , pmode = 0~34
    };

    static const I32         ANGLE_TABLE [] = {0, 0,  32,  26,  21,  17,  13,   9,    5, 2   , 0,   -2,   -5,  -9, -13, -17, -21, -26, -32, -26, -21, -17, -13,  -9,   -5,   -2, 0,    2,    5,   9,  13,  17,  21,  26,  32 };
    static const I32 ABS_INV_ANGLE_TABLE [] = {0, 0, 256, 315, 390, 482, 630, 910, 1638, 4096, 0, 4096, 1638, 910, 630, 482, 390, 315, 256, 315, 390, 482, 630, 910, 1638, 4096, 0, 4096, 1638, 910, 630, 482, 390, 315, 256 };
    
    const BOOL whether_filter_edge   = (ch==CH_Y) && (sz <= 16);
    const BOOL whether_filter_border = (ch==CH_Y) && WHETHER_FILTER_BORDER_FOR_Y_TABLE[sz/8][pmode];
    const UI8  bla = whether_filter_border ? fbla : ubla;
    const UI8 *blb = whether_filter_border ? fblb : ublb;
    const UI8 *bar = whether_filter_border ? fbar : ubar;
    
    I32 i, j;
    
    if        ( pmode == PMODE_PLANAR ) {                                        // planar mode
        for (i=0; i<sz; i++) {
            for (j=0; j<sz; j++) {
                const I32 hor_pred = (sz-j-1) * blb[i] + (j+1) * bar[sz];
                const I32 ver_pred = (sz-i-1) * bar[j] + (i+1) * blb[sz];
                dst[i][j] = (UI8)( (sz + hor_pred + ver_pred) / (sz*2) );
            }
        }
        
    } else if ( pmode == PMODE_DC     ) {                                        // DC mode
        I32 dc_pix = sz;
        for (i=0; i<sz; i++)
            dc_pix += blb[i] + bar[i];
        dc_pix /= (sz*2);                                                        // calculate the DC value (mean value of border pixels)
        
        for (i=0; i<sz; i++)
            for (j=0; j<sz; j++)
                dst[i][j] = (UI8)dc_pix;                                         // fill all predict pixels with dc_pix
        
        if (whether_filter_edge) {                                               // apply the edge filter for DC mode
            dst[0][0] = (UI8)( (2 + 2*dc_pix + blb[0] + bar[0]) >> 2 );          // filter the top-left pixel        of the predicted CU
            for (i=1; i<sz; i++) {
                dst[0][i] = (UI8)( (2 + 3*dc_pix + bar[i] ) >> 2 );              // filter the pixels in top row     of the predicted CU (except the top-left pixel)
                dst[i][0] = (UI8)( (2 + 3*dc_pix + blb[i] ) >> 2 );              // filter the pixels in left column of the predicted CU (except the top-left pixel)
            }
        }
        
    } else if ( pmode == PMODE_HOR    ) {                                        // angular mode : pure horizontal
        for (i=0; i<sz; i++)
            for (j=0; j<sz; j++)
                dst[i][j] = blb[i];
        
        if (whether_filter_edge)
            for (j=0; j<sz; j++) {
                const I32 bias = (bar[j] - bla) >> 1;
                dst[0][j] = PIX_CLIP( bias + dst[0][j] );
            }
        
    } else if ( pmode == PMODE_VER    ) {                                        // angular mode : pure vertical
        for (i=0; i<sz; i++)
            for (j=0; j<sz; j++)
                dst[i][j] = bar[j];
        
        if (whether_filter_edge)
            for (i=0; i<sz; i++) {
                const I32 bias = (blb[i] - bla) >> 1;
                dst[i][0] = PIX_CLIP( bias + dst[i][0] );
            }
        
    } else {                                                                     // pmode = 2~9, 11~25, 27~34  (angular mode without pure horizontal and pure vertical)
        const BOOL is_horizontal = (pmode < PMODE_DEG135);
        const I32  angle         = ANGLE_TABLE        [pmode];
        const I32  abs_inv_angle = ABS_INV_ANGLE_TABLE[pmode];
        
        const UI8 *bmain = is_horizontal ? blb : bar;
        const UI8 *bside = is_horizontal ? bar : blb;
        
        UI8  ref_buff0 [CTU_SZ*4+1] ;
        UI8 *ref_buff = ref_buff0 + CTU_SZ*2;
        
        ref_buff[0] = bla;
        
        for (i=0; i<sz*2; i++)
            ref_buff[1+i] = bside[i];
        
        for (i=-1; i>((sz*angle)>>5); i--) {
            j = (128 - abs_inv_angle * i) >> 8;
            ref_buff[i] = ref_buff[j];
        }
        
        for (i=0; i<sz*2; i++)
            ref_buff[1+i] = bmain[i];
        
        for (i=0; i<sz; i++) {
            const I32 offset   = angle * (i+1);
            const I32 offset_i = offset >> 5;
            const I32 offset_f = offset & 0x1f;
            for (j=0; j<sz; j++) {
                const UI8 pix1 = ref_buff[offset_i+j+1];
                const UI8 pix2 = ref_buff[offset_i+j+2];
                const UI8 pix  = (UI8)( ( (32-offset_f)*pix1 + offset_f*pix2 + 16 ) >> 5 );
                if (is_horizontal)
                    dst[j][i] = pix;
                else
                    dst[i][j] = pix;
            }
        }
    }
}





///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// transform and inverse transform
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const I32 DST4_MAT  [][CTU_SZ] = {       // DST matrix for block 4x4
  { 29,  55,  74,  84 },
  { 74,  74,   0, -74 }, 
  { 84, -29, -74,  55 },
  { 55, -84,  74, -29 }
};


const I32 DCT8_MAT  [][CTU_SZ] = {       // DCT matrix for block 8x8
  { 64,  64,  64,  64,  64,  64,  64,  64 },
  { 89,  75,  50,  18, -18, -50, -75, -89 },
  { 83,  36, -36, -83, -83, -36,  36,  83 },
  { 75, -18, -89, -50,  50,  89,  18, -75 },
  { 64, -64, -64,  64,  64, -64, -64,  64 },
  { 50, -89,  18,  75, -75, -18,  89, -50 },
  { 36, -83,  83, -36, -36,  83, -83,  36 },
  { 18, -50,  75, -89,  89, -75,  50, -18 }
};


const I32 DCT16_MAT [][CTU_SZ] = {       // DCT matrix for block 16x16
  { 64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64 },
  { 90,  87,  80,  70,  57,  43,  25,   9,  -9, -25, -43, -57, -70, -80, -87, -90 },
  { 89,  75,  50,  18, -18, -50, -75, -89, -89, -75, -50, -18,  18,  50,  75,  89 },
  { 87,  57,   9, -43, -80, -90, -70, -25,  25,  70,  90,  80,  43,  -9, -57, -87 },
  { 83,  36, -36, -83, -83, -36,  36,  83,  83,  36, -36, -83, -83, -36,  36,  83 },
  { 80,   9, -70, -87, -25,  57,  90,  43, -43, -90, -57,  25,  87,  70,  -9, -80 },
  { 75, -18, -89, -50,  50,  89,  18, -75, -75,  18,  89,  50, -50, -89, -18,  75 },
  { 70, -43, -87,   9,  90,  25, -80, -57,  57,  80, -25, -90,  -9,  87,  43, -70 },
  { 64, -64, -64,  64,  64, -64, -64,  64,  64, -64, -64,  64,  64, -64, -64,  64 },
  { 57, -80, -25,  90,  -9, -87,  43,  70, -70, -43,  87,   9, -90,  25,  80, -57 },
  { 50, -89,  18,  75, -75, -18,  89, -50, -50,  89, -18, -75,  75,  18, -89,  50 },
  { 43, -90,  57,  25, -87,  70,   9, -80,  80,  -9, -70,  87, -25, -57,  90, -43 },
  { 36, -83,  83, -36, -36,  83, -83,  36,  36, -83,  83, -36, -36,  83, -83,  36 },
  { 25, -70,  90, -80,  43,   9, -57,  87, -87,  57,  -9, -43,  80, -90,  70, -25 },
  { 18, -50,  75, -89,  89, -75,  50, -18, -18,  50, -75,  89, -89,  75, -50,  18 },
  {  9, -25,  43, -57,  70, -80,  87, -90,  90, -87,  80, -70,  57, -43,  25,  -9 }
};


const I32 DCT32_MAT [][CTU_SZ] = {       // DCT matrix for block 32x32
  { 64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64 },
  { 90,  90,  88,  85,  82,  78,  73,  67,  61,  54,  46,  38,  31,  22,  13,   4,  -4, -13, -22, -31, -38, -46, -54, -61, -67, -73, -78, -82, -85, -88, -90, -90 },
  { 90,  87,  80,  70,  57,  43,  25,   9,  -9, -25, -43, -57, -70, -80, -87, -90, -90, -87, -80, -70, -57, -43, -25,  -9,   9,  25,  43,  57,  70,  80,  87,  90 },
  { 90,  82,  67,  46,  22,  -4, -31, -54, -73, -85, -90, -88, -78, -61, -38, -13,  13,  38,  61,  78,  88,  90,  85,  73,  54,  31,   4, -22, -46, -67, -82, -90 },
  { 89,  75,  50,  18, -18, -50, -75, -89, -89, -75, -50, -18,  18,  50,  75,  89,  89,  75,  50,  18, -18, -50, -75, -89, -89, -75, -50, -18,  18,  50,  75,  89 },
  { 88,  67,  31, -13, -54, -82, -90, -78, -46,  -4,  38,  73,  90,  85,  61,  22, -22, -61, -85, -90, -73, -38,   4,  46,  78,  90,  82,  54,  13, -31, -67, -88 },
  { 87,  57,   9, -43, -80, -90, -70, -25,  25,  70,  90,  80,  43,  -9, -57, -87, -87, -57,  -9,  43,  80,  90,  70,  25, -25, -70, -90, -80, -43,   9,  57,  87 },
  { 85,  46, -13, -67, -90, -73, -22,  38,  82,  88,  54,  -4, -61, -90, -78, -31,  31,  78,  90,  61,   4, -54, -88, -82, -38,  22,  73,  90,  67,  13, -46, -85 },
  { 83,  36, -36, -83, -83, -36,  36,  83,  83,  36, -36, -83, -83, -36,  36,  83,  83,  36, -36, -83, -83, -36,  36,  83,  83,  36, -36, -83, -83, -36,  36,  83 },
  { 82,  22, -54, -90, -61,  13,  78,  85,  31, -46, -90, -67,   4,  73,  88,  38, -38, -88, -73,  -4,  67,  90,  46, -31, -85, -78, -13,  61,  90,  54, -22, -82 },
  { 80,   9, -70, -87, -25,  57,  90,  43, -43, -90, -57,  25,  87,  70,  -9, -80, -80,  -9,  70,  87,  25, -57, -90, -43,  43,  90,  57, -25, -87, -70,   9,  80 },
  { 78,  -4, -82, -73,  13,  85,  67, -22, -88, -61,  31,  90,  54, -38, -90, -46,  46,  90,  38, -54, -90, -31,  61,  88,  22, -67, -85, -13,  73,  82,   4, -78 },
  { 75, -18, -89, -50,  50,  89,  18, -75, -75,  18,  89,  50, -50, -89, -18,  75,  75, -18, -89, -50,  50,  89,  18, -75, -75,  18,  89,  50, -50, -89, -18,  75 },
  { 73, -31, -90, -22,  78,  67, -38, -90, -13,  82,  61, -46, -88,  -4,  85,  54, -54, -85,   4,  88,  46, -61, -82,  13,  90,  38, -67, -78,  22,  90,  31, -73 },
  { 70, -43, -87,   9,  90,  25, -80, -57,  57,  80, -25, -90,  -9,  87,  43, -70, -70,  43,  87,  -9, -90, -25,  80,  57, -57, -80,  25,  90,   9, -87, -43,  70 },
  { 67, -54, -78,  38,  85, -22, -90,   4,  90,  13, -88, -31,  82,  46, -73, -61,  61,  73, -46, -82,  31,  88, -13, -90,  -4,  90,  22, -85, -38,  78,  54, -67 },
  { 64, -64, -64,  64,  64, -64, -64,  64,  64, -64, -64,  64,  64, -64, -64,  64,  64, -64, -64,  64,  64, -64, -64,  64,  64, -64, -64,  64,  64, -64, -64,  64 },
  { 61, -73, -46,  82,  31, -88, -13,  90,  -4, -90,  22,  85, -38, -78,  54,  67, -67, -54,  78,  38, -85, -22,  90,   4, -90,  13,  88, -31, -82,  46,  73, -61 },
  { 57, -80, -25,  90,  -9, -87,  43,  70, -70, -43,  87,   9, -90,  25,  80, -57, -57,  80,  25, -90,   9,  87, -43, -70,  70,  43, -87,  -9,  90, -25, -80,  57 },
  { 54, -85,  -4,  88, -46, -61,  82,  13, -90,  38,  67, -78, -22,  90, -31, -73,  73,  31, -90,  22,  78, -67, -38,  90, -13, -82,  61,  46, -88,   4,  85, -54 },
  { 50, -89,  18,  75, -75, -18,  89, -50, -50,  89, -18, -75,  75,  18, -89,  50,  50, -89,  18,  75, -75, -18,  89, -50, -50,  89, -18, -75,  75,  18, -89,  50 },
  { 46, -90,  38,  54, -90,  31,  61, -88,  22,  67, -85,  13,  73, -82,   4,  78, -78,  -4,  82, -73, -13,  85, -67, -22,  88, -61, -31,  90, -54, -38,  90, -46 },
  { 43, -90,  57,  25, -87,  70,   9, -80,  80,  -9, -70,  87, -25, -57,  90, -43, -43,  90, -57, -25,  87, -70,  -9,  80, -80,   9,  70, -87,  25,  57, -90,  43 },
  { 38, -88,  73,  -4, -67,  90, -46, -31,  85, -78,  13,  61, -90,  54,  22, -82,  82, -22, -54,  90, -61, -13,  78, -85,  31,  46, -90,  67,   4, -73,  88, -38 },
  { 36, -83,  83, -36, -36,  83, -83,  36,  36, -83,  83, -36, -36,  83, -83,  36,  36, -83,  83, -36, -36,  83, -83,  36,  36, -83,  83, -36, -36,  83, -83,  36 },
  { 31, -78,  90, -61,   4,  54, -88,  82, -38, -22,  73, -90,  67, -13, -46,  85, -85,  46,  13, -67,  90, -73,  22,  38, -82,  88, -54,  -4,  61, -90,  78, -31 },
  { 25, -70,  90, -80,  43,   9, -57,  87, -87,  57,  -9, -43,  80, -90,  70, -25, -25,  70, -90,  80, -43,  -9,  57, -87,  87, -57,   9,  43, -80,  90, -70,  25 },
  { 22, -61,  85, -90,  73, -38,  -4,  46, -78,  90, -82,  54, -13, -31,  67, -88,  88, -67,  31,  13, -54,  82, -90,  78, -46,   4,  38, -73,  90, -85,  61, -22 },
  { 18, -50,  75, -89,  89, -75,  50, -18, -18,  50, -75,  89, -89,  75, -50,  18,  18, -50,  75, -89,  89, -75,  50, -18, -18,  50, -75,  89, -89,  75, -50,  18 },
  { 13, -38,  61, -78,  88, -90,  85, -73,  54, -31,   4,  22, -46,  67, -82,  90, -90,  82, -67,  46, -22,  -4,  31, -54,  73, -85,  90, -88,  78, -61,  38, -13 },
  {  9, -25,  43, -57,  70, -80,  87, -90,  90, -87,  80, -70,  57, -43,  25,  -9,  -9,  25, -43,  57, -70,  80, -87,  90, -90,  87, -80,  70, -57,  43, -25,   9 },
  {  4, -13,  22, -31,  38, -46,  54, -61,  67, -73,  78, -82,  85, -88,  90, -90,  90, -90,  88, -85,  82, -78,  73, -67,  61, -54,  46, -38,  31, -22,  13,  -4 }
};



// description : matrix multiply
void matMul (
    const I32  sz,
    const BOOL src1_transpose,
    const BOOL src2_transpose,
    const I32  dst_sft,
    const BOOL dst_clip,
    const I32  src1  [][CTU_SZ],
    const I32  src2  [][CTU_SZ],
          I32  dst   [][CTU_SZ]
) {
    const I32 dst_add = 1 << dst_sft >> 1;
    I32 i, j, k, s;
    for (i=0; i<sz; i++) {
        for (j=0; j<sz; j++) {
            s = dst_add;
            for (k=0; k<sz; k++)
                s += (src1_transpose ? src1[k][i] : src1[i][k]) * (src2_transpose ? src2[j][k] : src2[k][j]);
            s >>= dst_sft;
            if (dst_clip)
                s = COEF_CLIP(s);
            dst[i][j] = s;
        }
    }
}



// description : do transform (DCT or 4x4 DST) , or inverse transform  (inv-DCT or 4x4 inv-DST)
void transform (
    const I32  sz,                                        // block size
    const BOOL inverse,                                   // 0:transform    1:inverse transform
    const I32  src   [][CTU_SZ],
          I32  dst   [][CTU_SZ]
) {
    //                                                TU size     4x4       8x8      16x16            32x32
    static const I32   TABLE_A_FOR_TRANSFORM   []       = {       1 ,       2,         3,   -1,         4};
    static const I32 (*TABLE_TRANSFORM_MAT[5]) [CTU_SZ] = {DST4_MAT, DCT8_MAT, DCT16_MAT, NULL, DCT32_MAT};

    I32  tmp [CTU_SZ][CTU_SZ];

    const I32 (*mat) [CTU_SZ] = TABLE_TRANSFORM_MAT[sz/8];

    const I32 a = inverse ?  7 : TABLE_A_FOR_TRANSFORM[sz/8];
    const I32 b = inverse ? 12 : a + 7;

    matMul(sz, inverse,  0, a, inverse, mat, src, tmp);                // (W = C * X) for transform , (W = CT * X) for inverse-transform
    matMul(sz, 0, !inverse, b, inverse, tmp, mat, dst);
}





///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// quantize and de-quantize
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

I32 estimateCoeffRate (I32 level) {
    static const I32 LEVEL_RATE_TABLE [] = {0, 70000, 90000, 92000, 157536, 190304};
    I32 i;
    if ( level < 6 )
        return LEVEL_RATE_TABLE[level];
    level -= 6;
    for (i=0; (1<<i)<=level; i++)
        level -= 1<<i;
    return 92000 + ((3+i*2+1)<<15);
}



// description : simplified rate-distortion optimized quantize (RDOQ) for a TU
void quantize (
    const I32  qpd6,
    const I32  sz,
    const I32  src [][CTU_SZ],
          I32  dst [][CTU_SZ]
) {
    //                            TU size     4x4   8x8   16x16      32x32
    static const I32 DIST_SHIFT_TABLE  [] = {   8 ,   7,      6, -1,     5};
    static const I32 LEVEL_SHIFT_TABLE [] = {  19 ,  18,     17, -1,    16};

    const I32  dist_sft =  DIST_SHIFT_TABLE[sz/8];
    const I32  sft      = LEVEL_SHIFT_TABLE[sz/8] + qpd6;
    const I32  add      = (1<<sft>>1);
    const I32  max_dlevel = I32_MAX_VALUE - add;
    const I32  cg_dlevel_threshold = 9 << sft >> 2;

    I32  yc, xc, y, x;
    
    for (yc=0; yc<sz; yc+=CG_SZ) {                                                                  // for all CGs
        for (xc=0; xc<sz; xc+=CG_SZ) {
            I32 cg_sum_dlevel = 0;

            for (y=yc; y<yc+CG_SZ; y++) {                                                           // for all coefficients in this CG
                for (x=xc; x<xc+CG_SZ; x++) {
                    I32  absval    = ABS(src[y][x]);
                    I32  dlevel    = (absval>0x1ffff) ? max_dlevel : MIN( (absval & 0x1ffff)<<14 , max_dlevel );
                    I32  level     = COEF_CLIP( (dlevel+add) >> sft );
                    I32  min_level = MAX(0, level-2);
                    I32  best_cost = I32_MAX_VALUE;

                    for (; level>=min_level; level--) {
                        I32 dist1 = ABS( dlevel-(level<<sft) ) >> dist_sft;
                        I32 dist  = ( (dist1<46340) ? (dist1*dist1) : I32_MAX_VALUE ) >> 7;         // 46340^2 ~= I32_MAX_VALUE
                        I32 cost  = calcRDcost(qpd6, dist, estimateCoeffRate(level));

                        if (cost < best_cost) {                                                     // if current cost is smaller than previous cost
                            best_cost = cost;
                            dst[y][x] = level;
                        }
                    }

                    if (src[y][x] < 0)
                        dst[y][x] *= -1;                                                            // recover the sign
                    
                    cg_sum_dlevel += MIN( dlevel , cg_dlevel_threshold );
                }
            }

            if ( cg_sum_dlevel < cg_dlevel_threshold )                                              // if this CG is too weak
                for (y=yc; y<yc+CG_SZ; y++)
                    for (x=xc; x<xc+CG_SZ; x++)
                        dst[y][x] = 0;                                                              // clear all items in CG
        }
    }
}



// description : de-quantize
void deQuantize (
    const I32  qpd6,
    const I32  sz,
    const I32  src [][CTU_SZ],
          I32  dst [][CTU_SZ]
) {
    //                         TU size    4x4   8x8  16x16     32x32
    static const I32 Q_SHIFT_TABLE [5] = { 5 ,   4,     3, -1,    2};

    const I32 q_sft = Q_SHIFT_TABLE[sz/8] + qpd6;
    I32 i, j;

    for (i=0; i<sz; i++)
        for (j=0; j<sz; j++)
            dst[i][j] = COEF_CLIP( src[i][j] << q_sft );
}





///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// functions for putting HEVC header to a buffer (byte array)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void putBitsToBuffer (UI8 **ppbuf, I32 *bitpos, I32 bits, I32 len) {
    for (len--; len>=0; len--) {
        UI8 bit = (bits>>len) & 1;
        if (bit)
            **ppbuf |=  (1<<(*bitpos));           // set the bit to 1, and do not effect other bits of this byte
        else
            **ppbuf &= ~(1<<(*bitpos));           // set the bit to 0, and do not effect other bits of this byte
        if ((*bitpos) > 0) {                      // current byte not end
            (*bitpos) --;
        } else {                                  // current byte end
            (*bitpos) = 7;
            (*ppbuf) ++;                          // move to next byte
        }
    }
}


void putUVLCtoBuffer (UI8 **ppbuf, I32 *bitpos, I32 val) {
    I32 tmp, len = 1;
    val ++;
    for (tmp=val+1; tmp!=1; tmp>>=1)
        len += 2;
    putBitsToBuffer(ppbuf, bitpos, (val & ((1<<((len+1)>>1))-1)) , ((len>>1) + ((len+1)>>1)) );
}


void alignBitsToByte (UI8 **ppbuf, I32 *bitpos) {
    if ( (*bitpos) < 7 )
        *((*ppbuf)++) &= 0xfe << (*bitpos);           // set all tail bits to 0, and move the buffer pointer to next byte
    (*bitpos) = 7;
}


void putBytesToBuffer (UI8 **ppbuf, const UI8 *bytes, I32 len) {
    I32 i;
    for (i=0; i<len; i++)
        *((*ppbuf)++) = bytes[i];
}


void putHeaderToBuffer (UI8 **ppbuf, const I32 qpd6, const I32 ysz, const I32 xsz) {
    static const UI8 VPS [] = {0x00, 0x00, 0x01, 0x40, 0x01, 0x0C, 0x01, 0xFF, 0xFF, 0x03, 0x10, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0xB4, 0xF0, 0x24};
    static const UI8 SPS [] = {0x00, 0x00, 0x01, 0x42, 0x01, 0x01, 0x03, 0x10, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0xB4};
    static const UI8 PPS [] = {0x00, 0x00, 0x01, 0x44, 0x01, 0xC0, 0x90, 0x91, 0x81, 0xD9, 0x20};
    
    static const UI8 SLICE_HEADERS [][8] = {
        {0x00, 0x00, 0x01, 0x26, 0x01, 0xAC, 0x16, 0xDE} ,       // qpd6=0
        {0x00, 0x00, 0x01, 0x26, 0x01, 0xAC, 0x10, 0xDE} ,       // qpd6=1
        {0x00, 0x00, 0x01, 0x26, 0x01, 0xAC, 0x2B, 0x78} ,       // qpd6=2
        {0x00, 0x00, 0x01, 0x26, 0x01, 0xAC, 0x4D, 0xE0} ,       // qpd6=3
        {0x00, 0x00, 0x01, 0x26, 0x01, 0xAC, 0x97, 0x80}         // qpd6=4
    };
    
    I32 bitpos = 7;
    
    putBytesToBuffer(ppbuf, VPS, sizeof(VPS));
    putBytesToBuffer(ppbuf, SPS, sizeof(SPS));
    putBitsToBuffer (ppbuf, &bitpos, 0x0A, 4);
    putUVLCtoBuffer (ppbuf, &bitpos, xsz);
    putUVLCtoBuffer (ppbuf, &bitpos, ysz);
    putBitsToBuffer (ppbuf, &bitpos, 0x197EE4, 22);
    //putBitsToBuffer (ppbuf, &bitpos, 0x707B44, 24);       // max_transform_hierarchy_depth_intra = 0
    putBitsToBuffer (ppbuf, &bitpos, 0x681ED1, 24);       // max_transform_hierarchy_depth_intra = 1
    alignBitsToByte (ppbuf, &bitpos);
    putBytesToBuffer(ppbuf, PPS, sizeof(PPS));
    putBytesToBuffer(ppbuf, SLICE_HEADERS[qpd6], sizeof(SLICE_HEADERS[qpd6]));
}





///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// definations and operations for CABAC context value
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const UI8 CONTEXT_NEXT_STATE_MPS [] = {2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,124,125,126,127};
const UI8 CONTEXT_NEXT_STATE_LPS [] = {1,  0,  0,  1,  2,  3,  4,  5,  4,  5,  8,  9,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 18, 19, 22, 23, 22, 23, 24, 25, 26, 27, 26, 27, 30, 31, 30, 31, 32, 33, 32, 33, 36, 37, 36, 37, 38, 39, 38, 39, 42, 43, 42, 43, 44, 45, 44, 45, 46, 47, 48, 49, 48, 49, 50, 51, 52, 53, 52, 53, 54, 55, 54, 55, 56, 57, 58, 59, 58, 59, 60, 61, 60, 61, 60, 61, 62, 63, 64, 65, 64, 65, 66, 67, 66, 67, 66, 67, 68, 69, 68, 69, 70, 71, 70, 71, 70, 71, 72, 73, 72, 73, 72, 73, 74, 75, 74, 75, 74, 75, 76, 77, 76, 77,126,127};

const UI8 CABAC_LPS_TABLE [][4] = {
  {128, 176, 208, 240}, {128, 167, 197, 227}, {128, 158, 187, 216}, {123, 150, 178, 205}, {116, 142, 169, 195}, {111, 135, 160, 185}, {105, 128, 152, 175}, {100, 122, 144, 166},
  { 95, 116, 137, 158}, { 90, 110, 130, 150}, { 85, 104, 123, 142}, { 81,  99, 117, 135}, { 77,  94, 111, 128}, { 73,  89, 105, 122}, { 69,  85, 100, 116}, { 66,  80,  95, 110},
  { 62,  76,  90, 104}, { 59,  72,  86,  99}, { 56,  69,  81,  94}, { 53,  65,  77,  89}, { 51,  62,  73,  85}, { 48,  59,  69,  80}, { 46,  56,  66,  76}, { 43,  53,  63,  72},
  { 41,  50,  59,  69}, { 39,  48,  56,  65}, { 37,  45,  54,  62}, { 35,  43,  51,  59}, { 33,  41,  48,  56}, { 32,  39,  46,  53}, { 30,  37,  43,  50}, { 29,  35,  41,  48},
  { 27,  33,  39,  45}, { 26,  31,  37,  43}, { 24,  30,  35,  41}, { 23,  28,  33,  39}, { 22,  27,  32,  37}, { 21,  26,  30,  35}, { 20,  24,  29,  33}, { 19,  23,  27,  31},
  { 18,  22,  26,  30}, { 17,  21,  25,  28}, { 16,  20,  23,  27}, { 15,  19,  22,  25}, { 14,  18,  21,  24}, { 14,  17,  20,  23}, { 13,  16,  19,  22}, { 12,  15,  18,  21},
  { 12,  14,  17,  20}, { 11,  14,  16,  19}, { 11,  13,  15,  18}, { 10,  12,  15,  17}, { 10,  12,  14,  16}, {  9,  11,  13,  15}, {  9,  11,  12,  14}, {  8,  10,  12,  14},
  {  8,   9,  11,  13}, {  7,   9,  11,  12}, {  7,   9,  10,  12}, {  7,   8,  10,  11}, {  6,   8,   9,  11}, {  6,   7,   9,  10}, {  6,   7,   8,   9}, {  2,   2,   2,   2}
};

const UI8 CABAC_RENORM_TABLE [] = { 6, 5, 4, 4, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };


// operations for context value (ctx_val), note that ctx_val must be UI8 type
#define   UPDATE_LPS(ctx_val)       { (ctx_val) = CONTEXT_NEXT_STATE_LPS[(ctx_val)]; }
#define   UPDATE_MPS(ctx_val)       { (ctx_val) = CONTEXT_NEXT_STATE_MPS[(ctx_val)]; }
#define   GET_CTX_STATE(ctx_val)    ( (ctx_val)>>1 )
#define   GET_CTX_MPS(ctx_val)      ( (ctx_val)&1  )
#define   GET_LPS(ctx_val,range)    ( CABAC_LPS_TABLE[GET_CTX_STATE(ctx_val)][((range)>>6)&3] )
#define   GET_NBIT(lps)             ( CABAC_RENORM_TABLE[(lps)>>3] )


UI8 initContextValue (UI8 init_val, I32 qpd6) {
    I32 qp = qpd6 * 6 + 4;
    I32 init_state = ((((init_val>>4)*5-45)*qp) >> 4) + ((init_val&15) << 3) - 16;
    init_state = CLIP(init_state, 1, 126);
    if (init_state >= 64)
        return (UI8)( ((init_state-64)<<1) | 1 );
    else
        return (UI8)( ((63-init_state)<<1) );
}





///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HEVC CABAC context set
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct {                      // context set for HEVC encoder
    UI8 split_cu_flag [3];
    UI8 partsize         ;
    UI8 Y_pmode          ;
    UI8 UV_pmode         ;
    UI8 split_tu_flag [3];
    UI8 Y_qt_cbf      [2];
    UI8 UV_qt_cbf     [5];
    UI8 last_x     [5][5];
    UI8 last_y     [5][5];
    UI8 sig_map       [2];
    UI8 sig_sc       [44];
    UI8 one_sc       [24];
    UI8 abs_sc        [6];
} ContextSet;


ContextSet newContextSet (I32 qpd6) {
    ContextSet tCtxs = {
        {139, 141, 157},
        184,
        184,
        63,
        {153, 138, 138},
        {111, 141},
        {94, 138, 182, 154, 154},
        {{110, 110, 124}, {125, 140, 153}, {125, 127, 140, 109}, {111, 143, 127, 111, 79}, {108, 123,  63, 154}},
        {{110, 110, 124}, {125, 140, 153}, {125, 127, 140, 109}, {111, 143, 127, 111, 79}, {108, 123,  63, 154}},
        {91, 171},
        {111, 111, 125, 110, 110,  94, 124, 108, 124, 107, 125, 141, 179, 153, 125, 107, 125, 141, 179, 153, 125, 107, 125, 141, 179, 153, 125, 141, 140, 139, 182, 182, 152, 136, 152, 136, 153, 136, 139, 111, 136, 139, 111, 111},
        {140,  92, 137, 138, 140, 152, 138, 139, 153,  74, 149,  92, 139, 107, 122, 152, 140, 179, 166, 182, 140, 227, 122, 197},
        {138, 153, 136, 167, 152, 152}
    };
    
    UI8 *ptr    = (UI8*)&tCtxs;
    UI8 *endptr = ptr + sizeof(tCtxs);
    for(; ptr<endptr; ptr++)
        *ptr = initContextValue(*ptr, qpd6);                // initial all the UI8 items in ctxs using function initContextValue
    
    return tCtxs;
}





///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CABAC coder
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define TMPBUF_LEN  (CTU_SZ*CTU_SZ*3+128)

typedef struct {
    UI8 tmpbuf   [TMPBUF_LEN];        // internal temporary buffer of CABAC coder
    I32 tmpcnt               ;        // indicate the byte count in tmpbuf
    I32 count00              ;        // indicate the number of 0x00 that has been just put to tmpbuf, if the last byte is not 0x00, set count0x00=0
    I32 range                ;
    I32 low                  ;
    I32 nbits                ;
    I32 nbytes               ;
    I32 bufbyte              ;
} CABACcoder;


CABACcoder newCABACcoder (void) {
    CABACcoder tCABAC = { {0x00}, 0, 0, 510, 0, 23, 0, 0xFF };
    return tCABAC;
}


void CABACsubmitToBuffer (CABACcoder *p, UI8 **ppbuf) {
    putBytesToBuffer(ppbuf, p->tmpbuf, p->tmpcnt);                // move all bytes from tmpbuf to ppbuf
    p->tmpcnt = 0;                                                // now tmpbuf as no bytes, so set tmpcnt=0
}


void CABACput (CABACcoder *p, I32 byte) {
    if ( p->count00 >= 2  &&  (UI8)byte <= 0x03 ) {
        p->tmpbuf[ p->tmpcnt++ ] = 0x03;
        p->count00 = 0;
    }
    p->tmpbuf[ p->tmpcnt++ ] = (UI8)byte;
    if ( (UI8)byte == 0x00 )
        p->count00 ++;
    else
        p->count00 = 0;
    //if ( p->tmpcnt >= TMPBUF_LEN ) {}              // overflow: should never, since p->tmpbuf (internal buffer of CABAC coder) is large enough
}


I32  CABAClen (const CABACcoder *p) {
    return  8 * (p->tmpcnt + p->nbytes) + 23 - p->nbits ;
}


void CABACfinish (CABACcoder *p) {
    I32 tmp = 0x00;
    if ( ( (p->low) >> (32-p->nbits) ) > 0 ) {
        CABACput(p, p->bufbyte+1);
        p->low -= (1<<(32-p->nbits));
    } else {
        if (p->nbytes > 0)
            CABACput(p, p->bufbyte);
        tmp = 0xff;
    }
    for (; p->nbytes>1; p->nbytes--)
        CABACput(p, tmp);
    tmp = (p->low >> 8) << p->nbits ;
    CABACput(p, tmp >> 16 );
    CABACput(p, tmp >> 8  );
    CABACput(p, tmp       );
}


void CABACupdate (CABACcoder *p) {
    if (p->nbits < 12) {
        I32 lead_byte = p->low >> (24-p->nbits);
        p->nbits += 8;
        p->low &= (0xFFFFFFFF >> p->nbits);
        if (lead_byte == 0xFF) {
            p->nbytes ++;
        } else if ( p->nbytes > 0 ) {
            I32 carry = lead_byte >> 8;
            I32 byte  = carry + p->bufbyte;
            p->bufbyte = lead_byte & 0xFF;
            CABACput(p, byte);
            byte = (0xFF + carry) & 0xFF;
            for (; p->nbytes>1; p->nbytes--)
                CABACput(p, byte);
        } else {
            p->nbytes = 1;
            p->bufbyte = lead_byte;
        }
    }
}


void CABACputTerminate (CABACcoder *p, BOOL bin) {
    bin = !!bin;
    p->range -= 2;
    if (bin) {
        p->low += p->range;
        p->low   <<= 7;
        p->range = 2<<7;
        p->nbits -= 7;
    } else if (p->range < 256) {
        p->low   <<= 1;
        p->range <<= 1;
        p->nbits --;
    }
    CABACupdate(p);
}


void CABACputBins (CABACcoder *p, I32 bins, I32 len) {   // put bins without context model
    bins &= ((1<<len)-1);
    while (len > 0) {
        const I32 len_curr = MIN(len, 8);
        I32 bins_curr;
        len -= len_curr;
        bins_curr = (bins>>len) & ((1<<len_curr)-1);
        p->low <<= len_curr;
        p->low += p->range * bins_curr;
        p->nbits -= len_curr;
        CABACupdate(p);
    }
}


void CABACputBin (CABACcoder *p, BOOL bin, UI8 *pCtx) {   // put bin with context model
    I32 lps  = GET_LPS(*pCtx, p->range);
    I32 nbit = GET_NBIT(lps);
    bin = !!bin;
    p->range -= lps;
    if ( bin != GET_CTX_MPS(*pCtx) ) {
        UPDATE_LPS(*pCtx);
        p->low   = ( p->low + p->range ) << nbit;
        p->range = lps << nbit;
        p->nbits -= nbit;
    } else {
        UPDATE_MPS(*pCtx);
        if (p->range < 256) {
            p->low   <<= 1;
            p->range <<= 1;
            p->nbits --;
        }
    }
    CABACupdate(p);
}





///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// functions for putting HEVC elements using CABAC coder
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void putSplitCUflag (CABACcoder *pCABAC, ContextSet *pCtxs, const I32 sz, const BOOL split_cu_flag, const BOOL larger_than_left_cu, const BOOL larger_than_above_cu) {
    const UI8 ctx_idx = (!!larger_than_left_cu) + (!!larger_than_above_cu);
    if (sz >= 16)                                                         // only the CU that equal or larger than 16x16 can be further split to 4 sub-CUs
        CABACputBin(pCABAC, split_cu_flag, &pCtxs->split_cu_flag[ctx_idx]);
}


// put PartSize (PART_2Nx2N or PART_NxN)
// partNxN=1 indicate PART_NxN (split to 4 PUs)    partNxN=0 indicate part2Nx2N (do not split to 4 PUs)
void putPartSize (CABACcoder *pCABAC, ContextSet *pCtxs, const I32 sz, const BOOL partNxN) {
    if (sz == 8)                                                          // only the 8x8 CU can be split to 4 sub-PUs
        CABACputBin(pCABAC, !partNxN, &pCtxs->partsize);
}


void getProbablePmodes (const I32 pmode_left, const I32 pmode_above, I32  pmodes [3]) {
    if (pmode_left != pmode_above) {
        pmodes[0] = pmode_left;
        pmodes[1] = pmode_above;
        if (pmode_left != PMODE_PLANAR  &&  pmode_above != PMODE_PLANAR)
            pmodes[2] = PMODE_PLANAR;
        else if (pmode_left + pmode_above < 2)
            pmodes[2] = PMODE_VER;
        else
            pmodes[2] = PMODE_DC;
    } else if (pmode_left > PMODE_DC) {
        pmodes[0] = pmode_left;
        pmodes[1] = ((pmode_left + 29) % 32) + 2;
        pmodes[2] = ((pmode_left - 1 ) % 32) + 2;
    } else {
        pmodes[0] = PMODE_PLANAR;
        pmodes[1] = PMODE_DC;
        pmodes[2] = PMODE_VER;
    }
}


// put predict mode(s)
// partNxN=1 indicate PART_NxN (split to 4 PUs)    partNxN=0 indicate part2Nx2N (do not split to 4 PUs)
// pmode : the predict mode(s) to be encoded, when partNxN=1, there should be 4 pmodes in this array, otherwise there should be only 1
// pmode_left  : when partNxN=1, there should be 4 pmodes in this array, otherwise there should be only 1
// pmode_above : when partNxN=1, there should be 4 pmodes in this array, otherwise there should be only 1
void putYpmode (CABACcoder *pCABAC, ContextSet *pCtxs, const BOOL partNxN, const I32 pmode [], const I32 pmode_left [], const I32 pmode_above []) {
    const I32   part_count = partNxN ? 4 : 1;
    I32  probable_pmodes [4][3];
    I32  hit_index [4] = {-1, -1, -1, -1};
    I32  i, j, tmp;
    
    for (i=0; i<part_count; i++) {
        getProbablePmodes(pmode_left[i], pmode_above[i], probable_pmodes[i]);
        for (j=0; j<3; j++)
            if (probable_pmodes[i][j] == pmode[i])
                hit_index[i] = j;                                               // hit
        CABACputBin(pCABAC, (hit_index[i]>=0) , &pCtxs->Y_pmode);               // if hit encode 1.   if miss encode 0
    }

    for (i=0; i<part_count; i++) {
        j = hit_index[i];
        if (j >= 0) {                                                           // hit
            CABACputBins(pCABAC, (j>0), 1);
            if (j > 0)
                CABACputBins(pCABAC, (j-1), 1);
        } else {                                                                // miss
            // sort the 3 items in probable_pmodes[i], from large to small
            if (probable_pmodes[i][0] < probable_pmodes[i][1]) { tmp=probable_pmodes[i][0];  probable_pmodes[i][0]=probable_pmodes[i][1];  probable_pmodes[i][1]=tmp; }
            if (probable_pmodes[i][1] < probable_pmodes[i][2]) { tmp=probable_pmodes[i][1];  probable_pmodes[i][1]=probable_pmodes[i][2];  probable_pmodes[i][2]=tmp; }
            if (probable_pmodes[i][0] < probable_pmodes[i][1]) { tmp=probable_pmodes[i][0];  probable_pmodes[i][0]=probable_pmodes[i][1];  probable_pmodes[i][1]=tmp; }

            tmp = pmode[i];
            for (j=0; j<3; j++)
                if (tmp > probable_pmodes[i][j])
                    tmp --;
            CABACputBins(pCABAC, tmp, 5);
        }
    }
}


void putUVpmode (CABACcoder *pCABAC, ContextSet *pCtxs) {
    CABACputBin(pCABAC, 0, &pCtxs->UV_pmode);                                   // since this design is for mono-chrome image, here let UV pmode always be as same as Y pmode. And let coeff of UV always be zero, thus the reconstructed UV pixels will always be 0x80
}


void putSplitTUflag (CABACcoder *pCABAC, ContextSet *pCtxs, const I32 sz, const BOOL split_tu_flag) {
    if      (sz == 32)
        CABACputBin(pCABAC, split_tu_flag, &pCtxs->split_tu_flag[0] );
    else if (sz == 16)
        CABACputBin(pCABAC, split_tu_flag, &pCtxs->split_tu_flag[1] );
    else if (sz == 8 )
        CABACputBin(pCABAC, split_tu_flag, &pCtxs->split_tu_flag[2] );
}


// put CBF bit (CBF bit is to indicate whether a coefficient block is not all zero)
// tu_depth_in_cu : for example, if a 8x8 TU is divided from a 16x16 CU, then tu_depth_in_cu=1. If a 8x8 TU is from a 8x8 CU, then tu_depth_in_cu=0
void putQtCbf (CABACcoder *pCABAC, ContextSet *pCtxs, const I32 tu_depth_in_cu, const ChannelType ch, const BOOL cbf) {
    if (ch == CH_Y)
        CABACputBin(pCABAC, cbf, &pCtxs->Y_qt_cbf[!tu_depth_in_cu] );
    else
        CABACputBin(pCABAC, cbf, &pCtxs->UV_qt_cbf[tu_depth_in_cu] );
}


void putLastSignificantXY (CABACcoder *pCABAC, ContextSet *pCtxs, const I32 sz, const ChannelType ch, const ScanType scan_type, const I32 y, const I32 x) {
    static const UI8 GROUP_INDEX_TABLE [] = {0, 1, 2, 3, 4, 4, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9};
    static const UI8 MIN_IN_GROUP_TABLE[] = {0, 1, 2, 3, 4, 6, 8, 12, 16, 24};

    //                                   Y 4x4 8x8 16x16    32x32    U/V 4x4 8x8 16x16    32x32
    static const UI8 ADDR_TABLE [][5] = { {  0,  1,    2, 0,    3} ,    {  4,  4,    4, 0,    4} };
    static const UI8 SFT_TABLE  [][5] = { {  0,  1,    1, 0,    1} ,    {  0,  1,    2, 0,    3} };

    const I32 addr = ADDR_TABLE[ch!=CH_Y][sz/8];
    const I32 sft  =  SFT_TABLE[ch!=CH_Y][sz/8];

    I32 ty = (scan_type==SCAN_TYPE_VER) ? x : y;
    I32 tx = (scan_type==SCAN_TYPE_VER) ? y : x;
    I32 gy = GROUP_INDEX_TABLE[ty];
    I32 gx = GROUP_INDEX_TABLE[tx];
    
    I32 i;
    
    for (i=0; i<gx; i++)
        CABACputBin(pCABAC, 1, &pCtxs->last_x[addr][ i>>sft] );
    
    if (gx < GROUP_INDEX_TABLE[sz-1])
        CABACputBin(pCABAC, 0, &pCtxs->last_x[addr][gx>>sft] );
    
    for (i=0; i<gy; i++)
        CABACputBin(pCABAC, 1, &pCtxs->last_y[addr][ i>>sft] );
    
    if (gy < GROUP_INDEX_TABLE[sz-1])
        CABACputBin(pCABAC, 0, &pCtxs->last_y[addr][gy>>sft] );
    
    if (gx > 3) {
        tx -= MIN_IN_GROUP_TABLE[gx];
        for (i=(((gx-2)>>1)-1); i>=0; i--)
            CABACputBins(pCABAC, (tx>>i)&1 , 1 );
    }
    
    if (gy > 3) {
        ty -= MIN_IN_GROUP_TABLE[gy];
        for (i=(((gy-2)>>1)-1); i>=0; i--)
            CABACputBins(pCABAC, (ty>>i)&1 , 1 );
    }
}


// description : get the context index of significance bit-map
// return: ctx_idx
I32 getSigCtxIdx (const I32 sz, const ChannelType ch, const ScanType scan_type, const I32 y, const I32 x, const I32 sig_ctx) {
    static const UI8 CTX_OFFSET_4x4_TABLE [4][4] = {{0, 1, 4, 5}, {2, 3, 4, 5}, {6, 6, 8, 8}, {7, 7, 8, 8}};
    static const UI8 CTX_OFFSET_POSITION []      = {2, 1, 1, 0, 0, 0, 0};

    I32 ctx_idx = ch == CH_Y ? 0 : 28;

    if (y==0 && x==0)                                       // special case for the DC context variable
        return ctx_idx;
    
    if (sz == 4)
        return ctx_idx + CTX_OFFSET_4x4_TABLE[y][x];
    
    ctx_idx += 9;

    if (ch == CH_Y) {                                      // for Y channel
        if ( sz >= 16 )
            ctx_idx += 12;
        if ( sz == 8 && scan_type != SCAN_TYPE_DIAG )      // 8x8 Non-Diagonal Scan
            ctx_idx += 6;
        if ( !(GETnCG(y) == 0 && GETnCG(x) == 0) )         // not the first CG
            ctx_idx += 3;
    } else if ( sz >= 16 )
        ctx_idx += 3;

    switch (sig_ctx) {
        case 0 :   return ctx_idx + CTX_OFFSET_POSITION[ GETn_inCG(y) + GETn_inCG(x) ];
        case 1 :   return ctx_idx + CTX_OFFSET_POSITION[ GETn_inCG(y) << 1 ];
        case 2 :   return ctx_idx + CTX_OFFSET_POSITION[ GETn_inCG(x) << 1 ];
        default:   return ctx_idx + 2;
    }
}


// return scan_type
// output the pointer pointing to a scan order table on **scan
ScanType  getScanOrder (const I32 sz, const I32 pmode, const UI8 (**scan) [2]) {
    static const UI8 SCAN_HOR_8x8    [][2] = {{0,0},{0,1},{0,2},{0,3},{1,0},{1,1},{1,2},{1,3},{2,0},{2,1},{2,2},{2,3},{3,0},{3,1},{3,2},{3,3},{0,4},{0,5},{0,6},{0,7},{1,4},{1,5},{1,6},{1,7},{2,4},{2,5},{2,6},{2,7},{3,4},{3,5},{3,6},{3,7},{4,0},{4,1},{4,2},{4,3},{5,0},{5,1},{5,2},{5,3},{6,0},{6,1},{6,2},{6,3},{7,0},{7,1},{7,2},{7,3},{4,4},{4,5},{4,6},{4,7},{5,4},{5,5},{5,6},{5,7},{6,4},{6,5},{6,6},{6,7},{7,4},{7,5},{7,6},{7,7}};
    static const UI8 SCAN_VER_8x8    [][2] = {{0,0},{1,0},{2,0},{3,0},{0,1},{1,1},{2,1},{3,1},{0,2},{1,2},{2,2},{3,2},{0,3},{1,3},{2,3},{3,3},{4,0},{5,0},{6,0},{7,0},{4,1},{5,1},{6,1},{7,1},{4,2},{5,2},{6,2},{7,2},{4,3},{5,3},{6,3},{7,3},{0,4},{1,4},{2,4},{3,4},{0,5},{1,5},{2,5},{3,5},{0,6},{1,6},{2,6},{3,6},{0,7},{1,7},{2,7},{3,7},{4,4},{5,4},{6,4},{7,4},{4,5},{5,5},{6,5},{7,5},{4,6},{5,6},{6,6},{7,6},{4,7},{5,7},{6,7},{7,7}};
    static const UI8 SCAN_DIAG_8x8   [][2] = {{0,0},{1,0},{0,1},{2,0},{1,1},{0,2},{3,0},{2,1},{1,2},{0,3},{3,1},{2,2},{1,3},{3,2},{2,3},{3,3},{4,0},{5,0},{4,1},{6,0},{5,1},{4,2},{7,0},{6,1},{5,2},{4,3},{7,1},{6,2},{5,3},{7,2},{6,3},{7,3},{0,4},{1,4},{0,5},{2,4},{1,5},{0,6},{3,4},{2,5},{1,6},{0,7},{3,5},{2,6},{1,7},{3,6},{2,7},{3,7},{4,4},{5,4},{4,5},{6,4},{5,5},{4,6},{7,4},{6,5},{5,6},{4,7},{7,5},{6,6},{5,7},{7,6},{6,7},{7,7}};
    static const UI8 SCAN_DIAG_16x16 [][2] = {{0,0},{1,0},{0,1},{2,0},{1,1},{0,2},{3,0},{2,1},{1,2},{0,3},{3,1},{2,2},{1,3},{3,2},{2,3},{3,3},{4,0},{5,0},{4,1},{6,0},{5,1},{4,2},{7,0},{6,1},{5,2},{4,3},{7,1},{6,2},{5,3},{7,2},{6,3},{7,3},{0,4},{1,4},{0,5},{2,4},{1,5},{0,6},{3,4},{2,5},{1,6},{0,7},{3,5},{2,6},{1,7},{3,6},{2,7},{3,7},{8,0},{9,0},{8,1},{10,0},{9,1},{8,2},{11,0},{10,1},{9,2},{8,3},{11,1},{10,2},{9,3},{11,2},{10,3},{11,3},{4,4},{5,4},{4,5},{6,4},{5,5},{4,6},{7,4},{6,5},{5,6},{4,7},{7,5},{6,6},{5,7},{7,6},{6,7},{7,7},{0,8},{1,8},{0,9},{2,8},{1,9},{0,10},{3,8},{2,9},{1,10},{0,11},{3,9},{2,10},{1,11},{3,10},{2,11},{3,11},{12,0},{13,0},{12,1},{14,0},{13,1},{12,2},{15,0},{14,1},{13,2},{12,3},{15,1},{14,2},{13,3},{15,2},{14,3},{15,3},{8,4},{9,4},{8,5},{10,4},{9,5},{8,6},{11,4},{10,5},{9,6},{8,7},{11,5},{10,6},{9,7},{11,6},{10,7},{11,7},{4,8},{5,8},{4,9},{6,8},{5,9},{4,10},{7,8},{6,9},{5,10},{4,11},{7,9},{6,10},{5,11},{7,10},{6,11},{7,11},{0,12},{1,12},{0,13},{2,12},{1,13},{0,14},{3,12},{2,13},{1,14},{0,15},{3,13},{2,14},{1,15},{3,14},{2,15},{3,15},{12,4},{13,4},{12,5},{14,4},{13,5},{12,6},{15,4},{14,5},{13,6},{12,7},{15,5},{14,6},{13,7},{15,6},{14,7},{15,7},{8,8},{9,8},{8,9},{10,8},{9,9},{8,10},{11,8},{10,9},{9,10},{8,11},{11,9},{10,10},{9,11},{11,10},{10,11},{11,11},{4,12},{5,12},{4,13},{6,12},{5,13},{4,14},{7,12},{6,13},{5,14},{4,15},{7,13},{6,14},{5,15},{7,14},{6,15},{7,15},{12,8},{13,8},{12,9},{14,8},{13,9},{12,10},{15,8},{14,9},{13,10},{12,11},{15,9},{14,10},{13,11},{15,10},{14,11},{15,11},{8,12},{9,12},{8,13},{10,12},{9,13},{8,14},{11,12},{10,13},{9,14},{8,15},{11,13},{10,14},{9,15},{11,14},{10,15},{11,15},{12,12},{13,12},{12,13},{14,12},{13,13},{12,14},{15,12},{14,13},{13,14},{12,15},{15,13},{14,14},{13,15},{15,14},{14,15},{15,15}};
    static const UI8 SCAN_DIAG_32x32 [][2] = {{0,0},{1,0},{0,1},{2,0},{1,1},{0,2},{3,0},{2,1},{1,2},{0,3},{3,1},{2,2},{1,3},{3,2},{2,3},{3,3},{4,0},{5,0},{4,1},{6,0},{5,1},{4,2},{7,0},{6,1},{5,2},{4,3},{7,1},{6,2},{5,3},{7,2},{6,3},{7,3},{0,4},{1,4},{0,5},{2,4},{1,5},{0,6},{3,4},{2,5},{1,6},{0,7},{3,5},{2,6},{1,7},{3,6},{2,7},{3,7},{8,0},{9,0},{8,1},{10,0},{9,1},{8,2},{11,0},{10,1},{9,2},{8,3},{11,1},{10,2},{9,3},{11,2},{10,3},{11,3},{4,4},{5,4},{4,5},{6,4},{5,5},{4,6},{7,4},{6,5},{5,6},{4,7},{7,5},{6,6},{5,7},{7,6},{6,7},{7,7},{0,8},{1,8},{0,9},{2,8},{1,9},{0,10},{3,8},{2,9},{1,10},{0,11},{3,9},{2,10},{1,11},{3,10},{2,11},{3,11},{12,0},{13,0},{12,1},{14,0},{13,1},{12,2},{15,0},{14,1},{13,2},{12,3},{15,1},{14,2},{13,3},{15,2},{14,3},{15,3},{8,4},{9,4},{8,5},{10,4},{9,5},{8,6},{11,4},{10,5},{9,6},{8,7},{11,5},{10,6},{9,7},{11,6},{10,7},{11,7},{4,8},{5,8},{4,9},{6,8},{5,9},{4,10},{7,8},{6,9},{5,10},{4,11},{7,9},{6,10},{5,11},{7,10},{6,11},{7,11},{0,12},{1,12},{0,13},{2,12},{1,13},{0,14},{3,12},{2,13},{1,14},{0,15},{3,13},{2,14},{1,15},{3,14},{2,15},{3,15},{16,0},{17,0},{16,1},{18,0},{17,1},{16,2},{19,0},{18,1},{17,2},{16,3},{19,1},{18,2},{17,3},{19,2},{18,3},{19,3},{12,4},{13,4},{12,5},{14,4},{13,5},{12,6},{15,4},{14,5},{13,6},{12,7},{15,5},{14,6},{13,7},{15,6},{14,7},{15,7},{8,8},{9,8},{8,9},{10,8},{9,9},{8,10},{11,8},{10,9},{9,10},{8,11},{11,9},{10,10},{9,11},{11,10},{10,11},{11,11},{4,12},{5,12},{4,13},{6,12},{5,13},{4,14},{7,12},{6,13},{5,14},{4,15},{7,13},{6,14},{5,15},{7,14},{6,15},{7,15},{0,16},{1,16},{0,17},{2,16},{1,17},{0,18},{3,16},{2,17},{1,18},{0,19},{3,17},{2,18},{1,19},{3,18},{2,19},{3,19},{20,0},{21,0},{20,1},{22,0},{21,1},{20,2},{23,0},{22,1},{21,2},{20,3},{23,1},{22,2},{21,3},{23,2},{22,3},{23,3},{16,4},{17,4},{16,5},{18,4},{17,5},{16,6},{19,4},{18,5},{17,6},{16,7},{19,5},{18,6},{17,7},{19,6},{18,7},{19,7},{12,8},{13,8},{12,9},{14,8},{13,9},{12,10},{15,8},{14,9},{13,10},{12,11},{15,9},{14,10},{13,11},{15,10},{14,11},{15,11},{8,12},{9,12},{8,13},{10,12},{9,13},{8,14},{11,12},{10,13},{9,14},{8,15},{11,13},{10,14},{9,15},{11,14},{10,15},{11,15},{4,16},{5,16},{4,17},{6,16},{5,17},{4,18},{7,16},{6,17},{5,18},{4,19},{7,17},{6,18},{5,19},{7,18},{6,19},{7,19},{0,20},{1,20},{0,21},{2,20},{1,21},{0,22},{3,20},{2,21},{1,22},{0,23},{3,21},{2,22},{1,23},{3,22},{2,23},{3,23},{24,0},{25,0},{24,1},{26,0},{25,1},{24,2},{27,0},{26,1},{25,2},{24,3},{27,1},{26,2},{25,3},{27,2},{26,3},{27,3},{20,4},{21,4},{20,5},{22,4},{21,5},{20,6},{23,4},{22,5},{21,6},{20,7},{23,5},{22,6},{21,7},{23,6},{22,7},{23,7},{16,8},{17,8},{16,9},{18,8},{17,9},{16,10},{19,8},{18,9},{17,10},{16,11},{19,9},{18,10},{17,11},{19,10},{18,11},{19,11},{12,12},{13,12},{12,13},{14,12},{13,13},{12,14},{15,12},{14,13},{13,14},{12,15},{15,13},{14,14},{13,15},{15,14},{14,15},{15,15},{8,16},{9,16},{8,17},{10,16},{9,17},{8,18},{11,16},{10,17},{9,18},{8,19},{11,17},{10,18},{9,19},{11,18},{10,19},{11,19},{4,20},{5,20},{4,21},{6,20},{5,21},{4,22},{7,20},{6,21},{5,22},{4,23},{7,21},{6,22},{5,23},{7,22},{6,23},{7,23},{0,24},{1,24},{0,25},{2,24},{1,25},{0,26},{3,24},{2,25},{1,26},{0,27},{3,25},{2,26},{1,27},{3,26},{2,27},{3,27},{28,0},{29,0},{28,1},{30,0},{29,1},{28,2},{31,0},{30,1},{29,2},{28,3},{31,1},{30,2},{29,3},{31,2},{30,3},{31,3},{24,4},{25,4},{24,5},{26,4},{25,5},{24,6},{27,4},{26,5},{25,6},{24,7},{27,5},{26,6},{25,7},{27,6},{26,7},{27,7},{20,8},{21,8},{20,9},{22,8},{21,9},{20,10},{23,8},{22,9},{21,10},{20,11},{23,9},{22,10},{21,11},{23,10},{22,11},{23,11},{16,12},{17,12},{16,13},{18,12},{17,13},{16,14},{19,12},{18,13},{17,14},{16,15},{19,13},{18,14},{17,15},{19,14},{18,15},{19,15},{12,16},{13,16},{12,17},{14,16},{13,17},{12,18},{15,16},{14,17},{13,18},{12,19},{15,17},{14,18},{13,19},{15,18},{14,19},{15,19},{8,20},{9,20},{8,21},{10,20},{9,21},{8,22},{11,20},{10,21},{9,22},{8,23},{11,21},{10,22},{9,23},{11,22},{10,23},{11,23},{4,24},{5,24},{4,25},{6,24},{5,25},{4,26},{7,24},{6,25},{5,26},{4,27},{7,25},{6,26},{5,27},{7,26},{6,27},{7,27},{0,28},{1,28},{0,29},{2,28},{1,29},{0,30},{3,28},{2,29},{1,30},{0,31},{3,29},{2,30},{1,31},{3,30},{2,31},{3,31},{28,4},{29,4},{28,5},{30,4},{29,5},{28,6},{31,4},{30,5},{29,6},{28,7},{31,5},{30,6},{29,7},{31,6},{30,7},{31,7},{24,8},{25,8},{24,9},{26,8},{25,9},{24,10},{27,8},{26,9},{25,10},{24,11},{27,9},{26,10},{25,11},{27,10},{26,11},{27,11},{20,12},{21,12},{20,13},{22,12},{21,13},{20,14},{23,12},{22,13},{21,14},{20,15},{23,13},{22,14},{21,15},{23,14},{22,15},{23,15},{16,16},{17,16},{16,17},{18,16},{17,17},{16,18},{19,16},{18,17},{17,18},{16,19},{19,17},{18,18},{17,19},{19,18},{18,19},{19,19},{12,20},{13,20},{12,21},{14,20},{13,21},{12,22},{15,20},{14,21},{13,22},{12,23},{15,21},{14,22},{13,23},{15,22},{14,23},{15,23},{8,24},{9,24},{8,25},{10,24},{9,25},{8,26},{11,24},{10,25},{9,26},{8,27},{11,25},{10,26},{9,27},{11,26},{10,27},{11,27},{4,28},{5,28},{4,29},{6,28},{5,29},{4,30},{7,28},{6,29},{5,30},{4,31},{7,29},{6,30},{5,31},{7,30},{6,31},{7,31},{28,8},{29,8},{28,9},{30,8},{29,9},{28,10},{31,8},{30,9},{29,10},{28,11},{31,9},{30,10},{29,11},{31,10},{30,11},{31,11},{24,12},{25,12},{24,13},{26,12},{25,13},{24,14},{27,12},{26,13},{25,14},{24,15},{27,13},{26,14},{25,15},{27,14},{26,15},{27,15},{20,16},{21,16},{20,17},{22,16},{21,17},{20,18},{23,16},{22,17},{21,18},{20,19},{23,17},{22,18},{21,19},{23,18},{22,19},{23,19},{16,20},{17,20},{16,21},{18,20},{17,21},{16,22},{19,20},{18,21},{17,22},{16,23},{19,21},{18,22},{17,23},{19,22},{18,23},{19,23},{12,24},{13,24},{12,25},{14,24},{13,25},{12,26},{15,24},{14,25},{13,26},{12,27},{15,25},{14,26},{13,27},{15,26},{14,27},{15,27},{8,28},{9,28},{8,29},{10,28},{9,29},{8,30},{11,28},{10,29},{9,30},{8,31},{11,29},{10,30},{9,31},{11,30},{10,31},{11,31},{28,12},{29,12},{28,13},{30,12},{29,13},{28,14},{31,12},{30,13},{29,14},{28,15},{31,13},{30,14},{29,15},{31,14},{30,15},{31,15},{24,16},{25,16},{24,17},{26,16},{25,17},{24,18},{27,16},{26,17},{25,18},{24,19},{27,17},{26,18},{25,19},{27,18},{26,19},{27,19},{20,20},{21,20},{20,21},{22,20},{21,21},{20,22},{23,20},{22,21},{21,22},{20,23},{23,21},{22,22},{21,23},{23,22},{22,23},{23,23},{16,24},{17,24},{16,25},{18,24},{17,25},{16,26},{19,24},{18,25},{17,26},{16,27},{19,25},{18,26},{17,27},{19,26},{18,27},{19,27},{12,28},{13,28},{12,29},{14,28},{13,29},{12,30},{15,28},{14,29},{13,30},{12,31},{15,29},{14,30},{13,31},{15,30},{14,31},{15,31},{28,16},{29,16},{28,17},{30,16},{29,17},{28,18},{31,16},{30,17},{29,18},{28,19},{31,17},{30,18},{29,19},{31,18},{30,19},{31,19},{24,20},{25,20},{24,21},{26,20},{25,21},{24,22},{27,20},{26,21},{25,22},{24,23},{27,21},{26,22},{25,23},{27,22},{26,23},{27,23},{20,24},{21,24},{20,25},{22,24},{21,25},{20,26},{23,24},{22,25},{21,26},{20,27},{23,25},{22,26},{21,27},{23,26},{22,27},{23,27},{16,28},{17,28},{16,29},{18,28},{17,29},{16,30},{19,28},{18,29},{17,30},{16,31},{19,29},{18,30},{17,31},{19,30},{18,31},{19,31},{28,20},{29,20},{28,21},{30,20},{29,21},{28,22},{31,20},{30,21},{29,22},{28,23},{31,21},{30,22},{29,23},{31,22},{30,23},{31,23},{24,24},{25,24},{24,25},{26,24},{25,25},{24,26},{27,24},{26,25},{25,26},{24,27},{27,25},{26,26},{25,27},{27,26},{26,27},{27,27},{20,28},{21,28},{20,29},{22,28},{21,29},{20,30},{23,28},{22,29},{21,30},{20,31},{23,29},{22,30},{21,31},{23,30},{22,31},{23,31},{28,24},{29,24},{28,25},{30,24},{29,25},{28,26},{31,24},{30,25},{29,26},{28,27},{31,25},{30,26},{29,27},{31,26},{30,27},{31,27},{24,28},{25,28},{24,29},{26,28},{25,29},{24,30},{27,28},{26,29},{25,30},{24,31},{27,29},{26,30},{25,31},{27,30},{26,31},{27,31},{28,28},{29,28},{28,29},{30,28},{29,29},{28,30},{31,28},{30,29},{29,30},{28,31},{31,29},{30,30},{29,31},{31,30},{30,31},{31,31}};

    if (sz <= 8) {                                       // 4x4 or 8x8 block
        if        ( ABS(pmode-PMODE_VER) <= 4 ) {        // pmode is near vertical
            *scan = SCAN_HOR_8x8;                        // for 4x4 block, since SCAN_HOR_4x4 is same as the previous 16 items in SCAN_HOR_8x8, so we needn't arange SCAN_HOR_4x4 additionaly
            return SCAN_TYPE_HOR;
        } else if ( ABS(pmode-PMODE_HOR) <= 4 ) {        // pmode is near horizontal
            *scan = SCAN_VER_8x8;                        // for 4x4 block, since SCAN_VER_4x4 is same as the previous 16 items in SCAN_VER_8x8, so we needn't arange SCAN_VER_4x4 additionaly
            return SCAN_TYPE_VER;
        }
    }

    switch (sz) {
        case 4 :   *scan =  SCAN_DIAG_8x8;    break;     // for 4x4 block, since SCAN_DIAG_4x4 is same as the previous 16 items in SCAN_DIAG_8x8, so we needn't arange SCAN_DIAG_4x4 additionaly
        case 8 :   *scan =  SCAN_DIAG_8x8;    break;     // for 8x8 block
        case 16:   *scan =  SCAN_DIAG_16x16;  break;     // for 16x16 block
        case 32:   *scan =  SCAN_DIAG_32x32;  break;     // for 32x32 block
    }
    return SCAN_TYPE_DIAG;
}


void putRemainExGolomb (CABACcoder *pCABAC, I32 value, I32 rparam) {
    I32 len, tmp;
    if ( value < (3<<rparam) ) {
        len = value >> rparam;
        CABACputBins(pCABAC, (1<<(len+1))-2    , len+1  );
        CABACputBins(pCABAC, value%(1<<rparam) , rparam );
    } else {
        len = rparam;
        value -= (3<<rparam);
        for(; value>=(1<<len); len++)
            value -= (1<<len);
        tmp = 4 + len - rparam;
        CABACputBins(pCABAC, (1<<tmp)-2 , tmp );
        CABACputBins(pCABAC, value      , len );
    }
}


// put a coefficient block
void putCoef (CABACcoder *pCABAC, ContextSet *pCtxs, const I32 sz, const ChannelType ch, const I32 pmode, const I32 blk [][CTU_SZ]) {
    const UI8 (*scan) [2] = NULL;
    const ScanType scan_type = getScanOrder(sz, pmode, &scan);
    
    I32  i, i_last=0, j_nz=0, signs=0, sig_ctx=0, c1=1;
    I32  arr_abs_nz [CG_SZxSZ];

    BOOL  sig_map [ GETnCG(CTU_SZ) ][ GETnCG(CTU_SZ) ];
    BLK_SET(GETnCG(sz), 0, sig_map);                                        // initialize sig_map to all-zero
    
    for (i=0; i<sz*sz; i++) {                                               // for all coefficient
        const I32  y = scan[i][0];
        const I32  x = scan[i][1];
        if ( blk[y][x] != 0 ) {
            sig_map[GETnCG(y)][GETnCG(x)] = 1;
            i_last = i;
        }
    }
    
    putLastSignificantXY(pCABAC, pCtxs, sz, ch, scan_type, scan[i_last][0], scan[i_last][1]);

    for (i=i_last; i>=0; i--) {                                             // for all coefficient (reverse scan)
        const I32  y = scan[i][0];
        const I32  x = scan[i][1];
        const I32  y_cg = GETnCG(y);
        const I32  x_cg = GETnCG(x);
        const BOOL sig_cg = sig_map[y_cg][x_cg];
        const BOOL sig  = (blk[y][x] != 0);
        const BOOL sign = (blk[y][x] <  0);
        const BOOL is_final       = i == i_last;
        const BOOL is_first_cg    = y_cg==0 && x_cg==0;
        const BOOL is_first_in_cg = GETi_inCG(i) == 0;
        const BOOL is_final_in_cg = GETi_inCG(i) == CG_SZxSZ-1 || is_final;

        if (is_final_in_cg) {                                                                 // scan to a new CG
            const BOOL sig_cg_right = x_cg < GETnCG(sz)-1 && sig_map[y_cg][x_cg+1];           // this CG is not near the block's right border , and the right CG is significant
            const BOOL sig_cg_below = y_cg < GETnCG(sz)-1 && sig_map[y_cg+1][x_cg];           // this CG is not near the block's bottom border, and the bottom CG is significant

            sig_ctx = (sig_cg_below<<1) | sig_cg_right;
            j_nz  = 0;
            signs = 0;
            
            if ( !is_first_cg && !is_final )
                CABACputBin(pCABAC, sig_cg, &pCtxs->sig_map[!!sig_ctx] );
        }
        
        if ( !is_final && ( is_first_cg || (sig_cg && (!is_first_in_cg || j_nz>0)) ) ) {
            const I32 ctx_idx = getSigCtxIdx(sz, ch, scan_type, y, x, sig_ctx);
            CABACputBin(pCABAC, sig, &pCtxs->sig_sc[ctx_idx] );
        }
        
        if (sig) {
            arr_abs_nz[j_nz++] = ABS(blk[y][x]);
            signs = (signs<<1) | sign;
        }
        
        if ( is_first_in_cg && j_nz>0 ) {
            const I32 ctx_set = (ch==CH_Y ? 0 : 4) + ((ch==CH_Y && !is_first_cg) ? 2 : 0) + (c1==0 ? 1 : 0);
            BOOL escape_flag = j_nz > 8;
            I32  j, c2_flag=-1;
            c1 = 1;
            
            for (j=0; j<8 && j<j_nz; j++) {
                CABACputBin(pCABAC,  (arr_abs_nz[j]>1), &pCtxs->one_sc[4*ctx_set+c1] );
                if  (arr_abs_nz[j]>1) {
                    c1 = 0;
                    if (c2_flag < 0)
                        c2_flag = arr_abs_nz[j] > 2;
                    else
                        escape_flag = 1;
                } else if (c1>0 && c1<3)
                    c1 ++;
            }
            
            if (c1==0 && c2_flag>=0) {
                CABACputBin(pCABAC, (BOOL)c2_flag, &pCtxs->abs_sc[ctx_set] );
                escape_flag |= c2_flag;
            }
            
            CABACputBins(pCABAC, signs, j_nz);
            
            if (escape_flag) {
                I32 first_coeff2=3 , gorice_param=0;
                for (j=0; j<j_nz; j++) {
                    const I32 escape_value = arr_abs_nz[j] - ( j<8 ? first_coeff2 : 1 );
                    if (escape_value >= 0) {
                        putRemainExGolomb(pCABAC, escape_value, gorice_param);
                        if (arr_abs_nz[j] > (3<<gorice_param))
                            gorice_param = MIN(gorice_param+1 , 4);
                    }
                    if (arr_abs_nz[j] >= 2)
                        first_coeff2 = 2;
                }
            }
        }
    }
}


void putCU_Part2Nx2N_noTUsplit (                // put a CU to HEVC stream, where part_type = part2Nx2N , no splitting to 4 TUs
    CABACcoder *pCABAC,
    ContextSet *pCtxs,
    const I32   sz,
    const I32   pmode,
    const I32   pmode_left,
    const I32   pmode_above,
    const I32   blk [][CTU_SZ]
) {
    BOOL Ycbf = blkNotAllZero(sz, blk);
    putPartSize   (pCABAC, pCtxs, sz, 0);                                             // 0 indicate part2Nx2N
    putYpmode     (pCABAC, pCtxs, 0, &pmode, &pmode_left, &pmode_above);              // 0 indicate part2Nx2N
    putUVpmode    (pCABAC, pCtxs);                                                    //
    putSplitTUflag(pCABAC, pCtxs, sz, 0);                                             // 0 indicate do not split to 4 TUs
    putQtCbf      (pCABAC, pCtxs, 0, CH_U, 0);                                        // U channel is always zero, so Ucbf = 0. Note that TU depth in CU = 0
    putQtCbf      (pCABAC, pCtxs, 0, CH_V, 0);                                        // V channel is always zero, so Vcbf = 0. Note that TU depth in CU = 0
    putQtCbf      (pCABAC, pCtxs, 0, CH_Y, Ycbf);                                     // Ycbf. Note that TU depth in CU = 0
    if (Ycbf)
        putCoef   (pCABAC, pCtxs, sz, CH_Y, pmode, blk);
}


void putCU_Part2Nx2N_TUsplit (                  // put a CU to HEVC stream, where part_type = part2Nx2N , splitting to 4 TUs
    CABACcoder *pCABAC,
    ContextSet *pCtxs,
    const I32   sz,
    const I32   pmode,
    const I32   pmode_left,
    const I32   pmode_above,
          I32   sub_blk      [4][CTU_SZ/2][CTU_SZ]
) {
    I32  isub;
    putPartSize   (pCABAC, pCtxs, sz, 0);                                          // 0 indicate part2Nx2N
    putYpmode     (pCABAC, pCtxs, 0, &pmode, &pmode_left, &pmode_above);           // 0 indicate part2Nx2N
    putUVpmode    (pCABAC, pCtxs);                                                 //
    putSplitTUflag(pCABAC, pCtxs, sz, 1);                                          // 1 indicate split to 4 TUs
    putQtCbf      (pCABAC, pCtxs, 0, CH_U, 0);                                     // U channel is always zero, so Ucbf = 0. Note that TU depth in CU = 0
    putQtCbf      (pCABAC, pCtxs, 0, CH_V, 0);                                     // V channel is always zero, so Vcbf = 0. Note that TU depth in CU = 0
    for (isub=0; isub<4; isub++) {
        BOOL Ycbf = blkNotAllZero(sz/2, sub_blk[isub]) ;
        putQtCbf  (pCABAC, pCtxs, 1, CH_Y, Ycbf);                                  // Ycbf. Note that TU depth in CU = 1
        if (Ycbf)
            putCoef(pCABAC, pCtxs, sz/2, CH_Y, pmode, sub_blk[isub]);
    }
}


void putCU_PartNxN (                            // put a CU to HEVC stream, where part_type = partNxN (splitting to 4 PUs)
    CABACcoder *pCABAC,
    ContextSet *pCtxs,
    const I32   sz,
    const I32   pmodes       [4],
    const I32   pmodes_left  [4],
    const I32   pmodes_above [4],
          I32   sub_blk      [4][CTU_SZ/2][CTU_SZ]
) {
    I32  isub;
    putPartSize   (pCABAC, pCtxs, sz, 1);                                          // 1 indicate partNxN
    putYpmode     (pCABAC, pCtxs, 1, pmodes, pmodes_left, pmodes_above);           // 1 indicate partNxN
    putUVpmode    (pCABAC, pCtxs);                                                 //
    putQtCbf      (pCABAC, pCtxs, 0, CH_U, 0);                                     // U channel is always zero, so Ucbf = 0. Note that TU depth in CU = 0
    putQtCbf      (pCABAC, pCtxs, 0, CH_V, 0);                                     // V channel is always zero, so Vcbf = 0. Note that TU depth in CU = 0
    for (isub=0; isub<4; isub++) {
        BOOL Ycbf = blkNotAllZero(sz/2, sub_blk[isub]) ;
        putQtCbf  (pCABAC, pCtxs, 1, CH_Y, Ycbf);                                  // Ycbf. Note that TU depth in CU = 1
        if (Ycbf)
            putCoef(pCABAC, pCtxs, sz/2, CH_Y, pmodes[isub], sub_blk[isub]);
    }
}





///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// process a CU (recursive). This function will give you some small small C pointer shake
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void processCURecurs (
    const I32   qpd6,
    CABACcoder *pCABAC,
    ContextSet *pCtxs,
          UI8   blk_orig   [][CTU_SZ],                          // pointing to the original pixels block of this CU (blk_orig[0][0] will be the pixel on top-left corner in this CU)
          UI8   blk_rcon   [][1+CTU_SZ*2],                      // pointing to the reconstructed pixels block of this CU
          UI8   map_cu_sz  [][1+nTUinROW],                      // pointing to the context buffer of this CU
          UI8   map_pmode  [][1+nTUinROW],                      // pointing to the context buffer of this CU
    const I32   sz,                                             // CU size
    const BOOL  bll_exist,                                      // whether border on left exist
    const BOOL  blb_exist,                                      // whether border on left-below exist
    const BOOL  baa_exist,                                      // whether border on above exist
    const BOOL  bar_exist                                       // whether border on above-right exist
) {
    const CABACcoder oCABAC = *pCABAC;                          // backup the original CABAC coder at oCABAC. note that this operation will copy all the struct elements.
    const ContextSet oCtxs  = *pCtxs;                           // backup the original Context set at oCtxs . note that this operation will copy all the struct elements.

    const I32  nTU = GETnTU(sz);                                // indicate how many rows/columns of minimal TUs in this CU
    
    const BOOL larger_than_left_cu  = sz > map_cu_sz[0][-1];    // current CU is larger than the left CU
    const BOOL larger_than_above_cu = sz > map_cu_sz[-1][0];    // current CU is larger than the above CU
    
    const I32  pmode_left  = map_pmode[0][-1];                  // left pmode context of this CU
    const I32  pmode_above = map_pmode[-1][0];                  // above pmode context of this CU

    // judge border existance for sub blocks:  left-top sub block  right-top sub block  left-bottom sub block  right-bottom sub block
    const BOOL sub_bll_exist [4] =           { bll_exist,          1        ,           bll_exist,             1 };
    const BOOL sub_blb_exist [4] =           { bll_exist,          0        ,           blb_exist,             0 };
    const BOOL sub_baa_exist [4] =           { baa_exist,          baa_exist,           1        ,             1 };
    const BOOL sub_bar_exist [4] =           { baa_exist,          bar_exist,           1        ,             0 };

    // construct pointers for sub blocks:                            left-top sub block                          right-top sub block                         left-bottom sub block                           right-bottom sub block
    UI8 (*sub_blk_orig [4]) [CTU_SZ]     = { (UI8(*)[CTU_SZ])     & (blk_orig[0][0]) , (UI8(*)[CTU_SZ])     & (blk_orig[0][sz/2])  , (UI8(*)[CTU_SZ])     & (blk_orig[sz/2][0])  , (UI8(*)[CTU_SZ])     & (blk_orig[sz/2][sz/2])   };
    UI8 (*sub_blk_rcon [4]) [1+CTU_SZ*2] = { (UI8(*)[1+CTU_SZ*2]) & (blk_rcon[0][0]) , (UI8(*)[1+CTU_SZ*2]) & (blk_rcon[0][sz/2])  , (UI8(*)[1+CTU_SZ*2]) & (blk_rcon[sz/2][0])  , (UI8(*)[1+CTU_SZ*2]) & (blk_rcon[sz/2][sz/2])   };
    UI8 (*sub_map_cu_sz[4]) [1+nTUinROW] = { (UI8(*)[1+nTUinROW]) &(map_cu_sz[0][0]) , (UI8(*)[1+nTUinROW]) &(map_cu_sz[0][nTU/2]) , (UI8(*)[1+nTUinROW]) &(map_cu_sz[nTU/2][0]) , (UI8(*)[1+nTUinROW]) &(map_cu_sz[nTU/2][nTU/2]) };
    UI8 (*sub_map_pmode[4]) [1+nTUinROW] = { (UI8(*)[1+nTUinROW]) &(map_pmode[0][0]) , (UI8(*)[1+nTUinROW]) &(map_pmode[0][nTU/2]) , (UI8(*)[1+nTUinROW]) &(map_pmode[nTU/2][0]) , (UI8(*)[1+nTUinROW]) &(map_pmode[nTU/2][nTU/2]) };
    
    UI8 ubla , ublb[CTU_SZ*2] , ubar[CTU_SZ*2];                 // to save unfiltered border pixels
    UI8 fbla , fblb[CTU_SZ*2] , fbar[CTU_SZ*2];                 // to save   filtered border pixels

    UI8 blk_tmp1  [CTU_SZ][CTU_SZ];
    I32 blk_tmp2  [CTU_SZ][CTU_SZ];
    I32 blk_quat  [CTU_SZ][CTU_SZ];
    I32 sub_blk_quat [4][CTU_SZ/2][CTU_SZ];
    UI8 best_rcon [CTU_SZ][CTU_SZ];                             // always hold the best reconstructed CU pixels, for finally recover the reconstructed CU

    I32 isub, pmode, distortion, rdcost, rdcost_best=I32_MAX_VALUE;


    //--------------------------------------------------------------------------------------------------------------------------------------------------------
    // step1 : try splitting to 4 CUs
    //--------------------------------------------------------------------------------------------------------------------------------------------------------
    
    if (sz > MIN_CU_SZ) {                                                                                               // if CU not larger than the smallest CU, try splitting to 4 CUs
        putSplitCUflag(pCABAC, pCtxs, sz, 1, larger_than_left_cu, larger_than_above_cu);                                // split_cu_flag=1 (split to 4 CUs)

        for (isub=0; isub<4; isub++)
            processCURecurs(qpd6, pCABAC, pCtxs, sub_blk_orig[isub], sub_blk_rcon[isub], sub_map_cu_sz[isub], sub_map_pmode[isub], sz/2, sub_bll_exist[isub], sub_blb_exist[isub], sub_baa_exist[isub], sub_bar_exist[isub]);
        
        CALC_BLK_SSE(sz, blk_orig, blk_rcon, distortion);
        rdcost_best = calcRDcost(qpd6, distortion, (CABAClen(pCABAC) - CABAClen(&oCABAC)) );

        BLK_COPY(sz, blk_rcon, best_rcon);                                                                              // backup the reconstructed block, since subsequent code will modify it
    }
    

    //--------------------------------------------------------------------------------------------------------------------------------------------------------
    // step2 : try no splitting to 4 CUs, part2Nx2N (no splitting to 4 PUs), no splitting to 4 TUs. Try all prediction modes
    //--------------------------------------------------------------------------------------------------------------------------------------------------------
    
    getBorder(sz, bll_exist, blb_exist, baa_exist, bar_exist, blk_rcon, &ubla, ublb, ubar, &fbla, fblb, fbar);          // get border pixels for reconstructed image

    for (pmode=0; pmode<PMODE_COUNT; pmode++) {                                                                         // for all prediction modes
        CABACcoder tCABAC = oCABAC;                                                                                     // copy for trying.
        ContextSet tCtxs  = oCtxs;

        predict   (sz, CH_Y, pmode, ubla, ublb, ubar, fbla, fblb, fbar, blk_tmp1);                                      // predict, dst=blk_tmp1
        BLK_SUB   (sz, blk_orig, blk_tmp1, blk_tmp2);                                                                   // calculate residual, dst=blk_tmp2
        transform (sz, 0, blk_tmp2, blk_tmp2);                                                                          // src=blk_tmp2  dst=blk_tmp2
        quantize  (qpd6, sz, blk_tmp2, blk_quat);                                                                       // src=blk_tmp2  dst=blk_quat
        deQuantize(qpd6, sz, blk_quat, blk_tmp2);                                                                       // src=blk_quat  dst=blk_tmp2
        transform (sz, 1, blk_tmp2, blk_tmp2);                                                                          // src=blk_tmp2  dst=blk_tmp2
        BLK_ADD_CLIP_TO_PIX(sz, blk_tmp2, blk_tmp1, blk_tmp1);                                                          // reconstruction, dst=blk_tmp1
        
        putSplitCUflag(&tCABAC, &tCtxs, sz, 0, larger_than_left_cu, larger_than_above_cu);                              // split_cu_flag=0 (do not split to 4 CUs)
        putCU_Part2Nx2N_noTUsplit(&tCABAC, &tCtxs, sz, pmode, pmode_left, pmode_above, blk_quat);                       // encode CU
        
        CALC_BLK_SSE(sz, blk_orig, blk_tmp1, distortion);
        rdcost = calcRDcost(qpd6, distortion, (CABAClen(&tCABAC) - CABAClen(&oCABAC)) );

        if (rdcost_best>= rdcost) {                                                                                     // if current pmode can let RD-cost be smaller than the previous best RD-cost
            rdcost_best = rdcost;
            *pCABAC     = tCABAC;                                                                                       // update the best CABAC coder
            *pCtxs      = tCtxs;                                                                                        // update the best Context set
            BLK_COPY(sz, blk_tmp1, best_rcon);
            BLK_SET (nTU, (UI8)sz   , map_cu_sz);                                                                       // fill map_cu_sz. Provide context for subsequent CUs
            BLK_SET (nTU, (UI8)pmode, map_pmode);                                                                       // fill map_pmode. Provide context for subsequent CUs
        }
    }
    

    //--------------------------------------------------------------------------------------------------------------------------------------------------------
    // step3 : try no splitting to 4 CUs, part2Nx2N (no splitting to 4 PUs), but splitting to 4 TUs. Try all prediction modes
    //--------------------------------------------------------------------------------------------------------------------------------------------------------
    
    for (pmode=0; pmode<PMODE_COUNT; pmode++) {                                                                         // for all prediction modes
        CABACcoder tCABAC = oCABAC;                                                                                     // copy for trying.
        ContextSet tCtxs  = oCtxs;

        for (isub=0; isub<4; isub++) {
            getBorder (sz/2, sub_bll_exist[isub], sub_blb_exist[isub], sub_baa_exist[isub], sub_bar_exist[isub], sub_blk_rcon[isub], &ubla, ublb, ubar, &fbla, fblb, fbar);    // get border pixels for reconstructed image
            predict   (sz/2, CH_Y, pmode, ubla, ublb, ubar, fbla, fblb, fbar, blk_tmp1);                                // predict, dst=blk_tmp1
            BLK_SUB   (sz/2, sub_blk_orig[isub], blk_tmp1, blk_tmp2);                                                   // calculate residual, dst=blk_tmp2
            transform (sz/2, 0, blk_tmp2, blk_tmp2);                                                                    // src=blk_tmp2  dst=blk_tmp2
            quantize  (qpd6, sz/2, blk_tmp2, sub_blk_quat[isub]);                                                       // src=blk_tmp2  dst=sub_blk_quat[isub]
            deQuantize(qpd6, sz/2, sub_blk_quat[isub], blk_tmp2);                                                       // src=sub_blk_quat[isub]  dst=blk_tmp2
            transform (sz/2, 1, blk_tmp2, blk_tmp2);                                                                    // src=blk_tmp2  dst=blk_tmp2
            BLK_ADD_CLIP_TO_PIX(sz/2, blk_tmp2, blk_tmp1, sub_blk_rcon[isub]);                                          // reconstruction, dst=sub_blk_rcon[isub]
        }

        putSplitCUflag(&tCABAC, &tCtxs, sz, 0, larger_than_left_cu, larger_than_above_cu);                              // split_cu_flag=0 (do not split to 4 CUs)
        putCU_Part2Nx2N_TUsplit(&tCABAC, &tCtxs, sz, pmode, pmode_left, pmode_above, sub_blk_quat);                     // encode CU

        CALC_BLK_SSE(sz, blk_orig, blk_rcon, distortion);
        rdcost = calcRDcost(qpd6, distortion, (CABAClen(&tCABAC) - CABAClen(&oCABAC)) );

        if (rdcost_best>= rdcost) {                                                                                     // if current pmode can let RD-cost be smaller than the previous best RD-cost
            rdcost_best = rdcost;
            *pCABAC     = tCABAC;                                                                                       // update the best CABAC coder
            *pCtxs      = tCtxs;                                                                                        // update the best Context set
            BLK_COPY(sz, blk_rcon, best_rcon);
            BLK_SET (nTU, (UI8)sz   , map_cu_sz);                                                                       // fill map_cu_sz. Provide context for subsequent CUs
            BLK_SET (nTU, (UI8)pmode, map_pmode);                                                                       // fill map_pmode. Provide context for subsequent CUs
        }
    }
    

    //--------------------------------------------------------------------------------------------------------------------------------------------------------
    // step3 : try no splitting to 4 CUs, partNxN (splitting to 4 PUs).
    //--------------------------------------------------------------------------------------------------------------------------------------------------------
    
    if (sz == MIN_CU_SZ) {
        CABACcoder tCABAC = oCABAC;                                                                                     // copy for trying.
        ContextSet tCtxs  = oCtxs;

        I32  sub_pmodes       [4] = {-1, -1, -1, -1};
        I32  sub_pmodes_left  [4] = {-1, -1, -1, -1};
        I32  sub_pmodes_above [4] = {-1, -1, -1, -1};
        
        for (isub=0; isub<4; isub++) {
            I32 rdcost_subpart_best = I32_MAX_VALUE;

            getBorder(sz/2, sub_bll_exist[isub], sub_blb_exist[isub], sub_baa_exist[isub], sub_bar_exist[isub], sub_blk_rcon[isub], &ubla, ublb, ubar, &fbla, fblb, fbar);

            for (pmode=0; pmode<PMODE_COUNT; pmode++) {
                CABACcoder nCABAC = newCABACcoder();
                ContextSet nCtxs  = newContextSet(qpd6);

                predict   (sz/2, CH_Y, pmode, ubla, ublb, ubar, fbla, fblb, fbar, blk_tmp1);                            // predict, dst=blk_tmp1
                BLK_SUB   (sz/2, sub_blk_orig[isub], blk_tmp1, blk_tmp2);                                               // calculate residual, dst=blk_tmp2
                transform (sz/2, 0, blk_tmp2, blk_tmp2);                                                                // src=blk_tmp2  dst=blk_tmp2
                quantize  (qpd6, sz/2, blk_tmp2, blk_quat);                                                             // src=blk_tmp2  dst=blk_quat
                deQuantize(qpd6, sz/2, blk_quat, blk_tmp2);                                                             // src=blk_quat  dst=blk_tmp2
                transform (sz/2, 1, blk_tmp2, blk_tmp2);                                                                // src=blk_tmp2  dst=blk_tmp2
                BLK_ADD_CLIP_TO_PIX(sz/2, blk_tmp2, blk_tmp1, blk_tmp1);                                                // reconstruction, dst=blk_tmp1

                putCoef(&nCABAC, &nCtxs, sz/2, CH_Y, pmode, blk_quat);

                CALC_BLK_SSE(sz/2, sub_blk_orig[isub], blk_tmp1, distortion);
                rdcost = calcRDcost(qpd6, distortion, CABAClen(&nCABAC) );

                if (rdcost_subpart_best>= rdcost) {
                    rdcost_subpart_best = rdcost;
                    sub_pmodes[isub] = pmode;                                                                           // save the currently best pmode of this sub-part
                    BLK_COPY(sz/2, blk_quat, sub_blk_quat[isub]);                                                       // backup the currently best quat to sub_blk_quat[isub], for further encoding.
                    BLK_COPY(sz/2, blk_tmp1, sub_blk_rcon[isub]);                                                       // backup the reconstructed sub-part , as next sub-part's border reference.
                }
            }
        }

        // organize the context predict modes of the 4 PUs
        sub_pmodes_left [0] = pmode_left;
        sub_pmodes_above[0] = pmode_above;
        sub_pmodes_left [1] = sub_pmodes[0];
        sub_pmodes_above[1] = sub_map_pmode[1][-1][ 0];
        sub_pmodes_left [2] = sub_map_pmode[2][ 0][-1];
        sub_pmodes_above[2] = sub_pmodes[0];
        sub_pmodes_left [3] = sub_pmodes[2];
        sub_pmodes_above[3] = sub_pmodes[1];

        putSplitCUflag(&tCABAC, &tCtxs, sz, 0, larger_than_left_cu, larger_than_above_cu);                              // split_cu_flag=0 (do not split to 4 CUs)
        putCU_PartNxN(&tCABAC, &tCtxs, sz, sub_pmodes, sub_pmodes_left, sub_pmodes_above, sub_blk_quat);                // encode CU

        CALC_BLK_SSE(sz, blk_orig, blk_rcon, distortion);
        rdcost = calcRDcost(qpd6, distortion, (CABAClen(&tCABAC) - CABAClen(&oCABAC)) );

        if (rdcost_best>= rdcost) {                                                                                     // if current pmode can let RD-cost be smaller than the previous best RD-cost
            rdcost_best = rdcost;
            *pCABAC     = tCABAC;                                                                                       // update the best CABAC coder
            *pCtxs      = tCtxs;                                                                                        // update the best Context set
            BLK_SET (nTU, (UI8)sz   , map_cu_sz);                                                                       // fill map_cu_sz. Provide context for subsequent CUs
            BLK_SET (nTU/2, (UI8)sub_pmodes[0], sub_map_pmode[0]);                                                      // fill map_pmode. Provide context for subsequent CUs
            BLK_SET (nTU/2, (UI8)sub_pmodes[1], sub_map_pmode[1]);                                                      // fill map_pmode. Provide context for subsequent CUs
            BLK_SET (nTU/2, (UI8)sub_pmodes[2], sub_map_pmode[2]);                                                      // fill map_pmode. Provide context for subsequent CUs
            BLK_SET (nTU/2, (UI8)sub_pmodes[3], sub_map_pmode[3]);                                                      // fill map_pmode. Provide context for subsequent CUs
            return;
        }
    }

    BLK_COPY(sz, best_rcon, blk_rcon);                                                                                  // finially write the best reconstructed CU to blk_rcon
}





///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// top function of HEVC intra-frame image encoder
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

I32 HEVCImageEncoder (           // return   HEVC stream length (in bytes)
          UI8 *pbuffer,          // buffer to save HEVC stream
    const UI8 *img,              // 2-D array in 1-D buffer, height=ysz, width=xsz. Input the image to be compressed.
          UI8 *img_rcon,         // 2-D array in 1-D buffer, height=ysz, width=xsz. The HEVC encoder will save the reconstructed image here.
          I32 *ysz,              // point to image height, will be modified (clip to a multiple of CTU_SZ)
          I32 *xsz,              // point to image width , will be modified (clip to a multiple of CTU_SZ)
    const I32  qpd6              // quant value, must be 0~4. The larger, the higher compression ratio, but the lower quality.
) {
    CABACcoder tCABAC = newCABACcoder();
    ContextSet tCtxs  = newContextSet(qpd6);
    
    const I32 yszn = ((MIN(*ysz, MAX_YSZ) + CTU_SZ - 1) / CTU_SZ) * CTU_SZ;                                            // pad the image height to multiple of CTU_SZ
    const I32 xszn = ((MIN(*xsz, MAX_XSZ) + CTU_SZ - 1) / CTU_SZ) * CTU_SZ;                                            // pad the image width  to multiple of CTU_SZ
    
    UI8 *pbuf = pbuffer;

    I32 y, x, i, j;

    UI8   ctu_orig   [  CTU_SZ][  CTU_SZ  ];
    UI8   ctu_rcon_0 [1+CTU_SZ][1+CTU_SZ*2];
    UI8 (*ctu_rcon)            [1+CTU_SZ*2] = (UI8 (*) [1+CTU_SZ*2]) &(ctu_rcon_0[1][1]) ;                             // ctu_rcon <- ctu_rcon_0[1][1]
    
    UI8 map_cu_sz_0 [1+nTUinCTU][1+nTUinROW];                                                                          // context line-buffer for CU-size
    UI8 map_pmode_0 [1+nTUinCTU][1+nTUinROW];                                                                          // context line-buffer for predict mode

    for (i=0; i<=nTUinCTU; i++) {
        for (j=0; j<=nTUinROW; j++) {
            map_cu_sz_0[i][j] = CTU_SZ;                                                                                // set all items in map_cu_sz_0 = CTU_SZ
            map_pmode_0[i][j] = PMODE_DC;                                                                              // set all items in map_pmode_0 = PMODE_DC
        }
    }
    
    putHeaderToBuffer(&pbuf, qpd6, yszn, xszn);
    
    for (y=0; y<yszn; y+=CTU_SZ) {                                                                                     // for all CTU rows
        for (x=0; x<xszn; x+=CTU_SZ) {                                                                                 // for all CTU columns
            const BOOL bll_exist = x > 0;
            const BOOL blb_exist = 0;
            const BOOL baa_exist = y > 0;
            const BOOL bar_exist = baa_exist && (x+CTU_SZ < xszn);

            UI8 (*map_cu_sz) [1+nTUinROW] = (UI8 (*) [1+nTUinROW]) &(map_cu_sz_0[1][1+GETnTU(x)]);                     // pointer: map_cu_sz <- map_cu_sz_0[1][1+x]
            UI8 (*map_pmode) [1+nTUinROW] = (UI8 (*) [1+nTUinROW]) &(map_pmode_0[1][1+GETnTU(x)]);                     // pointer: map_pmode <- map_pmode_0[1][1+x]
            
            for (i=0; i<CTU_SZ; i++)
                ctu_rcon[i][-1] = GET2D(img_rcon, yszn, xszn, y+i, x-1);                                               // sample CTU border from reconstructed image
            
            for (j=-1; j<CTU_SZ*2; j++)
                ctu_rcon[-1][j] = GET2D(img_rcon, yszn, xszn, y-1, x+j);                                               // sample CTU border from reconstructed image

            for (i=0; i<CTU_SZ; i++)
                for (j=0; j<CTU_SZ; j++)
                    ctu_orig[i][j] = GET2D(img, *ysz, *xsz, y+i, x+j);                                                 // sample CTU from the original image
            
            processCURecurs(qpd6, &tCABAC, &tCtxs, ctu_orig, ctu_rcon, map_cu_sz, map_pmode, CTU_SZ, bll_exist, blb_exist, baa_exist, bar_exist);       // encode a CTU

            for (i=0; i<CTU_SZ; i++)
                for (j=0; j<CTU_SZ; j++)
                    GET2D(img_rcon, yszn, xszn, y+i, x+j) = ctu_rcon[i][j];                                            // write reconstructed CTU back to reconstructed image

            CABACputTerminate(&tCABAC, (y+32>=yszn && x+32>=xszn) );                                                   // encode a terminate bit
            CABACsubmitToBuffer(&tCABAC, &pbuf);                                                                       // submit the commpressed bytes from CABAC coder's buffer to output buffer
        }

        for (j=1; j<=nTUinROW; j++) {
            map_cu_sz_0[0][j] = map_cu_sz_0[nTUinCTU][j];                                                              // scroll line-buffer: put the context in current CTU rows to the previous CTU rows. 
            // map_pmode_0[0][j] = map_pmode_0[nTUinCTU][j];                                                           // Note that map_pmode do not need to be scrolled, since we never use the pmode in the previous line as context.
        }
    }
    
    CABACfinish(&tCABAC);
    CABACsubmitToBuffer(&tCABAC, &pbuf);                                                                               // submit the commpressed bytes from CABAC coder's buffer to output buffer

    *ysz = yszn;                                                                                                       // change the value of *ysz, so that the user can get the clipped image size
    *xsz = xszn;                                                                                                       // change the value of *xsz, so that the user can get the clipped image size
    
    return pbuf - pbuffer;                                                                                             // return the compressed length
}



