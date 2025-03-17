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
    // --- Setup signal handling for background operation ---
    // Ignore SIGTTIN and SIGTTOU signals so that the shell doesn't get stopped when
    // it attempts to read from or write to the terminal while in the background.
    struct sigaction sac;
    sac.sa_handler = SIG_IGN;    // Ignore handler
    if (sigfillset(&sac.sa_mask) == -1) {
        perror("sigfillset");
        return 1;
    }
    sac.sa_flags = 0;
    if (sigaction(SIGTTIN, &sac, NULL) == -1 || sigaction(SIGTTOU, &sac, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    // --- Initialize the tokens vector and job list ---
    strvec_t tokens;
    if (strvec_init(&tokens) != 0) {    // Check for initialization failure if applicable
        fprintf(stderr, "Failed to initialize tokens vector\n");
        return -1;
    }
    job_list_t jobs;
    job_list_init(&jobs);    // Initialize the job list to track background/stopped jobs

    char cmd[CMD_LEN];    // Buffer to hold user command input

    // --- Main command loop ---
    printf("%s", PROMPT);    // Print initial prompt
    while (fgets(cmd, CMD_LEN, stdin) != NULL) {
        // Remove the trailing newline character from the input
        int i = 0;
        while (cmd[i] != '\n') {
            i++;
        }
        cmd[i] = '\0';

        // Tokenize the command line input (split by spaces)
        if (tokenize(cmd, &tokens) != 0) {
            printf("Failed to parse command\n");
            strvec_clear(&tokens);
            job_list_free(&jobs);
            return 1;
        }
        // If no tokens were generated (empty command), simply reprompt.
        if (tokens.length == 0) {
            printf("%s", PROMPT);
            continue;
        }
        // Retrieve the first token (command name) from the token vector.
        const char *first_token = strvec_get(&tokens, 0);

        // --- Built-in command: pwd ---
        if (strcmp(first_token, "pwd") == 0) {
            // Print the current working directory using getcwd()
            char *buffer;
            buffer = (char *) malloc(CMD_LEN);
            if (buffer == NULL) {
                perror("unable to allocate memory.");
                return -1;
            }
            if (getcwd(buffer, CMD_LEN) == NULL) {
                perror("getcwd");
                free(buffer);
                return -1;
            }
            printf("%s\n", buffer);
            free(buffer);
        }
        // --- Built-in command: cd ---
        else if (strcmp(first_token, "cd") == 0) {
            // Change directory: if a second token is provided, use it;
            // otherwise, change to the home directory specified by the HOME environment variable.
            const char *second_token = strvec_get(&tokens, 1);
            const char *home = getenv("HOME");
            if (second_token) {
                if (chdir(second_token) != 0) {
                    perror("chdir");
                    if (home == NULL) {
                        fprintf(stderr, "cd: HOME environment variable not set properly\n");
                    } else if (chdir(home) != 0) {
                        perror("chdir");
                    }
                }
            } else {
                if (home == NULL) {
                    fprintf(stderr, "cd: HOME environment variable not set properly\n");
                } else if (chdir(home) != 0) {
                    perror("chdir");
                }
            }
        }
        // --- Built-in command: exit ---
        else if (strcmp(first_token, "exit") == 0) {
            // Clear the tokens vector and exit the loop.
            strvec_clear(&tokens);
            break;
        }
        // --- Built-in command: jobs ---
        else if (strcmp(first_token, "jobs") == 0) {
            // Print all jobs in the job list along with their status (background or stopped)
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
        // --- Built-in command: fg ---
        else if (strcmp(first_token, "fg") == 0) {
            // Resume a stopped job in the foreground.
            if (resume_job(&tokens, &jobs, 1) == -1) {
                printf("Failed to resume job in foreground\n");
            }
        }
        // --- Built-in command: bg ---
        else if (strcmp(first_token, "bg") == 0) {
            // Resume a stopped job in the background.
            if (resume_job(&tokens, &jobs, 0) == -1) {
                printf("Failed to resume job in background\n");
            }
        }
        // --- Built-in command: wait-for ---
        else if (strcmp(first_token, "wait-for") == 0) {
            // Wait for a specific background job to terminate or stop.
            if (await_background_job(&tokens, &jobs) == -1) {
                printf("Failed to wait for background job\n");
            }
        }
        // --- Built-in command: wait-all ---
        else if (strcmp(first_token, "wait-all") == 0) {
            // Wait for all background jobs to terminate or stop, then clean them up.
            if (await_all_background_jobs(&jobs) == -1) {
                printf("Failed to wait for all background jobs\n");
            }
        }
        // --- Non-built-in command: execute external command ---
        else {
            // Check if the last token is "&", which indicates background execution.
            int background = 0;
            if (tokens.length > 0 && strcmp(tokens.data[tokens.length - 1], "&") == 0) {
                background = 1;
                // Remove the "&" token from the tokens vector using strvec_take.
                strvec_take(&tokens, tokens.length - 1);
            }

            int status;
            // Fork a child process to execute the external command.
            pid_t cpid = fork();
            if (cpid < 0) {
                perror("fork failed.");
                return 1;
            } else if (cpid == 0) {
                // In the child process, call run_command() to execute the command.
                if (run_command(&tokens) == -1) {
                    exit(1);
                }
                exit(0);
            } else {
                // In the parent process, handle background and foreground jobs differently.
                if (background) {
                    // For background jobs, do not wait; just add the job to the jobs list.
                    // (Note: Depending on design, consider duplicating tokens.data[0] if
                    // necessary.)
                    job_list_add(&jobs, cpid, tokens.data[0], BACKGROUND);
                } else {
                    // For foreground jobs, move the child to the foreground,
                    // wait for it to complete or stop, and then restore the shell to the
                    // foreground.
                    pid_t shell_pid = getpid();
                    if (setpgid(cpid, cpid) == -1) {
                        perror("setpgid failed");
                        return -1;
                    }
                    if (tcsetpgrp(STDIN_FILENO, cpid) == -1) {
                        perror("tcsetpgrp failed");
                        return -1;
                    }
                    pid_t terminated_pid = waitpid(cpid, &status, WUNTRACED);
                    if (terminated_pid < 0) {
                        perror("waitpid() failed");
                        return -1;
                    }
                    if (tcsetpgrp(STDIN_FILENO, shell_pid) == -1) {
                        perror("Restoring parent process group failed");
                        return -1;
                    }
                    // If the process was stopped, add it to the job list as a stopped job.
                    if (WIFSTOPPED(status)) {
                        job_list_add(&jobs, cpid, tokens.data[0], status);
                    }
                }
            }
        }
        // Print the prompt for the next command.
        printf("%s", PROMPT);
        // Clear the tokens vector for the next iteration.
        strvec_clear(&tokens);
    }

    // Free the job list resources.
    job_list_free(&jobs);
    return 0;
}
