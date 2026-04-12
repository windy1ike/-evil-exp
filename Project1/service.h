//查询和统计功能的头文件
#ifndef SERVICE_H
#define SERVICE_H

#include "system.h"
#include "validate.h"

// 服务接口声明

// 增加诊疗记录（手动录入）
int svc_add_record_manual();

// 批量导入诊疗记录（从文件）
int svc_import_records_from_file(const char* filename);

// 修改诊疗记录：标记撤销 + 新增一条
int svc_modify_record(int recordId);

// 删除诊疗记录（包括清理床位等）
int svc_delete_record(int recordId);

// 科室诊疗信息查询
void svc_query_by_department(int deptId, UserRole role, int currentPatientId, int currentDoctorId);

// 医生诊疗信息查询
void svc_query_by_doctor(int doctorId, UserRole role, int currentPatientId, int currentDoctorId);

// 患者历史诊疗查询
void svc_query_by_patient(int patientId, UserRole role, int currentPatientId, int currentDoctorId);

// 运营数据统计
void svc_stat_revenue();
void svc_report_inpatients();

// 医生工作统计
void svc_stat_doctor_workload();

// 时间段诊疗统计
void svc_stat_by_time_range(SimpleTime start, SimpleTime end);

// 多权限报表
void svc_report_for_role(UserRole role, int currentPatientId, int currentDoctorId);

// 数据持久化
void svc_save_all();
void svc_load_all();

#endif