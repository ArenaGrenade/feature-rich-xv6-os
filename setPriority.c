#include "types.h"
#include "stat.h"
#include "user.h"
 
int
main(int argc, char **argv)
{
    if(argc != 3 || strcmp(argv[0], "setPriority")) {
        printf(2, "Arguements wrong\n");
        exit();
    }

    set_priority(atoi(argv[2]), atoi(argv[1]));

    exit();
}