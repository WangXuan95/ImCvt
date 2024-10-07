 ![language](https://img.shields.io/badge/language-C-green.svg) ![build](https://img.shields.io/badge/build-Windows-blue.svg) ![build](https://img.shields.io/badge/build-linux-FF1010.svg) 

ImCvt
===========================

**ImCvt** is a **image format converter** which includes lightweight encoders/decoders of several image formats:

|                            format                            | file suffix |                     supported color type                     |         convert from          |       convert to        |                   source code                    |
| :----------------------------------------------------------: | :---------: | :----------------------------------------------------------: | :---------------------------: | :---------------------: | :----------------------------------------------: |
|    **[PNM](https://netpbm.sourceforge.net/doc/pnm.html)**    |    .pnm     | ![gray8](https://img.shields.io/badge/-Gray8-lightgray.svg) ![rgb24](https://img.shields.io/badge/-RGB24-c534e0.svg) |      :white_check_mark:       |   :white_check_mark:    |      [150 lines of C](./src/imageio_pnm.c)       |
|         **[PNG](https://en.wikipedia.org/wiki/PNG)**         |    .png     | ![gray8](https://img.shields.io/badge/-Gray8-lightgray.svg) ![rgb24](https://img.shields.io/badge/-RGB24-c534e0.svg) |    :ballot_box_with_check:    | :ballot_box_with_check: | [uPNG](https://github.com/elanthis/upng) library |
|   **[BMP](https://en.wikipedia.org/wiki/BMP_file_format)**   |    .bmp     | ![gray8](https://img.shields.io/badge/-Gray8-lightgray.svg) ![rgb24](https://img.shields.io/badge/-RGB24-c534e0.svg) |    :ballot_box_with_check:    | :ballot_box_with_check: |      [180 lines of C](./src/imageio_bmp.c)       |
|              **[QOI](https://qoiformat.org/)**               |    .qoi     |   ![rgb24](https://img.shields.io/badge/-RGB24-c534e0.svg)   |      :white_check_mark:       |   :white_check_mark:    |      [240 lines of C](./src/imageio_qoi.c)       |
|     **[JPEG-LS](https://www.itu.int/rec/T-REC-T.87/en)**     |    .jls     | ![gray8](https://img.shields.io/badge/-Gray8-lightgray.svg) ![rgb24](https://img.shields.io/badge/-RGB24-c534e0.svg) | :negative_squared_cross_mark: | :ballot_box_with_check: |      [480 lines of C](./src/imageio_jls.c)       |
| **[H.265](https://en.wikipedia.org/wiki/High_Efficiency_Video_Coding)** |    .h265    | ![gray8](https://img.shields.io/badge/-Gray8-lightgray.svg)  | :negative_squared_cross_mark: | :ballot_box_with_check: |          [1650 lines of C](./src/HEVCe)          |

> :white_check_mark: = fully supported
>
> :ballot_box_with_check: = partially supported
>
> :negative_squared_cross_mark: = unsupported

Illustration:

* **[PNM](https://netpbm.sourceforge.net/doc/pnm.html)** (Portable Any Map), **[PGM](https://netpbm.sourceforge.net/doc/pgm.html)** (Portable Gray Map), and **[PPM](https://netpbm.sourceforge.net/doc/ppm.html)** (Portable Pix Map) are simple uncompressed image formats which store raw pixels.
* **[PNG](https://en.wikipedia.org/wiki/PNG)** (Portable Network Graph) is the most popular lossless image compression format. This repo uses [uPNG](https://github.com/elanthis/upng) library to decode PNG.
* **[BMP](https://en.wikipedia.org/wiki/BMP_file_format)** (Bitmap Image File) is a popular uncompressed image formats which store raw pixels.
* **[QOI](https://qoiformat.org/)** (Quite OK Image) is a simple, fast lossless RGB image compression format. This repo implements a simple QOI encoder/decoder in only 240 lines of C.
* **[JPEG-LS](https://www.itu.int/rec/T-REC-T.87/en)** is a lossless/lossy image compression standard which can get better grayscale compression ratio compared to PNG and Lossless-WEBP. JPEG-LS uses the maximum difference between the pixels before and after compression (NEAR value) to control distortion, **NEAR=0** is the lossless mode; **NEAR>0** is the lossy mode. The specification of JPEG-LS is [ITU-T T.87](https://www.itu.int/rec/T-REC-T.87/en) [1]. Another reference JPEG-LS encoder/decoder implementation [can be found in UBC's website](http://www.stat.columbia.edu/~jakulin/jpeg-ls/mirror.htm). This repo implements a simpler JPEG-LS encoder in only 480 lines of C.
* **[H.265/HEVC](https://en.wikipedia.org/wiki/High_Efficiency_Video_Coding)** is a video coding standard. This repo implements a lightweight grayscale 8-bit H.265/HEVC intra-frame image compressor in only 1650 lines of C, which may be the simplest H.265 implementation.

　

　

# Compile

### compile in Windows (MinGW)

If you installed MinGW, run following compiling command in CMD:

```powershell
gcc src\*.c src\HEVCe\HEVCe.c src\uPNG\uPNG.c -static -O3 -Wall -Wno-array-bounds -o ImCvt.exe
```

which will get executable file [**ImCvt.exe**](./ImCvt.exe)

### compile in Windows (MSVC)

Also, you can use MSVC to compile. If you added MSVC (cl.exe) to your environment, run following compiling command in CMD:

```powershell
cl src\*.c src\HEVCe\HEVCe.c src\uPNG\uPNG.c /MT /Ox /FeImCvt.exe
```

which will get executable file [**ImCvt.exe**](./ImCvt.exe)

### compile in Linux (gcc)

```bash
gcc src/*.c src/HEVCe/HEVCe.c src/uPNG/uPNG.c -static -O3 -Wall -Wno-array-bounds -o ImCvt
```

which will get Linux binary file [**ImCvt**](./ImCvt)

　

　

# Usage

Run the program without any parameters to display usage:

```
> .\ImCvt.exe
|------------------------------------------------------------------------------------|
| ImageConverter (v0.5)  by https://github.com/WangXuan95/                           |
|------------------------------------------------------------------------------------|
| Usage:                                                                             |
|   ImCvt [-switches]  <in1> -o <out1>  [<in2> -o <out2]  ...                        |
|                                                                                    |
| Where <in> and <out> can be:                                                       |
|   .pnm (Portable Any Map)          : gray 8-bit or RGB 24-bit                      |
|   .pgm (Portable Gray Map)         : gray 8-bit                                    |
|   .ppm (Portable Pix Map)          : RGB 24-bit                                    |
|   .png (Portable Network Graphics) : gray 8-bit or RGB 24-bit                      |
|   .bmp (Bitmap Image File)         : gray 8-bit or RGB 24-bit                      |
|   .qoi (Quite OK Image)            : RGB 24-bit                                    |
|   .jls (JPEG-LS Image)             : gray 8-bit or RGB 24-bit, can be <out> only!  |
|   .h265 (H.265/HEVC Image)         : gray 8-bit              , can be <out> only!  |
|                                                                                    |
| switches:    -f                    : force overwrite of output file                |
|              -0, -1, -2, -3, -4    : JPEG-LS near value or H.265 (qp-4)/6 value    |
|------------------------------------------------------------------------------------|
```

### Example Usage

convert a PNG to a PNM:

```powershell
ImCvt.exe image\1.png -o image\1.pnm
```

convert a PNM to a QOI:

```powershell
ImCvt.exe image\1.pnm -o image\1.qoi
```

convert a QOI to a BMP:

```powershell
ImCvt.exe image\1.qoi -o image\1.bmp
```

convert a BMP to a JPEG-LS, with near=1 (lossy):

```powershell
ImCvt.exe image\1.bmp -o image\1.jls -1
```

convert a BMP to a H.265 image file, with (qp-4)/6=2 (lossy):

```powershell
ImCvt.exe image\1.bmp -o image\1.h265
```

convert multiple files by a single command:

```powershell
ImCvt.exe -f image\1.png -o image\1.qoi image\2.png -o image\2.jls image\3.png -o image\3.bmp
```

　

　

# About JPEG-LS

This repo only offers [JPEG-LS compressor](./src/imageio_jls.c) instead of decompressor. We show two ways to decompress a .jls file.

The most convenient way is to use [this website](https://products.groupdocs.app/viewer/JLS) to view .jls image online (but it may not work sometimes).

Another way is to use python `pillow` and `pillow_jpls` library [4]. You should firstly install them use `pip` :

```
python -m pip install pillow pillow_jpls
```

Then use the following python script to decompress a .jls file to a .png file:

```python
from PIL import Image
import pillow_jpls
Image.open("2.jls").save("2d.png")   # decompress 2.jls to 2d.png
```

　

　

# About H.265

This repo implements [a lightweight grayscale 8-bit H.265/HEVC intra-frame image compressor](./src/HEVCe/) in only 1650 lines of C, which may be the simplest H.265 implementation.

* This code ([**HEVCe.c**](./src/HEVCe/HEVCe.c)) is highly portable：
  - Only use two data types: `unsigned char` and `int`;
  - Do not include any headers;
  - Do not use dynamic memory.
* Supported H.265/HEVC features:
  - Quality parameter can be 0~4, corresponds to Quantize Parameter (QP) = 4, 10, 16, 22, 28.
  - CTU : 32x32
  - CU : 32x32, 16x16, 8x8
  - TU : 32x32, 16x16, 8x8, 4x4
  - The maximum depth of CU splitting into TUs is 1 (each CU is treated as a TU, or divided into 4 small TUs, while the small TUs are not divided into smaller TUs).
  - part_mode: The 8x8 CU is treated as a PU (`PART_2Nx2N`), or divided into 4 PUs (`PART_NxN`)
  - Supports all 35 prediction modes
  - Simplified RDOQ (Rate Distortion Optimized Quantize)

For more knowledge about H.265, see [H.265/HEVC 帧内编码详解：CU层次结构、预测、变换、量化、编码](https://zhuanlan.zhihu.com/p/607679114) [10]

### Source code of H.265

The source code is in [src/HEVCe/](./src/HEVCe/) , includes two files:

- [HEVCe.c](./src/HEVCe/HEVCe.c) : implements a H.265/HEVC gray 8-bit encoder.
- [HEVCe.h](./src/HEVCe/HEVCe.h) : header for users.

The users can call the `HEVCImageEncoder` function in [HEVCe.h](./src/HEVCe/HEVCe.h) :

```c
int HEVCImageEncoder (                 // return the length of encoded stream (unit: bytes)
    unsigned char       *pbuffer,      // encoded stream will be stored here
    const unsigned char *img,          // The original grayscale pixels of the image need to be input here, with each pixel occupying 8-bit (i.e. an unsigned char), in the order of left to right and top to bottom.
    unsigned char       *img_rcon,     // The pixel buffer of the reconstructed image, with each pixel occupying 8-bit (i.e. an unsigned char), in the order of left to right and top to bottom. Note: Even if you don't care about the reconstructed image, you must pass in an array space of the same size as the input image here, otherwise the image encoder will not work.
    int                 *ysz,          // Enter the height of the image here. For values that are not multiples of 32, they will be supplemented with multiples of 32, so this is a pointer and the function will modify the value internally.
    int                 *xsz,          // Enter the width of the image here. For values that are not multiples of 32, they will be supplemented with multiples of 32, so this is a pointer and the function will modify the value internally.
    const int            qpd6          // Quality parameter, can be 0~4, corresponds to Quantize Parameter (QP) = 4, 10, 16, 22, 28.
);
```

### Notice

This repo only offers H.265/HEVC compressor instead of decompressor. You can use [File Viewer Plus](https://fileinfo.com/software/windows_file_viewer) or [Elecard HEVC Analyzer](https://elecard-hevc-analyzer.software.informer.com/) to view the compressed .h265 image file.

　

　

# Related links

[1] Official website of QOI : https://qoiformat.org/

[2] ITU-T T.87 : JPEG-LS baseline specification : https://www.itu.int/rec/T-REC-T.87/en

[3] UBC's JPEG-LS baseline Public Domain Code : http://www.stat.columbia.edu/~jakulin/jpeg-ls/mirror.htm

[4] pillow-jpls library for Python :  https://pypi.org/project/pillow-jpls

[5] PNM Image File Specification : https://netpbm.sourceforge.net/doc/pnm.html

[6] uPNG: a simple PNG decoder: https://github.com/elanthis/upng

[7] FPGA-based Verilog QOI compressor/decompressor: https://github.com/WangXuan95/FPGA-QOI

[8] FPGA-based Verilog JPEG-LS encoder (basic version which support 8-bit gray lossless and lossy compression) : https://github.com/WangXuan95/FPGA-JPEG-LS-encoder

[9] FPGA-based Verilog JPEG-LS encoder (ultra high performance version which support 8-bit gray lossless compression) : https://github.com/WangXuan95/UH-JLS

[10] H.265/HEVC 帧内编码详解：CU层次结构、预测、变换、量化、编码：https://zhuanlan.zhihu.com/p/607679114

[11] The H.265/HEVC reference software HM: https://github.com/listenlink/HM
