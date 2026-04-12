//校验相关函数的声明

#ifndef VALIDATE_H
#define VALIDATE_H

#include "constants.h"  // 常量定义
#include "time_util.h"  // 时间工具函数
#include "entities.h"   // 实体定义
#include "drug.h"       // 药品相关定义
#include "ward.h"       // 床位相关定义
#include "record.h"     // 记录相关定义
#include "system.h"     // 系统相关定义

// 年龄、金额、数量、字符串、押金、容量等校验函数的声明
int validate_age(int age);  // 校验年龄
int validate_money(double m);  // 校验金额
int validate_string(const char* s);  // 校验字符串
int validate_deposit(double deposit,const SimpleTime* start,const SimpleTime* expectedEnd); // 校验押金
int validate_drug_count_per_visit(int drugCount);  // 校验每次就诊药品数量

// 检查唯一性：患者编号 / 医生工号 / 药品编号 / 床位编号 / 挂号单号
int is_unique_patient_id(int id);  // 检查患者编号唯一性
int is_unique_doctor_id(int id);  // 检查医生工号唯一性
int is_unique_drug_id(int id);  // 检查药品编号唯一性
int is_unique_bed_id(int id);  // 检查床位编号唯一性
int is_unique_reg_id(long long regId);  // 检查挂号单号唯一性

// 挂号规则相关校验：每日总数 / 每医生每日20个 / 每患者最多5个 / 同科室1个
int check_registration_rules(Patient* p, Doctor* d, Department* dept, const SimpleTime* regTime);
int validate_box_per_drug(int boxCount);

#endif