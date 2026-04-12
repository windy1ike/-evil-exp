#ifndef QUERY_H
#define QUERY_H

#include "system.h"
#include "time_util.h"
#include "constants.h"

/* 按科室查询诊疗信息
 * 参数：
 *   deptId     科室编号
 *   ascending   1: 升序；0: 降序
 */
void query_records_by_department(int deptId, int ascending);

/* 按医生工号查询诊疗信息
 * 参数：
 *   doctorId   医生工号
 *   descending  1: 降序；0: 升序
 */
void query_records_by_doctor(int doctorId, int descending);

/* 按患者编号查询历史诊疗信息
 * 参数：
 *   patientId  患者编号
 *   ascending   1: 升序；0: 降序
 */
void query_records_by_patient(int patientId, int ascending);

/* 按时间段查询诊疗记录
 * 参数：
 *   start, end  起止时间
 *   printDetail  1: 详细记录；0: 统计数量
 * 返回：
 *   匹配记录数量
 */
int query_records_by_timerange(const SimpleTime* start,
    const SimpleTime* end,
    int printDetail);

#endif