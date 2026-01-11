/*
   Copyright (C) 2014 nieyong

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.
*/

/*
   LD_PRELOAD library to make bind and connect to use a virtual
   IP address as localaddress. Specified via the environment
   variable BIND_ADDR.

   Compile on Linux with:
   gcc -nostartfiles -fpic -shared bindp.c -o libindp.so -ldl -D_GNU_SOURCE
   or just use make be easy:
   make

   Example in bash to make inetd only listen to the localhost
   lo interface, thus disabling remote connections and only
   enable to/from localhost:

   BIND_ADDR="127.0.0.1" BIND_PORT="49888" LD_PRELOAD=./libindp.so curl http://192.168.190.128

   OR:
   BIND_ADDR="127.0.0.1" LD_PRELOAD=./libindp.so curl http://192.168.190.128

   Example in bash to use your virtual IP as your outgoing
   sourceaddress for ircII:

   BIND_ADDR="your-virt-ip" LD_PRELOAD=./bind.so ircII

   Note that you have to set up your servers virtual IP first.

   Add SO_REUSEPORT support within Centos7 or Linux OS with kernel >= 3.9, for the applications with multi-process support just listen one port now
   REUSE_ADDR=1 REUSE_PORT=1 LD_PRELOAD=./libindp.so python server.py &
   REUSE_ADDR=1 REUSE_PORT=1 LD_PRELOAD=./libindp.so java -server -jar your.jar &

   email: nieyong@staff.weibo.com
   web:   http://www.blogjava.net/yongboy
*/
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <dlfcn.h>
#include <errno.h>
#include <arpa/inet.h>

int debug_enabled = 0;

int (*real_bind)(int, const struct sockaddr *, socklen_t);
int (*real_connect)(int, const struct sockaddr *, socklen_t);

in_addr_t bind_addr_saddr = 0;
struct sockaddr_in local_sockaddr_in[] = { 0 };

in_port_t bind_port_saddr = 0;
int32_t reuse_port = 0;
int32_t reuse_addr = 0;
int32_t ip_transparent = 0;

int32_t parse_int_flag(const char *flag_str, const char *flag_name) {
    if (strcmp(flag_str, "0") == 0) {
        return 0;
    } else if (strcmp(flag_str, "1") == 0) {
        return 1;
    } else {
        fprintf(stderr, "invalid value '%s' for variable '%s'\n", flag_str,
            flag_name);
        return 0;
    }
}

void _init (void) {
    const char *err = NULL;
    char *bind_addr_env = NULL;
    char *bind_port_env = NULL;
    char *reuse_addr_env = NULL;
    char *reuse_port_env = NULL;
    char *ip_transparent_env = NULL;

    real_bind = dlsym (RTLD_NEXT, "bind");
    if ((err = dlerror ()) != NULL) {
        fprintf (stderr, "dlsym (bind): %s\n", err);
    }

    real_connect = dlsym (RTLD_NEXT, "connect");
    if ((err = dlerror ()) != NULL) {
        fprintf (stderr, "dlsym (connect): %s\n", err);
    }

    if ((bind_addr_env = getenv ("BIND_ADDR"))) {
        struct in_addr temp_in_addr = { 0 };
        int inet_aton_rslt = inet_aton (bind_addr_env, &temp_in_addr);
        local_sockaddr_in->sin_family = AF_INET;
        local_sockaddr_in->sin_port = htons (0);
        if (inet_aton_rslt == 1) {
            bind_addr_saddr = temp_in_addr.s_addr;
            local_sockaddr_in->sin_addr.s_addr = bind_addr_saddr;
        } else {
            fprintf (stderr, "invalid value '%s' for variable 'BIND_ADDR'\n",
                bind_addr_env);
            local_sockaddr_in->sin_addr.s_addr = htons (0);
        }
    }

    if ((bind_port_env = getenv ("BIND_PORT"))) {
        long temp = 0;

        errno = 0;
        temp = strtol(bind_port_env, NULL, 10);
        if (errno == 0 && temp >= 0 && temp <= 65535) {
            bind_port_saddr = (in_port_t)temp;
            local_sockaddr_in->sin_port = htons (bind_port_saddr);
        } else {
            fprintf (stderr, "invalid value '%ld' for variable 'BIND_PORT'\n",
                temp);
            local_sockaddr_in->sin_port = htons (0);
        }
    }

    if ((reuse_addr_env = getenv ("REUSE_ADDR"))) {
        reuse_addr = parse_int_flag (reuse_addr_env, "REUSE_ADDR");
    }

    if ((reuse_port_env = getenv ("REUSE_PORT"))) {
        reuse_port = parse_int_flag (reuse_port_env, "REUSE_PORT");
    }

    if ((ip_transparent_env = getenv ("IP_TRANSPARENT"))) {
        ip_transparent = parse_int_flag (ip_transparent_env, "IP_TRANSPARENT");
    }
}

unsigned short get_address_family(const struct sockaddr *sk) {
    /*
        As defined in linux/socket.h ,__kernel_sa_family_t is 2 bytes wide.
        We read the first two bytes of sk without using cast to protocol families
    */
    unsigned short _pf = *((const unsigned short*) sk);
    return _pf;
}


int bind (int fd, const struct sockaddr *sk, socklen_t sl) {
    unsigned short _pf = get_address_family(sk);
    switch (_pf) {
        case AF_INET:
        {
            static struct sockaddr_in *lsk_in;

            /*
             * We intentionally discard the const qualifier as we do indeed
             * change the pointed-to memory of sk later on.
             *
             * TODO: Modifying a const pointer is a horrible idea. Is there
             * some reason we can't copy sk to a non-const struct, modify it,
             * and pass that to real_bind?
             *
             * We also don't have to worry about alignment here because sk
             * actually contains a sockaddr_in when the address family is
             * AF_INET.
             */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wcast-align"
            lsk_in = (struct sockaddr_in *)sk;
#pragma GCC diagnostic pop

            if (debug_enabled) {
                char original_ip [INET_ADDRSTRLEN];
                int original_port = 0;
                char *l_bind_addr = NULL;
                char *l_bind_port = NULL;

                inet_ntop(AF_INET,&(lsk_in->sin_addr),original_ip,INET_ADDRSTRLEN);
                original_port = ntohs(lsk_in->sin_port);
                l_bind_addr = getenv ("BIND_ADDR");
                l_bind_port = getenv ("BIND_PORT");
                printf("[-] LIB received AF_INET bind request\n");
                if (l_bind_addr && l_bind_port) {
                    printf("[-] Changing %s:%d to %s:%s\n" , original_ip,original_port,l_bind_addr,l_bind_port);
                } else if (l_bind_addr) {
                    printf("[-] Changing %s to %s\n" , original_ip,l_bind_addr);
                    printf("[-] AF_INET: Leaving port unchanged\n");
                } else if (l_bind_port) {
                    printf("[-] Changing %d to %s\n" ,original_port,l_bind_port);
                    printf("[-] AF_INET: Leaving ip unchanged\n");
                } else {
                    printf("[!] AF_INET: Leaving request unchanged\n");
                }
            }

            if (bind_addr_saddr)
                lsk_in->sin_addr.s_addr = bind_addr_saddr;

            if (bind_port_saddr)
                lsk_in->sin_port = htons (bind_port_saddr);

            break;
        }

        case AF_UNIX:
            if (debug_enabled) {
                printf("[-] LIB received AF_UNIX bind request\n");
                printf("[-] AF_UNIX: Leaving request unchanged\n");
            }
            break;

        /*
            Other families handling
        */

        default:
            if (debug_enabled) {
                printf("[!] LIB received unmanaged address family\n");
            }
            break;
    }

    /*
        FIXME: Be careful when using setsockopt
        Is it valid to use these options for AF_UNIX?
        Must be checked
    */

    if (reuse_addr) {
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));
    }

#ifdef SO_REUSEPORT
    if (reuse_port) {
        setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &reuse_port, sizeof(reuse_port));
    }
#endif

    if (ip_transparent) {
        setsockopt(fd, SOL_IP, IP_TRANSPARENT, &ip_transparent, sizeof(ip_transparent));
    }

    return real_bind (fd, sk, sl);
}

int connect (int fd, const struct sockaddr *sk, socklen_t sl) {
    unsigned short _pf = get_address_family(sk);
    if (_pf == AF_INET) {
        /*
            the default behavior of connect function is that when you
            don't specify BIND_PORT environmental variable it sets port 0 for
            the local socket. OS network stack will choose a random number
            for the port in this case and also in the case of duplicate port
            numbers for client sockets
        */
        if (debug_enabled) {
            printf("[-] connect(): AF_INET connect() call, binding to local address\n");
        }

        if (bind_addr_saddr || bind_port_saddr) {
            bind (fd, (struct sockaddr *)local_sockaddr_in, sizeof (struct sockaddr));
        }
        return real_connect (fd, sk, sl);

    } else {
        if (debug_enabled) {
            printf("[-] connect(): ignoring to change local address for non AF_INET socket\n");
        }
        return real_connect (fd, sk, sl);
    }
}

int main(__attribute__((unused)) int argc,
    __attribute__((unused)) char **argv) {
    return 0;
}
