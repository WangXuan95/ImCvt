#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "HEVCe/HEVCe.h"


// return:   0 : success    1 : failed
int writeHEVCImageFile (const char *p_filename, const uint8_t *p_buf, int is_rgb, uint32_t height, uint32_t width, int qpd6) {
    FILE *fp;
    size_t i;
    int failed = 1;
    int h, w, hevc_size;
    unsigned char *p_hevc = (unsigned char*)malloc(2*(width+32)*(height+32)+65536 + ((width+32)*(height+32)+1048576)*2);
    unsigned char *p_img_orig = p_hevc           + 2*(width+32)*(height+32)+65536;
    unsigned char *p_img_rcon = p_img_orig                                        + ((width+32)*(height+32)+1048576);
    
    if (p_hevc == NULL)
        return 1;
    
    if (is_rgb) {
        printf("   warning: this HEVCencoder currently only support gray 8-bit image instead of RGB image. Only compress the green channel of this image.\n");
        
        for (i=0; i<(size_t)height*width; i++) {
            p_img_orig[i] = p_buf[i*3+1];
        }
        
    } else {
        for (i=0; i<(size_t)height*width; i++) {
            p_img_orig[i] = p_buf[i];
        }
    }
    
    h = (int)height;
    w = (int)width;
    hevc_size = HEVCImageEncoder(p_hevc, p_img_orig, p_img_rcon, &h, &w, qpd6);
    
    if (hevc_size<=0 || h<=0 || w<=0) {
        free(p_hevc);
        return 1;
    }
    
    fp = fopen(p_filename, "wb");
    
    if (fp) {
        failed = ((size_t)hevc_size != fwrite(p_hevc, sizeof(uint8_t), (size_t)hevc_size, fp));
        fclose(fp);
    }
    
    free(p_hevc);
    
    return failed;
}
