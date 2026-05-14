#include "mgr_device.h"
#include <math.h>
#include <algorithm>
#include <boost/format.hpp>
#include <boost/thread.hpp>

// 程序内部自定义的告警阈值范围宏
#define TH_OUT_TMP_RANGE         0x0001
#define TH_48V_TMP_RANGE         0x0002
#define HF_UP_TMP_RANGE          0x0003
#define HF_DOWN_TMP_RANGE        0x0004
#define MF_TOP_TMP_RANGE         0x0005
#define POW_48V_OUT_TMP_RANGE    0x0006
#define POW_48V_TMP_RANGE        0x0007
#define CDU_STATUS_TMP_RANGE     0x0008
#define NO_POW_WATER_TMP_RANGE   0x0009
#define TH_CABLE_BOX_TMP_VALUE   0x000A
#define HF_BOX_TMP_VALUE         0x000B
#define TH_IN_BOX_TMP_VALUE      0x000C

mgr_device::mgr_device()
{
    m_sys_config = xconfig::get_instance();
    m_network_mgr = mgr_network::get_instance();
}

mgr_device::~mgr_device()
{
    m_sys_config = NULL;
    m_network_mgr = NULL;
}

void mgr_device::init() // 初始化
{
    const char *c_filt_port[] = {PORT_HOT_SIMULATION};
    for(int i = 0; i < sizeof(c_filt_port)/sizeof(char *); i++)
        this->add_filter(c_filt_port[i]); // 设置网络消息筛选端口
    m_network_mgr->add_listener(this); // 将自己加入网络消息监听器
}

void mgr_device::work(unsigned long ticket) // 工作函数
{
    if((ticket%5) == 0)
    {
        check_heartbeat(); // 检测各个单板的心跳
    }

    // 发送每个监控板实时数据
    // refresh_data(ticket);
    // 发送每个监控板实时固有数据
    // fetch_permanent_data(ticket);

    // 检测各个单板升级会话状态
    check_ota_session();
}

// 网络接收通知
int mgr_device::on_network(NET_MSG msg_type, const char *port_name, xpacket *packet)
{
    if(msg_type == NET_DATA) // 网络端来数据
    {
        if(strcmp(port_name, PORT_HOT_SIMULATION) == 0)
        {
            xusbpackage *the_pack = static_cast<xusbpackage *>(packet);
            boost::shared_ptr<xmboard> the_board;
            // TH监控板或MF监控板或热模拟板设备发来的包
            the_board = add_board(the_pack->m_srcid, port_name);
            // 如果心跳消息的应答设置心跳保活状态
            if((the_pack->m_msg_cmd & MSG_MASK) == MSG_RESPONSE && (the_pack->m_msg_cmd & MSG_CMD_MASK) == xusbpackage::MC_HEARTBEAT)
            {
                set_serial_keepalived(the_pack->m_srcid, HEARTBEAT_ALIVED);
            }

            if(the_board != NULL)
                the_board->on_recv(port_name, the_pack);
        }
    }
    else if(msg_type == NET_CONNECTED) // 网络端刚连接
    {
    }

    return 0;
}

int mgr_device::on_network_send(int board_id, const char *port_name, xpacket *packet) // 代理消息发送接口
{
    int ret = 0;
    if(strcmp(port_name, PORT_HOT_SIMULATION) == 0)
    {
        // 发送给热模拟板的消息
        xusbpackage *the_pack = static_cast<xusbpackage *>(packet);
        ret = m_network_mgr->send_to_hotsimulation_board(board_id, the_pack);
    }
    else
    {
        // do noting
    }

    return ret;
}

boost::shared_ptr<xmboard> mgr_device::find_board(int board_id) // 寻找单板
{
    boost::shared_lock<boost::shared_mutex> lock(m_mux_board); // 读锁
    boost::unordered_map<int, boost::shared_ptr<xmboard>>::iterator iter = m_map_board.find(board_id);
    if(iter != m_map_board.end())
    {
        return iter->second;
    }
    return boost::shared_ptr<xmboard>();
}

boost::shared_ptr<xmboard> mgr_device::add_board(int board_id, const char *port_name) // 加入单板,如果已经存在,则返回该单板
{
    boost::shared_ptr<xmboard> the_board = find_board(board_id);
    if(the_board != NULL)
    {
        // 已经存在
        the_board->set_port_name(port_name);
        return the_board;
    }

    if(xconfig::debug() >= 1)
    {
        xbasic::debug_output("<board> add new board:%d\n", board_id);
    }

    boost::unique_lock<boost::shared_mutex> lock(m_mux_board); // 写锁
    boost::shared_ptr<xmboard> new_board(new xmboard(board_id, port_name)); // 新建单板对象
    new_board->set_proxy_send(boost::bind(&mgr_device::on_network_send, this, _1, _2, _3)); // 设置代理发送函数
    m_map_board.insert(std::make_pair(board_id, new_board)); // 将新单板加入到集合中
    return new_board;
}

void mgr_device::del_board(int board_id) // 删除单板
{
    boost::unique_lock<boost::shared_mutex> lock(m_mux_board); // 写锁
    boost::unordered_map<int, boost::shared_ptr<xmboard>>::iterator iter = m_map_board.find(board_id);
    if(iter != m_map_board.end())
        m_map_board.erase(iter);
}

int mgr_device::check_heartbeat() // 检测各个单板的心跳
{
    time_t tm_now = time(NULL);
    boost::shared_lock<boost::shared_mutex> lock(m_mux_board); // 读锁
    for(boost::unordered_map<int, boost::shared_ptr<xmboard>>::iterator iter = m_map_board.begin(); iter != m_map_board.end(); iter++)
    {
        boost::shared_ptr<xmboard> the_board = iter->second;
        if(the_board->get_id() == BOARD_X86)
            continue; // 是x86自身

        unsigned long time_diff = (unsigned long)abs(tm_now - the_board->get_activetm());
        if(time_diff > 30) // 单板心跳已经超时
        {
            // 重置上次接收时间,防止当client一直不给心跳,心跳频率退化成1s一次
            the_board->set_activetm(tm_now);
            set_serial_keepalived(the_board->get_id(), HEARTBEAT_NOT_ALIVED);
        }
        else if(time_diff > 10) // 单板通讯即将超时
        {
            // 发送心跳消息,心跳放在发送不合适
            // 当服务端没有给数据消息时,是无法发送心跳消息,客户端网络不断重启重连
            // 放在网络层更合适
            // the_board->send_heartbeat();
            // 重置上次接收时间,防止当client一直不给心跳,心跳频率退化成1s一次
            the_board->set_activetm(tm_now);
        }
    }

    lock.unlock();
    return 0;
}

int mgr_device::check_ota_session()
{
    boost::shared_lock<boost::shared_mutex> lock(m_mux_board); // 读锁
    for(boost::unordered_map<int, boost::shared_ptr<xmboard>>::iterator iter = m_map_board.begin(); iter != m_map_board.end(); iter++)
    {
        boost::shared_ptr<xmboard> the_board = iter->second;
        the_board->check_ota_status();
    }
    lock.unlock();
    return 0;
}

int mgr_device::find_serial_keepalived(int id)
{
    LOG_MSG(MSG_LOG, "Enter into mgr_serial::find_serial_keepalived()");
    boost::shared_lock<boost::shared_mutex> lock(m_mux_keepalived); // 读锁
    boost::unordered_map<int, int>::iterator iter = m_map_keepalived.find(id);
    if(iter != m_map_keepalived.end()) {
        return 0;
    }
    LOG_MSG(MSG_LOG, "Exited mgr_serial::find_serial_keepalived()");
    return -1;
}

void mgr_device::set_serial_keepalived(int id, int status)
{
    LOG_MSG(MSG_LOG, "Enter into mgr_device::set_serial_keepalived()");
    int is_exsited = find_serial_keepalived(id);
    boost::unique_lock<boost::shared_mutex> lock(m_mux_keepalived); // 写锁
    if(is_exsited == 0) { // 存在
        m_map_keepalived[id] = status;
        LOG_MSG(MSG_LOG, "mgr_device::set_serial_keepalived() have founded.");
        return ;
    }
    LOG_MSG(WRN_LOG, "mgr_device::set_serial_keepalived() insert keepalived map");
    m_map_keepalived.insert(std::make_pair(id, status)); // 将新的单板加入到集合中
    LOG_MSG(MSG_LOG, "Exited mgr_device::set_serial_keepalived()");
}

int mgr_device::get_serial_keepalived(int id)
{
    LOG_MSG(MSG_LOG, "Enter into mgr_device::get_serial_keepalived()");
    int status = HEARTBEAT_NOT_ALIVED;
    int is_exsited = find_serial_keepalived(id);
    boost::shared_lock<boost::shared_mutex> lock(m_mux_keepalived); // 读锁
    if(is_exsited == 0) { // 存在
        status = m_map_keepalived[id];
    }
    LOG_MSG(MSG_LOG, "Exited mgr_device::get_serial_keepalived()");
    return status;
}