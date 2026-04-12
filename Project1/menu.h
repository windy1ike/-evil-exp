#ifndef MENU_H
#define MENU_H

/*
 * 命令行菜单模块：
 * 适合在没有 GUI 时调试业务逻辑
 * main.c 中可以选择：
 *   main_menu();   // 命令行版
 * 或
 *   run_gui();     // EasyX 图形版
 */

void main_menu();          // 主菜单

// 子菜单
void record_menu();        // 诊疗记录管理菜单（增加/修改/删除/导入）
void query_menu();         // 查询菜单（按科室/医生/患者/时间段）
void stats_menu();         // 统计菜单（营业额/医生工作量/住院报表）
void storage_menu();       // 数据存储/载入菜单
void base_data_menu();     // 基础数据维护菜单（患者/医生/科室/药品/床位）

#endif