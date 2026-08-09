#include <stdio.h>
#include <string.h>
#include <windows.h>

/* Exact Hamlib 4.7.1 check_com_port_in_use logic (unguarded prefix) */
static int check_unguarded(const char *port)
{
    char device[1024];
    snprintf(device, sizeof(device), "\\\\.\\%s", port);
    printf("  unguarded: port=[%s] -> device=[%s] (hex: ", port, device);
    for (const unsigned char *p = (const unsigned char *)device; *p; ++p)
        printf("%02x", *p);
    printf(")\n");
    HANDLE h = CreateFileA(device, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        DWORD e = GetLastError();
        printf("  CreateFile FAIL err=%lu\n", (unsigned long)e);
        return -1;
    }
    printf("  CreateFile OK handle=%p\n", (void*)h);
    CloseHandle(h);
    return 0;
}

/* Patched (idempotent) version */
static int check_guarded(const char *port)
{
    char device[1024];
    if (strncmp(port, "\\\\.\\", 4) != 0)
        snprintf(device, sizeof(device), "\\\\.\\%s", port);
    else
        snprintf(device, sizeof(device), "%s", port);
    printf("  guarded:   port=[%s] -> device=[%s]\n", port, device);
    HANDLE h = CreateFileA(device, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        DWORD e = GetLastError();
        printf("  CreateFile FAIL err=%lu\n", (unsigned long)e);
        return -1;
    }
    printf("  CreateFile OK handle=%p\n", (void*)h);
    CloseHandle(h);
    return 0;
}

int main(void)
{
    const char *ports[] = {
        "COM1",
        "\\\\.\\COM1",
        "COM99",
        "\\\\.\\COM99",
        NULL
    };
    printf("U6 CreateFile probe (mimics Hamlib check_com_port_in_use)\n");
    printf("Windows: ");
    fflush(stdout);
    system("cmd /c ver");
    for (int i = 0; ports[i]; ++i) {
        printf("\n=== port string [%s] ===\n", ports[i]);
        printf("unguarded:\n");
        check_unguarded(ports[i]);
        printf("guarded:\n");
        check_guarded(ports[i]);
    }
    return 0;
}
