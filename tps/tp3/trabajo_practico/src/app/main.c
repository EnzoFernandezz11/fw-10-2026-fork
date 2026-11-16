#include "demo.h"
#include "print.h"
#include <string.h>

#define PROGRAM_NAME "building-process-demo"
#define PROGRAM_VERSION "1.1"

int main(int argc, char *argv[])
{
    if (argc > 1 && strcmp(argv[1], "--version") == 0) {
        print_str(PROGRAM_NAME " " PROGRAM_VERSION "\n");
        return 0;
    }

    demo_math();
    demo_str();
    demo_embedded();
    return 0;
}
