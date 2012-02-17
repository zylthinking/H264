
#ifndef console_buffer_h
#define console_buffer_h

#ifndef _DEBUG
#define cs /##/
#else

#include <windows.h>
#include <ostream>
#include <streambuf>

namespace std{
    template<class charT, class traits = char_traits<charT> >
    class console_buf : public basic_streambuf<charT, traits>
    {
        static HANDLE handle;
        CRITICAL_SECTION sec_out;
    protected:
        char_type buffer[10];

        console_buf(){
            InitializeCriticalSection(&sec_out);
            setp(buffer, buffer + (10 - 1));
            AllocConsole();
            handle = GetStdHandle(STD_OUTPUT_HANDLE);
        }
        ~console_buf(){
            FreeConsole();
            DeleteCriticalSection(&sec_out);
        }
    public:
        static console_buf* instance(){
            static int ref = 0;
            static console_buf* instance = NULL;
            if(!instance){
                instance = new console_buf;
            }

            ++ ref;
            return instance;
        }

        static void release(){
            if(ref == 0)
                return;
            -- ref;
            if(ref == 0){
                delete instance;
                instance = NULL;
            }
        }

    protected:
        int flush(){
            int size = (int) (pptr() - pbase());
            unsigned int to_write = size * sizeof(char_type);
            unsigned long wrote = 0;

            BOOL b = WriteFile(handle, (void *) pbase(), to_write, &wrote, NULL);
            if(!b){
                return -1;
            }
            
            pbump(-size);
            return size;
        }

        virtual int_type overflow(typename traits::int_type c){
            EnterCriticalSection(&sec_out);
            if(!traits::eq_int_type(c, traits::eof())){
                *pptr() = c;
                pbump(1);
            }

            int r = flush();
            LeaveCriticalSection(&sec_out);
            if(r == -1){
                return traits::eof();
            }
            return traits::not_eof(c);
        }

        virtual int sync(){
            EnterCriticalSection(&sec_out);
            int r = flush();
            LeaveCriticalSection(&sec_out);
            if(r == -1){
                return -1;
            }
            return 0;
        }

    };


    template<class charT, class traits> 
    HANDLE console_buf<charT, traits>::handle;

    typedef console_buf<char> console_buffer;
    
    extern std::ostream cs;
}

using namespace std;

#endif
#endif


