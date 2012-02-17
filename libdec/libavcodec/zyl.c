
#include "dsputil.h"
#include <stdint.h>

static const uint32_t svq3_dequant_coeff[32] = {
     3881,  4351,  4890,  5481,  6154,  6914,  7761,  8718,
     9781, 10987, 12339, 13828, 15523, 17435, 19561, 21873,
    24552, 27656, 30847, 34870, 38807, 43747, 49103, 54683,
    61694, 68745, 77615, 89113,100253,109366,126635,141533
};


void ff_svq3_luma_dc_dequant_idct_c(DCTELEM *block, int qp)
{
    const int qmul = svq3_dequant_coeff[qp];
#define stride 16
    int i;
    int temp[16];
    static const int x_offset[4] = {0, 1*stride, 4* stride,  5*stride};
    static const int y_offset[4] = {0, 2*stride, 8* stride, 10*stride};

    for (i = 0; i < 4; i++){
        const int offset = y_offset[i];
        const int z0 = 13*(block[offset+stride*0] +    block[offset+stride*4]);
        const int z1 = 13*(block[offset+stride*0] -    block[offset+stride*4]);
        const int z2 =  7* block[offset+stride*1] - 17*block[offset+stride*5];
        const int z3 = 17* block[offset+stride*1] +  7*block[offset+stride*5];

        temp[4*i+0] = z0+z3;
        temp[4*i+1] = z1+z2;
        temp[4*i+2] = z1-z2;
        temp[4*i+3] = z0-z3;
    }

    for (i = 0; i < 4; i++){
        const int offset = x_offset[i];
        const int z0 = 13*(temp[4*0+i] +    temp[4*2+i]);
        const int z1 = 13*(temp[4*0+i] -    temp[4*2+i]);
        const int z2 =  7* temp[4*1+i] - 17*temp[4*3+i];
        const int z3 = 17* temp[4*1+i] +  7*temp[4*3+i];

        block[stride*0 +offset] = ((z0 + z3)*qmul + 0x80000) >> 20;
        block[stride*2 +offset] = ((z1 + z2)*qmul + 0x80000) >> 20;
        block[stride*8 +offset] = ((z1 - z2)*qmul + 0x80000) >> 20;
        block[stride*10+offset] = ((z0 - z3)*qmul + 0x80000) >> 20;
    }
#undef stride
}

void ff_svq3_add_idct_c(uint8_t *dst, DCTELEM *block, int stride, int qp, int dc)
{
    const int qmul = svq3_dequant_coeff[qp];
    int i;
    uint8_t *cm = ff_cropTbl + MAX_NEG_CROP;

    if (dc) {
        dc = 13*13*((dc == 1) ? 1538*block[0] : ((qmul*(block[0] >> 3)) / 2));
        block[0] = 0;
    }

    for (i = 0; i < 4; i++) {
        const int z0 = 13*(block[0 + 4*i] +    block[2 + 4*i]);
        const int z1 = 13*(block[0 + 4*i] -    block[2 + 4*i]);
        const int z2 =  7* block[1 + 4*i] - 17*block[3 + 4*i];
        const int z3 = 17* block[1 + 4*i] +  7*block[3 + 4*i];

        block[0 + 4*i] = z0 + z3;
        block[1 + 4*i] = z1 + z2;
        block[2 + 4*i] = z1 - z2;
        block[3 + 4*i] = z0 - z3;
    }

    for (i = 0; i < 4; i++) {
        const int z0 = 13*(block[i + 4*0] +    block[i + 4*2]);
        const int z1 = 13*(block[i + 4*0] -    block[i + 4*2]);
        const int z2 =  7* block[i + 4*1] - 17*block[i + 4*3];
        const int z3 = 17* block[i + 4*1] +  7*block[i + 4*3];
        const int rr = (dc + 0x80000);

        dst[i + stride*0] = cm[ dst[i + stride*0] + (((z0 + z3)*qmul + rr) >> 20) ];
        dst[i + stride*1] = cm[ dst[i + stride*1] + (((z1 + z2)*qmul + rr) >> 20) ];
        dst[i + stride*2] = cm[ dst[i + stride*2] + (((z1 - z2)*qmul + rr) >> 20) ];
        dst[i + stride*3] = cm[ dst[i + stride*3] + (((z0 - z3)*qmul + rr) >> 20) ];
    }
}