// 定义科室、患者和医生的结构体及相关操作函数

#ifndef ENTITIES_H
#define ENTITIES_H

#include "constants.h"
#include "time_util.h"

// 科室结构体
typedef struct Department {
    int id;                         // 科室编号
    char name[MAX_NAME_LEN];        // 科室名称
    struct Department* next;        // 指向下一个科室
} Department;

// 患者结构体
typedef struct Patient {
    int id;                         // 患者编号（唯一）
    char name[MAX_NAME_LEN];        // 患者姓名
    int age;                        // 年龄（正整数）
    Gender gender;                 // 性别
    int recordCount;                // 诊疗记录数（≤30）
    struct Patient* next;          // 指向下一个患者
} Patient;

// 医生结构体
typedef struct Doctor {
    int id;                         // 工号（唯一）
    char name[MAX_NAME_LEN];        // 医生姓名
    DoctorTitle title;              // 医生职称
    Department* dept;               // 所属科室
    int todayRegCount;              // 今日挂号数（≤20）
    struct Doctor* next;            // 指向下一个医生
} Doctor;

// 创建科室
Department* create_department(int id, const char* name);
// 添加科室到链表
void add_department(Department** head, Department* dept);
// 根据科室编号查找科室
Department* find_department_by_id(Department* head, int id);

// 创建患者
Patient* create_patient(int id, const char* name, int age, Gender g);
// 添加患者到链表
void add_patient(Patient** head, Patient* p);
// 根据患者编号查找患者
Patient* find_patient_by_id(Patient* head, int id);

// 创建医生
Doctor* create_doctor(int id, const char* name, DoctorTitle title, Department* dept);
// 添加医生到链表
void add_doctor(Doctor** head, Doctor* d);
// 根据工号查找医生
Doctor* find_doctor_by_id(Doctor* head, int id);

// 释放科室链表
void free_departments(Department* head);
// 释放患者链表
void free_patients(Patient* head);
// 释放医生链表
void free_doctors(Doctor* head);

#endif
