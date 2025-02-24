#define _GNU_SOURCE

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "job_list.h"
#include "string_vector.h"
#include "swish_funcs.h"

#define CMD_LEN 512
#define PROMPT "@> "

int main(int argc, char **argv) {
    // Task 4: Set up shell to ignore SIGTTIN, SIGTTOU when put in background
    // You should adapt this code for use in run_command().
    struct sigaction sac;
    sac.sa_handler = SIG_IGN;
    if (sigfillset(&sac.sa_mask) == -1) {
        perror("sigfillset");
        return 1;
    }
    sac.sa_flags = 0;
    if (sigaction(SIGTTIN, &sac, NULL) == -1 || sigaction(SIGTTOU, &sac, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    strvec_t tokens;
    strvec_init(&tokens);
    job_list_t jobs;
    job_list_init(&jobs);
    char cmd[CMD_LEN];

    printf("%s", PROMPT);
    while (fgets(cmd, CMD_LEN, stdin) != NULL) {
        // Need to remove trailing '\n' from cmd. There are fancier ways.
        int i = 0;
        while (cmd[i] != '\n') {
            i++;
        }
        cmd[i] = '\0';

        if (tokenize(cmd, &tokens) != 0) {
            printf("Failed to parse command\n");
            strvec_clear(&tokens);
            job_list_free(&jobs);
            return 1;
        }
        if (tokens.length == 0) {
            printf("%s", PROMPT);
            continue;
        }
        const char *first_token = strvec_get(&tokens, 0);

        if (strcmp(first_token, "pwd") == 0) {
            // TODO Task 1: Print the shell's current working directory
            // Use the getcwd() system call
        }

        else if (strcmp(first_token, "cd") == 0) {
            // TODO Task 1: Change the shell's current working directory
            // Use the chdir() system call
            // If the user supplied an argument (token at index 1), change to that directory
            // Otherwise, change to the home directory by default
            // This is available in the HOME environment variable (use getenv())
        }

        else if (strcmp(first_token, "exit") == 0) {
            strvec_clear(&tokens);
            break;
        }

        // Task 5: Print out current list of pending jobs
        else if (strcmp(first_token, "jobs") == 0) {
            int i = 0;
            job_t *current = jobs.head;
            while (current != NULL) {
                char *status_desc;
                if (current->status == BACKGROUND) {
                    status_desc = "background";
                } else {
                    status_desc = "stopped";
                }
                printf("%d: %s (%s)\n", i, current->name, status_desc);
                i++;
                current = current->next;
            }
        }

        // Task 5: Move stopped job into foreground
        else if (strcmp(first_token, "fg") == 0) {
            if (resume_job(&tokens, &jobs, 1) == -1) {
                printf("Failed to resume job in foreground\n");
            }
        }

        // Task 6: Move stopped job into background
        else if (strcmp(first_token, "bg") == 0) {
            if (resume_job(&tokens, &jobs, 0) == -1) {
                printf("Failed to resume job in background\n");
            }
        }

        // Task 6: Wait for a specific job identified by its index in job list
        else if (strcmp(first_token, "wait-for") == 0) {
            if (await_background_job(&tokens, &jobs) == -1) {
                printf("Failed to wait for background job\n");
            }
        }

        // Task 6: Wait for all background jobs
        else if (strcmp(first_token, "wait-all") == 0) {
            if (await_all_background_jobs(&jobs) == -1) {
                printf("Failed to wait for all background jobs\n");
            }
        }

        else {
            // TODO Task 2: If the user input does not match any built-in shell command,
            // treat the input as a program name and command-line arguments
            // USE THE run_command() FUNCTION DEFINED IN swish_funcs.c IN YOUR IMPLEMENTATION
            // You should take the following steps:
            //   1. Use fork() to spawn a child process
            //   2. Call run_command() in the child process
            //   2. In the parent, use waitpid() to wait for the program to exit

            // TODO Task 4: Set the child process as the target of signals sent to the terminal
            // via the keyboard.
            // To do this, call 'tcsetpgrp(STDIN_FILENO, <child_pid>)', where child_pid is the
            // child's process ID just returned by fork(). Do this in the parent process.

            // TODO Task 5: Handle the issue of foreground/background terminal process groups.
            // Do this by taking the following steps in the shell (parent) process:
            // 1. Modify your call to waitpid(): Wait specifically for the child just forked, and
            //    use WUNTRACED as your third argument to detect if it has stopped from a signal
            // 2. After waitpid() has returned, call tcsetpgrp(STDIN_FILENO, <pid>) where pid is
            //    the process ID of the shell process (use getpid() to obtain it)
            // 3. If the child status was stopped by a signal, add it to 'jobs', the
            //    the terminal's jobs list.
            // You can detect if this has occurred using WIFSTOPPED on the status
            // variable set by waitpid()

            // TODO Task 6: If the last token input by the user is "&", start the current
            // command in the background.
            // 1. Determine if the last token is "&". If present, use strvec_take() to remove
            //    the "&" from the token list.
            // 2. Modify the code for the parent (shell) process: Don't use tcsetpgrp() or
            //    use waitpid() to interact with the newly spawned child process.
            // 3. Add a new entry to the jobs list with the child's pid, program name,
            //    and status BACKGROUND.
        }

        strvec_clear(&tokens);
        printf("%s", PROMPT);
    }

    job_list_free(&jobs);
    return 0;
}
