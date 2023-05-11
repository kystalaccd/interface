#include "configureInterface.hpp"

#define _gb_fd _uriEvent[GBURI]._fd
#define _gb_cpath _uriEvent[GBURI]._configPath
#define _net_fd _uriEvent[NETURI]._fd
#define _net_cpath _uriEvent[NETURI]._configPath

namespace micagent{
using namespace std;
using namespace neb;

int configInterface::_NetToJson(const struct netInformation& netinfo, string& netJson){
    CJsonObject transJson;  //总的json格式的报文
    CJsonObject netJsonObj; //网络配置信息格式的报文
    transJson.Add("ProtocolCode", 1007);
    transJson.AddEmptySubArray("Parameters");
    netJsonObj.Add("EthId",netinfo.EthId);
    netJsonObj.Add("IpAddr",netinfo.IpAddr);
    netJsonObj.Add("SubNetMask",netinfo.SubNetMask);
    netJsonObj.Add("GateWay",netinfo.GateWay);
    netJsonObj.Add("Mac",netinfo.Mac);

    transJson["Parameters"].Add(netJsonObj); 
    netJson=transJson.ToString();
    //cout<<transJson.ToFormattedString();

    return 0;
}

int configInterface::_getNet(vector<netInformation>& netinfo_v){
    int fd;
    int interfaceNum = 0;
    struct ifreq buf[MAXNICNUM];
    struct ifconf ifc;
    struct ifreq ifrcopy;
    char mac[18] = {0};
    char ip[32] = {0};
    char broadAddr[32] = {0};
    char subnetMask[32] = {0};

    struct netInformation netinfo;

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket");

        close(fd);
        return -1;
    }

    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = (caddr_t)buf;
    
    /*step 01. 获取所有网卡默认网关信息*/
    FILE *fp=popen("ip route show","r");	//网关地址无法用ioctl获取，因此使用ip命令
    char gw_buf[1024]={0};
    map<string,string> gw_ip_map;
    while(fgets(gw_buf, sizeof(gw_buf), fp) != nullptr)
    {
        /*
        以本机为例，获取到的信息为：
        default via 192.168.2.3 dev ens33 onlink 
        default via 192.168.200.2 dev ens38 proto dhcp metric 101 
        */
        if(strstr(gw_buf,"default")!=nullptr)
        {
            char ip_str[32]={0};
            char dev_name[256]={0};
            if(sscanf(gw_buf,"default via %s dev %s",ip_str,dev_name)==2)
            {
                gw_ip_map.emplace(dev_name,ip_str);	//按照“dev_name <---> ip_str”的键值对格式存放默认网关信息
            }
        }
        else {
            break;
        }
        memset(gw_buf,0,1024);
    }
    /*
    最后获取到的信息为
    dev_name   	   ip_str
        eth0		192.168.8.1
    */
    pclose(fp);

    
    if (!ioctl(fd, SIOCGIFCONF, (char *)&ifc))
    {
        interfaceNum = ifc.ifc_len / sizeof(struct ifreq);
        //printf("interface num = %d\n", interfaceNum);
        while (interfaceNum-- > 0)
        {
            //printf("\ndevice name: %s\n", buf[interfaceNum].ifr_name);
            string ethName(buf[interfaceNum].ifr_name);     //网卡名称
            //int isExist = ethName.find(dev_name);  //只有在网卡名匹配eth%d时才有效
            int isExist = gw_ip_map.count(ethName);
            if(isExist == 0){    //没有找到的话，就不执行以下操作
                continue;
            }
            else{
                //存入网卡编号
                string ethid=string(ethName.begin()+3,ethName.end());
                netinfo.EthId=stoi(ethid);   //转换为网卡编号
            }

            //get the gateway of this interface
            netinfo.GateWay=gw_ip_map[ethName];


            //ignore the interface that not up or not runing  
            ifrcopy = buf[interfaceNum];
            if (ioctl(fd, SIOCGIFFLAGS, &ifrcopy))
            {
                printf("ioctl: %s [%s:%d]\n", strerror(errno), __FILE__, __LINE__);

                close(fd);
                return -1;
            }

            //get the mac of this interface  
           if (!ioctl(fd, SIOCGIFHWADDR, (char *)(&buf[interfaceNum])))
            {
                memset(mac, 0, sizeof(mac));
                snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
                        (unsigned char)buf[interfaceNum].ifr_hwaddr.sa_data[0],
                        (unsigned char)buf[interfaceNum].ifr_hwaddr.sa_data[1],
                        (unsigned char)buf[interfaceNum].ifr_hwaddr.sa_data[2],

                        (unsigned char)buf[interfaceNum].ifr_hwaddr.sa_data[3],
                        (unsigned char)buf[interfaceNum].ifr_hwaddr.sa_data[4],
                        (unsigned char)buf[interfaceNum].ifr_hwaddr.sa_data[5]);
                //printf("device mac: %s\n", mac);
                netinfo.Mac=mac;
            }
            else
            {
                printf("ioctl: %s [%s:%d]\n", strerror(errno), __FILE__, __LINE__);
                close(fd);
                return -1;
            }

            //get the IP of this interface  

            if (!ioctl(fd, SIOCGIFADDR, (char *)&buf[interfaceNum]))
            {
                snprintf(ip, sizeof(ip), "%s",
                        (char *)inet_ntoa(((struct sockaddr_in *)&(buf[interfaceNum].ifr_addr))->sin_addr));
                //printf("device ip: %s\n", ip);
                netinfo.IpAddr=ip;
            }
            else
            {
                printf("ioctl: %s [%s:%d]\n", strerror(errno), __FILE__, __LINE__);
                close(fd);
                return -1;
            }

            //get the subnet mask of this interface  
            if (!ioctl(fd, SIOCGIFNETMASK, &buf[interfaceNum]))
            {
                snprintf(subnetMask, sizeof(subnetMask), "%s",
                        (char *)inet_ntoa(((struct sockaddr_in *)&(buf[interfaceNum].ifr_netmask))->sin_addr));
                //printf("device subnetMask: %s\n", subnetMask);
                netinfo.SubNetMask=subnetMask;
            }
            else
            {
                printf("ioctl: %s [%s:%d]\n", strerror(errno), __FILE__, __LINE__);
                close(fd);
                return -1;

            }

            netinfo_v.push_back(netinfo);
        }
    }
    else
    {
        printf("ioctl: %s [%s:%d]\n", strerror(errno), __FILE__, __LINE__);
        close(fd);
        return -1;
    }

    close(fd);

    return 0;
}

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


int configInterface::_GBToJson(int gbfd, string& gbJson){
    char *buf{new char[4096]};
    lseek(gbfd, SEEK_SET, 0);   //将文件读指针移动到文件开头
    int n = read(gbfd, buf, 4096);
    if(n<0){
        cout<<__FILE__<<"; "<<__FUNCTION__<<"; "<<__LINE__<<"; "<<"ERROR: ";
        perror(NULL);
        return -1;
    }
    CJsonObject jsonBuf;
    jsonBuf.Add("ProtocolCode", 1003);
    jsonBuf.AddEmptySubArray("Parameters");
    jsonBuf["Parameters"].Add(CJsonObject(string(buf)));
#ifdef __DEBUG_CZX_
cout<<__FILE__<<"; "<<__FUNCTION__<<"; "<<__LINE__<<"; "<<"GBConfig Reply: "<<"\n"<<jsonBuf.ToFormattedString()<<endl;
#endif
    gbJson=jsonBuf.ToString();
    delete[] buf;
    return 0;
}


void configInterface::_handler(struct mg_connection* c, int ev, void* ev_data, void* fn_data){
    /* 网络配置修改相关变量 */
    static int net_tag=0;   //网络配置修改标志，tag为0时代表没有被修改过，否则代表已修改，此时将会重启系统
    string net_configPath=(*((map<string, paramType>*)fn_data))[NETURI]._configPath;
    string net_jsonBuf(""); //存放接收到的json格式报文
    vector<netInformation> nic_vec; //存放设备所有网卡的详细信息
    

    /* GB配置修改相关变量 */
    constexpr int bufSize=4096;
    static atomic_bool gb_flag(true);  //GB配置修改标志，为了解决多次修改配置文件的问题，这次到来的数据是否是新的报文内容，如果是，则为true，否则为false
    int gb_wfd=(*((map<string, paramType>*)fn_data))[GBURI]._fd;
    char gb_buf[bufSize]{0};
    CJsonObject gb_bufjson;
    bool gb_modify_tag=false;   //标志是否是修改gbdevice配置信息的报文，如果为true，代表是修改报文，如果为false，则不是

    /* 共有的变量 */
    struct mg_http_message *hm = (struct mg_http_message *) ev_data;
    int ProtocolCode=0;

    /* FUNCTION.01  请求网络配置信息 以及 更改网络配置信息*/
    if (ev == MG_EV_HTTP_CHUNK && mg_http_match_uri(hm, NETURI)) {
        net_jsonBuf+=string(hm->body.ptr);
#ifdef __DEBUG_CZX_
cout<<"net: hm->chunk.ptr: "<<hm->chunk.ptr<<endl;
cout<<"net_jsonBuf: "<<net_jsonBuf<<endl;
MG_INFO(("Last chunk received, sending response"));
#endif
        mg_http_delete_chunk(c,hm);
        CJsonObject(net_jsonBuf).Get("ProtocolCode",ProtocolCode); 
        if(ProtocolCode==1004){     //网络信息配置
            mg_http_reply(c, 200, HTTP_RESPONSE_JSON, "{\"ProtocolCode\":1005,\"Parameters\":0}");
            ++net_tag;
            _setNet(net_jsonBuf, net_configPath);
            net_jsonBuf.clear();
        }
        else if(ProtocolCode==1006){        //网络信息获取
            string transJson;
            _getNet(nic_vec);
            _NetToJson(nic_vec[0], transJson);  //暂时只获取第一个网卡的信息
            //char replyBuf[4096];
            //sprintf(replyBuf, "Content-Type: application/json\r\n%s", HTTP_HEADERS);
            mg_http_reply(c, 200, HTTP_RESPONSE_JSON, transJson.c_str());
            nic_vec.clear();
        }
    }
    /* FUNCTION.02  请求GBDevice的配置文件详细信息 以及 更改GBDevice的配置文件详细信息 */
    else if (ev == MG_EV_HTTP_CHUNK && mg_http_match_uri(hm, GBURI)) {
        CJsonObject(string(hm->body.ptr)).Get("ProtocolCode",ProtocolCode);
        if(ProtocolCode==1002){     //GB设备信息获取
            string gb_get_reply;
            _GBToJson(gb_wfd, gb_get_reply);
            mg_http_reply(c, 200, HTTP_RESPONSE_JSON, gb_get_reply.c_str());
        }
        else if(ProtocolCode==1000){    //GB设备信息配置
            gb_modify_tag=true;
        }
        if(gb_modify_tag==true){
            if(hm->chunk.len>0 && gb_flag){
                ftruncate(gb_wfd,0);
                lseek(gb_wfd,SEEK_SET,0);
                gb_flag.exchange(false);
            }

#ifdef __DEBUG_CZX_
cout<<__FILE__<<";  "<<__LINE__<<";  GBCHUNK: "<<hm->chunk.ptr<<endl;
#endif

            int gb_n = write(gb_wfd,(const char*)hm->chunk.ptr,hm->chunk.len);
            if(gb_n<0){
                perror("write error");
                exit(-1);
            }
            mg_http_delete_chunk(c, hm);
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

            mg_http_reply(c, 200, HTTP_RESPONSE_JSON, "{\"ProtocolCode\":1001,\"Parameters\":0}");
            gb_flag.exchange(true);

            //此时修改成功，将修改报文标志位置为fase
            gb_modify_tag=false;
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
