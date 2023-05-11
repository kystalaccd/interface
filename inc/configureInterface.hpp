#ifndef __CONFIGUREINTERFACE_HPP__
#define __CONFIGUREINTERFACE_HPP__

#define __DEBUG_CZX_

#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <linux/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cmath>

#include <atomic>
#include <iostream>
#include <string>
#include <map>
#include <mutex>
#include <vector>

#include "mongoose.h"
#include "CJsonObject.hpp"
#include "iniparser.h"
#include "dictionary.h"

/*******************************************可用的URI*******************************************/
#define GBURI "/changeConfigure"
#define NETURI "/netConfigure"
/*********************************************end*********************************************/

/*******************************************HTTP响应头*******************************************/
#define HTTP_HEADERS                                                  \
                                            "Access-Control-Allow-Origin: *\r\n"                              \
                                            "Access-Control-Max-Age: 3600\r\n"                                \
                                            "Access-Control-Allow-Headers: X-Requested-With,content-type\r\n" \
                                            "Access-Control-Allow-Methods: GET,POST,OPTIONS,DELETE\r\n"       \
                                            "Access-Control-Allow-Credentials:true\r\n"

#define HTTP_RESPONSE_JSON                  \
                                            "Content-Type: application/json\r\n"                            \
                                            "Access-Control-Allow-Origin: *\r\n"                             \
                                            "Access-Control-Max-Age: 3600\r\n"                                \
                                            "Access-Control-Allow-Headers: X-Requested-With,content-type\r\n" \
                                            "Access-Control-Allow-Methods: GET,POST,OPTIONS,DELETE\r\n"       \
                                            "Access-Control-Allow-Credentials:true\r\n"                         
/*********************************************end*********************************************/




#define _PORT 6006 //使用的端口
#define _DEFAULT_GBCONFIG_PATH "/server/micagent/server/gbdevice/config.json"
#define _DEFAULT_NETCONFIG_PATH "/server/micagent/server/config/networking.ini"
#define URLLEN 24
#define MAXNICNUM 4 //设备网卡最大数量

/* 修改自mongoose网络库，将以下接口开放出来 */
void read_conn(struct mg_connection *c);
void write_conn(struct mg_connection *c);
void close_conn(struct mg_connection* c);

namespace micagent{
using namespace std;
using namespace neb;

enum  EVENT{
    GBCONFIG=0, /* 改变gbdevice的配置信息 */
    NETCONFIG,   /* 改变网络配置信息 */
    INVALID
};

static const char* URIArray[INVALID]={
    GBURI,
    NETURI
};


class configInterface{

    /*网卡信息*/
    struct netInformation{
        int EthId;
        string IpAddr;
        string SubNetMask;
        string GateWay;
        string Mac;
    };

    /*handler传入参数*/
    struct paramType{
        int _fd; //打开的文件描述符
        string _configPath;  //需要保存的文件路径
    };
public:
    static configInterface& Instance(const string& ip, short port, const char*fmt/* 格式: %s %s (用空格分隔)*/, .../* 请按照enum EVENT的顺序输入文件路径 */){
        map<string, paramType> uriEvent{    //默认map
            {GBURI/* 修改gbdevice配置文件对应的uri */, {-1/* 待写入的文件 */, _DEFAULT_GBCONFIG_PATH/* 配置文件的默认路径 */}},
            {NETURI/* 修改网络配置对应的uri */, {-1, _DEFAULT_NETCONFIG_PATH}}
        };

        if(fmt==NULL){
            static configInterface instance(uriEvent, ip, port);
            return instance;
        }
        const int bufSize=1024;
        char str[bufSize]{0};
        unsigned int len, i, index;
        char* strTemp;
        unsigned int fast, slow;
        va_list args;

        va_start(args, fmt);

        len=strlen(fmt);
        for(i=0, index=0; i<len; ++i){
            if(fmt[i] != '%'){
                str[index++]=fmt[i];
            }
            else{
               switch(fmt[i+1]){
                    case 's':
                    case 'S':
                        strTemp=va_arg(args, char*);
                        strcpy(str+index, strTemp);
                        index += strlen(strTemp);
                        ++i;
                        break;
                    default:    //不合法的输入格式，采用默认的参数
                        static configInterface instance(uriEvent, ip, port);
                        return instance;
               }
            }
        }
        str[index]='\0';
        va_end(args);

        //运行到此处，说明输入都合法，此时清空uriEvent中的映射关系
        //uriEvent.clear();

        //取出字符串中的路径
        int j;
        for(j=0, slow=0,fast=0;fast<=strlen(str);++fast){
            if(str[fast]==' ' || str[fast]==','){
#ifdef __DEBUG_CZX_
cout<<__FILE__<<__FUNCTION__<<__LINE__<<string(str+slow, str+fast)<<endl;
#endif
                struct paramType temp{-1, string(str+slow, str+fast)};
#ifdef __DEBUG_CZX_
cout<<__FILE__<<__FUNCTION__<<__LINE__<<temp._configPath<<endl;
#endif
                slow=fast+1;
                uriEvent[URIArray[j++]]=temp;
            }
            else if(fast==strlen(str)){      //最后一个字符串特殊处理
#ifdef __DEBUG_CZX_
cout<<__FILE__<<__FUNCTION__<<__LINE__<<string(str+slow, str+fast)<<endl;
#endif
                struct paramType temp{-1, string(str+slow, str+fast)};
                uriEvent[URIArray[j++]]=temp;
            }
        }

        static configInterface instance(uriEvent, ip, port);
        return instance;

    }

#ifdef __DEBUG_CZX_
    void _print_event(){
       cout<<_url<<endl;
       for(int i=0;i<NETCONFIG+1;++i){
            cout<< URIArray[i]<<": "<< _uriEvent[URIArray[i]]._configPath <<endl;
       }
    }
#endif

    /**
     * @brief run 开始运行
     * */
    void run(void);

    /**
     * @brief stop 停止运行
    */
    void stop(void);

private:
    configInterface(const map<string, paramType>& uriEvent, const string& ip, short port)
        :_running(false),
        _ipAddress(ip),
        _port(port),
        _uriEvent{uriEvent}
    {
        _running=true;
        char str_port[6]={0};
        sprintf(str_port,"%d",_port);
        string sport(str_port);
        string temp=_ipAddress + ":" + sport;
        strcpy(_url, temp.c_str());
    }

    ~configInterface(){stop();}

    static void _handler(struct mg_connection *c, int ev, void* ev_data, void* fn_data);

    static int _setNet(struct netInformation* netinfo, const string& netPath);
    static int _setNet(const string& netinfo, const string& netPath);
    static int _getNet(vector<netInformation>& netinfo);
    static int _NetToJson(const struct netInformation& netinfo, string& netJson);

    char _url[URLLEN]{0};
    string _ipAddress;   //监听ip地址
    short _port;
    map<string, paramType> _uriEvent;  //不同uri对应的事件
    struct mg_mgr _mgr;
    atomic_bool _running;
};



}











#endif
