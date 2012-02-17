
#ifndef x264_dll_h
#define x264_dll_h

#define x264_type_auto          0x0000
#define x264_type_idr           0x0001
#define x264_type_i             0x0002
#define x264_type_p             0x0003
#define x264_type_bref          0x0004
#define x264_type_b             0x0005
#define x264_type_keyframe      0x0006

#define x264_csp_mask           0x00ff  /* */
#define x264_csp_none           0x0000  /* Invalid mode     */
#define x264_csp_i420           0x0001  /* yuv 4:2:0 planar */
#define x264_csp_yv12           0x0002  /* yvu 4:2:0 planar */
#define x264_csp_nv12           0x0003  /* yuv 4:2:0, with one y plane and one packed u+v */
#define x264_csp_max            0x0004  /* end of list */
#define x264_csp_vflip          0x1000  /* */

enum nal_unit_type
{
    nal_unknown     = 0,
    nal_slice       = 1,
    nal_slice_dpa   = 2,
    nal_slice_dpb   = 3,
    nal_slice_dpc   = 4,
    nal_slice_idr   = 5,
    nal_sei         = 6,
    nal_sps         = 7,
    nal_pps         = 8,
    nal_aud         = 9,
    nal_filter      = 12,
    /* ref_idc == 0 for 6,9,10,11,12 */
};

typedef struct
{
    int i_ref_idc;  /* nal_priority_e */
    int i_type;     /* nal_unit_type_e */
    int b_long_startcode;
    int i_first_mb; /* If this NAL is a slice, the index of the first MB in the slice. */
    int i_last_mb;  /* If this NAL is a slice, the index of the last MB in the slice. */

    /* Size of payload in bytes. */
    int     i_payload;
    /* If param->b_annexb is set, Annex-B bytestream with startcode.
     * Otherwise, startcode is replaced with a 4-byte size.
     * This size is the size used in mp4/similar muxing; it is equal to i_payload-4 */
    unsigned char* p_payload;
} x264_nal;

struct x264_rawpic{
    // colorspace,  encoding: in, decoding: out
    int       i_csp;
    // number of image planes, encoding: in, decoding: out
    int       i_planes;
    // strides for each plane, encoding: in, decoding: out
    int       i_stride[4];
    // height for decoded picture, encoding: not used, decoding: out
    int       i_height;
    // width for decoded picture, encoding: not used, decoding: out
    int       i_width;

    // presentation time stamp
    // encoding: in/out, 
    // decoding: in/out, must in ms
	__int64   io_pts;
    // decoding timestamp, encoding: out, decoding: used internal
	__int64   o_dts;
    // frame type, encoding, out, decoding: used internal, don't touch
	int	      o_type;
    // key frame or not, encoding: out, decoding, in
    int       o_keyfrm;
    
    union{
        // number of nal, encoding: out, decoding: not used
        int o_nr_nal;
        // pointer to next x264_rawpic struct, encoding: not used, decoding: out
        x264_rawpic* next;
    };

    union{
        // pointers to each plane, encoding: in, decoding: out
        char* i_plane[4];
        // pointer to nal struct, encoding: out, decoding: in
        x264_nal* o_nal;
    };
};

extern "C" {
   __declspec(dllexport) int x264_open(int width, int height, int framerate, int bitrate, int ifrm_min_interval, int ifrm_max_interval, int tps);
   __declspec(dllexport) int x264_enc_header(int handle, x264_rawpic* out);
   __declspec(dllexport) int x264_encode(int handle, x264_rawpic* in, x264_rawpic* out);
   __declspec(dllexport) int x264_close(int handle);

   __declspec(dllexport)  int h264_open();
   __declspec(dllexport)  void h264_delay_allowed(int handle, unsigned int ms);
   __declspec(dllexport)  int h264_decode(int handle, x264_rawpic* in, x264_rawpic** out);
   __declspec(dllexport)  int h264_release(int handle, x264_rawpic* pic);
   __declspec(dllexport)  int h264_close(int handle);
}

#endif