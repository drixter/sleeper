/*
* sleep_progress.c
*
* Usage:
* ./sleep_progress <seconds> [--multiline] [--quiet]
*
* Behavior:
* Prints start time and ETA once, then shows a clean progress bar.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

/* ANSI Color Codes */
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_BOLD          "\x1b[1m"

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
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            SetConsoleMode(hOut, dwMode | 0x0004); // ENABLE_VIRTUAL_TERMINAL_PROCESSING
        }
        SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
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
        sigaction(SIGINT, &sa, NULL);
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

/* Renders the progress bar and percentage */
static void print_bar(long elapsed, long total) {
    const int BAR_WIDTH = 20;
    float percentage = (total == 0) ? 1.0f : (float)elapsed / total;
    int filled_width = (int)(percentage * BAR_WIDTH);

    printf(" [" ANSI_COLOR_GREEN);
    for (int i = 0; i < BAR_WIDTH; ++i) {
        if (i < filled_width) printf("#");
        else printf(ANSI_COLOR_RESET "-");
    }
    printf(ANSI_COLOR_RESET "] %3d%%", (int)(percentage * 100));
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <seconds> [--multiline] [--quiet]\n", argv[0]);
        return 1;
    }

    int multiline = 0;
    int quiet = 0;
    long total = -1;

    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--multiline") == 0) {
            multiline = 1;
        } else if (strcmp(argv[i], "--quiet") == 0 || strcmp(argv[i], "-q") == 0) {
            quiet = 1;
        } else if (total == -1) {
            char *end = NULL;
            total = strtol(argv[i], &end, 10);
            if (errno != 0 || end == argv[i] || *end != '\0' || total < 0) {
                fprintf(stderr, "Error: <seconds> must be a non-negative integer.\n");
                return 1;
            }
        }
    }

    if (total == -1) {
        fprintf(stderr, "Error: Missing <seconds> argument.\n");
        return 1;
    }

    install_handler();

    /* Calculate and display the Header */
    time_t now = time(NULL);
    time_t finish = now + total;
    
    char start_str[10], eta_str[10];
    strftime(start_str, sizeof(start_str), "%H:%M:%S", localtime(&now));
    strftime(eta_str, sizeof(eta_str), "%H:%M:%S", localtime(&finish));

    printf("Start Time: " ANSI_COLOR_CYAN "%s" ANSI_COLOR_RESET " | ETA: " ANSI_COLOR_YELLOW "%s" ANSI_COLOR_RESET "\n", 
            start_str, eta_str);
    printf("Sleeping for %ld second%s...\n", total, (total == 1 ? "" : "s"));

    long elapsed = 0;
    while (elapsed <= total) {
        if (!quiet) {
            if (multiline) {
                printf("Elapsed: %4ld s | Remaining: %4ld s", elapsed, total - elapsed);
                print_bar(elapsed, total);
                printf("\n");
            } else {
                /* Removed ETA from this line to keep it clean */
                printf("\rElapsed: %4ld s | Remaining: %4ld s", elapsed, total - elapsed);
                print_bar(elapsed, total);
                printf("    "); // Padding for clean terminal display
                fflush(stdout);
            }
        }

        if (elapsed == total) break;

        if (was_interrupted() || sleep_one_second() != 0 || was_interrupted()) {
            if (!quiet && !multiline) putchar('\n');
            fprintf(stderr, "Interrupted at %ld/%ld seconds.\n", elapsed, total);
            return 130;
        }
        elapsed++;
    }

    if (!quiet && !multiline) putchar('\n');
    printf(ANSI_BOLD ANSI_COLOR_GREEN "Done." ANSI_COLOR_RESET " Total time: %lds.\n", total);
    return 0;
}