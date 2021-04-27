#include "connectionmgr.h"

bool ConnectionMgr::init(event_base* base) {
	m_base = base;
	return true;
}

bool ConnectionMgr::release() {
	for (auto iter = m_fd2conn.begin(); iter != m_fd2conn.end(); iter++) {
		Connection* conn = iter->second;
		conn->release();
		delete conn;
	}
	m_fd2conn.clear();

	return true;
}

bool ConnectionMgr::open(evutil_socket_t fd, int bev_opt_events, int ip, int port) {
	auto iter = m_fd2conn.find(fd);
	if (iter != m_fd2conn.end()) {
		Connection* conn = iter->second;
		conn->release();
		delete conn;

		m_fd2conn.erase(fd);
	}

	Connection* new_conn = new Connection();
	new_conn->init(m_base, fd, bev_opt_events, ip, port, -2);
	new_conn->enable(EV_READ | EV_WRITE);
	m_fd2conn[fd] = new_conn;

	return true;
}

void ConnectionMgr::close(evutil_socket_t fd) {
	auto iter = m_fd2conn.find(fd);
	if (iter == m_fd2conn.end())
		return;

	Connection* conn = iter->second;
	conn->release();
	delete conn;

	m_fd2conn.erase(fd);
}

Connection* ConnectionMgr::get_by(evutil_socket_t fd) {
	auto iter = m_fd2conn.find(fd);
	if (iter == m_fd2conn.end())
		return NULL;

	return iter->second;
}