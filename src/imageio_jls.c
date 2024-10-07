#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>


#define    ABS(x)               ( ((x) < 0) ? (-(x)) : (x) )                         // get absolute value
#define    MAX(x, y)            ( ((x)<(y)) ? (y) : (x) )                            // get the minimum value of x, y
#define    MIN(x, y)            ( ((x)<(y)) ? (x) : (y) )                            // get the maximum value of x, y
#define    CLIP(x, min, max)    ( MIN(MAX((x), (min)), (max)) )                      // clip x between min~max

#define    G2D(ptr,xsz,y,x)     (*( (ptr) + (xsz)*(y) + (x) ))

#define    RESET_VAL            64

static const int J [] = {0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,5,5,6,6,7,7,8,9,10,11,12,13,14,15};


static int getQbpp (int alpha) {
    int qbpp = 1;
    while ((1<<qbpp) < alpha)
        qbpp ++;
    return qbpp;
}


static void setParameters (int bpp, int near, int *alpha, int *t1, int *t2, int *t3, int *quant, int *qbeta, int *qbpp, int *limit, int *a_init) {
    int tmp;
    *alpha = 1 << bpp;
    tmp = (MIN(*alpha, 4096) + 127) / 256;
    *t1 =     tmp + 2 + 3 * near;
    *t2 = 4 * tmp + 3 + 5 * near;
    *t3 = 17* tmp + 4 + 7 * near;
    *quant = 2 * near + 1;
    *qbeta = (*alpha + 4*near) / *quant;
    *qbpp  = getQbpp(*qbeta);
    *limit = 4 * bpp - *qbpp - 1;
    *a_init = MAX((*qbeta+32)/64, 2);
}


static int isNear (int near, int v1, int v2) {
    return (v1-v2 <= near) && (v2-v1 <= near);
}


static void samplePixels (int *img, int xsz, int i, int j, int *a, int *b, int *c, int *d) {
    *a = *b = *c = *d = 0;
    
    if (i > 0) {
        *b = G2D(img, xsz, i-1, j);
        *d = *b;
        if (j+1 < xsz)
            *d = G2D(img, xsz, i-1, j+1);
    }
    
    if (j <= 0) {
        *a = *b;
        if (i > 1)
            *c = G2D(img, xsz, i-2, j);
    } else {
        *a = G2D(img, xsz, i, j-1);
        if (i > 0)
            *c = G2D(img, xsz, i-1, j-1);
    }
}


static int gradientQuantize (int val, int near, int t1, int t2, int t3) {
    int absval = ABS(val);
    int sign   = (val < 0);
    if (absval >=  t3) return sign ? -4 : 4;
    if (absval >=  t2) return sign ? -3 : 3;
    if (absval >=  t1) return sign ? -2 : 2;
    if (absval > near) return sign ? -1 : 1;
    return 0;
}


static int getQ (int near, int t1, int t2, int t3, int a, int b, int c, int d) {
    int Q1 = gradientQuantize(d-b, near, t1, t2, t3);
    int Q2 = gradientQuantize(b-c, near, t1, t2, t3);
    int Q3 = gradientQuantize(c-a, near, t1, t2, t3);
    return 81*Q1 + 9*Q2 + Q3;
}


static int predict (int a, int b, int c) {
    if      (c >= MAX(a, b))
        return MIN(a, b);
    else if (c <= MIN(a, b))
        return MAX(a, b);
    else
        return a + b - c;
}


static int quantize (int near, int quant, int errval) {
    if (errval < 0)
        return -( (near - errval) / quant );
    else
        return    (near + errval) / quant  ;
}


static int modRange (int qbeta, int val) {
    if      (val < 0)
        val += qbeta;
    if (val >= (qbeta+1) / 2)
        val -= qbeta;
    return val;
}


static int getK (int At, int Nt, int ritype) {
    int k = 0;
    if (ritype)
        At += (Nt >> 1);
    while ((Nt << k) < At)
        k ++;
    return k;
}


typedef struct {
    uint8_t  bitmask;
    uint8_t  byte;
    uint8_t *pbuf;
    uint8_t *pbuf_base;
} BitWriter_t;


static BitWriter_t initBitWriter (uint8_t *pbuf) {
    BitWriter_t bw;
    bw.bitmask = 0x80;
    bw.byte    = 0x00;
    bw.pbuf    = pbuf;
    bw.pbuf_base = pbuf;
    return bw;
}


static int getBitWriterLength (BitWriter_t *pbw) {
    return pbw->pbuf - pbw->pbuf_base;
}


static void writeValue (BitWriter_t *pbw, int value, int byte_cnt) {
    int i = byte_cnt * 8;
    for (i-=8; i>=0; i-=8) {
        pbw->pbuf[0] = (value >> i) & 0xFF;
        pbw->pbuf ++;
    }
}


static void writeBit (BitWriter_t *pbw, int bit) {
    if (bit)
        pbw->byte |= pbw->bitmask;
    pbw->bitmask >>= 1;
    if (pbw->bitmask == 0) {
        pbw->pbuf[0] = pbw->byte;
        pbw->pbuf ++;
        pbw->bitmask = 0x80;
        if (pbw->byte == 0xFF)
            pbw->bitmask >>= 1;
        pbw->byte = 0x00;
    }
}


static void writeBits (BitWriter_t *pbw, int bits, int n) {
    for (n--; n>=0; n--)
        writeBit(pbw, (bits>>n)&1);
}


static void flushBits (BitWriter_t *pbw) {
    if (pbw->bitmask < 0x80) {
        pbw->pbuf[0] = pbw->byte;
        pbw->pbuf ++;
        pbw->bitmask = 0x80;
        pbw->byte = 0x00;
    }
}


static void GolombCoding (BitWriter_t *pbw, int qbpp, int limit, int val, int k) {
    if ((val>>k) < limit) {
        writeBits(pbw, 0, val>>k);
        writeBit (pbw, 1);
        writeBits(pbw, val, k);
    } else {
        writeBits(pbw, 0, limit);
        writeBit (pbw, 1);
        writeBits(pbw, val-1, qbpp);
    }
}


static void writeScanHeader (BitWriter_t *pbw, int compID, int near) {
    writeValue(pbw, 0xFFDA     , 2);
    writeValue(pbw, 0x0008     , 2);
    writeValue(pbw, 0x01       , 1);
    writeValue(pbw, compID     , 1);
    writeValue(pbw, 0x00       , 1);
    writeValue(pbw, near       , 1);
    writeValue(pbw, 0x0000     , 2);
}


static void writeJLShearderGray (BitWriter_t *pbw, int bpp, int ysz, int xsz) {
    writeValue(pbw, 0xFFD8     , 2);
    writeValue(pbw, 0xFFF7000B , 4);
    writeValue(pbw, bpp        , 1);
    writeValue(pbw, ysz        , 2);
    writeValue(pbw, xsz        , 2);
    writeValue(pbw, 0x01       , 1);
    writeValue(pbw, 0x011100   , 3);
}


static void writeJLShearderRGB  (BitWriter_t *pbw, int bpp, int ysz, int xsz) {
    writeValue(pbw, 0xFFD8     , 2);
    writeValue(pbw, 0xFFF70011 , 4);
    writeValue(pbw, bpp        , 1);
    writeValue(pbw, ysz        , 2);
    writeValue(pbw, xsz        , 2);
    writeValue(pbw, 0x03       , 1);
    writeValue(pbw, 0x011100   , 3);
    writeValue(pbw, 0x021100   , 3);
    writeValue(pbw, 0x031100   , 3);
}


static void writeJLSfooter (BitWriter_t *pbw) {
    writeValue(pbw, 0xFFD9     , 2);
}


static void JLSencodeScan (BitWriter_t *p_bw, int bpp, int near, int ysz, int xsz, int *img, int *imgrcon) {
    int A  [364];
    int B  [364];
    int C  [364];
    int N  [364];
    int Ar [2];
    int Br [2];
    int Nr [2];
    
    int alpha, t1, t2, t3, quant, qbeta, qbpp, limit, a_init;
    int run_idx = 0;
    int i, j;
    
    bpp = CLIP(bpp, 8, 16);
    setParameters(bpp, near, &alpha, &t1, &t2, &t3, &quant, &qbeta, &qbpp, &limit, &a_init);
    
    for (i=0; i<364; i++) {
        A[i] = a_init;
        B[i] = 0;
        C[i] = 0;
        N[i] = 1;
    }
    
    for (i=0; i<2; i++) {
        Ar[i] = a_init;
        Br[i] = 0;
        Nr[i] = 1;
    }
    
    for (i=0; i<ysz; i++) {
        int running=0 , run_cnt=0;
        
        for (j=0; j<xsz; j++) {
            int runend = 0;
            int x, px, rx, a, b, c, d, q, sign, errval, k, map, merrval;
            
            x = G2D(img, xsz, i, j);
            samplePixels(imgrcon, xsz, i, j, &a, &b, &c, &d);
            
            q = getQ(near, t1, t2, t3, a, b, c, d);
            
            sign = (q<0) ? -1 : +1;
            q = ABS(q);
            
            running |= (q == 0);
            
            if (running) {
                running = isNear(near, x, a);
                runend  = !running;
            }
            
            if (running) {
                G2D(imgrcon, xsz, i, j) = a;
                
                run_cnt ++;
                if (run_cnt >= (1<<J[run_idx])) {
                    writeBit(p_bw, 1);
                    run_cnt -= (1<<J[run_idx]);
                    if (run_idx < 31)
                        run_idx ++;
                }
                
                if (j == xsz-1 && run_cnt > 0)
                    writeBit(p_bw, 1);
                
            } else if (runend) {
                int glimit = limit - 1 - J[run_idx];
                
                writeBits(p_bw, run_cnt, J[run_idx]+1);
                run_cnt = 0;
                if (run_idx > 0)
                    run_idx --;
                
                q = isNear(near, a, b);
                sign = (a > (b + near)) ? -1 : +1;
                
                px = q ? a : b;
                errval = sign * (x - px);
                errval = quantize(near, quant, errval);
                
                if (near) {
                    rx = px + sign * quant * errval;
                    rx = CLIP(rx, 0, alpha-1);
                    G2D(imgrcon, xsz, i, j) = rx;
                } else {
                    G2D(imgrcon, xsz, i, j) = x;
                }
                
                errval = modRange(qbeta, errval);
                k = getK(Ar[q], Nr[q], q);
                
                map = (errval!=0) && ( (errval>0) == (k==0 && 2*Br[q]<Nr[q]) );
                merrval = 2 * ABS(errval) - q - map;
                
                GolombCoding(p_bw, qbpp, glimit, merrval, k);
                
                if (errval < 0)
                    Br[q] ++;
                Ar[q] += ((merrval+1-q) >> 1);
                if (Nr[q] >= RESET_VAL) {
                    Ar[q] >>= 1;
                    Br[q] >>= 1;
                    Nr[q] >>= 1;
                }
                Nr[q] ++;
                
            } else {
                run_cnt = 0;
                q --;
                
                px = predict(a,b,c) + sign * C[q];
                px = CLIP(px, 0, alpha-1);
                
                errval = sign * (x - px);
                errval = quantize(near, quant, errval);
                
                if (near) {
                    rx = px + sign * quant * errval;
                    rx = CLIP(rx, 0, alpha-1);
                    G2D(imgrcon, xsz, i, j) = rx;
                } else {
                    G2D(imgrcon, xsz, i, j) = x;
                }
                
                errval = modRange(qbeta, errval);
                k = getK(A[q], N[q], 0);
                
                map = (k==0) && (2*B[q]<=-N[q]) && (near==0);
                merrval = 2 * ABS(errval);
                if (errval < 0)
                    merrval -= map + 1;
                else
                    merrval += map;
                
                GolombCoding(p_bw, qbpp, limit, merrval, k);
                
                B[q] += errval * quant;
                A[q] += ABS(errval);
                if (N[q] >= RESET_VAL) {
                    A[q] >>= 1;
                    B[q] >>= 1;
                    N[q] >>= 1;
                }
                N[q] ++;
                if (B[q] <= -N[q]) {
                    B[q] += N[q];
                    B[q] = MAX(B[q], -N[q]+1);
                    C[q] --;
                } else if (B[q] > 0) {
                    B[q] -= N[q];
                    B[q] = MIN(B[q], 0);
                    C[q] ++;
                }
                C[q] = CLIP(C[q], -128, 127);
            }
        }
    }
    
    flushBits(p_bw);
}


static int JLSencodeImageGray (int bpp, int near, int ysz, int xsz, int *img                         , uint8_t *pbuf) {
    BitWriter_t bw = initBitWriter(pbuf);
    writeJLShearderGray(&bw, bpp, ysz, xsz);
    writeScanHeader(&bw, 1, near);
    JLSencodeScan(&bw, bpp, near, ysz, xsz, img, img);
    writeJLSfooter(&bw);
    return getBitWriterLength(&bw);
}


static int JLSencodeImageRGB (int bpp, int near, int ysz, int xsz, int *img_r, int *img_g, int *img_b, uint8_t *pbuf) {
    BitWriter_t bw = initBitWriter(pbuf);
    writeJLShearderRGB(&bw, bpp, ysz, xsz);
    writeScanHeader(&bw, 1, near);
    JLSencodeScan(&bw, bpp, near, ysz, xsz, img_r, img_r);
    writeScanHeader(&bw, 2, near);
    JLSencodeScan(&bw, bpp, near, ysz, xsz, img_g, img_g);
    writeScanHeader(&bw, 3, near);
    JLSencodeScan(&bw, bpp, near, ysz, xsz, img_b, img_b);
    writeJLSfooter(&bw);
    return getBitWriterLength(&bw);
}



// return:   0 : success    1 : failed
int writeJLSImageFile (const char *p_filename, const uint8_t *p_buf, int is_rgb, uint32_t height, uint32_t width, int near) {
    int *p_r, *p_g, *p_b;
    uint8_t *p_jls;
    size_t jls_len;
    int failed = 1;
    uint32_t i;
    FILE *fp;
    
    if (width<1 || width>32767 || height<1 || height>32767)
        return 1;
    
    p_jls = (uint8_t*)malloc( (size_t)8*width*height+65536 + (size_t)(is_rgb?3:1)*height*width*sizeof(int) );
    p_r   = (int*)(p_jls + (size_t)8*width*height+65536);
    p_g   = p_r + ((size_t)height * width);
    p_b   = p_g + ((size_t)height * width);
    
    if (p_jls == NULL)
        return 1;
    
    if (is_rgb) {
        int *pr = p_r, *pg = p_g, *pb = p_b;
        for (i=(height*width); i>0; i--) {
            *(pr++) = *(p_buf++);
            *(pg++) = *(p_buf++);
            *(pb++) = *(p_buf++);
        }
    } else {
        int *pr = p_r;
        for (i=(height*width); i>0; i--) {
            *(pr++) = *(p_buf++);
        }
    }
    
    if (is_rgb) {
        jls_len = JLSencodeImageRGB (8, near, height, width, p_r, p_g, p_b, p_jls);
    } else {
        jls_len = JLSencodeImageGray(8, near, height, width, p_r,           p_jls);
    }
    
    fp = fopen(p_filename, "wb");
    
    if (fp) {
        failed = (jls_len != fwrite(p_jls, sizeof(uint8_t), jls_len, fp));
        fclose(fp);
    }
    
    free(p_jls);
    
    return failed;
}
