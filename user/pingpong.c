#include "kernel/types.h"
#include "user/user.h"

#define INDEX_READ 0
#define INDEX_WRITE 1

int main(int argc, char *argv[])
{
    int fds_p2c[2]; // P2C
    int fds_c2p[2]; // C2P

    pipe(fds_c2p);
    pipe(fds_p2c);

    if (fork() == 0) // Child
    {
        close(fds_c2p[INDEX_READ]);
        close(fds_p2c[INDEX_WRITE]);

        char buf;
        if (read(fds_p2c[INDEX_READ], &buf, 1) == 1)
        {
            printf("%d: received ping\n", getpid());
        }

        write(fds_c2p[INDEX_WRITE], "c", 1);
        exit(0);
    }
    else // Parent
    {
        close(fds_c2p[INDEX_WRITE]);
        close(fds_p2c[INDEX_READ]);

        write(fds_p2c[INDEX_WRITE], "c", 1);

        char buf;
        if (read(fds_c2p[INDEX_READ], &buf, 1) == 1)
        {
            printf("%d: received pong\n", getpid());
        }
    }

    exit(0);
}