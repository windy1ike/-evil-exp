// 报表层聚合头文件：查询 + 统计函数
// 使用方只需 #include "reports.h"

#ifndef REPORTS_H
#define REPORTS_H

#include "query.h"   // query_records_by_department/doctor/patient/timerange
#include "stats.h"   // stat_total_revenue, stat_inpatient_report, stat_doctor_workload,
                     // stat_records_in_timerange, report_for_*_view

#endif /* REPORTS_H */
