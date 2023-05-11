#include "configureInterface.hpp"

#define _gb_fd _uriEvent[GBURI]._fd
#define _gb_cpath _uriEvent[GBURI]._configPath
#define _net_fd _uriEvent[NETURI]._fd
#define _net_cpath _uriEvent[NETURI]._configPath

namespace micagent{
using namespace std;
using namespace neb;

int configInterface::_setNet(struct netInformation* netinfo, const string& netPath){
    dictionary* defaultConfig=dictionary_new(0);
    /*修改网卡编号*/
    char EthId[6]{0};
    sprintf(EthId, "eth%d", netinfo->EthId);
    iniparser_set(defaultConfig, EthId,NULL);  //设置section
    string EthIdStr(EthId);
    /*修改ipv4地址*/
    string SecKey(EthIdStr+":"+"ipaddr");
    iniparser_set(defaultConfig, SecKey.c_str(), (netinfo->IpAddr).c_str());
    /*修改子网掩码*/
    SecKey=EthIdStr+":"+"netmask";
    iniparser_set(defaultConfig, SecKey.c_str(), (netinfo->SubNetMask).c_str());
    /*修改网关*/
    SecKey=EthIdStr+":"+"gateway";
    iniparser_set(defaultConfig, SecKey.c_str(), (netinfo->GateWay).c_str());
    /*修改mac地址*/
    SecKey=EthIdStr+":"+"mac";
    iniparser_set(defaultConfig, SecKey.c_str(), (netinfo->Mac).c_str());



    //新建默认网络配置文件
    FILE* nfd=fopen(netPath.c_str(), "w+"); //w+会清空文件内容
    if(nfd==nullptr){
        return -1;
    }
    else{
        iniparser_dumpsection_ini(defaultConfig, EthId, nfd);
        fclose(nfd);
        iniparser_freedict(defaultConfig);
        return 0;   //默认配置信息修改成功
    }
}

/* 修改或新增 */
int configInterface::_setNet(const string& netinfo, const string& netPath){
    cout<<"IN::_setNet"<<endl;
    if(netinfo.length()<2 || netinfo[0] != '{'){
        return -1;  //字符串无效时，直接返回
        cout<<"invalid"<<endl;
    }
    //cout<<"here"<<endl;
    CJsonObject totaljson(netinfo);
    CJsonObject arrjson;
    totaljson.Get("Parameters",arrjson);
    CJsonObject netjson(arrjson[0]);
    cout<<arrjson[0].ToFormattedString()<<endl;
    //CJsonObject netjson(netinfo);   //读取json内容
    dictionary* defaultConfig=iniparser_load(netPath.c_str());
    struct netInformation net_s{0};

    /*修改网卡编号*/
    char EthId[6]{0};
    netjson.Get("EthId", net_s.EthId);
    sprintf(EthId, "eth%d", net_s.EthId);
    iniparser_set(defaultConfig, EthId,NULL);  //设置section
    string EthIdStr(EthId);
    /*修改ipv4地址*/
    string SecKey(EthIdStr+":"+"ipaddr");
    netjson.Get("IpAddr", net_s.IpAddr);
    iniparser_set(defaultConfig, SecKey.c_str(), (net_s.IpAddr).c_str());
    /*修改子网掩码*/
    SecKey=EthIdStr+":"+"netmask";
    netjson.Get("SubNetMask", net_s.SubNetMask);
    iniparser_set(defaultConfig, SecKey.c_str(), (net_s.SubNetMask).c_str());
    /*修改网关*/
    SecKey=EthIdStr+":"+"gateway";
    netjson.Get("GateWay", net_s.GateWay);
    iniparser_set(defaultConfig, SecKey.c_str(), (net_s.GateWay).c_str());
    /*修改mac地址*/
    SecKey=EthIdStr+":"+"mac";
    netjson.Get("Mac", net_s.Mac);
    iniparser_set(defaultConfig, SecKey.c_str(), (net_s.Mac).c_str());



    //新建默认网络配置文件

    FILE* nfd=fopen(netPath.c_str(), "w+"); //r+只有在已存在的文件上才有用
    if(nfd==nullptr){
        return -1;
    }
    else{
        iniparser_dump_ini(defaultConfig, nfd);
        fclose(nfd);
        iniparser_freedict(defaultConfig);
        return 0;   //默认配置信息修改成功
    }
}

void configInterface::_handler(struct mg_connection* c, int ev, void* ev_data, void* fn_data){
    /* 网络配置修改相关变量 */
    static int net_tag=0;   //网络配置修改标志，tag为0时代表没有被修改过，否则代表已修改，此时将会重启系统
    string net_configPath=(*((map<string, paramType>*)fn_data))[NETURI]._configPath;
    string net_jsonBuf(""); //存放接收到的json格式报文

    /* GB配置修改相关变量 */
    constexpr int bufSize=4096;
    static atomic_bool gb_flag(true);  //GB配置修改标志，为了解决多次修改配置文件的问题，这次到来的数据是否是新的报文内容，如果是，则为true，否则为false
    int gb_wfd=(*((map<string, paramType>*)fn_data))[GBURI]._fd;
    char gb_buf[bufSize]{0};
    CJsonObject gb_bufjson;

    /* 共有的变量 */
    struct mg_http_message *hm = (struct mg_http_message *) ev_data;

    if (ev == MG_EV_HTTP_CHUNK && mg_http_match_uri(hm, NETURI)) {
        ++net_tag;
        net_jsonBuf+=string(hm->body.ptr);
#ifdef __DEBUG_CZX_
cout<<"net: hm->chunk.ptr: "<<hm->chunk.ptr<<endl;
cout<<"net_jsonBuf: "<<net_jsonBuf<<endl;
MG_INFO(("Last chunk received, sending response"));
#endif
        mg_http_delete_chunk(c,hm);
        mg_http_reply(c, 0, "Content-Type: application/json\r\n", "{\"ProtocolCode\":1003,\"Parameters\":0}");
        _setNet(net_jsonBuf, net_configPath);
        net_jsonBuf.clear();
    }
    else if (ev == MG_EV_HTTP_CHUNK && mg_http_match_uri(hm, GBURI)) {
        if(hm->chunk.len>0 && gb_flag){
            ftruncate(gb_wfd,0);
            lseek(gb_wfd,SEEK_SET,0);
            gb_flag.exchange(false);
        }
        int gb_n = write(gb_wfd,(const char*)hm->chunk.ptr,hm->chunk.len);
        if(gb_n<0){
            perror("write error");
            exit(-1);
        }
        mg_http_delete_chunk(c, hm);
        if (hm->chunk.len == 0) {
            MG_INFO(("Last chunk received, sending response"));
            lseek(gb_wfd,SEEK_SET,0);
            read(gb_wfd,gb_buf,bufSize);
            CJsonObject final{string(gb_buf)};
            final.Get("Parameters",gb_bufjson);
            if(gb_bufjson.IsArray()) {
#ifdef __DEBUG_CZX_
cout<<gb_bufjson.ToFormattedString()<<endl;  //测试
cout<<gb_bufjson[0].ToFormattedString()<<endl;
#endif
                ftruncate(gb_wfd, 0);
                lseek(gb_wfd, SEEK_SET, 0);
                write(gb_wfd, gb_bufjson[0].ToFormattedString().c_str(), gb_bufjson[0].ToFormattedString().length()); //重新写入符合格式的文件
            }

            mg_http_reply(c, 0, "", "{\"ProtocolCode\":1001,\"Parameters\":0}");
            gb_flag.exchange(true);
        }
    }


    if(net_tag != 0){   //成功接收一次消息
        write_conn(c);
        //read_conn(c);
        //close_conn(c);
        cout<<"Reboot soon..."<<endl;
        sleep(5);
        system("reboot");
    }

}


void configInterface::run(void){
    _running.exchange(true);

    mg_mgr_init(&_mgr);
    //gbdevice配置
    _gb_fd=open(_gb_cpath.c_str(), O_RDWR | O_CREAT | O_APPEND, 0777);
#ifdef __DEBUG_CZX_
    cout<<"Here: "<<_gb_cpath<<endl;
#endif
    if(_gb_fd<0){
        perror("here open error");
        exit(-1);
    }
    //网络信息配置
    _net_fd=open(_net_cpath.c_str(), O_RDWR | O_APPEND);
    if(_net_fd<0){
        //如果文件不存在，则用默认网卡配置信息替换
        netInformation temp{
            0,
            "192.168.8.154",
            "255.255.255.0",
            "192.168.8.1",
            "00:00:6a:6b:4c:54"
        };

        if(_setNet(&temp,_net_cpath)==0){
            cout<<"Create default NetConfigure successfully!"<<endl;
            close(_net_fd);
        }
        else{
            cout<<"Fail to create default NetConfigure!"<<endl;
        }
    }
    else{
        close(_net_fd);
    }

    mg_log_set(MG_LL_DEBUG);
    mg_http_listen(&_mgr, (const char*)_url, _handler, &_uriEvent);


    while(_running){
        mg_mgr_poll(&_mgr, 50);
    }
    mg_mgr_free(&_mgr);
}

void configInterface::stop(void){
    _running=false;
    close(_gb_fd);
    close(_net_fd);
}

}
