#include "configureInterface.hpp"
#include <iostream>

using namespace micagent;
using namespace std;


int main(int argc, char* argv[]){
    if(argc==1){
        configInterface::Instance("0.0.0.0", _PORT, NULL).run();
    }
    else if(argc==2){    //修改gb配置文件的路径
        configInterface::Instance("0.0.0.0", _PORT,"%s",argv[1]).run();
    }
    else if(argc==3){
        configInterface::Instance("0.0.0.0", _PORT, "%s %s", argv[1], argv[2]).run();
    }
    else{
        cout<<"Invalid file path!"<<endl;
        return -1;
    }

    return 0;
}
