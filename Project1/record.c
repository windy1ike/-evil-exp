#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "record.h"

// 创建医疗记录
MedicalRecord* create_record() {
    MedicalRecord* r = (MedicalRecord*)malloc(sizeof(MedicalRecord));
    if (!r) return NULL;
    memset(r, 0, sizeof(MedicalRecord));
    r->id = 0;              // ID由调用者分配
    r->patient = NULL;
    r->doctorInfo = NULL;
    r->regInfo.dept = NULL;
    r->regInfo.doctor = NULL;
    r->isCancelled = 0;
    r->next = NULL;
    return r;
}

// 添加医疗记录到链表头
void add_record(MedicalRecord** head, MedicalRecord* rec) {
    if (!rec) return;
    rec->next = *head;
    *head = rec;
}

// 根据ID查找医疗记录
MedicalRecord* find_record_by_id(MedicalRecord* head, int id) {
    MedicalRecord* p = head;
    while (p) {
        if (p->id == id) return p;
        p = p->next;
    }
    return NULL;
}

// 取消医疗记录
void cancel_record(MedicalRecord* rec) {
    if (!rec) return;
    rec->isCancelled = 1;
}

// 删除医疗记录
void delete_record(MedicalRecord** head, int id) {
    if (!head || !*head) return;
    MedicalRecord* p = *head;
    MedicalRecord* prev = NULL;
    while (p) {
        if (p->id == id) {
            if (prev) prev->next = p->next;
            else      *head = p->next;

            // TODO: 释放检查/开药子链表
            free(p);
            return;
        }
        prev = p;
        p = p->next;
    }
}

// 释放检查项目链表
static void free_exam_items(ExamItem* head) {
    ExamItem* p = head;
    while (p) {
        ExamItem* next = p->next;
        free(p);
        p = next;
    }
}

// 释放药品项目链表
static void free_drug_items(DrugItem* head) {
    DrugItem* p = head;
    while (p) {
        DrugItem* next = p->next;
        free(p);
        p = next;
    }
}

// 释放所有医疗记录
void free_records(MedicalRecord* head) {
    MedicalRecord* p = head;
    while (p) {
        MedicalRecord* next = p->next;

        // 释放检查/开药项目链表
        if (p->dispInfo.hasExam) {
            free_exam_items(p->dispInfo.exam.items);
        }
        if (p->dispInfo.hasDrug) {
            free_drug_items(p->dispInfo.prescription.items);
        }

        free(p);
        p = next;
    }
}
