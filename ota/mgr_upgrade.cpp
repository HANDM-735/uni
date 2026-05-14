#include "mgr_upgrade.h"
#include <stdio.h>
#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include "xcrypto.hpp"
#include "xpackage.hpp"
#include "xconfig.hpp"
#include "mgr_log.h"

void mgr_upgrade::init() // 初始化
{

}

void mgr_upgrade::work(unsigned long ticket) // 工作函数
{

}

void mgr_upgrade::load_otafiles() // 加载最新OTA固件
{
    if (m_otafile_path.length() <= 0)
        m_otafile_path = std::string(xbasic::get_module_path()) + "otafiles";
    std::vector<std::string> vct_files;
    xbasic::get_files_in_dir(m_otafile_path, &vct_files);

    for (unsigned int i = 0; i < vct_files.size(); i++)
    {
        std::string file_path = vct_files[i];
        const char *splitr = strrchr(file_path.c_str(), '/');
        std::string file_name = splitr ? (splitr + 1) : file_path.c_str(); // 文件名
        const char *exten = strrchr(file_path.c_str(), '.');
        std::string file_exten = exten ? (exten + 1) : ""; // 文件扩展名
        if (file_exten != "bin" && file_exten != "bit") continue; // 不是OTA文件
        file_name = file_name.substr(0, file_name.length() - 4); // 去掉扩展名

        std::vector<std::string> vct_name;
        int count = xbasic::split_string(file_name, "-", &vct_name);
        if (count != 2 || vct_name[0].length() <= 0) continue; // 文件命名不正确
        int fw_size = 0;
        boost::shared_ptr<xotafile> otafile_find = find_otafile(vct_name[0]);
        if (otafile_find != NULL) // 已经存在该类型的固件
        {
            if (xbasic::version_to_int(vct_name[1]) <= xbasic::version_to_int(otafile_find->m_version))
            {
                // 判断之前版本文件是否存在，如果存在忽略，否则将当前固件更新到map内作为当前最新版本
                bool old_existed = false;
                for (auto it : vct_files)
                {
                    if (it == otafile_find->m_path)
                    {
                        old_existed = true;
                        break;
                    }
                }
                if (old_existed == true)
                {
                    // 该当前固件的版本比之前版本的低，忽略
                    LOG_MSG(WRN_LOG, "mgr_upgrade::load_otafiles() current file-%s version(%s) lower latest file_path-%s version(%s)",
                            file_path.c_str(), vct_name[1].c_str(), otafile_find->m_path.c_str(), otafile_find->m_version.c_str());
                    continue;
                }
                else
                {
                    LOG_MSG(WRN_LOG, "mgr_upgrade::load_otafiles() latest file_path-%s version(%s) is not existed,current file_path-%s version(%s) will be updated",
                            otafile_find->m_path.c_str(), otafile_find->m_version.c_str(), file_path.c_str(), vct_name[1].c_str());
                }
            }
        }

        std::string fw_md5 = xcrypto::get_file_md5(file_path, &fw_size); // 计算文件的MD5同时获得文件长度
        if (fw_size <= 0 || fw_md5.length() != 32)
        {
            // MD5计算错误
            LOG_MSG(WRN_LOG, "mgr_upgrade::load_otafiles() update ota map file_path=%s md5 calculate error", file_path.c_str());
            continue;
        }

        // 更新最新固件文件属性
        if (otafile_find != NULL)
        {
            otafile_find->set(vct_name[0], vct_name[1], file_path, fw_size, fw_md5);
        }
        else
        {
            boost::shared_ptr<xotafile> otafile(new xotafile(vct_name[0], vct_name[1], file_path, fw_size, fw_md5));
            boost::unique_lock<boost::shared_mutex> lock(m_mux_otafile); // 写锁
            m_map_otafile.insert(std::make_pair(vct_name[0], otafile)); // 加入到集合中
            lock.unlock();
        }
    }

    if (xconfig::debug() < 1) return; // 不打印
    LOG_MSG(MSG_LOG, "< ota > otafiles list:");
    boost::shared_lock<boost::shared_mutex> lock(m_mux_otafile); // 读锁
    for (boost::unordered_map<std::string, boost::shared_ptr<xotafile>>::iterator iter = m_map_otafile.begin(); iter != m_map_otafile.end(); iter++)
    {
        boost::shared_ptr<xotafile> otafile_find = iter->second;
        LOG_MSG(MSG_LOG, "< ota > type:%s \tversion:%s \tsize:%d",
                otafile_find->m_type.c_str(), otafile_find->m_version.c_str(), otafile_find->m_size);
    }
    LOG_MSG(MSG_LOG, "\n");
}

boost::shared_ptr<xotafile> mgr_upgrade::find_otafile(std::string fw_type) // 寻找OTA固件
{
    boost::shared_lock<boost::shared_mutex> lock(m_mux_otafile); // 读锁
    boost::unordered_map<std::string, boost::shared_ptr<xotafile>>::iterator iter = m_map_otafile.find(fw_type);
    if (iter != m_map_otafile.end())
        return iter->second;
    return boost::shared_ptr<xotafile>();
}

int mgr_upgrade::check_ota() // 检测OTA升级
{
    return 0;
}