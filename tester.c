#include "svc.h"

int main(int argc, char **argv) {
    void *helper = svc_init();
    
    // TODO: write your own tests here
    
    cleanup(helper);
    
    return 0;
}

