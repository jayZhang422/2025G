#include "../Identification_Processing_System/src/User/include/g26_signal_analysis.h"

#ifdef G26_QEMU_SEMIHOST
static unsigned char g_qemu_stack[65536]
    __attribute__((aligned(8), used));

static void g26_qemu_write0(const char *text)
{
    register unsigned int operation __asm("r0") = 4U;
    register const char *argument __asm("r1") = text;

    __asm volatile("svc 0x123456"
                   : "+r"(operation)
                   : "r"(argument)
                   : "memory");
}

int main(void);

__attribute__((naked, noreturn, used)) void _start(void)
{
    __asm volatile(
        "ldr r0, =g_qemu_stack\n"
        "ldr r1, =65536\n"
        "add sp, r0, r1\n"
        "mrc p15, 0, r2, c1, c0, 2\n"
        "orr r2, r2, #(0xf << 20)\n"
        "mcr p15, 0, r2, c1, c0, 2\n"
        "isb\n"
        "mov r2, #0x40000000\n"
        "vmsr fpexc, r2\n"
        "bl main\n"
        "ldr r1, =0x20026\n"
        "mov r0, #0x18\n"
        "svc 0x123456\n"
        "b .\n");
}
#else
#include <stdio.h>
#endif

int main(void)
{
    int status = g26_signal_analysis_self_test();

#ifdef G26_QEMU_SEMIHOST
    char failure[] = "G26 signal analysis self-test failed code=000\n";

    if (status == G26_SIGNAL_OK) {
        g26_qemu_write0("G26 signal analysis self-test passed\n");
    } else {
        unsigned int case_number = (unsigned int)(-status);

        failure[42] = (char)('0' + case_number / 100U % 10U);
        failure[43] = (char)('0' + case_number / 10U % 10U);
        failure[44] = (char)('0' + case_number % 10U);
        g26_qemu_write0(failure);
    }
#else
    puts((status == G26_SIGNAL_OK) ?
         "G26 signal analysis self-test passed" :
         "G26 signal analysis self-test failed");
#endif
    return (status == G26_SIGNAL_OK) ? 0 : 1;
}
