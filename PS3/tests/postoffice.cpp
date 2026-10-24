#include <initializer_list>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <net/socket.h>
#include <net/poll.h>
#include <netinet/in.h>
#include "sock_impl.h"
static int connectResult, connectError, optionError, socketError, pollResult=1, closeCount, socketCount;
static short pollEvents=POLLOUT;
static int chunks[8], chunkCount, chunkIndex, ioError=EAGAIN;
static bool *cancelOnPoll;
extern "C" int socket(int family, int type, int proto) {
	assert(family==AF_INET && type==SOCK_STREAM && proto==6); ++socketCount; return 37|SOCKET_FD_MASK;
}
extern "C" int bind(int, const sockaddr *, socklen_t) { return 0; }
extern "C" int connect(int fd, const sockaddr *addr, socklen_t size) {
	assert(fd==(37|SOCKET_FD_MASK) && size==16 && addr->sa_len==16); errno=connectError; return connectResult;
}
extern "C" int setsockopt(int, int level, int opt, const void *value, socklen_t size) {
	assert(level==SOL_SOCKET && opt==SO_NBIO && size==4 && *(const int *)value==1); return optionError;
}
extern "C" int getsockopt(int, int level, int opt, void *value, socklen_t *size) {
	assert(level==SOL_SOCKET && opt==SO_ERROR && *size==4); *(int *)value=socketError; return 0;
}
extern "C" int close(int) { ++closeCount; return 0; }
extern "C" int poll(pollfd *fds, nfds_t count, int timeout) {
	assert(count==1 && (timeout==100 || timeout==0)); fds->revents=pollEvents;
	if(cancelOnPoll) *cancelOnPoll=true;
	return pollResult;
}
static int io(int size) {
	assert(chunkIndex<chunkCount); int result=chunks[chunkIndex++]; assert(result<=size); errno=ioError; return result;
}
extern "C" int send(int, const void *, int size, int flags) { assert(flags==0); return io(size); }
extern "C" int recv(int, void *, int size, int flags) { assert(flags==0 || flags==MSG_PEEK); return io(size); }
static void sequence(std::initializer_list<int> values) {
	chunkIndex=0; chunkCount=values.size(); int i=0; for(int value:values) chunks[i++]=value;
}
int main() {
	aemu_postoffice_sock_addr source{0x01020304,0x1234};
	assert(native_connect_tcp_sock(&source,nullptr,nullptr,nullptr,nullptr,nullptr)==(37|SOCKET_FD_MASK));
	connectResult=-1; connectError=EINPROGRESS;
	assert(native_connect_tcp_sock(&source,nullptr,nullptr,nullptr,nullptr,nullptr)==(37|SOCKET_FD_MASK));
	socketError=61; assert(native_connect_tcp_sock(&source,nullptr,nullptr,nullptr,nullptr,nullptr)==AEMU_POSTOFFICE_CLIENT_SESSION_NETWORK && closeCount==1);
	socketError=0; pollResult=0; assert(native_connect_tcp_sock(&source,nullptr,nullptr,nullptr,nullptr,nullptr)==AEMU_POSTOFFICE_CLIENT_SESSION_NETWORK && closeCount==2);
	pollResult=1; optionError=-1; assert(native_connect_tcp_sock(&source,nullptr,nullptr,nullptr,nullptr,nullptr)==AEMU_POSTOFFICE_CLIENT_SESSION_NETWORK && closeCount==3);
	optionError=0; int before=socketCount; assert(native_connect_tcp_sock(nullptr,nullptr,nullptr,nullptr,nullptr,nullptr)==AEMU_POSTOFFICE_CLIENT_SESSION_NETWORK && socketCount==before);
	bool cancelled = false;
	connectResult = -1; connectError = EINPROGRESS; pollResult = 0;
	cancelOnPoll = &cancelled;
	auto isCancelled = [](void *value) { return *static_cast<bool *>(value); };
	assert(native_connect_tcp_sock(&source, nullptr, nullptr, nullptr, isCancelled, &cancelled) == AEMU_POSTOFFICE_CLIENT_SESSION_NETWORK);
	assert(cancelled && closeCount == 4);
	cancelOnPoll = nullptr; pollResult = 1;
	char buf[8]{};
	sequence({-1}); assert(native_send(37,buf,4)==AEMU_POSTOFFICE_CLIENT_SESSION_WOULD_BLOCK);
	sequence({2}); assert(native_send(37,buf,4)==2);
	ioError=EINTR; sequence({-1,1}); assert(native_recv(37,buf,1)==1); ioError=EAGAIN;
	sequence({-1}); assert(!native_hung_up(37)); sequence({0}); assert(native_hung_up(37));
	pollEvents=POLLOUT; assert(native_send_buf_not_full(37)); pollEvents=POLLOUT|POLLERR; assert(!native_send_buf_not_full(37));
	puts("PS3 postoffice backend: IPv4, connect failure cleanup, partial IO, would-block, EOF, interruption and cancellation PASS (host API model)");
}
