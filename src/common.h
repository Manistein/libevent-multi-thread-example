#ifndef _common_h_
#define _common_h_

#include <iostream>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>
#include <map>
#include <thread>

#ifdef WIN32
#include <windows.h>

void usleep(__int64 usec);
#else
#include <unistd.h>
#endif

#endif