// Author: John Kolb <jhkolb@umn.edu>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "job_list.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

void job_list_init(job_list_t *list) {
    list->head = NULL;
    list->length = 0;
}

void job_list_free(job_list_t *list) {
    job_t *current = list->head;
    while (current != NULL) {
        job_t *temp = current;
        current = current->next;
        free(temp);
    }
    list->head = NULL;
    list->length = 0;
}

int job_list_add(job_list_t *list, pid_t pid, const char *name, job_status_t status) {
    if (list->head == NULL) {
        if ((list->head = malloc(sizeof(job_t))) == NULL) {
            return -1;
        }
        strncpy(list->head->name, name, NAME_LEN);
        list->head->name[NAME_LEN - 1] = '\0';
        list->head->status = status;
        list->head->next = NULL;
        list->head->pid = pid;
        list->length = 1;
        return 0;
    }

    job_t *current = list->head;
    while (current->next != NULL) {
        current = current->next;
    }
    if ((current->next = malloc(sizeof(job_t))) == NULL) {
        return -1;
    }
    strncpy(current->next->name, name, NAME_LEN);
    current->next->name[NAME_LEN - 1] = '\0';
    current->next->status = status;
    current->next->next = NULL;
    current->next->pid = pid;
    list->length++;
    return 0;
}

job_t *job_list_get(job_list_t *list, unsigned idx) {
    if (idx >= list->length) {
        return NULL;
    }

    job_t *current = list->head;
    for (int i = 0; i < idx; i++) {
        if (current == NULL) {
            return NULL;
        }
        current = current->next;
    }
    return current;
}

int job_list_remove(job_list_t *list, unsigned idx) {
    if (idx >= list->length) {
        return -1;
    }

    if (idx == 0) {
        job_t *temp = list->head;
        list->head = list->head->next;
        free(temp);
        list->length--;
        return 0;
    }

    job_t *current = list->head;
    for (int i = 0; i < idx - 1; i++) {
        current = current->next;
    }
    job_t *temp = current->next;
    current->next = current->next->next;
    free(temp);
    list->length--;
    return 0;
}

void job_list_remove_by_status(job_list_t *list, job_status_t status) {
    while (list->head != NULL && list->head->status == status) {
        job_t *temp = list->head;
        list->head = list->head->next;
        list->length--;
        free(temp);
    }

    if (list->head != NULL) {    // Could have removed all nodes in loop above
        job_t *current = list->head;
        while (current->next != NULL) {
            if (current->next->status == status) {
                job_t *temp = current->next;
                current->next = current->next->next;
                list->length--;
                free(temp);
            } else {
                current = current->next;
            }
        }
    }
}
