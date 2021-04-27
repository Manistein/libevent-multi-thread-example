#ifndef _listener_h_
#define _listener_h_

#include "common.h"

class Listener {
public:
	Listener() {};
	~Listener() {};

	bool init(event_base* base, int port, evconnlistener_cb cb);
	bool release();
private:
	event_base* m_base;
	evconnlistener* m_listener;
	int m_port;
	std::map<short, struct bufferevent*> m_buffervent_map;
};

#endif