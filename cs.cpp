
#include "stdafx.h"

#ifdef _DEBUG

#include "cs.h"

namespace std{
    console_buffer* buffer = console_buffer::instance();
    ostream cs(buffer);
}

#endif

