#include "ether.h"
#include "flags.h"
#include "ip.h"
#include "my_ifnet.h"

#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void add2event(int kq, int fd) {
    struct kevent kev;

    EV_SET(&kev, fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    kevent(kq, &kev, 1, NULL, 0, NULL);
}

void create_if() {
    char ipv4[16];
    int plen4;
    char in[101], out[101];

    printf("IPv4 address: ");
    fflush(stdout);
    scanf("%15s", ipv4);

    printf("prefix length of IPv4 address: ");
    fflush(stdout);
    scanf("%d", &plen4);

    printf("%s/%d\n", ipv4, plen4);

    printf("input UNIX socket: ");
    scanf("%100s", in);

    printf("output UNIX socket: ");
    scanf("%100s", out);

    add_if(ipv4, plen4, NULL, 0, in, out);
}

void eventloop() {
    int kq = kqueue();
    add2event(kq, STDIN_FILENO);

    for (;;) {
        printf("> ");
        fflush(stdout);

        struct kevent kev;
        kevent(kq, NULL, 0, &kev, 1, NULL);

        if (kev.ident == STDIN_FILENO) {
            char buf[4096];
            ssize_t size = read(STDIN_FILENO, buf, sizeof(buf));
            buf[size - 1] = '\0';

            if (memcmp("create", buf, 6) == 0) {
                create_if();
            } else if (memcmp("show", buf, 4) == 0) {
                print_if();
            } else if (memcmp("tcp", buf, 4) == 0) {
            } else if (memcmp("exit", buf, 4) == 0) {
                exit(0);
            } else {
                if (strlen(buf) != 0)
                    printf("%s is not supported\n", buf);
            }
        } else {
            dev_input(kev.ident);
        }
    }
}

int main(int argc, char *argv[]) {
    init_my_ifnet();
    init_ipv4();

    eventloop();

    return 0;
}