#define _GNU_SOURCE

#include "swish_funcs.h"

#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "job_list.h"
#include "string_vector.h"

#define MAX_ARGS 10

/**
 * tokenize - Breaks input string 's' into tokens (words separated by spaces)
 * and adds each token to the string vector 'tokens'.
 *
 * Returns 0 on success, or -1 if input is invalid or adding a token fails.
 */
int tokenize(char *s, strvec_t *tokens) {
    if (!s || !tokens) {
        return -1;
    }

    // Use strtok to split the input string by a single space.
    char *token = strtok(s, " ");
    while (token != NULL) {
        // Add the token to the vector; return -1 on error.
        if (strvec_add(tokens, token) == -1) {
            perror("Error: strvec_add");
            return -1;
        }
        token = strtok(NULL, " ");
    }
    return 0;
}

/**
 * run_command - Executes the command represented by the tokens vector.
 * This function handles input/output redirection, restores default signal
 * handlers, sets the process group, builds the argument array for execvp(),
 * and then calls execvp() to run the external command.
 *
 * Returns 0 if execvp() succeeds (which it never does on success) or -1 if an error occurs.
 */
int run_command(strvec_t *tokens) {
    // Ensure there is at least one token (the command name).
    if (tokens->length == 0) {
        fprintf(stderr, "Error: No command to execute.\n");
        return -1;
    }

    // Prepare an argument array (overestimate size is fine).
    char *args[tokens->length + 1];
    int in_index = -1, out_index = -1, append_index = -1;

    // Locate redirection operators within tokens.
    in_index = strvec_find(tokens, "<");
    out_index = strvec_find(tokens, ">");
    append_index = strvec_find(tokens, ">>");

    // --- Handle Input Redirection ---
    if (in_index != -1) {
        if (in_index + 1 >= tokens->length) {
            perror("Error: No input file specified.\n");
            return -1;
        }
        int fd = open(tokens->data[in_index + 1], O_RDONLY);
        if (fd < 0) {
            perror("Failed to open input file");
            return -1;
        }
        if (dup2(fd, STDIN_FILENO) < 0) {
            perror("dup2 failed for input redirection");
            close(fd);
            return -1;
        }
        close(fd);
    }

    // --- Handle Output Redirection ---
    if (out_index != -1 || append_index != -1) {
        int fd_out;
        if (out_index != -1) {
            // '>' operator: open file for writing, create if it doesn't exist, and truncate.
            if (out_index + 1 >= tokens->length) {
                perror("Error: No output file specified.\n");
                return -1;
            }
            fd_out =
                open(tokens->data[out_index + 1], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        } else {
            // ">>" operator: open file for writing, create if it doesn't exist, and append.
            if (append_index + 1 >= tokens->length) {
                perror("Error: No output file specified.\n");
                return -1;
            }
            fd_out = open(tokens->data[append_index + 1], O_WRONLY | O_CREAT | O_APPEND,
                          S_IRUSR | S_IWUSR);
        }
        if (fd_out < 0) {
            perror("Failed to open output file");
            return -1;
        }
        if (dup2(fd_out, STDOUT_FILENO) < 0) {
            perror("dup2 failed for output redirection");
            close(fd_out);
            return -1;
        }
        close(fd_out);
    }

    // --- Build Argument List for execvp() ---
    // Skip any tokens that are redirection operators and their arguments.
    int j = 0;
    for (int i = 0; i < tokens->length; i++) {
        if ((in_index != -1 && (i == in_index || i == in_index + 1)) ||
            (out_index != -1 && (i == out_index || i == out_index + 1)) ||
            (append_index != -1 && (i == append_index || i == append_index + 1))) {
            continue;
        }
        args[j++] = tokens->data[i];
    }
    args[j] = NULL;    // Null-terminate the array.

    // --- Restore Default Signal Handlers for SIGTTIN and SIGTTOU ---
    struct sigaction dft_sig_action;
    dft_sig_action.sa_handler = SIG_DFL;
    if (sigfillset(&dft_sig_action.sa_mask) == -1) {
        perror("sigfillset");
        return -1;
    }
    dft_sig_action.sa_flags = 0;
    if (sigaction(SIGTTIN, &dft_sig_action, NULL) == -1 ||
        sigaction(SIGTTOU, &dft_sig_action, NULL) == -1) {
        perror("sigaction");
        return -1;
    }

    // --- Set Process Group ---
    // Make the child process its own process group leader.
    pid_t pid = getpid();
    if (setpgid(pid, pid) == -1) {
        perror("setpgid");
        return -1;
    }

    // --- Execute the Command ---
    if (execvp(args[0], args) < 0) {
        perror("exec");
        _exit(1);    // Use _exit() in the child to avoid flushing parent's buffers.
    }

    return 0;    // Not reached if execvp() succeeds.
}

/**
 * resume_job - Resume a job that has been stopped.
 * If is_foreground is nonzero, resume the job in the foreground (and wait for it).
 * Otherwise, resume the job in the background.
 *
 * tokens: token vector containing the command (e.g., "fg 0" or "bg 0").
 * jobs: the job list structure.
 *
 * Returns 0 on success, or -1 if an error occurs.
 */
int resume_job(strvec_t *tokens, job_list_t *jobs, int is_foreground) {
    if (tokens->length < 2) {
        fprintf(stderr, "Usage: fg/bg <job_number>\n");
        return -1;
    }
    int job_index;
    if (sscanf(tokens->data[1], "%d", &job_index) != 1) {
        fprintf(stderr, "Invalid job number\n");
        return -1;
    }
    job_t *job = job_list_get(jobs, job_index);
    if (job == NULL) {
        fprintf(stderr, "Job index out of bounds\n");
        return -1;
    }
    // Save the shell's process group ID.
    pid_t shell_pgid = getpgid(0);
    if (is_foreground) {
        // Bring the job's process group to the foreground.
        if (tcsetpgrp(STDIN_FILENO, job->pid) == -1) {
            perror("tcsetpgrp");
            return -1;
        }
    }
    // Send SIGCONT to the entire process group of the job.
    if (kill(-job->pid, SIGCONT) == -1) {
        perror("kill");
        return -1;
    }
    if (is_foreground) {
        int status;
        // Wait for the job to either terminate or stop.
        if (waitpid(job->pid, &status, WUNTRACED) < 0) {
            perror("waitpid");
            return -1;
        }
        // If the job terminated, remove it from the job list.
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            job_list_remove(jobs, job_index);
        } else if (WIFSTOPPED(status)) {
            // If the job stopped, update its status to a STOPPED constant.
            job->status = STOPPED;
        }
        // Restore the shell's process group to the foreground.
        if (tcsetpgrp(STDIN_FILENO, shell_pgid) == -1) {
            perror("tcsetpgrp");
            return -1;
        }
    } else {
        // For background resumption, simply mark the job as BACKGROUND.
        job->status = BACKGROUND;
    }
    return 0;
}

/**
 * await_background_job - Wait for a specific background job to terminate or stop.
 *
 * tokens: token vector containing the command (e.g., "wait-for 0").
 * jobs: the job list structure.
 *
 * Returns 0 on success, or -1 if an error occurs.
 */
int await_background_job(strvec_t *tokens, job_list_t *jobs) {
    if (tokens->length < 2) {
        fprintf(stderr, "Usage: wait-for <job_number>\n");
        return -1;
    }
    int job_index;
    if (sscanf(tokens->data[1], "%d", &job_index) != 1) {
        fprintf(stderr, "Invalid job number\n");
        return -1;
    }
    job_t *job = job_list_get(jobs, job_index);
    if (job == NULL) {
        fprintf(stderr, "Job index out of bounds\n");
        return -1;
    }
    // Ensure the job is a background job.
    if (job->status != BACKGROUND) {
        fprintf(stderr, "Job index is for stopped process not background process\n");
        return -1;
    }
    int status;
    // Wait for the job's process to change state.
    if (waitpid(job->pid, &status, WUNTRACED) < 0) {
        perror("waitpid");
        return -1;
    }
    // If the job terminated, remove it from the job list.
    if (WIFEXITED(status) || WIFSIGNALED(status)) {
        job_list_remove(jobs, job_index);
    }
    return 0;
}

/**
 * await_all_background_jobs - Wait for all background jobs to either terminate or stop.
 * First, iterate through the job list and update the status of each background job.
 * Then, remove all jobs that remain in the BACKGROUND state (indicating termination).
 *
 * Returns 0 on success, or -1 if an error occurs.
 */
int await_all_background_jobs(job_list_t *jobs) {
    // First pass: Update statuses for each background job.
    job_t *current = jobs->head;
    while (current != NULL) {
        if (current->status == BACKGROUND) {
            int status;
            // Wait for the background job; WUNTRACED ensures we can detect if it stops.
            if (waitpid(current->pid, &status, WUNTRACED) < 0) {
                perror("waitpid");
                return -1;
            }
            if (WIFSTOPPED(status)) {
                // If the job stops, update its status to a STOPPED constant.
                current->status = STOPPED;
            }
            // For terminated jobs (WIFEXITED/WIFSIGNALED), leave the status as BACKGROUND.
        }
        current = current->next;
    }
    // Second pass: Remove all jobs that are still marked as BACKGROUND.
    // These represent jobs that have terminated.
    job_list_remove_by_status(jobs, BACKGROUND);
    return 0;
}
