#ifndef _IPAPPLY_H_
#define _IPAPPLY_H_

#if defined(_WINDOWS)
#define LIBIPAPPLY_API __declspec(dllexport)
#else
#define LIBIPAPPLY_API __attribute__((visibility("default")))
#define __stdcall
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define LIA_ERR_NONE        0       // 无错误(成功)
#define LIA_ERR_FAIL        -1      // 失败
#define LIA_ERR_PARAM       -1001   // 参数错误

/*获取 sync_ip
*参数:
*   val_buf:sync_ip存储内存, 由调用者申请
*   buf_len:存储内存大小
*返回值:
*   (=0): 成功;
*   -1   LIA_ERR_FAIL    失败
*   -1001 LIA_ERR_PARAM   参数错误
*   其他: 未知错误*/
LIBIPAPPLY_API int lia_get_sync_ip(char* val_buf, int buf_len);

/*获取 fan_monitor_ip
*参数:
*   fan_slot_id:风扇监控板id
*   val_buf:fan_monitor_ip存储内存, 由调用者申请
*   buf_len:存储内存大小
*返回值:
*   (=0): 成功;
*   -1   LIA_ERR_FAIL    失败
*   -1001 LIA_ERR_PARAM   参数错误
*   其他: 未知错误*/
LIBIPAPPLY_API int lia_get_fan_monitor_ip(int fan_slot_id, char* val_buf, int buf_len);

/*
* function:获取某一块单板的ip地址
* param:
*   slot_type:  单板类型
*   slot_id:    单板槽位号
*   cmd:        命令类型
*   val_buf:    所有ip列表缓存, 由调用者申请
*   buf_len:    存储内存大小
* return:
*   (=0)    LIA_ERR_NONE    成功
*   -1      LIA_ERR_FAIL    失败
*   -1001   LIA_ERR_PARAM   参数错误
*   其他: 未知错误
*/
LIBIPAPPLY_API int lia_get_single_board_ip(int slot_type, int slot_id, int cmd, char* val_buf, int buf_len);

/*
* function:获取相同单板的所有ip地址
* param:
*   slot_type:  获取对应单板类型的ip
*   cmd:        命令类型
*   val_buf:    所有ip列表缓存, 由调用者申请
*   buf_len:    存储内存大小
*   ipcnt:      获取ip数量
* return:
*   (=0)    LIA_ERR_NONE    成功
*   -1      LIA_ERR_FAIL    失败
*   -1001   LIA_ERR_PARAM   参数错误
*   其他: 未知错误
*/
LIBIPAPPLY_API int lia_get_all_same_board_iplist(int slot_type, int cmd, char* val_buf, int buf_len, int *ipcnt);

#ifdef __cplusplus
}
#endif

#endif // _IPAPPLY_H_