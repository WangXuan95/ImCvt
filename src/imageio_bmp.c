#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>


static void writeLittleEndian (uint32_t value, uint32_t len, FILE *fp) {
    for (; len>0; len--) {
        fputc((value&0xFF), fp);
        value >>= 8;
    }
}


static uint32_t loadLittleEndian (uint32_t len, FILE *fp) {
    uint32_t i, value=0;
    for (i=0; i<len; i++)
        value |= (fgetc(fp) << (8*i));
    return value;
}


// return:   0 : success    1 : failed
int writeBMPImageFile (const char *p_filename, const uint8_t *p_buf, int is_rgb, uint32_t height, uint32_t width) {
    const size_t row_size    = (size_t)(is_rgb?3:1) * width;
    const size_t row_size_a  = ((row_size+3)/4)*4;
    const size_t n_palette   = is_rgb ? 0 : 256;
    const size_t header_size = 14 + 40 + 4*n_palette;              // 14B BMP file header + 40B DIB header + palette + pixels
    const size_t file_size   = header_size + height * row_size_a;  // whole file size
    uint32_t i, j;
    int failed;
    FILE *fp;
    
    if (width < 1 || height < 1)
        return 1;
    
    if ((fp = fopen(p_filename, "wb")) == NULL)
        return 1;
    
    // write 14B BMP file header -----------------------------------------------------------------------
    writeLittleEndian(    0x4D42, 2, fp);   // 'BM'
    writeLittleEndian( file_size, 4, fp);   // whole file size
    writeLittleEndian(0x00000000, 4, fp);   // reserved
    writeLittleEndian(header_size,4, fp);   // start position of pixel data
    
    // write 40B DIB header ----------------------------------------------------------------------------
    writeLittleEndian(        40, 4, fp);   // DIB header size
    writeLittleEndian(     width, 4, fp);   // width
    writeLittleEndian(    height, 4, fp);   // height
    writeLittleEndian(    0x0001, 2, fp);   // one color plane
    writeLittleEndian(is_rgb?24:8,2, fp);   // bits per pixel
    writeLittleEndian(0x00000000, 4, fp);   // BI_RGB
    writeLittleEndian(0x00000000, 4, fp);   // pixel data size (height * width), a dummy 0 can be given for BI_RGB bitmaps
    writeLittleEndian(0x00000EC4, 4, fp);   // horizontal resolution of the image. (pixel per metre, signed integer)
    writeLittleEndian(0x00000EC4, 4, fp);   // vertical resolution of the image. (pixel per metre, signed integer)
    writeLittleEndian( n_palette, 4, fp);   // number of colors in the color palette, or 0 to default to 2^n
    writeLittleEndian(0x00000000, 4, fp);   // number of important colors used, or 0 when every color is important; generally ignored
    
    // write palette -----------------------------------------------------------------------------------
    for (i=0; i<n_palette; i++) {
        fputc(i, fp);
        fputc(i, fp);
        fputc(i, fp);
        fputc(0xFF, fp);
    }
    
    // write pixel data, note that the scan order of BMP is from down to up, from left to right --------
    for (i=0; i<height; i++) {
        const uint8_t *p_row = p_buf + (size_t)(height-1-i) * row_size;
        if (is_rgb) {
            for (j=0; j<width; j++) {
                fputc(p_row[2], fp);
                fputc(p_row[1], fp);
                fputc(p_row[0], fp);
                p_row += 3;
            }
        } else {
            fwrite(p_row, sizeof(uint8_t), row_size, fp);
        }
        writeLittleEndian(0, (row_size_a-row_size), fp);
    }
    
    failed = ((size_t)ftell(fp) != file_size);
    fclose(fp);
    return failed;
}


// return:  NULL     : failed
//          non-NULL : pointer to image pixels, allocated by malloc(), need to be free() later
uint8_t* loadBMPImageFile (const char *p_filename, int *p_is_rgb, uint32_t *p_height, uint32_t *p_width) {
    uint8_t palette_R [256] = {0};
    uint8_t palette_G [256] = {0};
    uint8_t palette_B [256] = {0};
    uint8_t *p_buf;
    uint32_t bm, offset, dib_size, bpp, cmprs_method, n_palette, bytepp, row_skip, i, j;
    FILE *fp;
    
    if ((fp = fopen(p_filename, "rb")) == NULL)
        return NULL;
    
    // BMP file header (14B) ----------------------
    bm          = loadLittleEndian(2, fp);  // 'BM'
                  loadLittleEndian(8, fp);  // whole file size + reserved
    offset      = loadLittleEndian(4, fp);  // offset of pixel data
    // DIB header ---------------------------------
    dib_size    = loadLittleEndian(4, fp);  // DIB header size
    *p_width    = loadLittleEndian(4, fp);  // width
    *p_height   = loadLittleEndian(4, fp);  // height
                  loadLittleEndian(2, fp);  // color plane
    bpp         = loadLittleEndian(2, fp);  // bits per pixel
    cmprs_method= loadLittleEndian(4, fp);  // compress method
                  loadLittleEndian(12,fp);  // skip: pixel data size + horizontal resolution + vertical resolution
    n_palette   = loadLittleEndian(4, fp);  // number of colors in the color palette, or 0 to default to 2^n
                  loadLittleEndian(4, fp);  // number of important colors used, or 0 when every color is important; generally ignored
    
    if (bm!=0x4D42 || offset<54 || dib_size<40 || (*p_width)<1 || (*p_height)<1 || (bpp!=8&&bpp!=24&&bpp!=32) || cmprs_method!=0 || n_palette>256) {
        fclose(fp);
        return NULL;
    }
    
    bytepp = bpp / 8;
    
    if (bytepp > 1) {
        *p_is_rgb = 1;
    } else {
        *p_is_rgb = 0;
        loadLittleEndian(dib_size-40, fp); // seek to the start of palette
        for (i=0; i<n_palette; i++) {      // load palette
            palette_B[i] = (uint8_t)fgetc(fp);
            palette_G[i] = (uint8_t)fgetc(fp);
            palette_R[i] = (uint8_t)fgetc(fp);
            fgetc(fp);
            if ( palette_B[i] != palette_G[i] || palette_G[i] != palette_R[i] ) *p_is_rgb = 1;
        }
    }
    
    if (fseek(fp, offset, SEEK_SET)) {     // seek to the start of pixel data
        fclose(fp);
        return NULL;
    }
    
    p_buf = (uint8_t*)malloc((size_t)((*p_is_rgb)?3:1) * (*p_width) * (*p_height));  // alloc pixel buffer
    
    if (p_buf == NULL) {
        fclose(fp);
        return NULL;
    }
    
    row_skip = ((bytepp*(*p_width)+3)/4)*4 - bytepp*(*p_width);
    
    // load pixel data, note that the scan order of BMP is from down to up, from left to right --------
    for (i=0; i<(*p_height); i++) {
        uint8_t *p_row = p_buf + (size_t)((*p_is_rgb)?3:1) * ((*p_height)-1-i) * (*p_width);
        if        (bytepp > 1) {
            for (j=0; j<(*p_width); j++) {
                p_row[2] = (uint8_t)fgetc(fp);
                p_row[1] = (uint8_t)fgetc(fp);
                p_row[0] = (uint8_t)fgetc(fp);
                p_row += 3;
                if (bytepp == 4) fgetc(fp);
            }
        } else {
            for (j=0; j<(*p_width); j++) {
                uint8_t value = (uint8_t)fgetc(fp);
                *(p_row++) = palette_R[value];
                if (*p_is_rgb) {
                    *(p_row++) = palette_G[value];
                    *(p_row++) = palette_B[value];
                }
            }
        }
        loadLittleEndian(row_skip, fp);
    }
    
    fclose(fp);
    return p_buf;
}
