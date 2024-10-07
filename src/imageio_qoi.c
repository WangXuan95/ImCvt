#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>


// return:   0 : success    1 : failed
int writeQOIImageFile (const char *p_filename, const uint8_t *p_buf, int is_rgb, uint32_t height, uint32_t width) {
    uint8_t *p_qoi_start, *p_qoi;
    FILE *fp;
    
    if (width < 1 || height < 1)
        return 1;
    
    p_qoi_start = p_qoi = (uint8_t*)malloc( (size_t)(5) * width * height + 65536 );
    
    if (p_qoi_start == NULL)
        return 1;
    
    if ((fp = fopen(p_filename, "wb")) == NULL) {
        free(p_qoi_start);
        return 1;
    }
    
    // encode qoi ------------------
    {   uint8_t idx, run=0;
        uint8_t pr=0, pg=0, pb=0, pa=255;
        uint8_t cr  , cg  , cb  , ca;
        uint8_t dr  , dg  , db      ;
        uint8_t ar[64]={0}, ag[64]={0}, ab[64]={0}, aa[64]={0};
        size_t i;
        
        for (i=(size_t)width*height; i>0; i--) {
            cr = cg = cb = *(p_buf++);
            if (is_rgb) {
                cg = *(p_buf++);
                cb = *(p_buf++);
            }
            ca = 255;
            
            idx = (3*cr + 5*cg + 7*cb + 11*ca) & 0x3f;
            
            if (cr==pr && cg==pg && cb==pb && ca==pa) {
                run ++;
                if (run >= 62) {
                    *(p_qoi++) = (0xc0 | (run-1));                    // QOI_OP_RUN
                    run = 0;
                }
                
            } else {
                if (run > 0) {
                    *(p_qoi++) = (0xc0 | (run-1));                    // QOI_OP_RUN
                    run = 0;
                }
                
                if (cr == ar[idx] && cg == ag[idx] && cb == ab[idx] && ca == aa[idx]) {
                    *(p_qoi++) = idx;                                 // QOI_OP_INDEX
                
                } else {
                    dr = cr - pr + 2;
                    dg = cg - pg + 2;
                    db = cb - pb + 2;
                    
                    if (dr<4 && dg<4 && db<4 && ca==pa) {
                        *(p_qoi++) = (0x40 | (dr<<4) | (dg<<2) | db); // QOI_OP_DIFF
                        
                    } else {
                        dr = dr - dg + 8;
                        db = db - dg + 8;
                        dg += 30;
                        
                        if (dr<16 && dg<64 && db<16 && ca==pa) {
                            *(p_qoi++) = (   0x80 | dg);              // QOI_OP_LUMA
                            *(p_qoi++) = ((dr<<4) | db);
                            
                        } else {
                            *(p_qoi++) = ((ca!=pa)?0xff:0xfe);        // QOI_OP_RGB or QOI_OP_RGBA
                            *(p_qoi++) = cr;
                            *(p_qoi++) = cg;
                            *(p_qoi++) = cb;
                            if (ca != pa)
                                *(p_qoi++) = ca;
                        }
                    }
                }
            }
            
            ar[idx] = pr = cr;
            ag[idx] = pg = cg;
            ab[idx] = pb = cb;
            aa[idx] = pa = ca;
        }
        
        if (run > 0) *(p_qoi++) = (0xc0 | (run-1));                   // QOI_OP_RUN
    }
    
    fprintf(fp, "qoif%c%c%c%c%c%c%c%c\x03%c",  // write qoi header
        (width >>24)&0xff,
        (width >>16)&0xff,
        (width >> 8)&0xff,
        (width     )&0xff,
        (height>>24)&0xff,
        (height>>16)&0xff,
        (height>> 8)&0xff,
        (height    )&0xff,
        0x00
    );
    
    {
        size_t qoi_size = p_qoi - p_qoi_start;
        int failed = (qoi_size!= fwrite(p_qoi_start, sizeof(uint8_t), qoi_size, fp));
        
        free(p_qoi_start);
        fclose(fp);
        
        return failed;
    }
}


// return:  NULL     : failed
//          non-NULL : pointer to image pixels, allocated by malloc(), need to be free() later
uint8_t* loadQOIImageFile (const char *p_filename, int *p_is_rgb, uint32_t *p_height, uint32_t *p_width) {
    uint8_t               *p_qoi, *p_qoi_end;
    uint8_t *p_buf_start, *p_buf, *p_buf_end;
    uint8_t ch;
    FILE *fp;
    
    if ((fp = fopen(p_filename, "rb")) == NULL)
        return NULL;
    
    // parse qoi header -----------------
    if (fgetc(fp) != 'q') {fclose(fp); return NULL;}
    if (fgetc(fp) != 'o') {fclose(fp); return NULL;}
    if (fgetc(fp) != 'i') {fclose(fp); return NULL;}
    if (fgetc(fp) != 'f') {fclose(fp); return NULL;}
    (*p_width)  =                    fgetc(fp);
    (*p_width)  =  ((*p_width)<<8) + fgetc(fp);
    (*p_width)  =  ((*p_width)<<8) + fgetc(fp);
    (*p_width)  =  ((*p_width)<<8) + fgetc(fp);
    (*p_height) =               fgetc(fp);
    (*p_height) = ((*p_height)<<8) + fgetc(fp);
    (*p_height) = ((*p_height)<<8) + fgetc(fp);
    (*p_height) = ((*p_height)<<8) + fgetc(fp);
    ch          =           (uint8_t)fgetc(fp);
                                     fgetc(fp);
    
    if ((*p_width)<1 || (*p_height)<1 || ch<3 || ch>4) {
        fclose(fp);
        return NULL;
    }
    
    {
        size_t qoi_size, qoi_data_start_pos;
        qoi_data_start_pos = ftell(fp);
        if (fseek(fp, 0, SEEK_END)) {
            fclose(fp);
            return NULL;
        }
        qoi_size = (size_t)ftell(fp) - qoi_data_start_pos;
        if (fseek(fp, qoi_data_start_pos, SEEK_SET)) {
            fclose(fp);
            return NULL;
        }
        if (qoi_size <= 0) {
            fclose(fp);
            return NULL;
        }
        
        p_buf_start = p_buf = (uint8_t*)malloc((size_t)(3)*(*p_width)*(*p_height)+16 + qoi_size+16);
        p_buf_end   = p_buf +                  (size_t)(3)*(*p_width)*(*p_height);
        p_qoi       = p_buf_end                                       + 16;
        p_qoi_end   = p_qoi                                                + qoi_size;
        
        if (p_buf_start == NULL) {
            fclose(fp);
            return NULL;
        }
        
        fread(p_qoi, sizeof(uint8_t), qoi_size, fp);
        fclose(fp);
    }
    
    (*p_is_rgb) = 1;
    
    
    {   // decode qoi ------------------
        uint8_t tag, type, run;
        uint8_t r=0, g=0, b=0, a=255;
        uint8_t ar [64] = {0};
        uint8_t ag [64] = {0};
        uint8_t ab [64] = {0};
        uint8_t aa [64] = {0};
        for (;;) {
            tag  = *(p_qoi++);
            type = (tag >> 6);
            tag &= 0x3f;
            run  = 1;
            
            switch (type) {
                case 0 :                     // QOI_OP_INDEX
                    r = ar[tag];
                    g = ag[tag];
                    b = ab[tag];
                    a = aa[tag];
                    break;
                
                case 1 :                     // QOI_OP_DIFF
                    r += ((tag >> 4)      ) - 2;
                    g += ((tag >> 2) & 0x3) - 2;
                    b += ((tag     ) & 0x3) - 2;
                    break;
                
                case 2 :                     // QOI_OP_LUMA
                    type = *(p_qoi++);
                    g += tag - 32;
                    r += tag - 40 + (type >> 4 );
                    b += tag - 40 + (type & 0xf);
                    break;
                
                default :
                    if (tag < 62) {          // QOI_OP_RUN
                        run = 1 + tag;
                    } else {                 // QOI_OP_RGB or QOI_OP_RGBA
                        r = *(p_qoi++);
                        g = *(p_qoi++);
                        b = *(p_qoi++);
                        if (tag == 63)
                            a = *(p_qoi++);
                    }
                    break;
            }
            
            tag = (3*r + 5*g + 7*b + 11*a) & 0x3f;
            ar[tag] = r;
            ag[tag] = g;
            ab[tag] = b;
            aa[tag] = a;
            
            for (; run>0; run--) {
                p_buf[0] = r;
                p_buf[1] = g;
                p_buf[2] = b;
                p_buf += 3;
                
                if (p_buf>=p_buf_end) {
                    return p_buf_start;
                }
            }
            
            if (p_qoi>=p_qoi_end) {
                return p_buf_start;
            }
        }
    }
}
