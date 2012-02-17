
#include "stdafx.h"
#include <list>
#include <assert.h>
#include "x264.h"
#include "bufman.h"
using std::list;

extern "C"{
    #include "./libdec/libavcodec/avcodec.h"
    #include "./libdec/libavcodec/imgconvert.h"
    #include "./libdec/libavutil/pixfmt.h"
    #include "./libdec/libavutil/mem.h"
}
#pragma comment(lib, "./lib/libdec.lib")

#pragma warning(disable: 4311)
#pragma warning(disable: 4312)

struct h264_handle_wrapper{
    AVCodecContext* h264_handle;
    AVFrame* frame_buffer;
    buffer_manager bufman;

    int id;
    int refs;

    unsigned int width;
    unsigned int height;
    int size[4];
    int stride[4];
    __int64 pts;
    __int64 base_ticks;
    __int64 base_pts;
    __int64 base_dts;
    unsigned int ms_delay;
    int last_pic_num;

#ifdef _DEBUG
    DWORD tid;
#endif
};

#define invalid_offset  ((__int64) 0x7fffffffffffffff)

#define align_up(addr, n)   (((addr) + (n) - 1) &~ ((n) - 1))
#define align_down(addr, n)   ((addr) &~ ((n) - 1))

static void initial_raw_pic(AVCodecContext* context, 
           h264_handle_wrapper* wrapper, x264_rawpic* raw_pic, int* size, int* stride){
    extern int x264_from_ffmpeg(int pixfmt);

    raw_pic->i_csp = x264_from_ffmpeg((int) context->pix_fmt);
    raw_pic->i_height = wrapper->height;
    raw_pic->i_width = wrapper->width;
    memcpy(&raw_pic->i_stride[0], stride, sizeof(raw_pic->i_stride));

    int base = (int) (raw_pic + 1);
    assert(align_up(base, 16) == align_down(base, 16));

    raw_pic->i_plane[0] = (char *) base;
    raw_pic->i_planes = 1;

    for(int i = 1; i < 4; ++i){
        if(size[i] <= 0){
            break;
        }

        raw_pic->i_plane[i] = raw_pic->i_plane[i - 1] + size[i - 1];
        ++raw_pic->i_planes;
    }

#ifndef exactly_neednot
    if(size[1] && !size[2]){
        ff_set_systematic_pal((uint32_t *)raw_pic->i_plane[1], context->pix_fmt);
    }
#endif
}

static int yuv_panels_information(AVCodecContext* context, int* size, int* stride){
    int i;
    int n;
    AVPicture picture;

    int width = context->width;
    int height = context->height;
    int stride_align[4];
    avcodec_align_dimensions2(context, &width, &height, stride_align);

    do{
        ff_fill_linesize(&picture, context->pix_fmt, width);
        width += width & ~(width - 1);

        n = 0;
        for(i = 0; i < 4;  ++i){
            n |= picture.linesize[i] % stride_align[i];
        }
    }while(n);
    memcpy(&stride[0], &picture.linesize[0], sizeof(picture.linesize));

    int panels_size = ff_fill_pointer(&picture, NULL, context->pix_fmt, height);
    if(panels_size < 0){
        return -1;
    }

    n = sizeof(x264_rawpic) + 16;
    for(i = 0; i < 3 && picture.data[i + 1]; ++i){
        size[i] = picture.data[i + 1] - picture.data[i];
        n += align_up(size[i], 16);
    }
    size[i] = panels_size - (picture.data[i] - picture.data[0]);
    n += align_up(size[i], 16);

    return n;
}

int alloc_frame_buffer(AVCodecContext* context, AVFrame *pic){
    h264_handle_wrapper* wrapper = (h264_handle_wrapper *) context->opaque;
    verify_thread(wrapper->tid);

    if(avcodec_check_dimensions(context,  context->width, context->height)){
        return -1;
    }

    buffer_manager* bufman = &wrapper->bufman;
    wrapper->last_pic_num++;

    int* size = &wrapper->size[0];
    int* stride = &wrapper->stride[0];
    if(context->width != wrapper->width || context->height != wrapper->height){
        int new_length = yuv_panels_information(context, size, stride);
        if(new_length == -1){
            return -1;
        }
        bufman->new_buffer_length(new_length);

        wrapper->width = context->width;
        wrapper->height = context->height;
    }

    int addr = (int) bufman->bind_frame();
    if(addr == (int) (NULL)){
        return -1;
    }

    int yuv = addr + sizeof(x264_rawpic) + 16;
    x264_rawpic* raw_pic = (x264_rawpic *) (align_down(yuv, 16) - sizeof(x264_rawpic));
    raw_pic->io_pts = wrapper->pts;
    // reuse raw_pic->o_type to save the orignal address
    raw_pic->o_type = addr;

    if(size[0] != 0 || pic->data[0] == NULL){
        initial_raw_pic(context, wrapper, raw_pic, size, stride);
    }

    // reuse raw_pic->o_dts to save the pic number
    if(raw_pic->o_dts == 0){
        pic->age = 256 * 256 * 256 * 64;
        raw_pic->o_dts = -pic->age;
    }else{
        pic->age = wrapper->last_pic_num - raw_pic->o_dts;
        raw_pic->o_dts = wrapper->last_pic_num;
    }

    pic->type = 1;
    memcpy(&pic->data[0], &raw_pic->i_plane[0], sizeof(raw_pic->i_plane));
    memcpy(&pic->linesize[0], &raw_pic->i_stride[0], sizeof(raw_pic->i_stride));
    return 0;
}

void free_frame_buffer(AVCodecContext* context, AVFrame* pic){
    h264_handle_wrapper* wrapper = (h264_handle_wrapper *) context->opaque;
    // I assumed the free_frame_buffer is called in same thread that 
    // calling the avcodec_decode_video2, however, it turns out that it
    // not true. 
    // So, I commenced this line. Keeps it for as a reminding at later day
    // verify_thread(wrapper->tid);

    char* buffer = (char *) pic->data[0];
    assert(buffer);
    x264_rawpic* raw_pic = (x264_rawpic *) (buffer - sizeof(x264_rawpic));
    // i reused raw_pic->o_type to save buffer address returned from bufman
    // see alloc_frame_buffer() function.
    buffer = (char *) raw_pic->o_type;

    for(int i = 0; i < 4; ++i){
        pic->data[i] = NULL;
    }

    buffer_manager* bufman = &wrapper->bufman;
    bufman->unbind_frame(buffer);
}

#undef align_up
#undef align_down

//=========================================================================

extern CRITICAL_SECTION cse;

struct h264_resource{
    unsigned int nr_entry;
    h264_handle_wrapper* entries[16];
};

#define invalid_id  -1
h264_resource hres = {0,};
int hid = invalid_id;

static int identy(h264_handle_wrapper* wrapper){
    assert(hres.nr_entry < 16);
    int i;

    for(i = 0; i < 16; ++i){
        if(hres.entries[i] == NULL){
            break;
        }
    }
    assert(i < 16);

    int base = hid & 0xfffffff0;
    hid = hid & 0x0000000f;

    if(hid < i){
        hid = base + i;
    }else{
        hid = base + i + 16;
    }

    if(hid == invalid_id){
        hid += 16;
    }
    hres.entries[i] = wrapper;
    wrapper->id = hid;
    ++hres.nr_entry;

    return hid;
}

int h264_open(){
    EnterCriticalSection(&cse);
    if(hres.nr_entry == 16){
        goto LABEL;
    }else if(hres.nr_entry == 0){
        // the following two function can be call multi times safely
        // and the library has not uninit, unregister functions
        avcodec_init();
        avcodec_register_all(); 
    }

    AVCodec* codec = avcodec_find_decoder(CODEC_ID_H264); 
    if(codec == NULL){
        goto LABEL;
    }
    
    AVCodecContext* codec_context = avcodec_alloc_context();
    if(codec_context == NULL){
        goto LABEL;
    }

    AVFrame* frame_buffer = avcodec_alloc_frame();
    if(frame_buffer == NULL){
        av_free(codec_context);
        goto LABEL;
    }

    if(0 != avcodec_open(codec_context, codec)){
        av_free(frame_buffer);
        av_free(codec_context);
        goto LABEL;
    }

    assert(codec->capabilities & CODEC_CAP_DR1);
    codec_context->get_buffer = alloc_frame_buffer;
    codec_context->release_buffer = free_frame_buffer;
    codec_context->flags |= CODEC_FLAG_EMU_EDGE;
    codec_context->flags2 |= CODEC_FLAG2_CHUNKS;

    h264_handle_wrapper* handle = new (std::nothrow) h264_handle_wrapper();
    if(handle == NULL){
        av_free(frame_buffer);
        av_free(codec_context);
        goto LABEL;
    }
    
    int id = identy(handle);
    LeaveCriticalSection(&cse);

    handle->h264_handle = codec_context;
    handle->frame_buffer = frame_buffer;
    handle->height = -1;
    handle->width = -1;
    handle->size[0] = handle->size[1] = handle->size[2] = handle->size[3] = 0;
    handle->stride[0] = handle->stride[1] = handle->stride[2] = handle->stride[3] = 0;
    codec_context->opaque = handle;

    handle->refs = 1;
    handle->ms_delay = (unsigned int) (-1);
    handle->base_pts = 0;
    handle->base_dts = 0;
    handle->base_ticks = 0;
    handle->last_pic_num = 0;

#ifdef _DEBUG
    handle->tid = 0xffffffff;
#endif
    return id;
LABEL:
    LeaveCriticalSection(&cse);
    return -1;
}

static inline h264_handle_wrapper* wrapper_get(int id){
    int indx = id % 16;
    EnterCriticalSection(&cse);

    h264_handle_wrapper* wrapper = hres.entries[indx];
    if(wrapper == NULL || wrapper->id != id || wrapper->refs == 0){
        LeaveCriticalSection(&cse);
        return NULL;
    }
    ++wrapper->refs;

    LeaveCriticalSection(&cse);
    return wrapper;
}

static inline void wrapper_put(h264_handle_wrapper* wrapper){
    assert(wrapper);

    EnterCriticalSection(&cse);
    int n = (--wrapper->refs);
    LeaveCriticalSection(&cse);

    assert(n >= 0);
    if(n == 0){
        int indx = wrapper->id % 16;
        assert(hres.entries[indx]);

        avcodec_close(wrapper->h264_handle);
        av_free(wrapper->h264_handle);
        av_free(wrapper->frame_buffer);

        hres.entries[indx] = NULL;
        EnterCriticalSection(&cse);
        --hres.nr_entry;
        LeaveCriticalSection(&cse);

        delete wrapper;
    }
}

void h264_delay_allowed(int handle, unsigned int ms){
    assert(handle != -1);
    h264_handle_wrapper* wrapper = wrapper_get(handle);
    if(wrapper == NULL){
        return;
    }

    wrapper->ms_delay = ms;
    wrapper_put(wrapper);
}

static int decode_nal(h264_handle_wrapper* wrapper, char* nal_buf, int nal_length, x264_rawpic** out){
    x264_rawpic** cur = out;

    if(nal_length < 0){
        return -1;
    }

    buffer_manager* bufman = &wrapper->bufman;
    AVCodecContext* h264_handle = wrapper->h264_handle;
    AVFrame* frame_buffer = wrapper->frame_buffer;

    AVPacket avpkt;
    av_init_packet(&avpkt);
    avpkt.data = (uint8_t *) nal_buf;
    avpkt.size = nal_length;

    while((nal_length == 0) || (avpkt.size > 0)){
        int pic_ready = 0;
        int consumed = avcodec_decode_video2(h264_handle, frame_buffer, &pic_ready, &avpkt);
        if(consumed < 0){
            return -1;
        }

        if(consumed == 0 && pic_ready == 0){
            break;
        }

        if(pic_ready != 0){
            char* buffer = (char *) frame_buffer->data[0];
            assert(buffer);

            x264_rawpic* pic = (x264_rawpic *) (buffer - sizeof(x264_rawpic));
            // i reused raw_pic->o_type to save buffer address returned from bufman
            // see alloc_frame_buffer() function.
            buffer = (char *) pic->o_type;
            bufman->output_binded_buffer(buffer);

            pic->next = 0;
            *cur = pic;
            cur = &((*cur)->next);
        }

        avpkt.size -= consumed;
        avpkt.data += consumed;
        assert(avpkt.size >= 0);
    }

    // to allow user to access head&tail of the list without loop
    if(*out != NULL){
        x264_rawpic* last = (x264_rawpic *) ((unsigned int) cur - offsetof(x264_rawpic, next));
        *cur = *out;
        *out = last;
    }

    return 0;
}

static bool need_drop(h264_handle_wrapper* wrapper, __int64 pts, __int64 dts){
    int ms_delayed;
    DWORD current = GetTickCount();

    if(wrapper->base_ticks == 0){
        wrapper->base_dts = dts;
        wrapper->base_pts = pts;
        wrapper->base_ticks = current;
        ms_delayed = 0;
    }else{
        DWORD ideal_tick = (DWORD) (wrapper->base_ticks + dts - wrapper->base_dts);
        ms_delayed = current - ideal_tick;
#if !playback
        if(ms_delayed < 0){
            wrapper->base_ticks += ms_delayed;
            pts += ms_delayed;
        }
#endif
    }

    if(wrapper->ms_delay != (unsigned) (-1) && ms_delayed > wrapper->ms_delay){
        return true;
    }
    
    wrapper->pts = pts - wrapper->base_pts;
    return false;
}

int h264_decode(int handle, x264_rawpic* in, x264_rawpic** out){
    assert(handle != -1);
    if(out == NULL){
        return -1;
    }
    *out = NULL;

    if(in && in->o_nr_nal != 1){
        return -1;
    }

    h264_handle_wrapper* wrapper = wrapper_get(handle);
    if(wrapper == NULL){
        return -1;
    }
    verify_thread(wrapper->tid);
    buffer_manager* bufman = &wrapper->bufman;

    char* nal_buffer;
    int nal_length;
    if(in == NULL){
        nal_buffer = NULL;
        nal_length = 0;
    }else{
        nal_buffer = (char *) in->o_nal->p_payload;
        nal_length = in->o_nal->i_payload;
    }

    if(nal_length < 0 || nal_length > 0 && nal_length < 5){
        wrapper_put(wrapper);
        return -1;
    }
    
    bool drop = false;
    if(nal_length != 0){
        if(in->o_type >= nal_unknown && in->o_type < nal_sei){
            drop = need_drop(wrapper, in->io_pts, in->o_dts);
            if(drop){
                drop = (in->o_keyfrm == 0);
            }
        }
    }else{
        assert(false);
    }
    
    if(!drop){
        decode_nal(wrapper, nal_buffer, nal_length, out);
    }
    wrapper_put(wrapper);
    return 0;
}

int h264_close(int handle){
    assert(handle != -1);
    h264_handle_wrapper* wrapper = wrapper_get(handle);
    if(wrapper == NULL){
        return -1;
    }

    InterlockedDecrement((long *) &wrapper->refs);
    wrapper_put(wrapper);
    return 0;
}

int h264_release(int handle, x264_rawpic* pic){
    assert(handle != -1);
    assert(pic != NULL);

    h264_handle_wrapper* wrapper = wrapper_get(handle);
    if(wrapper == NULL){
        assert(false);
        return -1;
    }

    buffer_manager* bufman = &wrapper->bufman;
    char* buffer = (char *) pic->o_type;
    bufman->release_out_buffer(buffer);

    wrapper_put(wrapper);
    return 0;
} 