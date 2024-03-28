#include "mutex.h"
#include "types.h"
#include "defs.h"
#include "x86.h"

//globals
void* chan = (void*)0x1000;

/**
 * Function to initialize lock
*/
int sys_minit(void)
{
    //get mutex
    mutex* m;
    if (argint(0, (int*)&m) < 0)
    {
        //error return
        return -1;
    }

    m->locked = 0;
    m->_chan = chan;
    chan++;

    //return success
    return 0;
}


