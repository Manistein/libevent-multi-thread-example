#ifndef _connectionmgr_h_
#define _connectionmgr_h_

#include "common.h"
#include "connection.h"

class ConnectionMgr {
public:
	ConnectionMgr() {};
	~ConnectionMgr() {};

	bool init(event_base* base);
	bool release();

	bool open(evutil_socket_t fd, int bev_opt_events, int ip, int port);
	void close(evutil_socket_t fd);
	Connection* get_by(evutil_socket_t fd);
private:
	event_base* m_base;
	std::map<evutil_socket_t, Connection*> m_fd2conn;
};

#endif