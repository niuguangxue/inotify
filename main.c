#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <string.h>

static void
handle_events(int fd, int mon_fd, const char* monitor_floder)
{
    /* Some systems cannot read integer variables if they are not
        properly aligned. On other systems, incorrect alignment may
        decrease performance. Hence, the buffer used for reading from
        the inotify file descriptor should have the same alignment as
        struct inotify_event. */

    char buf[4096]
        __attribute__ ((aligned(__alignof__(struct inotify_event))));
    const struct inotify_event *event;
    ssize_t len;

    /* Loop while events can be read from inotify file descriptor. */

    for (;;) {

        /* Read some events. */

        len = read(fd, buf, sizeof(buf));
        if (len == -1 && errno != EAGAIN) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        /* If the nonblocking read() found no events to read, then
            it returns -1 with errno set to EAGAIN. In that case,
            we exit the loop. */

        if (len <= 0)
            break;

        /* Loop over all events in the buffer. */

        for (char *ptr = buf; ptr < buf + len;
                ptr += sizeof(struct inotify_event) + event->len) {

            event = (const struct inotify_event *) ptr;

            /* Print event type. */

            if (event->mask & IN_OPEN)
                printf("IN_OPEN: ");
            if (event->mask & IN_CLOSE_NOWRITE)
                printf("IN_CLOSE_NOWRITE: ");
            if (event->mask & IN_CLOSE_WRITE)
                printf("IN_CLOSE_WRITE: ");

            /* Print the name of the watched directory. */
            if (mon_fd == event->wd) {
                printf("%s/", monitor_floder);
            }

            /* Print the name of the file. */

            if (event->len)
                printf("%s", event->name);

            /* Print type of filesystem object. */

            if (event->mask & IN_ISDIR)
                printf(" [directory]\n");
            else
                printf(" [file]\n");
        }
    }
}

int
main(int argc, char* argv[])
{
    // 需要监控的文件夹或者文件
    const char* monitor_floder = "/tmp";
    /* Create the file descriptor for accessing the inotify API. */
    // 设置成非阻塞模式
    int fd = inotify_init1(IN_NONBLOCK);
    if (fd == -1) {
        perror("inotify_init1");
        exit(EXIT_FAILURE);
    }

    /* Mark directories for events
        - file was opened
        - file was closed */
    int mon_fd = inotify_add_watch(fd, monitor_floder,
                                IN_OPEN | IN_CLOSE);//watch descriptors.
    if (mon_fd == -1) {
        fprintf(stderr, "Cannot watch '%s': %s\n",
                monitor_floder, strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Prepare for polling. */
    nfds_t nfds = 1;
    struct pollfd fds[1];
    fds[0].fd = fd;       /* Console input */
    fds[0].events = POLLIN;

    /* Wait for events and/or terminal input. */
    printf("Listening for events.\n");
    while (1) {
        int poll_num = poll(fds, nfds, -1);
        if (poll_num == -1) {
            if (errno == EINTR)
                continue;
            perror("poll");
            exit(EXIT_FAILURE);
        }

        if (poll_num > 0) {
            if (fds[0].revents & POLLIN) {
                /* Inotify events are available. */
                handle_events(fd, mon_fd, monitor_floder);
            }
        }
    }

    printf("Listening for events stopped.\n");

    /* Close inotify file descriptor. */

    close(fd);

    exit(EXIT_SUCCESS);
}