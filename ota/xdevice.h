#ifndef X_DEVICE_H
#define X_DEVICE_H

#include <string>
#include <map>
#include <list>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/format.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include "xbasic.hpp"
#include "xbasicmgr.hpp"
#include "xpackage.hpp"
#include "xconfig.hpp"
#include "xusbpackage.h"
#include "xota_session.h"
#include "mgr_network.h"
#include "mgr_usb_def.h"
#include "mgr_upgrade.h"
#include "mgr_device.h"
#include "xboard.h"

#define BOARD_X86          (0x01)      // x86板ID
#define BOARD_STV          (0x10)      // 箱体ID
#define BOARD_TST(slot)    (0x40+slot) // 单板ID

#define MAX_FILE_TRANSFER_LEN  1024

class mgr_device;
class temp_range;
class fan_speed;
class smoke_density;
class sensor_alarm;
class sensor_data;
class lcs_mode;
class lcs_fault;
class lcs_alarm;
class three_phase;
class servo_err;
class ps48v_info;

enum ota_type
{
    OTA_TYPE_FTTH_MCU,
    OTA_TYPE_FTMF_MCU,
    OTA_TYPE_RDBIHOT_MCU,

    OTA_TYPE_MAX
};

// TH/MF监控板
class xmboard : public xtransmitter // 单板类
{
public:
    xmboard(int id, std::string port_name="");
    ~xmboard();

public:
    int  get_id();
    void set_id(int id);
    void set_port_name(std::string port_name);
    void set_activtm(time_t activtm);
    time_t get_activtm();

    int send_ota_start(int dstid, int ota_type);
    int send_ota_cancel(int dstid, int ota_type);
    void send_heartbeat();
    void check_ota_status();

    std::string get_port_name();
    std::string get_value(unsigned char tid); // 获取指定数据

    static std::string get_ota_fw_name(int ota_type);  // 获得该单板的OTA升级文件名
    static std::string get_board_name(int board_id);   // 获得单板类型名称

    boost::shared_ptr<xusb_tvl> find_data(unsigned short tid);
    boost::shared_ptr<ota_session> get_ota_session();

public:
    // 通讯相关接口
    virtual int on_recv(const char *port_name, xusbpackage *pack); // 消息通知

protected:
    int handle_south_recv(xusbpackage *pack);                      // 南向消息处理
    int handle_south_request(xusbpackage *pack);                   // 南向请求消息处理
    int handle_south_response(xusbpackage *pack);                  // 南向应答消息处理

    int handle_ota_request_message(xusbpackage *the_pack);
    int handle_alarm_request_message(xusbpackage *the_pack);

private:
    int save_tlv_data(unsigned short board_id, std::list<boost::shared_ptr<xusb_tvl>> &list_tlv, unsigned char cmd);

public:

private:
    bool is_ota_session_status(int status);
    int  set_ota_transed_length(unsigned int len);
    int  get_ota_session_status(int status);
    int  read_otadata_from_file(const std::string filename, const unsigned int offset, const unsigned int len, char* read_buff);
    void send_ota_data(int req_sessionid, int dstid, boost::shared_ptr<xusb_tvl> &req_ota_data_tlv);
    int  del_ota_session();
    boost::shared_ptr<ota_session> add_ota_session(int status, int ota_type, int ota_version, unsigned int total_len);

protected:
    mgr_upgrade* m_upgrade_mgr; // 升级管理器

    mgr_device*   m_device_mgr;    // 监控板管理器
    xboardmgr*    m_board_mgr;      // 单板管理

    int           m_id;             // 单板ID
    std::string   m_port_name;      // 所在的端口名称
    time_t        m_activtm;        // 最后活跃的时刻

private:
    boost::shared_mutex            m_mux_session;   // ota升级会话读写锁
    boost::shared_ptr<ota_session> m_ota_session;  // ota升级会话
};

#endif