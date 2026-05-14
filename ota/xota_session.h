#ifndef XOTA_SESSION_H_H_
#define XOTA_SESSION_H_H_

#include <string>

enum ota_session_status
{
    OTA_SESSION_STATUS_IDLE = 0,
    OTA_SESSION_STATUS_STARTED,
    OTA_SESSION_STATUS_UPGRADING,
    OTA_SESSION_STATUS_CANCLED,
    OTA_SESSION_STATUS_COMPLETED,
    OTA_SESSION_STATUS_END,
    OTA_SESSION_STATUS_CALFILE_READ,
    OTA_SESSION_STATUS_CALFILE_DATA,
    OTA_SESSION_STATUS_CALFILE_COMPLETE,
    OTA_SESSION_STATUS_CALFILE_END,

    OTA_SESSION_MAX
};

class ota_session
{
public:
    ota_session();
    ota_session(int staus, int type, int version, unsigned int total_len = 0);
    virtual ~ota_session();

public:
    void set_session_status(int status);
    void set_ota_type(int type);
    void set_ota_version(int version);
    void set_ota_transed_len(unsigned int len);
    void set_ota_total_len(unsigned int len);
    void set_ota_md5(const unsigned char* md5);
    void set_xsession_id(int sessid);
    void set_xsession_portname(const std::string& port_name);

    int get_session_status();
    int get_ota_type();
    int get_ota_version();
    unsigned int get_ota_transed_len();
    unsigned int get_ota_total_len();
    void get_ota_md5(unsigned char* md5);
    int get_xsession_id();
    std::string get_xsession_portname();

private:
    // 升级ota会话状态
    int m_status;

    // ota固件类型
    int m_ota_type;

    // ota固件版本
    int m_ota_version;

    // 已经传输的长度
    unsigned int m_trans_len;

    // ota固件总长度
    unsigned int m_total_len;

    // 文件md5校验
    unsigned char m_md5[16];

    // 会话ID
    int m_session_id;

    // 串口端口名
    std::string m_port_name;
};

#endif