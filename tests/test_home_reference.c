#include <assert.h>
#include "home_reference.h"
int main(void)
{
    assert(!HomeReference_Triggered(0x101));
    assert(HomeReference_Triggered(0x409));
    assert(HomeReference_Triggered(0x402));
    assert(HomeReference_Triggered(0x420));
    assert(!HomeReference_Stopped(false, 0x401));
    assert(!HomeReference_Stopped(true, 0x101));
    assert(HomeReference_Stopped(true, 0x409));
    assert(HomeReference_Stopped(true, 0x401));
    assert(HomeReference_Stopped(true, 0x402));
    return 0;
}
