#include <assert.h>
#include "svc.h"

int main(int argc, char **argv) {
    void *helper = svc_init();
    
    // TODO: write your own tests here
    // Hint: you can use assert(EXPRESSION) if you want
    // e.g.  assert((2 + 3) == 5);
    
    cleanup(helper);
    
    return 0;
}

