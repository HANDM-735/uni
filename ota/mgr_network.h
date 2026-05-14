#ifndef MGR_NETWORK_H
#define MGR_NETWORK_H

#include <string>
#include <list>
#include <boost/array.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>
#include <boost/thread/mutex.hpp>
#include "xconfig.hpp"
#include "xbasic.hpp"
#include "xbasicmgr.hpp"
#include "xbasicasio.hpp"
#include "xpackage.hpp"
#include "xusbpackage.h"

#define PORT_ADAPTER       "<adapter>"      // 适配器
#define PORT_TH_MONITOR    "<thmonitor>"    // TH监控
#define PORT_MF_MONITOR    "<mfmonitor>"     // MF监控
#define PORT_PLATFROM      "<platform>"      // 平台
#define PORT_HOT_SIMULATION "<hotsimulation>" // 热仿真

#define PLATFORM_HEADER      0xFEAS         // 平台协议头
#define PLATFORM_HEADER_LEN  6              // 平台协议头长度
#define PLATFORM_MAXMSG_LEN  32768          // 平台最大消息长度

#define SLOT_TYPE_HOT_SIMULATION 6          // 热仿真类型

class json_packer : public xpacker  // JSON打包器
{
public:
    virtual boost::shared_ptr<const std::string> pack_data(const char *pdata, size_t datalen, PACKWAY pack_way)
    {
        boost::shared_ptr<const std::string> ppack;
        if (pdata == NULL || datalen == 0) return ppack;
        int totalen = (int)datalen;
        std::string *raw_str = new std::string();
        if (pack_way == PACK_BASIC)
        {
            char header[PLATFORM_HEADER_LEN] = {0};
            xbasic::write_bigendian(header, PLATFORM_HEADER, 2);
            xbasic::write_bigendian(header + 2, totalen, 4);
            raw_str->reserve(totalen + PLATFORM_HEADER_LEN);
            raw_str->append(header, PLATFORM_HEADER_LEN);
            raw_str->append(pdata, totalen);
        }
        else
        {
            raw_str->reserve(totalen);
            raw_str->append(pdata, totalen);
        }
        ppack.reset(raw_str);
        return ppack;
    }
};

class json_unpacker : public xunpacker  // JSON解包器
{
public:
    json_unpacker() { m_signed_len = (size_t)-1; m_data_len = 0; }
public:
    virtual void reset_data() { m_signed_len = (size_t)-1; m_data_len = 0; }
    virtual bool unpack_data(size_t bytes_data, boost::container::list<boost::shared_ptr<const std::string>> &list_pack)
    {
        m_data_len += bytes_data;
        bool unpack_ok = true;
        char *buff_head = m_raw_buff.begin();
        char *buff_data = buff_head;
        while (unpack_ok)
        {
            if (m_signed_len != (size_t)-1)
            {
                if (m_data_len >= m_signed_len)
                {
                    size_t pack_len = m_signed_len + PLATFORM_HEADER_LEN;
                    list_pack.push_back(boost::shared_ptr<std::string>(new std::string(buff_data, pack_len)));
                    std::advance(buff_data, pack_len);
                    m_data_len -= pack_len;
                    m_signed_len = (size_t)-1;
                }
                else
                    break;
            }
            else
            {
                if (m_data_len > PLATFORM_HEADER_LEN)
                {
                    if (xbasic::read_bigendian(buff_data, 2) == PLATFORM_HEADER)
                        m_signed_len = xbasic::read_bigendian(buff_data + 2, 4);
                    else
                        unpack_ok = false;
                    if (m_signed_len != (size_t)-1 && m_signed_len > (PLATFORM_MAXMSG_LEN - 1024))
                        unpack_ok = false;
                    else
                        break;
                }
                else
                    break;
            }
        }
        if (!unpack_ok)
        {
            xbasic::debug_bindata("json unpacker error", buff_data, m_data_len < 512 ? m_data_len : 512);
            reset_data();
            return unpack_ok;
        }
        if (m_data_len > 0 && buff_data != buff_head)
        {
            for (unsigned int i = 0; i < m_data_len; i++)
                buff_head[i] = buff_data[i];
        }
        return unpack_ok;
    }
    virtual boost::asio::mutable_buffers_1 prepare_buff(size_t &min_recv_len)
    {
        if (m_data_len > PLATFORM_MAXMSG_LEN) reset_data();
        if (m_data_len >= PLATFORM_HEADER_LEN)
        {
            char *next_buff = m_raw_buff.begin();
            if (xbasic::read_bigendian(next_buff, 2) == PLATFORM_HEADER)
            {
                min_recv_len = m_signed_len == (size_t)-1 ? PLATFORM_HEADER_LEN : m_signed_len - m_data_len;
            }
            else
            {
                min_recv_len = PLATFORM_HEADER_LEN;
            }
        }
        else
        {
            min_recv_len = PLATFORM_HEADER_LEN - m_data_len;
            if (min_recv_len > (size_t)-1 || min_recv_len > (PLATFORM_MAXMSG_LEN - 1024))
                min_recv_len = PLATFORM_HEADER_LEN;
        }
        return boost::asio::buffer(boost::asio::buffer(m_raw_buff) + m_data_len);
    }
private:
    boost::array<char, PLATFORM_MAXMSG_LEN> m_raw_buff;  // socket接收缓冲区
    size_t m_signed_len;
    size_t m_data_len;
};

class mgr_network : public xmgr_basic<mgr_network>
{
public:
    mgr_network();
    ~mgr_network();
public:
    void set_thmonitor_board_addr(std::string th_addr);
    void set_mfmonitor_board_addr(std::string mf_addr);
    int add_listener(xlistener *new_listener);
    void del_listener(xlistener *del_listener);
    int tel_listener(NET_MSG msg_type, const char *port_name, xpacket *packet);
    int send_to_thmonitor_board(xusbpackage *pack);
    int send_to_mfmonitor_board(xusbpackage *pack);
    int send_to_platform(xpackage *pack);
    int send_to_hotsimulation_board(boost::shared_ptr<xtcp_client> &hot_client, xusbpackage *pack);
    int send_to_hotsimulation_board(int board_id, xusbpackage *pack);
    int set_network_connect(const std::string &ip);
    void wait_network_complete();
    void stop_client();
public:
    virtual void init();          // 初始化
    virtual void work(unsigned long ticket); // 工作函数
protected:
    void on_tcp_msg(xtcp_client *cli, xtcp_client::CBKTCPCMSG tcp_msg, char *pdata, int data_len); // TCP消息回调函数
private:
    void send_heartbeat_tothmonitor();
    void send_heartbeat_tomfmonitor();
    void send_heartbeat_toplatform();
    void send_heartbeat_tohotsimulation(xtcp_client *cli);
    void send_heartbeat_tohotsimulation(boost::shared_ptr<xtcp_client> &hot_client);
    void load_hotsimulation_board_ip();
    boost::shared_ptr<xtcp_client> find_simulation_client_by_boardid(int board_id);
    boost::shared_ptr<xtcp_client> add_map_boardid_client(int board_id, xtcp_client *cli);
    boost::shared_ptr<xtcp_client> find_simulation_client_in_vector(xtcp_client *cli);
private:
    std::list<xlistener *> m_lst_listener;
    boost::shared_mutex m_mux_listener;
protected:
    xconfig *m_sys_config;
    int hotsimulation_timeout;
    int monitor_interval;
    boost::shared_ptr<xtcp_client> m_th_monitor_cli;
    boost::shared_ptr<xtcp_client> m_mf_monitor_cli;
    boost::shared_ptr<xtcp_client> m_platform_cli;
    std::vector<std::string> m_hot_simulation_ips;
    std::vector<boost::shared_ptr<xtcp_client>> m_hot_simulation_clis;
    boost::shared_mutex m_mux_hot_simulation_clis;
    boost::unordered_map<int, boost::shared_ptr<xtcp_client>> m_map_hot_simulation_clis;
    boost::shared_mutex m_mux_map_hot_simulation_clis;
};

#endif