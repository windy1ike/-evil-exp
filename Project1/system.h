//系统的头文件

#ifndef SYSTEM_H
#define SYSTEM_H

#include "entities.h"        // 包含患者、医生和科室的定义
#include "drug.h"           // 包含药品相关的定义
#include "ward.h"           // 包含病房相关的定义
#include "record.h"         // 包含医疗记录相关的定义

// 定义医院系统结构体
typedef struct HospitalSystem {
    double fund;                // 当前营业额（不含押金）

    Patient* patients;          // 患者列表
    Doctor* doctors;            // 医生列表
    Department* departments;    // 科室列表
    Drug* drugs;                // 药品列表
    DrugStockRecord* drugStocks; // 药品库存记录
    Ward* wards;                // 病房列表
    Bed* beds;                  // 床位列表
    MedicalRecord* records;     // 医疗记录列表

    int todayTotalReg;          // 今日全院挂号数（≤500）
} HospitalSystem;

extern HospitalSystem g_sys;    // 全局医院系统实例

// 初始化医院系统
void init_system();
// 销毁医院系统
void destroy_system();
//命令行主菜单
void system_main_menu();
#endif