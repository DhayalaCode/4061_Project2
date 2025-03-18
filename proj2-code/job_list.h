// Author: John Kolb <jhkolb@umn.edu>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef JOB_LIST_H
#define JOB_LIST_H

#include <stdlib.h>
#include <sys/types.h>

#define NAME_LEN 32

typedef enum {
    STOPPED,
    BACKGROUND,
} job_status_t;

typedef struct job {
    char name[NAME_LEN];
    int status;
    pid_t pid;
    struct job *next;
} job_t;

typedef struct {
    job_t *head;
    unsigned length;
} job_list_t;

/*
 * Initialize a new, empty jobs list
 * list: Pointer to the jobs list to initialize
 */
void job_list_init(job_list_t *list);

/*
 * Removes all entries from a jobs list
 * The underlying memory for the entries is also freed
 * list: Pointer to the job list to clear
 */
void job_list_free(job_list_t *list);

/*
 * Add a new job to a jobs list
 * list: The jobs list to add to
 * pid: The process ID of the job's underlying process (spawned from the shell)
 * name: The name of the job's program (e.g., "ls", "cat", or "wc")
 * status: The job's current status
 * Returns 0 on success or -1 on error
 */
int job_list_add(job_list_t *list, pid_t pid, const char *name, job_status_t status);

/*
 * Retrieve an element from a jobs list
 * list: Pointer to the jobs list to retrieve from
 * idx: Index of the entry to retrieve
 * Returns a pointer to a job_t (not a copy) on success or NULL on error
 */
job_t *job_list_get(job_list_t *list, unsigned idx);

/*
 * Removes an element at a specific index from a jobs list
 * The memory for this element is freed
 * list: Pointer to the jobs list to remove from
 * idx: Index of the element to remove
 * Returns 0 on success or -1 on error
 */
int job_list_remove(job_list_t *list, unsigned idx);

/*
 * Remove all jobs of a specific status (STOPPED or BACKGROUND) from a jobs list
 * The memory for all entries removed from the list is freed
 * list: The jobs list to remove from
 * status: The status of all jobs that should be removed (BACKGROUND or STOPPED)
 */
void job_list_remove_by_status(job_list_t *list, job_status_t status);

#endif    // JOB_LIST_H
