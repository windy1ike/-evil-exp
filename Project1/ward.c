#include <stdio.h>
#include <stdlib.h>
#include "ward.h"

// 创建病房
Ward* create_ward(int id, RoomType type, int deptId) {
    Ward* w = (Ward*)malloc(sizeof(Ward));
    if (!w) return NULL;
    w->id = id;
    w->type = type;
    w->deptId = deptId;
    w->next = NULL;
    return w;
}

// 添加病房到链表
void add_ward(Ward** head, Ward* w) {
    if (!w) return;
    w->next = *head;
    *head = w;
}

// 创建床位
Bed* create_bed(int id, Ward* ward, BedStatus status) {
    Bed* b = (Bed*)malloc(sizeof(Bed));
    if (!b) return NULL;
    b->id = id;
    b->ward = ward;
    b->status = status;
    b->next = NULL;
    return b;
}

// 添加床位到链表
void add_bed(Bed** head, Bed* b) {
    if (!b) return;
    b->next = *head;
    *head = b;
}

// 分配空闲床位
Bed* allocate_free_bed(Bed* beds, int deptId) {
    Bed* p = beds;
    while (p) {
        if (p->status == BED_FREE && p->ward && p->ward->deptId == deptId) {
            p->status = BED_OCCUPIED;
            return p;
        }
        p = p->next;
    }
    return NULL; // 无空闲床位
}

// 释放床位
void release_bed(Bed* bed) {
    if (!bed) return;
    if (bed->status == BED_OCCUPIED) {
        bed->status = BED_FREE;
    }
}

// 释放病房链表
void free_wards(Ward* head) {
    Ward* p = head;
    while (p) {
        Ward* next = p->next;
        free(p);
        p = next;
    }
}

// 释放床位链表
void free_beds(Bed* head) {
    Bed* p = head;
    while (p) {
        Bed* next = p->next;
        free(p);
        p = next;
    }
}
