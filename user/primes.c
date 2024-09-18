#include "kernel/types.h"
#include "user/user.h"

#define INDEX_READ 0
#define INDEX_WRITE 1

void child(int fds_p2c[])
{
    close(fds_p2c[INDEX_WRITE]);
    int i;
    if (read(fds_p2c[INDEX_READ], &i, sizeof(i)) == 0)
    {
        close(fds_p2c[INDEX_READ]);
        exit(0);
    }
    printf("prime %d\n", i);
    int num = 0;
    int fds_c2gc[2];//C2GC
    pipe(fds_c2gc);
    int pid= fork();
    if (pid == 0)
    {

        child(fds_c2gc);
    }
    else
    {
        close(fds_c2gc[INDEX_READ]);
        while (read(fds_p2c[INDEX_READ], &num, sizeof(num)) > 0)
        {
            if (num % i != 0)
            {
                write(fds_c2gc[INDEX_WRITE], &num, sizeof(num));
            }
        }
        close(fds_c2gc[INDEX_WRITE]);
        wait(0);
    }
    exit(0);
}

int main(int argc, char *argv[])
{
    int fds_p2c[2]; //P2C
    pipe(fds_p2c);
    int pid = fork();
    if (pid == 0)
    {
        child(fds_p2c);
    }
    else
    {
        close(fds_p2c[INDEX_READ]);
        for (int i = 2; i <= 35; i++)
        {
            write(fds_p2c[INDEX_WRITE], &i, sizeof(i));
        }
        close(fds_p2c[INDEX_WRITE]);
        wait(0);
    }
    exit(0);
}
