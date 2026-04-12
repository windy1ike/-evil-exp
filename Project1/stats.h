//统计相关函数声明头文件

#ifndef STATS_H
#define STATS_H

#include "system.h"

//营业额统计函数声明
double stat_total_revenue();

//住院患者报表函数声明
void stat_inpatient_report();

//医生出诊情况函数声明
void stat_doctor_workload();

//时间段统计函数声明
void stat_records_in_timerange(const SimpleTime* start,const SimpleTime* end);

//不同权限视角的报表函数声明
void report_for_doctor_view();  // 医生视角报表
void report_for_admin_view();   // 管理员视角报表
void report_for_patient_view();  // 患者视角报表

#endif // STATS_H