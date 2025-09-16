#define _GNU_SOURCE     /* Needed to get O_LARGEFILE definition */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fanotify.h>
#include <unistd.h>

size_t get_parent_dir(char* path, int mount_fd, struct file_handle* fh) {
	int parent_fd = open_by_handle_at(mount_fd, fh, O_PATH | O_DIRECTORY);

	if (parent_fd == -1) {
		if (errno == ESTALE) {
			printf("File handle is no longer valid. File has been deleted\n");
			return 0;
		} else {
			perror("open_by_handle_at");
			exit(EXIT_FAILURE);
		}
	}

	char procfd_path[PATH_MAX];
	snprintf(procfd_path, sizeof(procfd_path), "/proc/self/fd/%d", parent_fd);

	size_t path_len = readlink(procfd_path, path, PATH_MAX);
	close(parent_fd);
	return path_len;
}

static int format_mask(char *target, size_t size, uint64_t mask) {
	int written = 0;
	int first = 1;

	void append(const char* text) {
		if (!first) {
			written += snprintf(target + written, size - written, " | ");
		}
		written += snprintf(target + written, size - written, text);
		first = 0;
	}

	if (mask & FAN_ONDIR) {
		append("FAN_ONDIR");
	}
	if (mask & FAN_ATTRIB) {
		append("FAN_ATTRIB");
	}
	if (mask & FAN_CREATE) {
		append("FAN_CREATE");
	}
	if (mask & FAN_RENAME) {
		append("FAN_RENAME");
	}
	if (mask & FAN_DELETE) {
		append("FAN_DELETE");
	}
	if (mask & FAN_CLOSE_WRITE) {
		append("FAN_CLOSE_WRITE");
	}

	if (first) {
		append("-");
	}

	return written;
}


static void handle_events(int fafd, int mount_fd) {
	struct fanotify_event_metadata buf[200];
	while (true) {
		ssize_t size = read(fafd, buf, sizeof(buf));
		if (size == -1 && errno != EAGAIN) {
			perror("read");
			exit(EXIT_FAILURE);
		}

		if (size > 0) {
			const struct fanotify_event_metadata* metadata = buf;

			while (FAN_EVENT_OK(metadata, size)) {
				if (metadata->vers != FANOTIFY_METADATA_VERSION) {
					fprintf(stderr, "Mismatch of fanotify metadata version.\n");
					exit(EXIT_FAILURE);
				}

				char metadata_mask[128];
				format_mask(metadata_mask, sizeof(metadata_mask), metadata->mask);
				printf("Event mask: %s\n", metadata_mask);

				char *ptr = (char *)(metadata + 1);
				char *end = (char *)metadata + metadata->event_len;

				while (ptr < end) {
					struct fanotify_event_info_header *hdr = (struct fanotify_event_info_header *)ptr;

					if (hdr->info_type == FAN_EVENT_INFO_TYPE_DFID_NAME ||
						hdr->info_type == FAN_EVENT_INFO_TYPE_OLD_DFID_NAME ||
						hdr->info_type == FAN_EVENT_INFO_TYPE_NEW_DFID_NAME) {

						struct fanotify_event_info_fid *fid = (struct fanotify_event_info_fid *)hdr;
						struct file_handle *fh = (struct file_handle *)fid->handle;
						char parent_path[PATH_MAX];
						size_t parent_path_length = get_parent_dir(parent_path, mount_fd, fh);
						char *name = (char *)fh->f_handle + fh->handle_bytes;

						if (hdr->info_type == FAN_EVENT_INFO_TYPE_OLD_DFID_NAME)
							printf("  Old name: %.*s/%s\n", parent_path_length, parent_path, name);
						else if (hdr->info_type == FAN_EVENT_INFO_TYPE_NEW_DFID_NAME)
							printf("  New name: %.*s/%s\n", parent_path_length, parent_path, name);
						else
							printf("  Name: %.*s/%s\n", parent_path_length, parent_path, name);
					}

					ptr += hdr->len;
				}

				metadata = FAN_EVENT_NEXT(metadata, size);

			}
		} else {
			break;
		}

	}

}

int main(int argc, char *argv[]) {

	// Acquire arguments
	if (argc != 2) {
		fprintf(stderr, "Usage: %s MOUNT\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	char* mount_point = argv[1];

	// Open mount point for reading
	int mount_fd = open(mount_point, O_DIRECTORY | O_RDONLY);
	if (mount_fd == -1) {
		perror(mount_point);
		exit(EXIT_FAILURE);
	}

	// Create file descriptor for fanotify API
	int fafd = fanotify_init(FAN_CLOEXEC | FAN_CLASS_NOTIF | FAN_NONBLOCK | FAN_REPORT_FID | FAN_REPORT_DFID_NAME, O_RDONLY | O_LARGEFILE);
	if (fafd == -1) {
		perror("fanotify_init");
		exit(EXIT_FAILURE);
	}

	// Mark events for logging
	if (fanotify_mark(fafd, FAN_MARK_ADD | FAN_MARK_FILESYSTEM, FAN_ATTRIB | FAN_RENAME | FAN_CREATE | FAN_DELETE | FAN_CLOSE_WRITE | FAN_ONDIR, AT_FDCWD, mount_point) == -1) {
		perror("fanotify_mark");
		exit(EXIT_FAILURE);
	}

	// Setup file descriptors for epoll
	#define N_POLL_FDS 2
	struct pollfd poll_fds[N_POLL_FDS] = {
		{.fd = STDIN_FILENO, .events = POLLIN},
		{.fd = fafd, .events = POLLIN},
	};

	printf("Listening for events.\n");
	printf("Press enter key to terminate.\n");

	while (1) {
		int poll_num = poll(poll_fds, N_POLL_FDS, -1);
		if (poll_num == -1) {
			if (errno == EINTR)	// Interrupted by signal
				continue;
			perror("poll");     // Unexpected error
			exit(EXIT_FAILURE);
		}
		if (poll_num > 0) {
			if (poll_fds[0].revents & POLLIN) {	// Console input available
				char input_character;
				while (read(STDIN_FILENO, &input_character, 1) > 0 && input_character != '\n') continue;
				break;
			}
			if (poll_fds[1].revents & POLLIN) {	// fanotify events available
				handle_events(fafd, mount_fd);
			}
		}
	}
	printf("Listening for events stopped.\n");
	exit(EXIT_SUCCESS);
}
