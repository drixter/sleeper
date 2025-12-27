
/*
* sleep_progress.c
*
* Usage:
*   ./sleep_progress <seconds> [--multiline]
*
* Example:
*   ./sleep_progress 10
*   ./sleep_progress 10 --multiline
*
* Behavior:
*   Prints how many seconds have elapsed and how many are left.
*   Updates once per second. By default, it updates on a single line.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    static volatile LONG interrupted = 0;
    static BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType) {
        if (ctrlType == CTRL_C_EVENT) {
            InterlockedExchange(&interrupted, 1);
            return TRUE;
        }
        return FALSE;
    }
    static void install_handler(void) {
        if (!SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE)) {
            fprintf(stderr, "Failed to set console control handler\n");
        }
    }
    static int was_interrupted(void) {
        return (InterlockedCompareExchange(&interrupted, 1, 1) == 1);
    }
    static int sleep_one_second(void) {
        Sleep(1000);
        return 0;
    }
#else
    #include <signal.h>
    #include <time.h>
    #include <unistd.h>

    static volatile sig_atomic_t interrupted = 0;
    static void handle_sigint(int sig) {
        (void)sig;
        interrupted = 1;
    }
    static void install_handler(void) {
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = handle_sigint;
        sa.sa_flags = SA_RESTART;
        sigemptyset(&sa.sa_mask);
        if (sigaction(SIGINT, &sa, NULL) == -1) {
            perror("sigaction");
        }
    }
    static int was_interrupted(void) {
        return interrupted;
    }
    static int sleep_one_second(void) {
        struct timespec ts = { .tv_sec = 1, .tv_nsec = 0 };
        while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
            if (was_interrupted()) return -1;
        }
        return 0;
    }
#endif

static void print_usage(const char *argv0) {
    fprintf(stderr, "Usage: %s <seconds> [--multiline]\n", argv0);
}

int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 3) {
        print_usage(argv[0]);
        return 1;
    }

    int multiline = 0;
    if (argc == 3 && strcmp(argv[2], "--multiline") == 0) {
        multiline = 1;
    }

    char *end = NULL;
    errno = 0;
    long total = strtol(argv[1], &end, 10);
    if (errno != 0 || end == argv[1] || *end != '\0' || total < 0) {
        fprintf(stderr, "Error: <seconds> must be a non-negative integer.\n");
        return 1;
    }

    install_handler();

    long elapsed = 0;

    if (multiline) {
        printf("Sleeping for %ld second%s...\n", total, (total == 1 ? "" : "s"));
    } else {
        printf("Sleeping for %ld second%s... ", total, (total == 1 ? "" : "s"));
    }
    fflush(stdout);

    while (elapsed < total) {
        if (was_interrupted()) {
            if (!multiline) putchar('\n');
            fprintf(stderr, "Interrupted at %ld/%ld seconds.\n", elapsed, total);
            return 130;
        }

        if (sleep_one_second() != 0) {
            if (!multiline) putchar('\n');
            fprintf(stderr, "Interrupted at %ld/%ld seconds.\n", elapsed, total);
            return 130;
        }

        elapsed++;
        long remaining = total - elapsed;

        if (multiline) {
            printf("Elapsed: %ld s | Remaining: %ld s\n", elapsed, remaining);
        } else {
            printf("\rElapsed: %ld s | Remaining: %ld s", elapsed, remaining);
        }
        fflush(stdout);
    }

    if (!multiline) putchar('\n');
    printf("Done. Slept for %ld second%s.\n", total, (total == 1 ? "" : "s"));
    return 0;
}
