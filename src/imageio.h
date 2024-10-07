#ifndef   __IMAGE_IO_H__
#define   __IMAGE_IO_H__


// functions for image file read ------------------
// return:  NULL     : failed
//          non-NULL : pointer to image pixels, allocated by malloc(), need to be free() later
uint8_t* loadPNMImageFile (const char *p_filename, int *p_is_rgb, uint32_t *p_height, uint32_t *p_width);   // from imageio_pnm.c
uint8_t* loadPNGImageFile (const char *p_filename, int *p_is_rgb, uint32_t *p_height, uint32_t *p_width);   // from imageio_png.c
uint8_t* loadBMPImageFile (const char *p_filename, int *p_is_rgb, uint32_t *p_height, uint32_t *p_width);   // from imageio_bmp.c
uint8_t* loadQOIImageFile (const char *p_filename, int *p_is_rgb, uint32_t *p_height, uint32_t *p_width);   // from imageio_qoi.c


// functions for image file write -----------------
// return:   0 : success    1 : failed
int writePNMImageFile (const char *p_filename, const uint8_t *p_buf, int is_rgb, uint32_t height, uint32_t width);           // from imageio_pnm.c
int writePNGImageFile (const char *p_filename, const uint8_t *p_buf, int is_rgb, uint32_t height, uint32_t width);           // from imageio_png.c
int writeBMPImageFile (const char *p_filename, const uint8_t *p_buf, int is_rgb, uint32_t height, uint32_t width);           // from imageio_bmp.c
int writeQOIImageFile (const char *p_filename, const uint8_t *p_buf, int is_rgb, uint32_t height, uint32_t width);           // from imageio_qoi.c
int writeJLSImageFile (const char *p_filename, const uint8_t *p_buf, int is_rgb, uint32_t height, uint32_t width, int near); // from imageio_jls.c
int writeHEVCImageFile(const char *p_filename, const uint8_t *p_buf, int is_rgb, uint32_t height, uint32_t width, int qpd6); // from imageio_hevc.c


#endif // __IMAGE_IO_H__
