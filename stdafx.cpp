// stdafx.cpp : 只包括标准包含文件的源文件
// x264.pch 将作为预编译头
// stdafx.obj 将包含预编译类型信息

#include "stdafx.h"
#include <assert.h>

void verify_thread_fun(DWORD& tid){
    if(tid == 0xffffffff){ 
        tid = GetCurrentThreadId();  
    }else{   
        assert(tid == GetCurrentThreadId());
    }
}