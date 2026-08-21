// antidebug_linux.c
#include <sys/ptrace.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#include <sys/ptrace.h>
#include <unistd.h>

extern "C"
int __leet_is_debugger_present_tracer_pid(void)
{
    char buf[4096];
    int fd = open("/proc/self/status", O_RDONLY);
    if (fd >= 0)
    {
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0)
        {
            buf[n] = '\0';
            // Look for the line "TracerPid:\t<number>"
            char *p = strstr(buf, "TracerPid:");
            if (p)
            {
                p += sizeof("TracerPid:") - 1;
                // Skip whitespace
                while (*p == ' ' || *p == '\t')
                    p++;
                // Any non-zero value means a tracer is attached
                if (*p != '0' || (p[1] >= '0' && p[1] <= '9'))
                {
                   return 1;
                }
            }
        }
    }
    return 0;
}

extern "C"
int __leet_is_debugger_present_blacklist(void)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/comm", getppid());

    char buf[256];
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return 0;

    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0)
        return 0;

    buf[n] = '\0';
    if (buf[n-1] == '\n')
        buf[n-1] = '\0';

    // Exact matches
    const char *exact[] = {
        "gdb", "gdb-multiarch", "lldb", "lldb-server",
        "strace", "ltrace", "xtrace",
        "ida", "ida64", "idag", "idag64", "idaw", "idaw64",
        "radare2", "r2", "rarun2", "rasm2", "rahash2",
        "ghidra", "ghidraRun", "analyzeHeadless",
        "x64dbg", "x32dbg", "ollydbg", "windbg", "cdb",
        "binaryninja", "binja",
        "hopper", "Hopper",
        "cutter", "iaito",
        "frida", "frida-server", "frida-helper",
        "pin", "pinbin",
        "valgrind", "vgdb",
        "rr", "rr-record",
        "qemu", "qemu-x86_64", "qemu-i386",
        "wine", "wineserver",
        "dotnet", "mono",           // sometimes used for managed debuggers
        //"python", "python3",        // common for scripted debuggers / frida
        "perl", "ruby",
        NULL
    };

    for (int i = 0; exact[i]; i++)
    {
        if (strcmp(buf, exact[i]) == 0)
            return 1;
    }

    // Substring matches (catch renamed / wrapped tools)
    const char *subs[] = {
        "gdb", "lldb", "strace", "ltrace",
        "ida", "radare", "r2", "ghidra",
        "x64dbg", "x32dbg", "olly", "windbg",
        "binja", "hopper", "cutter", "iaito",
        "frida", "pinbin", "valgrind", "rr-",
        "trace", "debug", "dump", "inject",
        NULL
    };

    for (int i = 0; subs[i]; i++)
    {
        if (strstr(buf, subs[i]) != NULL)
            return 1;
    }

    return 0;
}