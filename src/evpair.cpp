#include "evpair.h"

bool EVPair::init(event_base* base, int mark) {
	m_base = base;

	int fd[2];
	if (evutil_socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == -1) {
		return false;
	}

	evutil_make_socket_nonblocking(fd[0]);
	evutil_make_socket_nonblocking(fd[1]);

	m_reader = new Connection();
	m_reader->init(m_base, fd[0], BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE, 0, 0, mark);
	m_reader->enable(EV_READ);
	m_reader->disable(EV_WRITE);

	m_writer = new Connection();
	m_writer->init(m_base, fd[1], BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE, 0, 0, mark);
	m_writer->enable(EV_WRITE);
	m_writer->disable(EV_READ);

	return true;
}

bool EVPair::release() {
	m_reader->release();
	m_reader = NULL;

	m_writer->release();
	m_writer = NULL;

	return true;
}

Connection* EVPair::get_writer() {
	return m_writer;
}

Connection* EVPair::get_reader() {
	return m_reader;
}