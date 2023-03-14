#include <fcntl.h>
#include <memory.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"


int main()
{
    // long long sz;

    char buf[3000];
    // char write_buf[] = "testing writing";
    // char *test_write = malloc(2);
    int offset = 100; /* TODO: try test something bigger than the limit */

    /* open "FIB_DEV" as a file (O_RDWR means that it can read and write)
     * https://man7.org/linux/man-pages/man2/open.2.html
     */
    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }
    // sz = write(fd, write_buf, strlen(write_buf));
    // printf("Writing to " FIB_DEV ", returned the sequence %lld\n", sz);
    // lseek(fd, offset, SEEK_SET);
    // sz = read(fd, buf, 1);
    // printf("Reading from " FIB_DEV
    //        " at offset %d, returned the sequence "
    //        "%s.\n",
    //        offset, buf);
    // printf("The string's length is %ld.\ncopy to user time spend : %lld
    // ns\n\n", strlen(buf), sz);
    // memset(buf, 0, sizeof(buf));
    // memset(test_write, 0, 2);
    // *test_write = '1';
    // for (int i = 2; i <= offset; i++) {
    //     test_write = realloc(test_write, i);
    //     memset(test_write, '1', i);
    //     *(test_write + i - 1) = 0;
    //     // printf("%s\n", test_write);
    //     sz = write(fd, test_write, i);
    //     // printf("Writing to " FIB_DEV ", returned the sequence %lld\n",
    //     sz);
    //     printf("%lld\n", sz);
    // }
    // printf("Reading\n");
    for (int i = 1; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        // sz = read(fd, buf, i);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               i, buf);
        // printf("%ld\n", strlen(buf));
        // printf("%lld\n", sz);
        // printf("The string's length is %ld.\ncopy to user time spend : %lld
        // ns\n\n", strlen(buf), sz);
        memset(buf, 0, sizeof(buf));
    }

    // for (int i = offset; i >= 0; i--) {
    //     lseek(fd, i, SEEK_SET);
    //     sz = read(fd, buf, 1);
    //     printf("Reading from " FIB_DEV
    //            " at offset %d, returned the sequence "
    //            "%s.\n",
    //            i, buf);
    //     memset(buf, 0, sizeof(buf));
    // }

    close(fd);
    return 0;
}
