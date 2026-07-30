#include "../Identification_Processing_System/src/User/include/modulation_analysis.h"

#include <stdio.h>

int main(void)
{
    int status = modulation_analysis_self_test();

    puts((status == 0) ? "modulation analysis self-test passed" :
                         "modulation analysis self-test failed");
    return status;
}
