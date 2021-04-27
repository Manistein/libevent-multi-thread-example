#include "connection.h"
#include "event2/buffer_compat.h"
#include "event2/bufferevent_struct.h"

static void readcb(bufferevent* bev, void* ctx) {
	Connection* conn = (Connection*)ctx;
	conn->read_data(bev);
}

static void writecb(bufferevent* bev, void* ctx) {
	//TODO
}

static void eventcb(bufferevent* bev, short what, void* ctx) {
	Connection* conn = (Connection*)ctx;
	conn->handle_error(what);
}

bool Connection::init(event_base* base, evutil_socket_t fd, int bev_opt_events, int ip, int port, int mark) {
	m_mark = mark;

	m_ip = ip;
	m_port = port;

	m_base = base;

	m_bev = bufferevent_socket_new(m_base, fd, bev_opt_events);
	if (!m_bev) {
		return false;
	}

	bufferevent_setcb(m_bev, readcb, writecb, eventcb, this);
	m_readbuf = new char[READ_BUF_DEFAULT_SIZE];
	m_readbuf_size = READ_BUF_DEFAULT_SIZE;

	if (!m_readbuf) {
		return false;
	}
	m_should_read = HEAD_SIZE;
	m_readsize = 0;
	m_status = CONNECTION_READ_STATUS::READ_HEAD;
	m_fd = fd;

	m_read_compelte_cb = NULL;
	m_error_callback = NULL;

	return true;
}

void Connection::set_callback(read_complete_callback r_callback, conn_error_callback e_callback) {
	m_read_compelte_cb = r_callback;
	m_error_callback = e_callback;
}

void Connection::enable(int ev_events) {
	bufferevent_enable(m_bev, ev_events);
}

void Connection::disable(int ev_events) {
	bufferevent_disable(m_bev, ev_events);
}

void Connection::write(const void* data, size_t size) {
	if (!(m_bev->ev_write.ev_events & EV_WRITE)) {
		return;
	}

	bufferevent_write(m_bev, data, size);
}

bool Connection::release() {
	m_base = NULL;
	if (m_bev)
		bufferevent_free(m_bev);

	if (m_readbuf) {
		delete m_readbuf;
		m_readbuf = NULL;
	}
	m_readbuf_size = 0;

	return true;
}

void Connection::handle_error(int what) {
	if (m_error_callback)
		m_error_callback(m_fd, "handle_error");
}

void Connection::read_data(bufferevent* bev) {
	size_t data_size = evbuffer_get_length(bufferevent_get_input(bev));
	while (data_size > 0) {
		unsigned int rsize = bufferevent_read(bev, m_readbuf + m_readsize, m_should_read);
		m_should_read -= rsize;
		m_readsize += rsize;
		data_size -= rsize;

		if (m_should_read <= 0) {
			if (m_status == CONNECTION_READ_STATUS::READ_HEAD) {
				memcpy(&m_should_read, m_readbuf, HEAD_SIZE);
				m_should_read = ntohs(m_should_read);
				m_readsize = 0;
				m_status = CONNECTION_READ_STATUS::READ_DATA;
				
				if (m_should_read <= 0) {
					if (m_error_callback) {
						m_error_callback(m_fd, "read data error");
					}
					else {
						break;
					}
				}

				if (m_should_read >= MAX_PACKET_SIZE) {
					m_error_callback(m_fd, "packet too large");
				}

				if (m_should_read > m_readbuf_size) {
					do {
						m_readbuf_size *= 2;
					} while (m_should_read > m_readbuf_size);

					delete m_readbuf;
					m_readbuf = new char[m_readbuf_size];
					if (!m_readbuf) {
						m_error_callback(m_fd, "out of memory");
						abort();
					}
				}
			}
			else {
				if (m_read_compelte_cb)
					m_read_compelte_cb(m_fd, m_readbuf, m_readsize, this);
				m_should_read = HEAD_SIZE;
				m_readsize = 0;
				m_status = CONNECTION_READ_STATUS::READ_HEAD;
			}
		}
	}
}
