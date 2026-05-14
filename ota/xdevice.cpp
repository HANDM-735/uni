#include "xdevice.h"
#include <boost/shared_array.hpp>


xmboard::xmboard(int id,std::string port_name) : xtransmitter("<board>")
{
    m_id = id;
    m_port_name = port_name;
    m_activetm = time(NULL);
    m_upgrad_mgr = mgr_upgrade::get_instance();
    m_device_mgr = mgr_device::get_instance();
    m_board_mgr = xboardmgr::get_instance();
}

xmboard::~xmboard()
{
    m_upgrad_mgr = NULL;
    m_device_mgr = NULL;
    m_board_mgr = NULL;
}

//消息通知
int xmboard::on_recv(const char *port_name,xusbpackage *pack)
{
    if(strcmp(port_name,PORT_TH_MONITOR) == 0 || strcmp(port_name,PORT_MF_MONITOR) == 0 )
    {
        set_activetm(time(NULL));
        //南向消息处理
        handle_south_recv(pack);
    }
    return xtransmitter::on_recv(port_name,pack);
}

//南向消息处理
int xmboard::handle_south_recv(xusbpackage *pack)
{
    switch(pack->m_msg_cmd & USB_MESSAGE_MASK)
    {
        case USB_REQUEST_MESSAGE:
            handle_south_request(pack);
            break;
        case USB_RESPONE_MESSAGE:
            handle_south_response(pack);
            break;
    }
    return 0;
}

//南向请求消息处理
int xmboard::handle_south_request(xusbpackage *pack)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::handle_south_request()");
    xusbpackage *the_pack = static_cast<xusbpackage *>(pack);
    if(!the_pack->crc_check())
    {
        LOG_MSG(WRN_LOG,"xmboard::handle_south_request(),message msg_cmd=%d crc check failed!",the_pack->m_msg_cmd);
        return -1;
    }

    if ((the_pack->m_msg_cmd & MSG_CMD_MASK) == xusbpackage::MC_OTA)
    {
        //OTA命令处理
        int ret = handle_ota_request_message(the_pack);
        if(ret != 0) {
            LOG_MSG(WRN_LOG,"xmboard::handle_south_request(),handle_ota_request_message msg_cmd=%d failed!",the_pack->m_msg_cmd);
            return -1;
        }
    }

    if ((the_pack->m_msg_cmd & MSG_CMD_MASK)== xusbpackage::MC_ALARM)
    {
        //ALarm命令处理
        int ret = handle_alarm_request_message(the_pack);
        if(ret != 0) {
            LOG_MSG(WRN_LOG,"xmboard::handle_south_request(),handle_alarm_request_message msg_cmd=%d failed!",the_pack->m_msg_cmd);
            return -1;
        }
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::handle_south_request()");

    return 0;
}

int xmboard::handle_ota_request_message(xusbpackage *the_pack)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::handle_ota_request_message()");
    if(m_ota_session == NULL)
    {
        LOG_MSG(WRN_LOG,"xmboard::handle_ota_request_message(),m_srcid = %d ota session not existed!",the_pack->m_srcid);
        return -1;
    }
    //判断当前状态机是否在ota升级逻辑的状机中,为了控制ota升级与cal校准文件读取流程是互斥进行的
    int status = m_ota_session->get_session_status();
    if(is_ota_session_status(status) == false)
    {
        LOG_MSG(WRN_LOG,"xmboard::handle_ota_request_message(),current status(%d) is not in ota session status range",status);
        return -1;
    }

    //OTA命令处理
    if((the_pack->m_msg_cmd & MSG_CMD_MASK) == xusbpackage::MC_OTA)
    {
        LOG_MSG(MSG_LOG,"xmboard::handle_ota_request_message( receive OTA message)");
        boost::shared_ptr<xusb_tvl> ota_data_tlv = the_pack->find_data(FIRMWARE_DATA);
        if(ota_data_tlv.get() != NULL)
        {
            //发送固件数据
            LOG_MSG(MSG_LOG,"xmboard::handle_ota_request_message() send OTA firmware data message");
            send_ota_data(the_pack->m_session,the_pack->m_srcid,ota_data_tlv);
        }
        //LOG_MSG(MSG_LOG,"mgr_usb::handle_ota_request_message( debug:");
        boost::shared_ptr<xusb_tvl> ota_completed_tlv = the_pack->find_data(OTA_COMPLETE);
        if(ota_completed_tlv.get() != NULL)
        {
            //是否需要发送应答给usb
            //暂时不需要响应这个请求的应答
            LOG_MSG(MSG_LOG,"xmboard::handle_ota_request_message( m_srcid=%d receive OTA firmware data complete message",the_pack->m_srcid);
            int ret = set_ota_transed_length(m_ota_session->get_ota_total_len());
            if(ret != 0) {
                LOG_MSG(WRN_LOG,"xmboard::send_ota_data() set ota transed length failed!");
            }
            set_ota_session_status(OTA_SESSION_STATUS_COMPLETED);
        }
    }
    LOG_MSG(MSG_LOG,"Exited xmboard::handle_ota_request_message()");
    return 0;
}

int xmboard::send_ota_start(int dstid,int ota_type)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::send_ota_start() dstid=0x%X ota_type=%d",dstid,ota_type);
    int status = m_device_mgr->get_serial_keepalived(dstid);
    if(status != HEARTBEAT_ALIVED) {
        LOG_MSG(WRN_LOG,"xmboard::send_ota_start(),dstid = %d usb heartbeat not alived!",dstid);
        return -1;
    }

    if(m_ota_session != NULL)
    {
        //再次检查ota会话的状态,如果会话已经结束删除旧的会话
        int status = m_ota_session->get_session_status();
        if( (status == OTA_SESSION_STATUS_COMPLETED) || (status == OTA_SESSION_STATUS_END) || (status == OTA_SESSION_STATUS_CANCLED) || (status == OTA_SESSION_STATUS_CALFILE_END))
        {
            LOG_MSG(WRN_LOG,"xmboard::send_ota_start(),dstid=0x%X ota_type=%d ota session/cal session have exited,status=%d",dstid,ota_type,status);
            del_ota_session();
        }
        else
        {
            LOG_MSG(ERR_LOG,"xmboard::send_ota_start(),dstid=0x%X ota_type=%d ota session/cal session have exited,status=%d",dstid,ota_type,status);
            return -1;
        }
    }

    std::string fw_typestr = xmboard::get_ota_fw_name(ota_type);
    boost::shared_ptr<xotafile> otafile = m_upgrad_mgr->find_otafile(fw_typestr);
    if(otafile.get() == NULL)
    {
        LOG_MSG(WRN_LOG,"xmboard::send_ota_start() find %s type ota file failure",fw_typestr.c_str());
        return -1;
    }

    xusbpackage req_pack(xusbpackage::MC_OTA,PROTO_VERSION,0,xconfig::self_id(),dstid);
    unsigned char value[7] = {0};
    //固件类型
    unsigned char fw_type = ota_type;
    //固件版本
    unsigned short fw_version = otafile->m_ver_int;
    //固件总长度
    unsigned int fw_length = otafile->m_size;
    //文件检验md5
    unsigned char file_md5[16] = {0};
    xbasic::str_to_hex(&otafile->m_md5[0],&file_md5[0],otafile->m_md5.length());

    value[0] = fw_type;
    xbasic::write_bigendian(&value[1], fw_version, 2);
    xbasic::write_bigendian(&value[3], fw_length, 4);
    std::string type_value((char*)&value[0],7);
    type_value.append(std::string((char*)file_md5,16));
    req_pack.add_data(OTA_START,type_value,xusbpackage::MC_OTA);
    int len = send(m_port_name.c_str(),(xpacket*)&req_pack);
    add_ota_session(OTA_SESSION_STATUS_STARTED,fw_type,fw_version,fw_length);

    LOG_MSG(MSG_LOG,"Exited xmboard::send_ota_start()");

    return 0;
}

int xmboard::send_ota_cancle(int dstid,int ota_type)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::send_ota_cancle() dstid=%d ota_type = %d",dstid,ota_type);
    int serial_status = m_device_mgr->get_serial_keepalived(dstid);
    if(serial_status != HEARTBEAT_ALIVED) {
        LOG_MSG(WRN_LOG,"xmboard::send_ota_cancle(),dstid = %d usb heartbeat not alived!",dstid);
        return -1;
    }

    if(m_ota_session == NULL)
    {
        LOG_MSG(WRN_LOG,"xmboard::send_ota_cancle(),dstid = %d ota session not existed!",dstid);
        return -1;
    }
    //判断当前状态机是否在ota升级逻辑的状机中,为了控制ota升级与cal校准文件读取流程是互斥进行的
    int ota_status = m_ota_session->get_session_status();
    if(is_ota_session_status(ota_status) == false)
    {
        LOG_MSG(WRN_LOG,"xmboard::send_ota_cancle(),current status(%d) is not in ota session status range",ota_status);
        return -1;
    }

    std::string fw_typestr = xmboard::get_ota_fw_name(ota_type);
    boost::shared_ptr<xotafile> otafile = m_upgrad_mgr->find_otafile(fw_typestr);
    if(otafile.get() == NULL)
    {
        LOG_MSG(WRN_LOG,"xmboard::send_ota_start() find %s type ota file failure",fw_typestr.c_str());
        return -1;
    }

    xusbpackage req_pack(xusbpackage::MC_OTA,PROTO_VERSION,0,xconfig::self_id(),dstid);
    unsigned char value[3] = {0};
    //固件类型
    unsigned char fw_type = ota_type;
    //固件版本
    unsigned short fw_version = otafile->m_ver_int;

    value[0] = fw_type;
    xbasic::write_bigendian(&value[1], fw_version, 2);
    std::string type_value((char*)&value[0],3);
    req_pack.add_data(OTA_CANCEl,type_value,xusbpackage::MC_OTA);
    send(m_port_name.c_str(),(xpacket*)&req_pack);
    set_ota_session_status(OTA_SESSION_STATUS_CANCLED);
    LOG_MSG(MSG_LOG,"Exited xmboard::send_ota_cancle()");
    return 0;
}

void xmboard::send_ota_data(int req_sessionid,int dstid,boost::shared_ptr<xusb_tvl>& req_ota_data_tlv)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::send_ota_data() dstid=%d",dstid);
    std::string tlv_value = req_ota_data_tlv->get_data();
    unsigned char ota_type = tlv_value[0];
    unsigned int req_data_offset = 0;
    req_data_offset=xbasic::read_bigendian(&tlv_value[1], 4);
    unsigned short req_data_len = 0;
    req_data_len=xbasic::read_bigendian(&tlv_value[5], 2);

    LOG_MSG(MSG_LOG,"xmboard::send_ota_data() req_data_offset=%u request_data_len=%hu",req_data_offset,req_data_len);

    if(req_data_len > MAX_FILE_TRANSFER_LEN)
    {
        LOG_MSG(WRN_LOG,"xmboard::send_ota_data() request data len(%u) > max file transfer len(%d)",req_data_len,MAX_FILE_TRANSFER_LEN);
        req_data_len = MAX_FILE_TRANSFER_LEN;
    }

    //从OTA文件中在指定的offset开始读取req_data_len长度
    boost::shared_array<char> pdata(new char[req_data_len]);
    if(pdata.get() == NULL)
    {
        LOG_MSG(WRN_LOG,"xmboard::send_ota_data() allocate ota buffer memory failed!");
        return ;
    }

    std::string fw_typestr=xmboard::get_ota_fw_name(ota_type);
    boost::shared_ptr<xotafile> otafile = m_upgrad_mgr->find_otafile(fw_typestr);
    //otafile类成员m_path保存是ota文件完整路径
    if(otafile == NULL)
    {
        LOG_MSG(WRN_LOG,"xmboard::send_ota_data() not found ota type=%d,ota_file=%s",ota_type,fw_typestr.c_str());
        return;
    }
    //LOG_MSG(MSG_LOG,"mgr_usb::send_ota_data() debug0");
    std::string ota_filename = otafile->m_path;
    int ret = read_otadata_from_file(ota_filename,req_data_offset,req_data_len,pdata.get());
    if(ret != 0)
    {
        LOG_MSG(WRN_LOG,"xmboard::send_ota_data() read ota data req_data_offset=%u request_data_len=%hu from %s file failure",req_data_offset,req_data_len,ota_filename.c_str());
        return ;
    }

    LOG_MSG(MSG_LOG,"xmboard::send_ota_data() debug1");
    //封装OTA_DATA请求的回复包
    xusbpackage resp_pack(xusbpackage::MC_OTA|0x80,PROTO_VERSION,req_sessionid,xconfig::self_id(),dstid);
    unsigned char value[7] = {0};
    //固件类型
    unsigned char fw_type = ota_type;
    //数据偏移
    unsigned int data_offset = req_data_offset;
    //本次传输的长度
    unsigned short transfer_len = req_data_len;

    LOG_MSG(MSG_LOG,"xmboard::send_ota_data() debug2");
    value[0] = fw_type;
    xbasic::write_bigendian(&value[1], data_offset, 4);
    xbasic::write_bigendian(&value[5], transfer_len, 2);
    std::string type_valum((char*)&value[0],7);
    type_valum.append(std::string((char*)pdata.get(),(transfer_len)));
    resp_pack.add_data(FIRMWARE_DATA,type_valum,xusbpackage::MC_OTA);
    LOG_MSG(MSG_LOG,"xmboard::send_ota_data() debug3");
    ret = set_ota_session_status(OTA_SESSION_STATUS_UPGRADING);
    if(ret != 0)
    {
        LOG_MSG(WRN_LOG,"xmboard::send_ota_data() set ota session status failed!");
        return ;
    }
    //以请求偏移长度为传输成功的长度更准确
    ret = set_ota_transed_length(req_data_offset);
    if(ret != 0)
    {
        LOG_MSG(WRN_LOG,"xmboard::send_ota_data() set ota transed length failed!");
        return ;
    }
    LOG_MSG(MSG_LOG,"xmboard::send_ota_data() debug4");
    send(m_port_name.c_str(),(xpacket*)&resp_pack);

    LOG_MSG(MSG_LOG,"Exited xmboard::send_ota_data()");
}

void xmboard::send_heartbeat()
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::send_heartbeat()");

    xusbpackage pack(xusbpackage::MC_HEARTBEAT,PROTO_VERSION,0,xconfig::self_id(),0); //构造心跳请求包

    int len = send(m_port_name.c_str(),(xpacket*)&pack);

    LOG_MSG(MSG_LOG,"Exited xmboard::send_heartbeat(len=%d)",len);
}

void xmboard::check_ota_status()
{
    if(m_ota_session == NULL)
    {
        LOG_MSG(ERR_LOG,"xmboard::check_ota_status() ota session is not existed boardid=%d",m_id);
        return;
    }

    int status = m_ota_session->get_session_status();
    if(status == OTA_SESSION_STATUS_COMPLETED)
    {
        set_ota_session_status(OTA_SESSION_STATUS_END);
    }
}

int xmboard::read_otadata_from_file(const std::string filename, const unsigned int offset,const unsigned int len,char* read_buff)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::read_otadata_from_file() filename=%s offset=%d len=%d",filename.c_str(),offset,len);

    FILE *fp = fopen(filename.c_str(),"rb");
    if(!fp)
    {
        LOG_MSG(WRN_LOG,"xmboard::read_otadata_from_file() open %s file failure",filename.c_str());
        return -1;
    }

    if(len == 0)
    {
        LOG_MSG(MSG_LOG,"xmboard::read_otadata_from_file() filename=%s offset=%d len=%d,len is invalid!",filename.c_str(),offset,len);
        return -1;
    }

    fseek(fp,0L,SEEK_END);             //定位到文件末尾
    unsigned int file_len = ftell(fp);  //得到文件大小
    fseek(fp,offset,SEEK_SET);          //定位到文件开头

    if(offset+len > file_len)
    {
        LOG_MSG(WRN_LOG,"xmboard::read_otadata_from_file() read len(%u) byte from file offset(%u),exceed file total length(%u)",len,offset,file_len);
        return -1;
    }

    unsigned int read_len = 0;
    unsigned int index = len/1024;
    unsigned int reminder = len%1024;

    //以1024字节长度读取
    if(index > 0)
    {
        do
        {
            int ret = 0;
            ret = fread(read_buff+read_len,1,1024,fp);
            read_len += ret;
        } while(read_len < (index*1024));
    }

    //不足1024长度读取
    if(reminder > 0)
    {
        int pos = index*1024;
        fread(read_buff+pos,1,reminder,fp);
    }

    fclose(fp);
    fp = NULL;

    LOG_MSG(MSG_LOG,"xmboard::read_otadata_from_file() debug2 index=%d read_len=%d reminder=%d",index, read_len, reminder);
    LOG_MSG(MSG_LOG,"Exited xmboard::read_otadata_from_file()");

    return 0;
}


int xmboard::handle_alarm_request_message(xusbpackage *the_pack)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::handle_alarm_request_message()");

    int ret = 0;
    //alarm命令处理
    if((the_pack->m_msg_cmd & MSG_CMD_MASK) == xusbpackage::MC_ALARM)
    {
        LOG_MSG(MSG_LOG,"xmboard::handle_alarm_request_message() receive alarm message.");
        //将告警数据发送给平台
        // std::list<boost::shared_ptr<xusb_tvl> > list_xusbtlv;
        // std::list<boost::shared_ptr<xtvl> > list_xtlv;
        // int size = the_pack->get_tlv_data(list_xusbtlv);
        // if(size > 0)
        // {
        //     for(auto xusbtlv_tmp : list_xusbtlv)
        //     {
        //         //将告警相关xusb_tlv数据转换成平台xtlv数据
        //         convert_data(list_xtlv,xusbtlv_tmp);

        //         if((xusbtlv_tmp->m_tid == RMA_TH_SERVO_ALARM) && ((the_pack->m_dstid & BOARDTYPE_MASK) == BOARDTYPE_TH_MONITOR))
        //         {
        //             //将TH伺服器的告警通过http result接口上报给平台
        //             send_thservo_alarm(xusbtlv_tmp->m_value);
        //         }
        //     }
        // }

        // xpackage pack(xpackage::MT_NOTIFY,xpackage::MC_ALARM,PROTO_VERSION,0,the_pack->m_srcid); //构造包
        // pack.add_tlv_data(list_xtlv);

        // int len = send(PORT_PLATFORMFROM,(xpacket*)&pack);
        // if(len <= 0 )
        // {
        //     ret = -1;
        // }
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::handle_alarm_request_message() ret:%d",ret);

    return ret;

}


//南向应答消息处理
int xmboard::handle_south_response(xusbpackage *pack)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::handle_south_response()");
    int ret = -1;
    xusbpackage *the_pack = static_cast<xusbpackage *>(pack);
    if(!the_pack->crc_checksum())
    {
        LOG_MSG(ERR_LOG,"xmboard::handle_south_response(),message crc check failed!");
        return ret;
    }

    //READ命令处理
    if((the_pack->m_msg_cmd & MSG_CMD_MASK) == xusbpackage::MC_READ)
    {
        LOG_MSG(MSG_LOG,"xmboard::handle_south_response() receive read message.");
        std::list<boost::shared_ptr<xusb_tvl> > list_xusbtlv;
        std::list<boost::shared_ptr<xtvl> > list_xtlv;
        int size = the_pack->get_tlv_data(list_xusbtlv);
        if(size > 0)
        {
            //保存xusb tlv数据
            save_tlv_data(m_id,list_xusbtlv,xusbpackage::MC_READ);

            //将xusb tlv数据转换成平台xtlv格式数据
            // for(auto xusbtlv_tmp : list_xusbtlv)
            // {
            //     //将读取命令字的读取到固有和实时数据相关xusb tlv数据转换成平台xtlv数据
            //     convert_data(list_xtlv,xusbtlv_tmp);
            //     //将安全策略相关xusb tlv数据转换成平台xtlv数据
            //     //convert_securitypolicy(list_xtlv,xusbtlv_tmp);
            // }
        }
        // //发送给平台
        // xpackage pack(xpackage::MT_NOTIFY,xpackage::MC_READ,PROTO_VERSION,0,the_pack->m_srcid); //构造包
        // pack.add_tlv_data(list_xtlv);
        // int len = send(PORT_PLATFROM,(xpackage*)&pack);
        // if(len > 0)
        // {
        //     ret = 0;
        // }
        
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::handle_south_response() ret=%d",ret);
    return ret;
}



int xmboard::get_id()
{
    return m_id;
}

void xmboard::set_id(int id)
{
    m_id = id;
}

std::string xmboard::get_port_name()
{
    return m_port_name;
}

int xmboard::save_tlv_data(unsigned short board_id,std::list<boost::shared_ptr<xusb_tvl> >& list_tlv,unsigned char cmd)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::save_tlv_data()");
    boost::shared_ptr<xboard> the_board = m_board_mgr->add_board(board_id);
    if(the_board != NULL)
    {
        the_board->set_tlv_data(list_tlv,cmd); //将TLV数据设置到单板对象
    }
    LOG_MSG(MSG_LOG,"Exited xmboard::save_tlv_data()");
    return 0;
}

boost::shared_ptr<xusb_tvl> xmboard::find_data(unsigned short tid)
{
    int boardid = m_id;
    boost::shared_ptr<xboard> the_board = m_board_mgr->add_board(m_id);
    boost::shared_ptr<xusb_tvl> tvl;
    if(the_board != NULL)
    {
        tvl = the_board->find_data((int)tid);
    }

    return tvl;
}

void xmboard::set_port_name(std::string port_name)
{
    if(m_port_name != port_name)
    {
        m_port_name = port_name;
    }
}

time_t xmboard::get_activetm()
{
    return m_activetm;
}

void xmboard::set_activetm(time_t activetm)
{
    m_activetm = activetm;
}

std::string xmboard::get_ota_fw_name(int ota_type) //获得该单板的OTA升级文件名
{
    switch(ota_type)
    {
        case OTA_TYPE_FTTH_MCU:
            return "FTTH_MCU";
        case OTA_TYPE_FTMF_MCU:
            return "FTMF_MCU";
        case OTA_TYPE_CPTH_MCU:
            return "CPTH_MCU";
        case OTA_TYPE_CPMF_MCU:
            return "CPMF_MCU";
    }
    return "";
}

std::string xmboard::get_board_name(int board_id) //获得单板类型名称
{
    if(board_id == BOARD_X86)        return ("B-X86");
    else if(board_id == BOARD_STV)   return ("B-STV");
    else if(board_id >= BOARD_TST(0))return ("B-TST");
    return ("B-UNK");
}

bool xmboard::is_ota_session_status(int status)
{
    bool ret = false;
    if((status >= OTA_SESSION_STATUS_STARTED) && (status <= OTA_SESSION_STATUS_END))
    {
        ret = true;
    }
    return ret;
}

int xmboard::set_ota_transed_length(unsigned int len)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::set_ota_transed_length()");
    if(m_ota_session == NULL) {
        LOG_MSG(WRN_LOG,"xmboard::set_ota_transed_length() ota session is not exited!");
        return -1;
    }
    //boost::unique_lock<boost::shared_mutex> lock(m_mux_session);    //写锁
    m_ota_session->set_ota_transed_len(len);
    LOG_MSG(MSG_LOG,"Exited xmboard::set_ota_transed_length()");
    return 0;
}

int xmboard::set_ota_session_status(int status)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::set_ota_session() status=%d",status);
    if(m_ota_session == NULL) {
        LOG_MSG(WRN_LOG,"mgr_usb::set_ota_session() ota session is not exited!");
        return -1;
    }
    boost::unique_lock<boost::shared_mutex> lock(m_mux_session);    //写锁
    int previous_status = m_ota_session->get_session_status();
    if(previous_status == OTA_SESSION_STATUS_CANCLED || (previous_status == OTA_SESSION_STATUS_END) || (previous_status == OTA_SESSION_STATUS_CALFILE_END)) {
        LOG_MSG(WRN_LOG,"mgr_usb::set_ota_session() previous status(%d) is OTA_SESSION_STATUS_CANCLED or OTA_SESSION_STATUS_END,can't change to status(%d)",previous_status,status);
        return -1;
    }
    else
    {
        m_ota_session->set_session_status(status);
        LOG_MSG(MSG_LOG,"mgr_usb::set_ota_session() status from (%d) to (%d)",previous_status,status);
    }
    LOG_MSG(MSG_LOG,"Exited mgr_usb::set_ota_session()");
    return 0;
}

boost::shared_ptr<ota_session> xmboard::add_ota_session(int status,int ota_type,int ota_version,unsigned int total_len)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::add_ota_session()");
    if(m_ota_session != NULL) {
        LOG_MSG(MSG_LOG,"xmboard::add_ota_session(),ota_type=%d has existed.",ota_type);
        return m_ota_session; //已经存在
    }
    LOG_MSG(WRN_LOG,"chip add new add ota_session:ota_type=0x%02X",ota_type);
    boost::unique_lock<boost::shared_mutex> lock(m_mux_session);    //写锁
    boost::shared_ptr<ota_session> new_session(new ota_session(status,ota_type,ota_version,total_len));
    m_ota_session = new_session;
    lock.unlock();
    LOG_MSG(MSG_LOG,"Exitt xmboard::add_ota_session()");
    return m_ota_session;
}

boost::shared_ptr<ota_session> xmboard::get_ota_session()
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::del_ota_session()");
    boost::unique_lock<boost::shared_mutex> lock(m_mux_session);    //写锁
    LOG_MSG(MSG_LOG,"exited xmboard::del_ota_session()");
    return m_ota_session;
}

int xmboard::del_ota_session()
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::del_ota_session()");
    boost::unique_lock<boost::shared_mutex> lock(m_mux_session);    //写锁
    m_ota_session.reset();
    LOG_MSG(MSG_LOG,"exited xmboard::del_ota_session()");
    return 0;
}
