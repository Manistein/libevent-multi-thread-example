#ifndef _evpair_h_
#define _evpair_h_

#include "common.h"
#include "connection.h"

class EVPair {
public:
	EVPair() {};
	~EVPair() {};

	bool init(event_base* base, int mark);
	bool release();

	Connection* get_writer();
	Connection* get_reader();
private:
	event_base* m_base;
	Connection* m_reader;
	Connection* m_writer;
};

#endif