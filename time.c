#include "types.h"
#include "stat.h"
#include "user.h"
 
int
main(int argc, char **argv)
{
    if(argc <= 1 || strcmp(argv[0], "time")) {
        printf(2, "Arguemnts wrong\n");
        exit();
    }

    int pid = fork();
    if (pid < 0) {
        printf(2, "Fork failed\n");
        return -1;
    } else if (pid == 0) {
        // Child process here.
        exec(argv[1], argv + 1);
        printf(2, "Exec has failed\n");
        // sleep(100);
        exit();
    }

    // Parent code
    uint run_time = 0;
    uint wait_time = 0;
    int status;
    status = waitx(&wait_time, &run_time);
    // status = wait();

    printf(1, "\nProcess run time\t%d\nProcess wait time\t%d\nStatus\t%d\n", run_time, wait_time, status);

    exit();
}