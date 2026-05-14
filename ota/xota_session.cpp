#include "xota_session.h"
#include <string.h>
#include "mgr_log.h"

ota_session::ota_session()
{
    m_status = OTA_SESSION_STATUS_IDLE;
    m_ota_type = 0;
    m_ota_version = 0;
    m_trans_len = 0;
    m_total_len = 0;
    memset(m_md5, 0, sizeof(m_md5));

    //LOG_MSG(WRN_LOG,"ota_session::ota_session() construct debug1");
}

ota_session::ota_session(int staus, int type, int version, unsigned int total_len)
{
    m_status = staus;
    m_ota_type = type;
    m_ota_version = version;
    m_total_len = total_len;
    m_trans_len = 0;
    memset(m_md5, 0, sizeof(m_md5));

    //LOG_MSG(WRN_LOG,"ota_session::ota_session() construct debug2");
}

ota_session::~ota_session()
{
    //LOG_MSG(WRN_LOG,"ota_session::~ota_session() deconstruct");
}

void ota_session::set_session_status(int status)
{
    m_status = status;
}

void ota_session::set_ota_type(int type)
{
    m_ota_type = type;
}

void ota_session::set_ota_version(int version)
{
    m_ota_version = version;
}

void ota_session::set_ota_transed_len(unsigned int len)
{
    m_trans_len = len;
}

void ota_session::set_ota_total_len(unsigned int len)
{
    m_total_len = len;
}

void ota_session::set_ota_md5(const unsigned char* md5)
{
    if (md5 != NULL)
    {
        memcpy(m_md5, md5, sizeof(m_md5));
    }
}

int ota_session::get_session_status()
{
    return m_status;
}

int ota_session::get_ota_type()
{
    return m_ota_type;
}

int ota_session::get_ota_version()
{
    return m_ota_version;
}

unsigned int ota_session::get_ota_transed_len()
{
    return m_trans_len;
}

unsigned int ota_session::get_ota_total_len()
{
    return m_total_len;
}

void ota_session::get_ota_md5(unsigned char* md5)
{
    if ((m_md5[0] != '\0') && (md5 != NULL))
    {
        memcpy(md5, m_md5, sizeof(m_md5));
    }
}