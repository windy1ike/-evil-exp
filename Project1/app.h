// 应用层聚合头文件：系统入口、菜单、会话
// 使用方只需 #include "app.h"

#ifndef APP_H
#define APP_H

#include "system.h"   // HospitalSystem, g_sys, init_system, destroy_system, system_main_menu
#include "menu.h"     // main_menu, record_menu, query_menu, stats_menu, storage_menu, base_data_menu
#include "session.h"  // CurrentSession, g_session, do_login

#endif /* APP_H */
