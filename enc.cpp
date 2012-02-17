
#include "stdafx.h"
#include "cs.h"
#include <assert.h>

#include "./libenc/common/common.h"
#include "./libenc/x264.h"
#pragma comment(lib, "./lib/libenc.lib")

int frame_type[7];

struct x264_handle_wrapper{
	x264_t* x264_handle;
    long id;
    int tps;
	x264_picture_t pic;
};

long current = 0;
x264_handle_wrapper wrapper = {0, -1};
extern CRITICAL_SECTION cse;


int x264_open(int width, int height, int framerate, int bitrate, int ifrm_min_interval, int ifrm_max_interval, int tps){
    long id = InterlockedExchangeAdd(&current, 2);
    InterlockedCompareExchange(&wrapper.id, id, -1);
    if(wrapper.id != id){
        return -1;
    }

	x264_param_t param;
    x264_param_default(&param);

    param.rc.i_bitrate = bitrate;
    param.rc.i_rc_method = X264_RC_ABR;//X264_RC_ABR;
	param.rc.i_vbv_max_bitrate = bitrate * 1.5;
    param.analyse.i_me_method = X264_ME_UMH;
    param.analyse.i_subpel_refine = 9;


    //param.rc.f_rf_constant = 23;
    //param.rc.i_rc_method = X264_RC_CRF;

    //param.rc.i_rc_method = X264_RC_CQP;
    //param.rc.f_ip_factor = atof(value);
    //param.rc.f_pb_factor = atof(value);
    //param.rc.i_qp_constant = atof(value);

    param.rc.f_qcompress = 0;
    param.rc.i_lookahead = 0;
    param.i_frame_reference = 3;
    //param.analyse.b_mixed_references = 0;
    //param.analyse.inter = X264_ANALYSE_I8x8|X264_ANALYSE_I4x4;

    param.b_repeat_headers = 0;
    param.b_annexb = 0;
    param.b_dts_compress = 1;
    param.i_bframe = 0;

	param.b_vfr_input = 0;
    param.i_fps_num = framerate;
    param.i_fps_den = 1;
	param.i_timebase_num = param.i_fps_den;
	param.i_timebase_den = param.i_fps_num;

	param.i_width = width;
	param.i_height = height;
//modify by xingyanju 
//{{
//	param.i_keyint_min = ifrm_min_interval;
//	param.i_open_gop = X264_OPEN_GOP_BLURAY; 
//}}
	param.i_keyint_max = ifrm_max_interval;
	if(param.i_keyint_max < param.i_keyint_min){
		param.i_keyint_max = param.i_keyint_min;
	}

	x264_t* p = x264_encoder_open(&param);
	if(p == NULL){
        wrapper.id = -1;
		return -1;
	}

	wrapper.x264_handle = p;
    wrapper.tps = tps;
	x264_picture_init(&wrapper.pic);

    return (int) wrapper.id;
}

int x264_enc_header(int handle, x264_rawpic* out){
	if(out == NULL || handle == -1 || wrapper.id != handle){
		return -1;
	}

    x264_nal_t *headers;
    int i_nal;

    int n = x264_encoder_headers(wrapper.x264_handle, &headers, &i_nal) ;
    if(n < 0){
        return -1;
    }

    if(i_nal > 0){
        out->o_nal = (x264_nal *) headers;
        out->o_nr_nal = i_nal;
    }else{
        out->o_nal = NULL;
        out->o_nr_nal = 0;
    }
    return out->o_nr_nal;
}

int x264_encode(int handle, x264_rawpic* in, x264_rawpic* out){
	if(out == NULL || handle == -1 || wrapper.id != handle){
		return -1;
	}

	x264_picture_t* pic = &wrapper.pic;
    x264_param_t* param = &wrapper.x264_handle->thread[wrapper.x264_handle->i_thread_phase]->param;
	
	memcpy(pic->img.i_stride, in->i_stride, sizeof(in->i_stride));
    memcpy(pic->img.plane, in->i_plane, sizeof(in->i_plane));
	pic->img.i_csp = in->i_csp;
	pic->img.i_plane = in->i_planes;
	pic->i_type = X264_TYPE_AUTO;
	pic->i_qpplus1 = 0;
    pic->i_pts = in->io_pts;
	
	x264_picture_t pic_out;
    x264_nal_t *nal;
    int i_nal;

    EnterCriticalSection(&cse);
    if(wrapper.id != handle){
        LeaveCriticalSection(&cse);
        return -1;
    }
	int i_frame_size = x264_encoder_encode(wrapper.x264_handle, &nal, &i_nal, pic, &pic_out);
    LeaveCriticalSection(&cse);

	if(i_frame_size < 0){
		return -1;
	}

    if(i_frame_size > 0){
        out->io_pts = (int64_t) ((pic_out.i_pts * wrapper.tps * ((double) param->i_timebase_num / param->i_timebase_den)) + 0.5);
		out->o_dts = (int64_t) ((pic_out.i_dts * wrapper.tps * ((double) param->i_timebase_num / param->i_timebase_den)) + 0.5);
		out->o_type = frame_type[pic_out.i_type];
        out->o_keyfrm = pic_out.b_keyframe;
        out->o_nal = (x264_nal *) nal;
        out->o_nr_nal = i_nal;
    }

    return i_frame_size;
}

int x264_close(int handle){
	if(handle == -1 || wrapper.id != handle){
		return -1;
	}

    EnterCriticalSection(&cse);
	x264_encoder_close(wrapper.x264_handle);
    wrapper.id = -1;
    LeaveCriticalSection(&cse);

	return 0;
}

