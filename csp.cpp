
#include "stdafx.h"
#include "x264.h"
extern "C"{
    #include "./libdec/libavutil/pixfmt.h"
}

int x264_from_ffmpeg(int pixfmt){
    static int x264_pixfmt[71];
    static int n = 0;
    if(n == 0){
        memset(&x264_pixfmt[0], x264_csp_none, sizeof(x264_pixfmt));
        x264_pixfmt[PIX_FMT_YUV420P] = x264_csp_i420;
        x264_pixfmt[PIX_FMT_NV12] = x264_csp_nv12;

        n = 1;
    }

    if(pixfmt < 0 && pixfmt >= sizeof(x264_pixfmt)){
        return x264_csp_none;
    }
    return x264_pixfmt[pixfmt];
}