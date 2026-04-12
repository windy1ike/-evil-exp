#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "drug.h"

// 创建药品
Drug* create_drug(int id, const char* name, double price, int deptId, int stock) {
    Drug* d = (Drug*)malloc(sizeof(Drug));
    if (!d) return NULL;
    d->id = id;
    strncpy(d->name, name, MAX_NAME_LEN);
    d->name[MAX_NAME_LEN - 1] = '\0';
    d->price = price;
    d->deptId = deptId;
    d->stock = stock;
    d->next = NULL;
    return d;
}

// 添加药品到链表
void add_drug(Drug** head, Drug* d) {
    if (!d) return;
    d->next = *head;
    *head = d;
}

// 根据ID查找药品
Drug* find_drug_by_id(Drug* head, int id) {
    Drug* p = head;
    while (p) {
        if (p->id == id) return p;
        p = p->next;
    }
    return NULL;
}

// 释放药品链表的内存
void free_drugs(Drug* head) {
    Drug* p = head;
    while (p) {
        Drug* next = p->next;
        free(p);
        p = next;
    }
}

// 创建药品库存记录
DrugStockRecord* create_drug_stock_record(int id, int drugId, int quantity, int type, SimpleTime time) {
    DrugStockRecord* rec = (DrugStockRecord*)malloc(sizeof(DrugStockRecord));
    if (!rec) return NULL;
    rec->id = id;
    rec->drugId = drugId;
    rec->quantity = quantity;
    rec->type = type;
    rec->time = time;
    rec->next = NULL;
    return rec;
}

// 添加库存记录到链表
void add_drug_stock_record(DrugStockRecord** head, DrugStockRecord* rec) {
    if (!rec) return;
    rec->next = *head;
    *head = rec;
}

// 释放库存记录链表的内存
void free_drug_stock_records(DrugStockRecord* head) {
    DrugStockRecord* p = head;
    while (p) {
        DrugStockRecord* next = p->next;
        free(p);
        p = next;
    }
}

