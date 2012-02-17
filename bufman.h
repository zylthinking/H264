
#ifndef bufman_h
#define bufman_h

#include <list>
using std::list;

#define flag_free           0x00
#define flag_binded         0x01
#define flag_output         0x02

struct buffer_header{
    unsigned int flag;
    unsigned int length;
    char* buffer;
};

class buffer_manager{
    CRITICAL_SECTION cse;
    unsigned int buffer_length;
    list<buffer_header *> free_list;
#ifdef _DEBUG
    int nr_alloc;
#endif

public:
    buffer_manager();
    ~buffer_manager();

    void new_buffer_length(unsigned int length);
    char* bind_frame();
    void unbind_frame(char* buffer);
    
    void output_binded_buffer(char* buf);
    void release_out_buffer(char* buf);
};

#endif