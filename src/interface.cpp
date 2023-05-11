#include "interface.hpp"

namespace micagent{
using namespace std;

void interface::interface_init(int paramc, char* params[]){
    unique_lock<mutex> locker(_config_interface_conn_mtx);
    while(true){
cout<<__FILE__<<"; "<<__FUNCTION__<<"; "<<__LINE__<<"; IN !!"<<endl;
        if(paramc==0){
            configInterface* cf=configure_factory::create("0.0.0.0", _PORT, NULL);
            thread interface_run(&configInterface::run, cf);     //开始运行 
cout<<__FILE__<<" "<<__FUNCTION__<<" "<<__LINE__<<" "<<"Block here !"<<endl;
            _config_interface_conn.wait(locker);
            if(interface_run.joinable()){
                interface_run.join();
            }
            delete cf;
            sleep(10);
        }
        else if(paramc==1){    //修改gb配置文件的路径
            configInterface* cf=configure_factory::create("0.0.0.0", _PORT,"%s",params[0]);
            thread interface_run(&configInterface::run, cf);     //开始运行 
cout<<__FILE__<<" "<<__FUNCTION__<<" "<<__LINE__<<" "<<"Block here !"<<endl;
            _config_interface_conn.wait(locker);
            if(interface_run.joinable()){
                interface_run.join();
            }
            delete cf;
            sleep(10);
        }
        else if(paramc==2){
            configInterface* cf=configure_factory::create("0.0.0.0", _PORT, "%s %s", params[0], params[1]);
            thread interface_run(&configInterface::run, cf);     //开始运行 
cout<<__FILE__<<" "<<__FUNCTION__<<" "<<__LINE__<<" "<<"Block here !"<<endl;
            _config_interface_conn.wait(locker);
            if(interface_run.joinable()){
                interface_run.join();
            }
            delete cf;
            sleep(10);
        }
        else{
            cout<<"Invalid file path!"<<endl;
            return;
        }

    } 
}

}