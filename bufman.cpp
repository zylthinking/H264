
#include "stdafx.h"
#include "bufman.h"
#include <assert.h>

// the h264 decoder library seems running in single thread
// if so, we can avoid any multi-thread protection in this class.
// I did the checking outside before calling into the buffer_manager functions.
// because it looks weired if I do the checking in the buffer_manager class
// however, the release_out_buffer can be called freely by user
// that makes conflicts with h264 decoder library, and the no-locking benifit 
// is decreased to only one place in output_binded_buffer 

buffer_manager::buffer_manager(){
    InitializeCriticalSection(&cse);
    buffer_length = 0;
#ifdef _DEBUG
    nr_alloc = 0;
#endif
}

buffer_manager::~buffer_manager(){
    while(!free_list.empty()){
        buffer_header* cur = free_list.front();
        free_list.pop_front();
        free(cur);
#ifdef _DEBUG
        --nr_alloc;
#endif
    }

#ifdef _DEBUG
    assert(nr_alloc == 0);
    cs << "x264 free" << endl;
#endif
    DeleteCriticalSection(&cse);
}

void buffer_manager::new_buffer_length(unsigned int length){
    if(buffer_length < length){
        buffer_length = length;
    }
}

char* buffer_manager::bind_frame(){
    buffer_header* header = NULL;

    EnterCriticalSection(&cse);
    while(!free_list.empty()){
        buffer_header* cur = free_list.front();
        free_list.pop_front();
        if(cur->length < buffer_length){
            free(cur);
            continue;
        }
        header = cur;
        break;
    }
    LeaveCriticalSection(&cse);

    if(header == NULL){
        header = (buffer_header *) calloc(buffer_length + sizeof(buffer_header), 1);
        if(header == NULL){
            return NULL;
        }
#ifdef _DEBUG
        ++nr_alloc;
#endif
        header->flag = flag_free;
        header->length = buffer_length;
        header->buffer = (char *) (header + 1);
    }
    assert(header->flag == flag_free);

    header->flag = flag_binded;
    return header->buffer;
}

void buffer_manager::unbind_frame(char* buf){
    assert(buf);
    buffer_header* header = (buffer_header *) (buf - sizeof(buffer_header));
    assert(header->flag & flag_binded);

    EnterCriticalSection(&cse);
    if(0 == (header->flag & flag_output)){
        header->flag = flag_free;
        free_list.push_back(header);
    }
    header->flag &= (~flag_binded);
    LeaveCriticalSection(&cse);
}

void buffer_manager::output_binded_buffer(char* buf){
    assert(buf);
    buffer_header* header = (buffer_header *) (buf - sizeof(buffer_header));
    assert(header->flag == flag_binded);

#ifdef exactly_neednot
    header->flag |= flag_output;
#else
    EnterCriticalSection(&cse);
    header->flag |= flag_output;
    LeaveCriticalSection(&cse);
#endif
}

void buffer_manager::release_out_buffer(char* buf){
    assert(buf);
    buffer_header* header = (buffer_header *) (buf - sizeof(buffer_header));
    assert(header->flag & flag_output);

    EnterCriticalSection(&cse);
    if(0 == (header->flag & flag_binded)){
        header->flag = flag_free;
        free_list.push_back(header);
    }
    header->flag &= (~flag_output);
    LeaveCriticalSection(&cse);
}