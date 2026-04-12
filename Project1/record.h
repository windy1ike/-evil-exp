#ifndef RECORD_H
#define RECORD_H

#include "constants.h"
#include "entities.h"
#include "drug.h"
#include "ward.h"
#include "time_util.h"

// 注册信息结构体
typedef struct RegistrationInfo {
    long long regId;            // 挂号单号（按规则编码）
    SimpleTime regTime;         // 注册时间
    Department* dept;           // 所属科室指针
    Doctor* doctor;             // 医生指针
    int seqNoOfDay;             // 当日序号
} RegistrationInfo;

typedef struct DiagnosisInfo {
    char symptom[MAX_STR_LEN];
    char result[MAX_STR_LEN];
    char suggestion[MAX_STR_LEN];
} DiagnosisInfo;

// 检查
typedef struct ExamItem {
    char name[MAX_NAME_LEN];
    double fee;
    struct ExamItem* next;
} ExamItem;

typedef struct ExamInfo {
    ExamItem* items;
    double totalFee;
} ExamInfo;

// 开药
typedef struct DrugItem {
    int drugId;
    int quantity;           // 盒数 ≤ 100
    double unitPrice;
    struct DrugItem* next;
} DrugItem;

typedef struct PrescriptionInfo {
    DrugItem* items;
    double totalFee;
} PrescriptionInfo;

// 住院
typedef struct HospitalizationInfo {
    SimpleTime startTime;
    SimpleTime expectedLeaveTime;
    SimpleTime actualLeaveTime;   // 未出院时可 month=0 表示
    double deposit;
    int plannedDays;
    Bed* bed;
    double totalFee;              // 按实际天数 * 200
    int inHospital;               // 1=仍在住院, 0=已出院
} HospitalizationInfo;

typedef struct DisposalInfo {
    int hasExam;
    int hasDrug;
    int hasHospital;
    ExamInfo exam;
    PrescriptionInfo prescription;
    HospitalizationInfo hospitalization;
} DisposalInfo;

typedef struct MedicalRecord {
    int id;                     // 诊疗记录编号
    Patient* patient;
    RegistrationInfo regInfo;
    Doctor* doctorInfo;
    DiagnosisInfo diagInfo;
    DisposalInfo dispInfo;
    int isCancelled;            // 已撤销标记
    struct MedicalRecord* next;
} MedicalRecord;

// 链表操作
MedicalRecord* create_record();
void add_record(MedicalRecord** head, MedicalRecord* rec);
MedicalRecord* find_record_by_id(MedicalRecord* head, int id);
void cancel_record(MedicalRecord* rec);
void delete_record(MedicalRecord** head, int id);
void free_records(MedicalRecord* head);

#endif