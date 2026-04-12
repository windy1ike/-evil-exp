#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "entities.h"

// 创建科室
Department* create_department(int id, const char* name) {
    Department* dept = (Department*)malloc(sizeof(Department)); 
    if (!dept) return NULL; 
    dept->id = id; 
    strncpy(dept->name, name, MAX_NAME_LEN); 
    dept->name[MAX_NAME_LEN - 1] = '\0'; 
    dept->next = NULL; 
    return dept; 
}

// 添加科室到链表
void add_department(Department** head, Department* dept) {
    if (!dept) return; 
    dept->next = *head; 
    *head = dept; 
}

// 根据ID查找科室
Department* find_department_by_id(Department* head, int id) {
    Department* p = head; 
    while (p) {
        if (p->id == id) return p; 
        p = p->next; 
    }
    return NULL; 
}

// 释放科室链表内存
void free_departments(Department* head) {
    Department* p = head; 
    while (p) {
        Department* next = p->next; 
        free(p); 
        p = next; 
    }
}

// 创建患者
Patient* create_patient(int id, const char* name, int age, Gender g) {
    Patient* p = (Patient*)malloc(sizeof(Patient)); 
    if (!p) return NULL; 
    p->id = id; 
    strncpy(p->name, name, MAX_NAME_LEN); 
    p->name[MAX_NAME_LEN - 1] = '\0'; 
    p->age = age; 
    p->gender = g; 
    p->recordCount = 0; 
    p->next = NULL; 
    return p; 
}

// 添加患者到链表
void add_patient(Patient** head, Patient* p) {
    if (!p) return; 
    p->next = *head; 
    *head = p; 
}

// 根据ID查找患者
Patient* find_patient_by_id(Patient* head, int id) {
    Patient* p = head; 
    while (p) {
        if (p->id == id) return p; 
        p = p->next; 
    }
    return NULL; 
}

// 释放患者链表内存
void free_patients(Patient* head) {
    Patient* p = head; 
    while (p) {
        Patient* next = p->next; 
        free(p); 
        p = next; 
    }
}

// 创建医生
Doctor* create_doctor(int id, const char* name, DoctorTitle title, Department* dept) {
    Doctor* d = (Doctor*)malloc(sizeof(Doctor)); 
    if (!d) return NULL; 
    d->id = id; 
    strncpy(d->name, name, MAX_NAME_LEN); 
    d->name[MAX_NAME_LEN - 1] = '\0'; 
    d->title = title; 
    d->dept = dept; 
    d->todayRegCount = 0; 
    d->next = NULL; 
    return d; 
}

// 添加医生到链表
void add_doctor(Doctor** head, Doctor* d) {
    if (!d) return; 
    d->next = *head; 
    *head = d; 
}

// 根据ID查找医生
Doctor* find_doctor_by_id(Doctor* head, int id) {
    Doctor* p = head; 
    while (p) {
        if (p->id == id) return p; 
        p = p->next; 
    }
    return NULL; 
}

// 释放医生链表内存
void free_doctors(Doctor* head) {
    Doctor* p = head; 
    while (p) {
        Doctor* next = p->next; 
        free(p); 
        p = next; 
    }
}
