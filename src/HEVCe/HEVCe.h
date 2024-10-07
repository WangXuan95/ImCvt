#ifndef __HEVC_E__
#define __HEVC_E__


extern int HEVCImageEncoder (          // return   HEVC stream length (in bytes)
    unsigned char       *pbuffer,      // buffer to save HEVC stream
    const unsigned char *img,          // 2-D array in 1-D buffer, height=ysz, width=xsz. Input the image to be compressed.
    unsigned char       *img_rcon,     // 2-D array in 1-D buffer, height=ysz, width=xsz. The HEVC encoder will save the reconstructed image here.
    int                 *ysz,          // point to image height, will be modified (clip to a multiple of CTU_SZ)
    int                 *xsz,          // point to image width , will be modified (clip to a multiple of CTU_SZ)
    const int            qpd6          // quant value, must be 0~4. The larger, the higher compression ratio, but the lower quality.
);


#endif
