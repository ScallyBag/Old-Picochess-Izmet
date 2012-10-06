
#include "arm_neon.h"
#include <iostream>

int popcntll(unsigned long long int b)
{
	return __builtin_popcountll(b);
}



int popcnt_asm(unsigned long long int b)
    {
        //static uint8_t sums[8];
        
        /*
        uint8x8_t vec=vcreate_u8(b);//vld1_u8((void*)&b);
        uint8x8_t counts=vcnt_u8 (vec);
        //vst1_u8(sums,counts);
        //return sums[0]+sums[1]+sums[2]+sums[3]+sums[4]+sums[5]+sums[6]+sums[7];
        
        uint16x4_t tmp1=vpaddl_u8 (counts) ;
        uint32x2_t tmp2=vpaddl_u16 (tmp1);
        vst1_u32(sums32,tmp2);*/
        
        uint32_t sums[2];
        vst1_u32(sums,vpaddl_u16(vpaddl_u8(vcnt_u8(vcreate_u8(b)))));
        return sums[0]+sums[1];
    }
    
    
int main()
{
    unsigned long long int v=28712873827;
    std::cout<<popcntll(v)<<','<<popcnt_asm(v)<<std::endl;
    return 0;
}