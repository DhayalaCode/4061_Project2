// Author: John Kolb <jhkolb@umn.edu>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SWISH_FUNCS_H
#define SWISH_FUNCS_H

#include "job_list.h"
#include "string_vector.h"

/*
 * Task 0
 * Divide a string with substrings separated by a single space (" ")
 * into tokens. These tokens should be stored in the 'tokens' vector using
 * "strvec_add".
 * s: String to tokenize
 * vec: Pointer to vector in which to store tokens. Must be initialized
 *      before this function is called.
 * Returns 0 on success or -1 on error
 */
int tokenize(char *s, strvec_t *tokens);

/*
 * Task 2: Run a user-specified command (including arguments)
 * This should be called within a CHILD process of the shell
 * tokens: Vector containing tokens input by user into shell
 * Doesn't return on success (similar to exec) or returns -1 on error
 * Task 3: Improve this function to perform input/output redirection
 */
int run_command(strvec_t *tokens);

/*
 * Task 5: Resume a stopped (paused) process
 * This can be called from the shell process itself, no need for a fork()
 * tokens: Tokens from the command typed in by the user, e.g., "fg 0"
 * jobs: The list of current jobs for the shell
 * is_foreground: 1 if the job should be resumed in the foreground (Task 5), or
 *                0 if the job should be resumed in the background (Task 6)
 * Returns 0 on success or -1 on error
 */
int resume_job(strvec_t *tokens, job_list_t *jobs, int is_foreground);

/*
 * Task 6: Block the calling shell process until a specific background job
 * stops running (either is stopped or exits).
 * If the job process exits, remove it from the jobs list.
 * tokens: Tokens from the command typed in by the user (e.g., "wait-for 2")
 * Returns 0 on success or -1 on error
 */
int await_background_job(strvec_t *tokens, job_list_t *jobs);

/*
 * Task 6: Block the calling shell process until all background jobs
 * stop running (either stopped or exited)
 * Remove all jobs that exit (are not stopped) from the jobs list
 * Hint: It's easiest to await all background jobs, then remove them from the
 * shell's job list at the end
 * jobs: Pointer to the list of current jobs for the shell
 * Returns 0 on success or -1 on failure
 */
int await_all_background_jobs(job_list_t *jobs);

#endif    // SWISH_FUNCS_H
