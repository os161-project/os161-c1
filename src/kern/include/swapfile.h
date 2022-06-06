#ifndef __SWAPFILE__
#define __SWAPFILE__

#include <types.h>

// S = Swapped bit
//<----------------20------------>|<----6-----><-----6--->|
//_________________________________________________________
//|       Virtual Page Number     |         |S|     PID   |  
//|_______________________________|_______________________|

struct swapTable{
    uint32_t *entry;
    int size;
};

#endif