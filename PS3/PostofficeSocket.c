// PS3 IPv4 backend for the pinned aemu_postoffice dependency.
#include <errno.h>
#include <net/net.h>
#include <netinet/in.h>
#include <net/poll.h>
#include <string.h>
#include <net/socket.h>
#include <unistd.h>
#include "sock_impl.h"

static void to_native_sock_addr(struct sockaddr_in *dst, const struct aemu_postoffice_sock_addr *src) {
	memset(dst, 0, sizeof(*dst));
	dst->sin_len = sizeof(*dst);
	dst->sin_family = AF_INET;
	dst->sin_addr.s_addr = src->addr;
	dst->sin_port = src->port;
}

int native_connect_tcp_sock(const struct aemu_postoffice_sock_addr *addr4,
	const struct aemu_postoffice_sock_addr *bind_addr4,
	const struct aemu_postoffice_sock6_addr *addr6,
	const struct aemu_postoffice_sock6_addr *bind_addr6,
	bool (*abort_connection)(void *), void *abort_arg) {
	if (!addr4 || addr6 || bind_addr6 || (abort_connection && abort_connection(abort_arg))) {
		return AEMU_POSTOFFICE_CLIENT_SESSION_NETWORK;
	}
	struct sockaddr_in address;
	to_native_sock_addr(&address, addr4);
	int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (fd < 0) {
		return AEMU_POSTOFFICE_CLIENT_SESSION_NETWORK;
	}
	int enabled = 1;
	int result;
	if (setsockopt(fd, SOL_SOCKET, SO_NBIO, &enabled, sizeof(enabled)) < 0) {
		goto failed;
	}
	if (bind_addr4) {
		struct sockaddr_in local;
		to_native_sock_addr(&local, bind_addr4);
		if (bind(fd, (struct sockaddr *)&local, sizeof(local)) < 0) {
			goto failed;
		}
	}
	result = connect(fd, (struct sockaddr *)&address, sizeof(address));
	if (result < 0) {
		if (errno != EINPROGRESS && errno != EALREADY && errno != EWOULDBLOCK && errno != EAGAIN) {
			goto failed;
		}
		for (int attempt = 0; attempt < 50; ++attempt) {
			if (abort_connection && abort_connection(abort_arg)) {
				goto failed;
			}
			struct pollfd descriptor = {fd, POLLOUT, 0};
			result = poll(&descriptor, 1, 100);
			if (result < 0 && errno == EINTR) {
				continue;
			}
			if (result < 0 || (descriptor.revents & (POLLHUP | POLLNVAL))) {
				goto failed;
			}
			if (result > 0 && (descriptor.revents & (POLLOUT | POLLERR))) {
				int error = 0;
				socklen_t length = sizeof(error);
				if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) < 0 || error) {
					goto failed;
				}
				return fd;
			}
		}
		goto failed;
	}
	return fd;
failed:
	close(fd);
	return AEMU_POSTOFFICE_CLIENT_SESSION_NETWORK;
}

int native_close_tcp_sock(int fd) { return close(fd); }

static int receive(int fd, char *buf, int len, int flags) {
	int result;
	do { result = recv(fd, buf, len, flags); } while (result < 0 && errno == EINTR);
	if (result < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
		return AEMU_POSTOFFICE_CLIENT_SESSION_WOULD_BLOCK;
	return result;
}
int native_recv(int fd, char *buf, int len) { return receive(fd, buf, len, 0); }
int native_peek(int fd, char *buf, int len) { return receive(fd, buf, len, MSG_PEEK); }

int native_send(int fd, const char *buf, int len) {
	int result;
	do {
		result = send(fd, buf, len, 0);
	} while (result < 0 && errno == EINTR);
	if (result < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
		return AEMU_POSTOFFICE_CLIENT_SESSION_WOULD_BLOCK;
	}
	return result;
}
bool native_send_buf_not_full(int fd) {
	struct pollfd descriptor = {fd, POLLOUT, 0};
	return poll(&descriptor, 1, 0) > 0 && (descriptor.revents & POLLOUT)
		&& !(descriptor.revents & (POLLHUP | POLLERR | POLLNVAL));
}
bool native_hung_up(int fd) {
	char byte;
	int result = native_peek(fd, &byte, 1);
	return result != 1 && result != AEMU_POSTOFFICE_CLIENT_SESSION_WOULD_BLOCK;
}
