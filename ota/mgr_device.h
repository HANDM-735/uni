#ifndef MGR_DEVICE_H
#define MGR_DEVICE_H

#include <string>
#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>
#include <boost/thread/mutex.hpp>
#include "xbasic.hpp"
#include "xbasicmgr.hpp"
#include "xdevice.h"
#include "xconfig.hpp"
#include "mgr_network.h"
#include "xusbpackage.h"

#define MC_READ_INTERVAL  13

enum ErrorCode {
    Success = 0,
    FurnaceOffline,
    UnsupportedDataTypeRead,
    UnsupportedDataTypeWrite,
    UnsupportedRequestCommand,
    UnsupportedMessageType,
    ValueOutOfRange,
    SendDataToFurnaceFailed,
    NoResponseFromFurnace,
    FurnaceBusy,
    UnknownError
};

class xmboard;

class mgr_device : public xmgr_basic<mgr_device> // 监控板管理器
{
public:
    mgr_device();
    ~mgr_device();

public:
    boost::shared_ptr<xmboard> find_board(int board_id);        // 寻找单板
    boost::shared_ptr<xmboard> add_board(int board_id, const char *port_name); // 加入单板，如果已经存在，则返回该单板

    void del_board(int board_id);               // 删除单板
    int check_heartbeat();                      // 检测各个单板的心跳
    int check_ota_session();                    // 检测各个单板的升级会话状态
    int get_serial_keepalived(int id);

public:
    virtual void init();                        // 初始化
    virtual void work(unsigned long ticket);    // 工作函数

protected:
    virtual int on_network(NET_MSG msg_type, const char *port_name, xpacket *packet); // 网络接收通知
    int on_network_send(int board_id, const char *port_name, xpacket *packet); // 代理消息发送接口

private:
    int find_serial_keepalived(int id = 0);
    void set_serial_keepalived(int id, int status);

protected:
    xconfig                                 *m_sys_config;    // 系统配置
    mgr_network                             *m_network_mgr;   // 网络管理器

private:
    boost::unordered_map<int, boost::shared_ptr<xmboard>> m_map_board;    // 单板集合
    boost::shared_mutex m_mux_board;                                      // 单板集合读写锁

    boost::unordered_map<int, int> m_map_keepalived;                      // 心跳保活状态集合
    boost::shared_mutex m_mux_keepalived;                                 // 心跳保活状态集合读写锁
};

#endif