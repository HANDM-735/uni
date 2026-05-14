#include "mgr_network.h"
#include <boost/format.hpp>
#include <boost/thread.hpp>
#include "xcrypto.hpp"
#include "xpackage.hpp"
#include "xconfig.hpp"
#include "mgr_log.h"

mgr_network::mgr_network()
{
    /*
    m_sys_config = xconfig::get_instance();
    load_hotsimulation_board_ip();
    boost::unique_lock<boost::shared_mutex> lock(m_mux_hot_simulation_clis);
    for(const auto &ip : m_hot_simulation_ips)
    {
        boost::shared_ptr<xtcp_client> hot_client(new xtcp_client());
        hot_client->set_callback(boost::bind(&mgr_network::on_tcp_cli_msg, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
        hot_client->set_packer_unpacker(new usb_bin_packer, new usb_bin_unpacker);
        hot_client->set_server_addr(ip.c_str(), m_sys_config->get_data("hotsimulation_port", 13083));
        m_hot_simulation_clis.push_back(std::move(hot_client));
        LOG_MSG(MSG_LOG, "mgr_network::mgr_network() create hot_client, ip+port:%s", m_hot_simulation_clis.back()->get_server_addr().c_str());
    }
    */
}

mgr_network::~mgr_network()
{
    /*
    boost::unique_lock<boost::shared_mutex> lock(m_mux_hot_simulation_clis);
    for(auto &cli : m_hot_simulation_clis)
    {
        cli->close_socket();
    }
    */
}

void mgr_network::init()
{
    m_sys_config = xconfig::get_instance();
    monitor_interval = m_sys_config->get_data("monitor_interval", 15);
    monitor_timeout = m_sys_config->get_data("monitortimeout", 30);
    hotsimulation_timeout = m_sys_config->get_data("hotsimulation_timeout", 30);
}

void mgr_network::work(unsigned long ticket)
{
    LOG_MSG(MSG_LOG, "Enter into mgr_network::work()");
    // work
    boost::shared_lock<boost::shared_mutex> lock(m_mux_hot_simulation_clis);
    for(auto &cli : m_hot_simulation_clis)
    {
        if(!cli->is_connected())
        {
            cli->start_connect();
        }
        else
        {
            long curr_time = time(NULL);
            long last_recv_time = cli->get_last_recv_tm();
            long tm_recv_interval = (long)abs(curr_time - last_recv_time);
            LOG_MSG(WRN_LOG, "mgr_network::work() hotsimulation board curr_time:%ld last_recv_time:%ld tm_recv_interval:%ld", curr_time, last_recv_time, tm_recv_interval);
            if(tm_recv_interval > (long)monitor_interval * monitor_timeout)
            {
                LOG_MSG(WRN_LOG, "mgr_network::work() hotsimulation board heartbeat timeout curr_time:%ld last_recv_time:%ld tm_recv_interval:%ld", curr_time, last_recv_time, tm_recv_interval);
                cli->disconnect();
                // cli->reset_last_recv_tm(curr_time);
            }
            else if((ticket % hotsimulation_timeout) == 0)
            {
                send_heartbeat_tohotsimulation(cli);
            }
        }
    }
    LOG_MSG(MSG_LOG, "Exited mgr_network::work()");
}

void mgr_network::on_tcp_cli_msg(xtcp_client *cli, xtcp_client::CBKTCPCMSG tcp_msg, char *pdata, int data_len)
{
    if(tcp_msg == xtcp_client::TCP_DATA)
    {
        xusbpackage pack((unsigned char *)pdata, data_len);
        boost::shared_ptr<xtcp_client> hot_cli;
        hot_cli = add_map_boardid_client(pack.m_srcid, cli);
        tel_listener(NET_DATA, PORT_HOT_SIMULATION, &pack);
    }
    else if(tcp_msg == xtcp_client::TCP_CONNECTED)
    {
        if(xbasic::read_littleendian(pdata, sizeof(int)) != 0) return;
        LOG_MSG(WRN_LOG, "mgr_network::on_tcp_cli_msg() hotsimulation Board connected success!!!");
        send_heartbeat_tohotsimulation(cli);
        tel_listener(NET_CONNECTED, PORT_HOT_SIMULATION, NULL);
    }
    else if(tcp_msg == xtcp_client::TCP_DISCONNECTED)
    {
        boost::shared_lock<boost::shared_mutex> lock(m_mux_hot_simulation_clis);
        for(auto &hot_cli : m_hot_simulation_clis)
        {
            if(cli == hot_cli.get())
            {
                hot_cli->disconnect();
                LOG_MSG(WRN_LOG, "mgr_network::on_tcp_cli_msg() hotsimulation Board disconnected!!!");
                tel_listener(NET_DISCONNECTED, PORT_HOT_SIMULATION, NULL);
                break;
            }
        }
    }
}

void mgr_network::send_heartbeat_tohotsimulation(xtcp_client *cli)
{
    LOG_MSG(MSG_LOG, "Enter into mgr_network::send_heartbeat_tohotsimulation(xtcp_client *cli)");
    xusbpackage pack(xusbpackage::MC_HEARTBEAT, PROTO_VERSION, 0, xconfig::self_id(), 0);
    boost::shared_ptr<xtcp_client> hot_client = find_simulation_client_in_vector(cli);
    send_to_hotsimulation_board(hot_client, &pack);
    LOG_MSG(MSG_LOG, "Exited mgr_network::send_heartbeat_tohotsimulation(xtcp_client *cli)");
}

void mgr_network::send_heartbeat_tohotsimulation(boost::shared_ptr<xtcp_client> &hot_client)
{
    LOG_MSG(MSG_LOG, "Enter into mgr_network::send_heartbeat_tohotsimulation(boost::shared_ptr<xtcp_client> &hot_client)");
    xusbpackage pack(xusbpackage::MC_HEARTBEAT, PROTO_VERSION, 0, xconfig::self_id(), 0);
    send_to_hotsimulation_board(hot_client, &pack);
    LOG_MSG(MSG_LOG, "Exited mgr_network::send_heartbeat_tohotsimulation(boost::shared_ptr<xtcp_client> &hot_client)");
}

void mgr_network::load_hotsimulation_board_ip()
{
    LOG_MSG(MSG_LOG, "Enter into mgr_network::load_hotsimulation_board_ip()");
    unsigned char arr[4 * 4] = {'\0'};
    int ipcnt = 0;
    int ret = -1;
    if(m_sys_config->get_data("load_ip_from_config") == std::string("no"))
    {
        // ret = libapply::get_all_same_board_iplist(SLOT_TYPE_HOT_SIMULATION, 3, (char*)arr, sizeof(arr), &ipcnt);
    }
    if(ret == 0 && ipcnt != 0)
    {
        for(int i = 0; i < 4 * ipcnt; i += 4)
        {
            std::string ip;
            ip = std::to_string(arr[i + 3]);
            ip += ".";
            ip += std::to_string(arr[i + 2]);
            ip += ".";
            ip += std::to_string(arr[i + 1]);
            ip += ".";
            ip += std::to_string(arr[i]);
            m_hot_simulation_ips.push_back(ip);
            LOG_MSG(MSG_LOG, "mgr_network::load_hotsimulation_board_ip() from ipapply, ip:%s", ip.c_str());
        }
    }
    else
    {
        std::string ip = m_sys_config->get_data("hotsimulation_ip_addr");
        m_hot_simulation_ips.push_back(ip);
        LOG_MSG(MSG_LOG, "mgr_network::load_hotsimulation_board_ip() ip:%s", ip.c_str());
    }
    LOG_MSG(MSG_LOG, "Exited mgr_network::load_hotsimulation_board_ip()");
}

boost::shared_ptr<xtcp_client> mgr_network::find_simulation_client_by_boardid(int board_id)
{
    LOG_MSG(MSG_LOG, "Enter into mgr_network::find_simulation_client_by_boardid() board_id:%d", board_id);
    boost::shared_lock<boost::shared_mutex> lock(m_mux_map_hot_simulation_clis);
    boost::unordered_map<int, boost::shared_ptr<xtcp_client>>::iterator iter = m_map_hot_simulation_clis.find(board_id);
    if(iter != m_map_hot_simulation_clis.end())
    {
        LOG_MSG(MSG_LOG, "mgr_network::find_simulation_client_by_boardid() board_id:%d find client!!!", board_id);
        return iter->second;
    }
    LOG_MSG(MSG_LOG, "mgr_network::find_simulation_client_by_boardid() board_id:%d not find client!!!", board_id);
    return boost::shared_ptr<xtcp_client>();
}

boost::shared_ptr<xtcp_client> mgr_network::add_map_boardid_client(int board_id, xtcp_client *cli)
{
    LOG_MSG(MSG_LOG, "Enter into mgr_network::add_map_boardid_client() board_id:%d", board_id);
    boost::shared_ptr<xtcp_client> hot_cli = find_simulation_client_by_boardid(board_id);
    if(hot_cli != nullptr)
    {
        /*
        boost::shared_ptr<xtcp_client> tmp_client = find_simulation_client_in_vector(cli);
        if(tmp_client != nullptr)
        {
            if(tmp_client.get() != hot_cli.get())
            {
                LOG_MSG(MSG_LOG, "mgr_network::add_map_boardid_client board_id:%d map client has change", board_id);
            }
        }
        */
        LOG_MSG(MSG_LOG, "mgr_network::add_map_boardid_client() board_id:%d success", board_id);
        return hot_cli;
    }
    else
    {
        boost::shared_ptr<xtcp_client> tmp_client = find_simulation_client_in_vector(cli);
        if(tmp_client != nullptr)
        {
            boost::unique_lock<boost::shared_mutex> lock(m_mux_map_hot_simulation_clis);
            m_map_hot_simulation_clis.insert(std::make_pair(board_id, tmp_client));
            LOG_MSG(MSG_LOG, "mgr_network::add_map_boardid_client() board_id:%d success", board_id);
            return tmp_client;
        }
        LOG_MSG(WRN_LOG, "mgr_network::add_map_boardid_client() board_id:%d failed!", board_id);
    }
    return boost::shared_ptr<xtcp_client>();
}

boost::shared_ptr<xtcp_client> mgr_network::find_simulation_client_in_vector(xtcp_client *cli)
{
    LOG_MSG(MSG_LOG, "Enter into mgr_network::find_simulation_client_in_vector()");
    boost::shared_lock<boost::shared_mutex> lock(m_mux_hot_simulation_clis);
    for(auto &hot_cli : m_hot_simulation_clis)
    {
        if(hot_cli.get() == cli)
        {
            LOG_MSG(MSG_LOG, "mgr_network::find_simulation_client_in_vector() find client!!!");
            return hot_cli;
        }
    }
    LOG_MSG(MSG_LOG, "mgr_network::find_simulation_client_in_vector() client not find");
    return boost::shared_ptr<xtcp_client>();
}

int mgr_network::send_to_hotsimulation_board(boost::shared_ptr<xtcp_client> &hot_client, xusbpackage *pack)
{
    int ret = 0;
    std::string pack_data = pack->serial_to_bin();
    if(hot_client == NULL)
    {
        LOG_MSG(ERR_LOG, "mgr_network::send_to_hotsimulation_board() hot_client is NULL");
    }
    else
    {
        ret = hot_client->send_data(pack_data.data(), pack_data.length());
    }
    return ret;
}

int mgr_network::send_to_hotsimulation_board(int board_id, xusbpackage *pack)
{
    int ret = 0;
    std::string pack_data = pack->serial_to_bin();
    boost::shared_ptr<xtcp_client> hot_cli = find_simulation_client_by_boardid(board_id);
    if(hot_cli == nullptr)
    {
        LOG_MSG(ERR_LOG, "mgr_network::send_to_hotsimulation_board() hot_client is NULL");
    }
    else
    {
        ret = hot_cli->send_data(pack_data.data(), pack_data.length());
    }
    return ret;
}

int mgr_network::add_listener(xlistener *new_listener)
{
    boost::unique_lock<boost::shared_mutex> lock(m_mux_listener);
    for(std::list<xlistener*>::iterator iter=m_lst_listener.begin(); iter!=m_lst_listener.end(); iter++)
    {
        if((xlistener *)(*iter) == new_listener) return 0;
    }
    m_lst_listener.push_back(new_listener);
    return 1;
}

void mgr_network::del_listener(xlistener *del_listener)
{
    boost::unique_lock<boost::shared_mutex> lock(m_mux_listener);
    for(std::list<xlistener*>::iterator iter=m_lst_listener.begin(); iter!=m_lst_listener.end(); iter++)
    {
        if((xlistener *)(*iter)!=del_listener) {iter++; continue;}
        else
        {
            m_lst_listener.erase(iter); return;
        }
    }
}

int mgr_network::tel_listener(NET_MSG msg_type, const char *port_name, xpacket *packet)
{
    const char *flag_end = strstr(port_name, ">");
    std::string port_flag = (flag_end == NULL)?std::string(port_name, port_name+strlen(port_name)):"";
    LOG_MSG(MSG_LOG, "mgr_network::tel_listener() port_name:%s", port_name);
    boost::shared_lock<boost::shared_mutex> lock(m_mux_listener);
    for(std::list<xlistener*>::iterator iter=m_lst_listener.begin(); iter!=m_lst_listener.end(); iter++)
    {
        xlistener *listener = *iter;
        if(listener->judge_filter(port_flag)) listener->on_network_msg(msg_type, port_name, packet);
    }
    return m_lst_listener.size();
}

int mgr_network::set_network_connect(const std::string &ip)
{
    m_sys_config = xconfig::get_instance();
    boost::shared_ptr<xtcp_client> hot_client(new xtcp_client());
    hot_client->set_callback(boost::bind(&mgr_network::on_tcp_cli_msg, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
    hot_client->set_packer_unpacker(new usb_bin_packer, new usb_bin_unpacker);
    hot_client->set_server_addr(ip.c_str(), m_sys_config->get_data("hotsimulation_port", 13083));
    int ret = 0;
    if(!hot_client->is_connected())
    {
        ret = hot_client->start_connect();
        if(ret != 0) {
            return ret;
        }
    }
    boost::unique_lock<boost::shared_mutex> lock(m_mux_hot_simulation_clis);
    m_hot_simulation_clis.push_back(std::move(hot_client));
    LOG_MSG(MSG_LOG, "mgr_network::mgr_network() create hot_client, ip+port:%s", m_hot_simulation_clis.back()->get_server_addr().c_str());
    return ret;
}

bool mgr_network::wait_network_complete()
{
    boost::shared_lock<boost::shared_mutex> lock(m_mux_hot_simulation_clis);
    for (auto &item : m_hot_simulation_clis) {
        if (!item->is_connected()) {
            return false;
        }
    }
    return true;
}

void mgr_network::stop_client()
{
    boost::unique_lock<boost::shared_mutex> lock(m_mux_hot_simulation_clis);
    for(auto &cli : m_hot_simulation_clis)
    {
        cli->close_socket();
    }
}