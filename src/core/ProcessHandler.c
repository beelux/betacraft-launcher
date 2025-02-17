#include "ProcessHandler.h"
#include "Logger.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include "WindowsProcessHandler.h"
#elif defined(__APPLE__) || defined(__linux__)
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#endif

char* bc_process_log = NULL;

#if defined(__APPLE__) || defined(__linux__)
void bc_unixprocess_create(bc_process_args* args, int output) {
    args->arr[args->len] = NULL;

    int fd[2];
    if (output) {
        pipe(fd);
    }

    int pid = fork();

    if (pid == 0) {
        if (output) {
            close(fd[0]);
            dup2(fd[1], 1); // stdout
            dup2(fd[1], 2); // stderr
            close(fd[1]);
        }

        execvp(args->arr[0], args->arr);
    } else if (output) {
        char buffer[1024];
        close(fd[1]);

        while (read(fd[0], buffer, sizeof(buffer)) != 0) {
            write(1, buffer, strlen(buffer));
            if (bc_process_log != NULL) {
                char* newLog = malloc(strlen(bc_process_log) + strlen(buffer) + 1);
                strcpy(newLog, bc_process_log);
                strcat(newLog, buffer);

                free(bc_process_log);
                bc_process_log = newLog;
            } else {
                bc_process_log = malloc(strlen(buffer) + 1);
                strcpy(bc_process_log, buffer);
            }

            memset(buffer, 0, sizeof(buffer));
        }
    }

    if (!output) {
        waitpid(pid, NULL, 0);
    }
}
#endif

void bc_process_create(bc_process_args* args) {
#ifdef _WIN32
    bc_winprocess_create(args);
#elif defined(__APPLE__) || defined(__linux__)
    bc_unixprocess_create(args, 0);
#endif
}

void bc_process_create_log(bc_process_args* args) {
    if (bc_process_log != NULL) {
        free(bc_process_log);
        bc_process_log = NULL;
    }

#ifdef _WIN32
    bc_winprocess_create_log(args);
#elif defined(__APPLE__) || defined(__linux__)
    bc_unixprocess_create(args, 1);
#endif
}
