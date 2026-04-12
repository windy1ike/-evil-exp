// 登录会话上下文头文件

#ifndef SESSION_H
#define SESSION_H

#include "constants.h"

/*
 * 当前登录会话信息
 * role       - 当前角色
 * patientId  - 患者角色时的患者编号（其他角色为 0）
 * doctorId   - 医护角色时的医生工号（其他角色为 0）
 * deptId     - 医护角色时的所属科室编号（其他角色为 0）
 */
typedef struct {
    UserRole role;
    int patientId;
    int doctorId;
    int deptId;
} CurrentSession;

extern CurrentSession g_session;  // 全局会话实例

/*
 * show_welcome
 * 显示欢迎画面，按 Enter 进入，按 Q 退出程序。
 * 在程序启动时调用一次。
 */
void show_welcome(void);

/*
 * do_login
 * 显示登录界面（筭头选择角色），根据用户输入填充 g_session。
 * 返回值：
 *   1  = 登录成功
 *   0  = 用户选择退出系统（选择 “退出系统” 或 /q）
 *  -1  = EOF
 */
int do_login(void);

#endif /* SESSION_H */
