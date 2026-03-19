
#include <sys/select.h> // fd_set, FD_ZERO, FD_SET, FD_ISSET
#include <sys/socket.h> // socket

int main()
{
    int server = socket(2, 1, 0);
    fd_set a;

    // FD_ZERO
    do
    {
        unsigned int __i;
        fd_set *__arr = (&a);
        for (__i = 0; __i < sizeof(fd_set) / sizeof(__fd_mask); ++__i)
            ((__arr)->__fds_bits)[__i] = 0;
    } while (0);
    //
    // FD_SET
    ((void)(((&a)->__fds_bits)[((server) / (8 * (int)sizeof(__fd_mask)))] |= ((__fd_mask)(1UL << ((server) % (8 * (int)sizeof(__fd_mask)))))));
    fd_set *set_ptr = &a;
    int index = server / (8 * (int)sizeof(__fd_mask));
    int bit = server % (8 * (int)sizeof(__fd_mask));
    set_ptr->__fds_bits[index] |= (1UL << bit);
    //
    // FD_ISSET
    ((((&a)->__fds_bits)[((server) / (8 * (int)sizeof(__fd_mask)))] & ((__fd_mask)(1UL << ((server) % (8 * (int)sizeof(__fd_mask)))))) != 0);
    fd_set *set_ptr = &a;
    int index = server / (8 * (int)sizeof(__fd_mask));
    int bit = server % (8 * (int)sizeof(__fd_mask));
    int is_set = (set_ptr->__fds_bits[index] & (1UL << bit)) != 0;

    if (is_set)
    {
        printf("server fd is ready\n");
    }

    sizeof(a);
    // FD_ZERO
    FD_ZERO(&a);
    //
    // FD_SET
    FD_SET(server, &a);
    //
    // FD_ISSET
    FD_ISSET(server, &a);
}