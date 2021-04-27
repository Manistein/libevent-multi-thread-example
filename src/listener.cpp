#include "listener.h"

bool Listener::init(event_base* base, int port, evconnlistener_cb cb) {
	m_base = base;
	m_port = port;

	sockaddr_in sin = { 0 };
	sin.sin_family = AF_INET;
	sin.sin_port = htons(m_port);
	m_listener = evconnlistener_new_bind(m_base, cb, (void*)m_base, LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, -1, (sockaddr*)&sin, sizeof(sin));
	if (!m_listener) {
		std::cout << "can not create listener" << std::endl;
		return false;
	}

	return true;
}

bool Listener::release()
{
	evconnlistener_free(m_listener);
	m_listener = NULL;
	m_base = NULL;

	return true;
}