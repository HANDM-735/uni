#ifndef MGR_UPGRADE_H
#define MGR_UPGRADE_H

#include <string>
#include <list>
#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>
#include <boost/thread/mutex.hpp>
#include "xbasic.hpp"
#include "xbasicmgr.hpp"
#include "xpackage.hpp"
//#include "xdevice.hpp"

class xotafile // OTA文件
{
public:
    xotafile(std::string type, std::string version, std::string path, int fsize, std::string md5)
        : m_type(type), m_version(version), m_path(path), m_size(fsize), m_md5(md5)
    {
        m_ver_int = xbasic::version_to_int(m_version);
    }

    void set(std::string type, std::string version, std::string path, int fsize, std::string md5)
    {
        m_type = type;
        m_version = version;
        m_path = path;
        m_size = fsize;
        m_md5 = md5;
        m_ver_int = xbasic::version_to_int(m_version);
    }

public:
    std::string m_type;     // 固件类型
    std::string m_version;  // 固件版本
    int m_ver_int;          // 固件版本整型值
    std::string m_path;     // 固件文件路径
    int m_size;             // 固件文件长度
    std::string m_md5;      // 固件MD5值
};

class mgr_upgrade : public xmgr_basic<mgr_upgrade> // 升级管理器
{
public:
    mgr_upgrade() {}
    ~mgr_upgrade() {}

public:
    void set_ota_path(std::string otafile_path) { m_otafile_path = otafile_path; } // 设置OTA目录路径
    std::string get_ota_path() { return m_otafile_path; }                          // 获取OTA目录路径
    void load_otafiles();                                                          // 加载最新OTA固件
    boost::shared_ptr<xotafile> find_otafile(std::string fw_type);                  // 寻找OTA固件

    int check_ota(); // 检测各个单板OTA升级

public:
    virtual void init();                              // 初始化
    virtual void work(unsigned long ticket);         // 工作函数

protected:
    std::string m_otafile_path;                                        // OTA文件的路径
    boost::unordered_map<std::string, boost::shared_ptr<xotafile>> m_map_otafile;  // 各个最新OTA文件集合
    boost::shared_mutex m_mux_otafile;                                 // OTA文件集合读写锁
};

#endif