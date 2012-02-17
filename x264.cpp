// x264dll.cpp : 定义 DLL 应用程序的入口点。
//
#include "stdafx.h"
#include "./libenc/common/common.h"
#include "./libenc/x264.h"
#include "x264.h"


extern int frame_type[7];

#if X264_TYPE_AUTO != 0
#pragma error "x264 macro defination changed!"
#endif

#if X264_TYPE_KEYFRAME != 0x0006
#pragma error "x264 macro defination changed!"
#endif

CRITICAL_SECTION cse;

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		frame_type[X264_TYPE_AUTO] = x264_type_auto;
		frame_type[X264_TYPE_IDR] = x264_type_idr;
		frame_type[X264_TYPE_I] = x264_type_i;
		frame_type[X264_TYPE_P] = x264_type_p;
		frame_type[X264_TYPE_BREF] = x264_type_bref;
		frame_type[X264_TYPE_B] = x264_type_b;
		frame_type[X264_TYPE_KEYFRAME] = x264_type_keyframe;

        InitializeCriticalSection(&cse);
		break;
	case DLL_PROCESS_DETACH:
        DeleteCriticalSection(&cse);
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	}
    return TRUE;
}

