#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "imageio.h"


const char *USAGE = 
  "|------------------------------------------------------------------------------------|\n"
  "| ImageConverter (v0.5)  by https://github.com/WangXuan95/                           |\n"
  "|------------------------------------------------------------------------------------|\n"
  "| Usage:                                                                             |\n"
  "|   ImCvt [-switches]  <in1> -o <out1>  [<in2> -o <out2]  ...                        |\n"
  "|                                                                                    |\n"
  "| Where <in> and <out> can be:                                                       |\n"
  "|   .pnm (Portable Any Map)          : gray 8-bit or RGB 24-bit                      |\n"
  "|   .pgm (Portable Gray Map)         : gray 8-bit                                    |\n"
  "|   .ppm (Portable Pix Map)          : RGB 24-bit                                    |\n"
  "|   .png (Portable Network Graphics) : gray 8-bit or RGB 24-bit                      |\n"
  "|   .bmp (Bitmap Image File)         : gray 8-bit or RGB 24-bit                      |\n"
  "|   .qoi (Quite OK Image)            : RGB 24-bit                                    |\n"
  "|   .jls (JPEG-LS Image)             : gray 8-bit or RGB 24-bit, can be <out> only!  |\n"
  "|   .h265 (H.265/HEVC Image)         : gray 8-bit              , can be <out> only!  |\n"
  "|                                                                                    |\n"
  "| switches:    -f                    : force overwrite of output file                |\n"
  "|              -0, -1, -2, -3, -4    : JPEG-LS near value or H.265 (qp-4)/6 value    |\n"
  "|------------------------------------------------------------------------------------|\n"
  "\n";



// return:   1 : match    0 : mismatch
static int matchSuffixIgnoringCase (const char *string, const char *suffix) {
    #define  TO_LOWER(c)   ((((c) >= 'A') && ((c) <= 'Z')) ? ((c)+32) : (c))
    const char *p1, *p2;
    for (p1=string; *p1; p1++);
    for (p2=suffix; *p2; p2++);
    while (TO_LOWER(*p1) == TO_LOWER(*p2)) {
        if (p2 <= suffix)
            return 1;
        if (p1 <= string)
            return 0;
        p1 --;
        p2 --;
    }
    return 0;
}


static void replaceFileSuffix (char *p_dst, const char *p_src, const char *p_suffix) {
    char *p;
    char *p_base = p_dst;
    
    for (; *p_src; p_src++)         // traversal p_src
        *(p_dst++) = *p_src;        // copy p_src to p_dst
    *p_dst = '\0';
    
    for (p=p_dst; ; p--) {          // reverse traversal p_dst
        if (p < p_base || *p=='/' || *p=='\\') {
            break;
        }
        if (*p == '.') {
            p_dst = p;
            break;
        }
    }
    
    *(p_dst++) = '.';
    
    for (; *p_suffix; p_suffix++) { // traversal p_suffix
        *(p_dst++) = *p_suffix;     // copy p_suffix to p_dst
    }
    *p_dst = '\0';
}


static int fileExist (const char *p_filename) {
    FILE *fp = fopen(p_filename, "rb");
    if (fp) fclose(fp);
    return (fp != NULL);
}


#define  MAX_N_FILE  999


static void parseCommand (
    int   argc, char **argv,
    int   switches[128],
    int  *p_n_file,
    char *src_fnames[MAX_N_FILE],
    char *dst_fnames[MAX_N_FILE]
) {
    int i, next_is_dst=0;
    
    for (i=0; i<128; i++)
        switches[i] = 0;
    
    for (i=0; i<MAX_N_FILE; i++)
        src_fnames[i] = dst_fnames[i] = NULL;
    
    (*p_n_file) = -1;
    
    for (i=1; i<argc; i++) {
        char *arg = argv[i];
        
        if      (arg[0] == '-') {                   // parse switches
            
            for (arg++ ; *arg ; arg++) {
                if (0<= (int)(*arg) && (int)(*arg) < 128)
                    switches[(int)(*arg)] = 1;
                
                if (*arg == 'o')
                    next_is_dst = 1;
            }
            
        } else if ((*p_n_file)+1 < MAX_N_FILE) {   // parse file names
            
            if (next_is_dst) {
                next_is_dst = 0;
                
                dst_fnames[(*p_n_file)] = arg;
            } else {
                (*p_n_file)++;
                
                src_fnames[(*p_n_file)] = arg;
            }
        }
    }
    
    (*p_n_file)++;
}


#define  ERROR(error_message,fname) {   \
    printf("   ***ERROR: ");            \
    printf((error_message), (fname));   \
    printf("\n");                       \
    continue;                           \
}


int main (int argc, char **argv) {
    int  i_file, n_file, n_success=0, n_failed;
    
    int  switches[128];
    char *src_fnames[MAX_N_FILE], *dst_fnames[MAX_N_FILE];
    
    int force_write, jls_near;
    
    parseCommand(argc, argv, switches, &n_file, src_fnames, dst_fnames);
    
    force_write = switches['F'] || switches['f'];
    jls_near    = switches['4']?4: switches['3']?3: switches['2']?2: switches['1']?1: 0;
    
    if (n_file <= 0) {
        printf(USAGE);
        return -1;
    }
    
    
    for (i_file=0; i_file<n_file; i_file++) {
        char *p_src_fname = src_fnames[i_file];
        char *p_dst_fname = dst_fnames[i_file];
        
        uint8_t *img_buf=NULL;
        uint32_t height=0, width=0;
        int      is_rgb=0;
        int      failed=0;
        
        if (p_dst_fname == NULL) {
            static char dst_fname_buffer [16384];
            p_dst_fname = dst_fname_buffer;
            replaceFileSuffix(p_dst_fname, p_src_fname, "png");
        }
        
        printf("(%d/%d)  %s -> %s\n", i_file+1, n_file, p_src_fname, p_dst_fname);
        
        if (!fileExist(p_src_fname)) ERROR("%s not exist", p_src_fname);
        
        if (!force_write && fileExist(p_dst_fname)) ERROR("%s already exist", p_dst_fname);
        
        if (img_buf==NULL) img_buf = loadPNMImageFile(p_src_fname, &is_rgb, &height, &width);
        if (img_buf==NULL) img_buf = loadPNGImageFile(p_src_fname, &is_rgb, &height, &width);
        if (img_buf==NULL) img_buf = loadBMPImageFile(p_src_fname, &is_rgb, &height, &width);
        if (img_buf==NULL) img_buf = loadQOIImageFile(p_src_fname, &is_rgb, &height, &width);
        if (img_buf==NULL) ERROR("open %s failed", p_src_fname);
        
        if (matchSuffixIgnoringCase(p_dst_fname, "pnm") || matchSuffixIgnoringCase(p_dst_fname, "ppm") || matchSuffixIgnoringCase(p_dst_fname, "pgm")) {
            failed = writePNMImageFile(p_dst_fname, img_buf, is_rgb, height, width);
        } else if (matchSuffixIgnoringCase(p_dst_fname, "png")) {
            failed = writePNGImageFile(p_dst_fname, img_buf, is_rgb, height, width);
        } else if (matchSuffixIgnoringCase(p_dst_fname, "bmp")) {
            failed = writeBMPImageFile(p_dst_fname, img_buf, is_rgb, height, width);
        } else if (matchSuffixIgnoringCase(p_dst_fname, "qoi")) {
            failed = writeQOIImageFile(p_dst_fname, img_buf, is_rgb, height, width);
        } else if (matchSuffixIgnoringCase(p_dst_fname, "jls")) {
            failed = writeJLSImageFile(p_dst_fname, img_buf, is_rgb, height, width, jls_near);
        } else if (matchSuffixIgnoringCase(p_dst_fname, "h265") || matchSuffixIgnoringCase(p_dst_fname, "265") || matchSuffixIgnoringCase(p_dst_fname, "hevc")) {
            failed = writeHEVCImageFile(p_dst_fname,img_buf, is_rgb, height, width, jls_near);
        } else {
            free(img_buf);
            ERROR("unsupported output suffix: %s", p_dst_fname);
        }
        
        free(img_buf);
        
        if (failed) ERROR("write %s failed", p_dst_fname);
        
        n_success ++;
    }
    
    
    n_failed = n_file - n_success;
    
    if (n_file > 1) {
        printf("\nsummary:");
        if (n_success) printf("  %d file converted", n_success);
        if (n_failed)  printf("  %d failed"        , n_failed);
        printf("\n");
    }
    
    return n_failed;
}
