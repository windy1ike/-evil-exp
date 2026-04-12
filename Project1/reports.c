#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>

#include "reports.h"
#include "system.h"
#include "session.h"

/* ======================================================
 * 内部排序辅助
 * ====================================================== */

typedef struct {
    MedicalRecord* rec;
} RecordPtr;

static void sort_by_regtime_asc(RecordPtr* arr, int n) {
    int i, j, minIdx;
    for (i = 0; i < n - 1; ++i) {
        minIdx = i;
        for (j = i + 1; j < n; ++j) {
            if (cmp_time(&arr[j].rec->regInfo.regTime,
                &arr[minIdx].rec->regInfo.regTime) < 0) {
                minIdx = j;
            }
        }
        if (minIdx != i) {
            RecordPtr tmp = arr[i];
            arr[i] = arr[minIdx];
            arr[minIdx] = tmp;
        }
    }
}

static void sort_by_regtime_desc(RecordPtr* arr, int n) {
    int i, j, maxIdx;
    for (i = 0; i < n - 1; ++i) {
        maxIdx = i;
        for (j = i + 1; j < n; ++j) {
            if (cmp_time(&arr[j].rec->regInfo.regTime,
                &arr[maxIdx].rec->regInfo.regTime) > 0) {
                maxIdx = j;
            }
        }
        if (maxIdx != i) {
            RecordPtr tmp = arr[i];
            arr[i] = arr[maxIdx];
            arr[maxIdx] = tmp;
        }
    }
}

static void log_time_line(const char* prefix, const SimpleTime* t) {
    printf("%s%02d-%02d %02d:%02d\n", prefix, t->month, t->day, t->hour, t->minute);
}

/* ======================================================
 * 查询函数（原 query.c）
 * ====================================================== */

void query_records_by_department(int deptId, int ascending) {
    int count = 0;
    MedicalRecord* p;
    RecordPtr* arr;
    int i;

    p = g_sys.records;
    while (p) {
        if (!p->isCancelled && p->regInfo.dept && p->regInfo.dept->id == deptId) {
            count++;
        }
        p = p->next;
    }
    if (count == 0) {
        printf("  [提示] 科室 %d 暂无诊疗记录。\n", deptId);
        return;
    }

    arr = (RecordPtr*)malloc(sizeof(RecordPtr) * count);
    if (!arr) {
        printf("内存不足，无法进行查询。\n");
        return;
    }

    i = 0;
    p = g_sys.records;
    while (p) {
        if (!p->isCancelled && p->regInfo.dept && p->regInfo.dept->id == deptId) {
            arr[i].rec = p;
            i++;
        }
        p = p->next;
    }

    if (ascending) sort_by_regtime_asc(arr, count);
    else           sort_by_regtime_desc(arr, count);

    printf("\n=== [ 科室记录查询 ] 科室 ID: %d  排序: %s ===\n", deptId, ascending ? "时间正序" : "时间倒序");

    for (i = 0; i < count; ++i) {
        MedicalRecord* r = arr[i].rec;
        double fee = 0.0;

        printf("[记录ID] %d\n", r->id);
        printf("挂号单号: %lld\n", r->regInfo.regId);
        log_time_line("挂号时间: ", &r->regInfo.regTime);

        printf("患者编号: %d, 姓名: %s\n",
            r->patient ? r->patient->id : -1,
            r->patient ? r->patient->name : "未知");
        printf("医生工号: %d, 姓名: %s\n",
            r->doctorInfo ? r->doctorInfo->id : -1,
            r->doctorInfo ? r->doctorInfo->name : "未知");

        printf("症状: %s\n", r->diagInfo.symptom);
        printf("诊断: %s\n", r->diagInfo.result);
        printf("建议: %s\n", r->diagInfo.suggestion);

        if (r->dispInfo.hasExam)      fee += r->dispInfo.exam.totalFee;
        if (r->dispInfo.hasDrug)      fee += r->dispInfo.prescription.totalFee;
        if (r->dispInfo.hasHospital)  fee += r->dispInfo.hospitalization.totalFee;

        printf("本次就诊总费: %.2f\n", fee);
        printf("------------------------------------\n");
    }

    free(arr);
}

void query_records_by_doctor(int doctorId, int descending) {
    int count = 0;
    MedicalRecord* p;
    RecordPtr* arr;
    int i;

    p = g_sys.records;
    while (p) {
        if (!p->isCancelled && p->doctorInfo && p->doctorInfo->id == doctorId) {
            count++;
        }
        p = p->next;
    }
    if (count == 0) {
        printf("  [提示] 医生 %d 暂无诊疗记录。\n", doctorId);
        return;
    }

    arr = (RecordPtr*)malloc(sizeof(RecordPtr) * count);
    if (!arr) {
        printf("内存不足，无法进行查询。\n");
        return;
    }

    i = 0;
    p = g_sys.records;
    while (p) {
        if (!p->isCancelled && p->doctorInfo && p->doctorInfo->id == doctorId) {
            arr[i].rec = p;
            i++;
        }
        p = p->next;
    }

    if (descending) sort_by_regtime_desc(arr, count);
    else            sort_by_regtime_asc(arr, count);

    printf("\n=== [ 医生记录查询 ] 医生 ID: %d  排序: %s ===\n", doctorId, descending ? "时间倒序" : "时间正序");

    for (i = 0; i < count; ++i) {
        MedicalRecord* r = arr[i].rec;
        double fee = 0.0;

        printf("[记录ID] %d\n", r->id);
        log_time_line("挂号时间: ", &r->regInfo.regTime);

        printf("患者编号: %d, 姓名: %s\n",
            r->patient ? r->patient->id : -1,
            r->patient ? r->patient->name : "未知");

        if (r->dispInfo.hasExam)      fee += r->dispInfo.exam.totalFee;
        if (r->dispInfo.hasDrug)      fee += r->dispInfo.prescription.totalFee;
        if (r->dispInfo.hasHospital)  fee += r->dispInfo.hospitalization.totalFee;

        printf("本次就诊总费: %.2f\n", fee);
        printf("------------------------------------\n");
    }

    printf("医生 %d 累计接诊量：%d 条记录。\n", doctorId, count);
    free(arr);
}

void query_records_by_patient(int patientId, int ascending) {
    int count = 0;
    MedicalRecord* p;
    RecordPtr* arr;
    int i;

    p = g_sys.records;
    while (p) {
        if (!p->isCancelled && p->patient && p->patient->id == patientId) {
            count++;
        }
        p = p->next;
    }
    if (count == 0) {
        printf("  [提示] 患者 %d 暂无历史诊疗记录。\n", patientId);
        return;
    }

    arr = (RecordPtr*)malloc(sizeof(RecordPtr) * count);
    if (!arr) {
        printf("内存不足，无法进行查询。\n");
        return;
    }

    i = 0;
    p = g_sys.records;
    while (p) {
        if (!p->isCancelled && p->patient && p->patient->id == patientId) {
            arr[i].rec = p;
            i++;
        }
        p = p->next;
    }

    if (ascending) sort_by_regtime_asc(arr, count);
    else           sort_by_regtime_desc(arr, count);

    printf("\n=== [ 患者历史记录 ] 患者 ID: %d  排序: %s ===\n", patientId, ascending ? "时间正序" : "时间倒序");

    for (i = 0; i < count; ++i) {
        MedicalRecord* r = arr[i].rec;
        double fee = 0.0;

        printf("[记录ID] %d\n", r->id);
        log_time_line("挂号时间: ", &r->regInfo.regTime);

        printf("所属科室: %s\n", r->regInfo.dept ? r->regInfo.dept->name : "未知");
        printf("医生工号: %d, 姓名: %s\n",
            r->doctorInfo ? r->doctorInfo->id : -1,
            r->doctorInfo ? r->doctorInfo->name : "未知");

        printf("症状: %s\n", r->diagInfo.symptom);
        printf("诊断: %s\n", r->diagInfo.result);
        printf("建议: %s\n", r->diagInfo.suggestion);

        if (r->dispInfo.hasExam)      fee += r->dispInfo.exam.totalFee;
        if (r->dispInfo.hasDrug)      fee += r->dispInfo.prescription.totalFee;
        if (r->dispInfo.hasHospital)  fee += r->dispInfo.hospitalization.totalFee;

        printf("本次就诊总费: %.2f\n", fee);
        printf("------------------------------------\n");
    }

    free(arr);
}

int query_records_by_timerange(const SimpleTime* start,
    const SimpleTime* end,
    int printDetail) {
    int count = 0;
    double totalFee = 0.0;
    MedicalRecord* p;

    p = g_sys.records;
    while (p) {
        if (!p->isCancelled &&
            cmp_time(&p->regInfo.regTime, start) >= 0 &&
            cmp_time(&p->regInfo.regTime, end) <= 0) {

            double fee = 0.0;
            if (p->dispInfo.hasExam)      fee += p->dispInfo.exam.totalFee;
            if (p->dispInfo.hasDrug)      fee += p->dispInfo.prescription.totalFee;
            if (p->dispInfo.hasHospital)  fee += p->dispInfo.hospitalization.totalFee;

            count++;
            totalFee += fee;

            if (printDetail) {
                printf("[记录ID] %d\n", p->id);
                log_time_line("挂号时间: ", &p->regInfo.regTime);
                printf("患者编号: %d\n", p->patient ? p->patient->id : -1);
                printf("就诊费用: %.2f\n", fee);
                printf("------------------------------------\n");
            }
        }
        p = p->next;
    }

    printf("时间段 %02d-%02d %02d:%02d ~ %02d-%02d %02d:%02d 内：%d 条记录，总费用 %.2f 元\n",
        start->month, start->day, start->hour, start->minute,
        end->month, end->day, end->hour, end->minute,
        count, totalFee);

    return count;
}

/* ======================================================
 * 统计函数（原 stats.c）
 * ====================================================== */

double stat_total_revenue() {
    double total = 0.0;
    MedicalRecord* p = g_sys.records;

    while (p) {
        if (!p->isCancelled) {
            if (p->dispInfo.hasExam)     total += p->dispInfo.exam.totalFee;
            if (p->dispInfo.hasDrug)     total += p->dispInfo.prescription.totalFee;
            if (p->dispInfo.hasHospital) total += p->dispInfo.hospitalization.totalFee;
        }
        p = p->next;
    }

    printf("当前医院总营业额（检查+药品+住院，不含押金）：%.2f 元\n", total);
    return total;
}

void stat_inpatient_report() {
    int count = 0;
    MedicalRecord* p = g_sys.records;

    printf("\n=== [ 在院患者统计 ] ===\n");

    while (p) {
        if (!p->isCancelled &&
            p->dispInfo.hasHospital &&
            p->dispInfo.hospitalization.inHospital) {

            HospitalizationInfo* h = &p->dispInfo.hospitalization;

            printf("[记录ID] %d\n", p->id);
            printf("患者编号: %d, 姓名: %s\n",
                p->patient ? p->patient->id : -1,
                p->patient ? p->patient->name : "未知");

            if (h->bed && h->bed->ward) {
                printf("床位编号: %d, 病房类型: %d, 科室ID: %d\n",
                    h->bed->id,
                    (int)h->bed->ward->type,
                    h->bed->ward->deptId);
            }
            else if (h->bed) {
                printf("床位编号: %d（病房信息不完整）\n", h->bed->id);
            }
            else {
                printf("未绑定床位。\n");
            }

            printf("住院开始: %02d-%02d %02d:%02d\n",
                h->startTime.month, h->startTime.day,
                h->startTime.hour, h->startTime.minute);
            printf("预计出院: %02d-%02d %02d:%02d\n",
                h->expectedLeaveTime.month, h->expectedLeaveTime.day,
                h->expectedLeaveTime.hour, h->expectedLeaveTime.minute);

            printf("押金: %.2f 元\n", h->deposit);
            printf("预计住院天数: %d 天\n", h->plannedDays);
            printf("已产生住院费用: %.2f 元\n", h->totalFee);
            printf("------------------------------------\n");

            count++;
        }
        p = p->next;
    }

    if (count == 0) printf("当前没有正在住院的患者。\n");
    else            printf("共 %d 位在院患者。\n", count);
}

typedef struct {
    Doctor* doctor;
    int totalVisits;
} DoctorStat;

void stat_doctor_workload() {
    int docCount = 0;
    Doctor* d;
    DoctorStat* stats;
    int i, j;
    MedicalRecord* r;

    d = g_sys.doctors;
    while (d) { docCount++; d = d->next; }

    if (docCount == 0) {
        printf("  [提示] 当前暂无医生数据。\n");
        return;
    }

    stats = (DoctorStat*)malloc(sizeof(DoctorStat) * docCount);
    if (!stats) {
        printf("内存不足，无法统计医生工作量。\n");
        return;
    }

    i = 0;
    d = g_sys.doctors;
    while (d) {
        stats[i].doctor = d;
        stats[i].totalVisits = 0;
        i++;
        d = d->next;
    }

    r = g_sys.records;
    while (r) {
        if (!r->isCancelled && r->doctorInfo) {
            for (i = 0; i < docCount; ++i) {
                if (stats[i].doctor->id == r->doctorInfo->id) {
                    stats[i].totalVisits++;
                    break;
                }
            }
        }
        r = r->next;
    }

    /* 降序排序 */
    for (i = 0; i < docCount - 1; ++i) {
        int maxIdx = i;
        for (j = i + 1; j < docCount; ++j) {
            if (stats[j].totalVisits > stats[maxIdx].totalVisits) maxIdx = j;
        }
        if (maxIdx != i) {
            DoctorStat tmp = stats[i];
            stats[i] = stats[maxIdx];
            stats[maxIdx] = tmp;
        }
    }

    printf("\n=== [ 医生接诊量统计 ] 按接诊量降序 ===\n");
    for (i = 0; i < docCount; ++i) {
        printf("工号:%d 姓名:%s 接诊总量:%d\n",
            stats[i].doctor->id,
            stats[i].doctor->name,
            stats[i].totalVisits);
    }

    free(stats);
}

void stat_records_in_timerange(const SimpleTime* start,
    const SimpleTime* end) {
    int count = 0;
    double totalFee = 0.0;
    MedicalRecord* p = g_sys.records;

    while (p) {
        if (!p->isCancelled &&
            cmp_time(&p->regInfo.regTime, start) >= 0 &&
            cmp_time(&p->regInfo.regTime, end) <= 0) {

            double fee = 0.0;
            if (p->dispInfo.hasExam)      fee += p->dispInfo.exam.totalFee;
            if (p->dispInfo.hasDrug)      fee += p->dispInfo.prescription.totalFee;
            if (p->dispInfo.hasHospital)  fee += p->dispInfo.hospitalization.totalFee;

            count++;
            totalFee += fee;
        }
        p = p->next;
    }

    printf("时间段 %02d-%02d %02d:%02d ~ %02d-%02d %02d:%02d：记录 %d 条，总费用 %.2f 元\n",
        start->month, start->day, start->hour, start->minute,
        end->month, end->day, end->hour, end->minute,
        count, totalFee);
}

/* ===== 患者视角报表 ===== */
void report_for_patient_view() {
    if (g_session.role != ROLE_PATIENT) {
        printf("  [提示] 权限不足，仅患者可查看此报表。\n");
        return;
    }
    printf("\n=== [ 我的就诊报表 ] 患者编号：%d ===\n", g_session.patientId);

    int count = 0;
    double totalFee = 0.0;
    MedicalRecord* p = g_sys.records;
    while (p) {
        if (!p->isCancelled && p->patient && p->patient->id == g_session.patientId) {
            double fee = 0.0;
            if (p->dispInfo.hasExam)     fee += p->dispInfo.exam.totalFee;
            if (p->dispInfo.hasDrug)     fee += p->dispInfo.prescription.totalFee;
            if (p->dispInfo.hasHospital) fee += p->dispInfo.hospitalization.totalFee;

            printf("[记录ID] %d  挂号单号: %lld  费用: %.2f 元\n",
                p->id, p->regInfo.regId, fee);
            printf("  科室: %s  医生: %s\n",
                p->regInfo.dept ? p->regInfo.dept->name : "未知",
                p->doctorInfo ? p->doctorInfo->name : "未知");
            printf("  诊断: %s\n", p->diagInfo.result);
            printf("------------------------------------\n");

            totalFee += fee;
            count++;
        }
        p = p->next;
    }
    if (count == 0) printf("  [提示] 暂无就诊记录。\n");
    else            printf("共 %d 条记录，累计费用 %.2f 元\n", count, totalFee);
}

/* ===== 医护视角报表 ===== */
void report_for_doctor_view() {
    if (g_session.role != ROLE_DOCTOR) {
        printf("  [提示] 权限不足，仅医护人员可查看此报表。\n");
        return;
    }
    printf("\n=== [ 医护工作报表 ] 工号：%d  科室 ID：%d ===\n",
        g_session.doctorId, g_session.deptId);

    /* 本科室医生接诊统计 */
    printf("--- 本科室医生接诊统计 ---\n");
    Doctor* d = g_sys.doctors;
    while (d) {
        if (d->dept && d->dept->id == g_session.deptId) {
            int visits = 0;
            MedicalRecord* r = g_sys.records;
            while (r) {
                if (!r->isCancelled && r->doctorInfo && r->doctorInfo->id == d->id)
                    visits++;
                r = r->next;
            }
            printf("工号:%d 姓名:%s 接诊量:%d\n", d->id, d->name, visits);
        }
        d = d->next;
    }

    /* 本科室诊疗记录总数与费用 */
    int count = 0;
    double totalFee = 0.0;
    MedicalRecord* p = g_sys.records;
    while (p) {
        if (!p->isCancelled && p->regInfo.dept &&
            p->regInfo.dept->id == g_session.deptId) {
            double fee = 0.0;
            if (p->dispInfo.hasExam)     fee += p->dispInfo.exam.totalFee;
            if (p->dispInfo.hasDrug)     fee += p->dispInfo.prescription.totalFee;
            if (p->dispInfo.hasHospital) fee += p->dispInfo.hospitalization.totalFee;
            totalFee += fee;
            count++;
        }
        p = p->next;
    }
    printf("--- 本科室诊疗汇总：%d 条记录，总费用 %.2f 元 ---\n", count, totalFee);
}

/* ===== 管理员视角报表 ===== */
void report_for_admin_view() {
    if (g_session.role != ROLE_ADMIN) {
        printf("  [提示] 权限不足，仅管理员可查看全院报表。\n");
        return;
    }
    printf("\n=== [ 全院综合报表 ] ===\n");
    stat_total_revenue();
    stat_inpatient_report();
    stat_doctor_workload();
}
