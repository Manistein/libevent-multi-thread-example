#ifndef _connection_h_
#define _connection_h_

#include "common.h"

#define HEAD_SIZE 2
#define MAX_PACKET_SIZE 65536
#define READ_BUF_DEFAULT_SIZE 64

enum class CONNECTION_READ_STATUS {
	READ_HEAD = 1,
	READ_DATA = 2,
};

typedef void (*read_complete_callback)(evutil_socket_t, void*, size_t, void*);
typedef void(*conn_error_callback)(evutil_socket_t, const char*);

class Connection {
public:
	Connection() {};
	~Connection() {};

	bool init(event_base* base, evutil_socket_t fd, int bev_opt_events, int ip, int port, int mark);
	void set_callback(read_complete_callback r_callback, conn_error_callback e_callback);
	void enable(int ev_events);
	void disable(int ev_events);
	void write(const void* data, size_t size);
	bool release();
	void handle_error(int what);

	void read_data(bufferevent* bev);
	int get_mark() { return m_mark; }
private:
	int m_mark;

	int m_ip;
	int m_port;

	event_base* m_base;
	struct bufferevent* m_bev;
	evutil_socket_t m_fd;
	char* m_readbuf;
	unsigned int m_readbuf_size;
	unsigned int m_readsize;
	unsigned int m_should_read;
	CONNECTION_READ_STATUS m_status;
	read_complete_callback m_read_compelte_cb;
	conn_error_callback m_error_callback;
};

#endif
