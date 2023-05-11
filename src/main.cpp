#include "interface.hpp"
#include <iostream>

using namespace micagent;
using namespace std;


int main(int argc, char* argv[]){
    if(argc==1){
        interface::interface_init(0,NULL);
    }
    else if(argc>1){
        interface::interface_init(argc-1,argv+1);
    }
    return 0;
}