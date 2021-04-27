// multi-thread-example-for-libevent.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <thread>
#include <event2/event.h>
#include <event2/thread.h>

#include "listener.h"
#include "connectionmgr.h"
#include "evpair.h"

#define WORKER_THREAD_NUM 4

static ConnectionMgr* s_conn_mgr = NULL;
static std::thread* s_thread[WORKER_THREAD_NUM] = { 0 };
static EVPair* s_main_evpair = NULL;
static EVPair* s_evpair[WORKER_THREAD_NUM] = { 0 };

// mt is the abbreviation of main thread
// wt means worker thread
static void mt_connection_error_callback(evutil_socket_t fd, const char* text) {
	std::cout << fd << " " << text << std::endl;
	s_conn_mgr->close(fd);
}

static void mt_conn_read_complete(evutil_socket_t fd, void* data, size_t size, void* user_data) {
	char* temp = new char[size + 1];
	memcpy(temp, data, size + 1);
	temp[size] = '\0';
	std::cout << temp << " " << size << std::endl;
	delete temp;
	temp = NULL;

	size_t new_size = size + 2;
	unsigned char* new_data = new unsigned char[size + HEAD_SIZE + 2];
	int new_header = 0;
	memcpy(&new_header, &new_size, HEAD_SIZE);
	new_header = htons(new_header);
	
	memcpy(new_data, &new_header, HEAD_SIZE);
	new_data[HEAD_SIZE] = (fd & 0xffff) >> 8;
	new_data[HEAD_SIZE + 1] = fd & 0xff;

	memcpy(new_data + HEAD_SIZE + 2, data, size);

	for (int i = 0; i < WORKER_THREAD_NUM; i++) {
		EVPair* evpair = s_evpair[i];
		Connection* conn = evpair->get_writer();
		conn->write(new_data, size + HEAD_SIZE + 2);
	}

	delete new_data;
}

static void
mt_listener_cb(struct evconnlistener *listener, evutil_socket_t fd,
	struct sockaddr *sa, int socklen, void *user_data)
{
	char temp[512] = { 0 };
	sprintf_s(temp, "new connection accept %d", fd);
	std::cout << temp << std::endl;

	sockaddr_in* sin = (sockaddr_in*)sa;
	int port = sin->sin_port;

	int ip = inet_pton(AF_INET, sa->sa_data, &(sin->sin_addr));
	s_conn_mgr->open(fd, BEV_OPT_CLOSE_ON_FREE, ip, port);
	Connection* conn = s_conn_mgr->get_by(fd);
	conn->set_callback(mt_conn_read_complete, mt_connection_error_callback);
}

static void wt_evpair_error_callback(evutil_socket_t fd, const char* text) {
	char temp[512] = { 0 };
	sprintf_s(temp, "%d:%s", fd, text);
	std::cout << temp << std::endl;

}

static void wt_evpair_read_complete(evutil_socket_t self_fd, void* data, size_t size, void* user_data) {
	Connection* conn = (Connection*)user_data;
	unsigned char* byte = (unsigned char*)data;
	evutil_socket_t fd = byte[0] << 8 | byte[1];

	if (size <= 1024) {
		char temp[1025] = { 0 };
		memcpy(temp, byte + 2, size);
		temp[size - 2] = '\0';
		std::cout << temp << std::endl;
	}

	char tmp[256];
	sprintf_s(tmp, "mark:%d answer", conn->get_mark());
	unsigned int answer_size = strlen(tmp) + 2;
	unsigned int n_answer_size = 0;
	n_answer_size = htons(answer_size);

	unsigned char answer[512] = { 0 };
	memcpy(answer, &n_answer_size, HEAD_SIZE);
	answer[HEAD_SIZE] = (fd & 0xffff) >> 8;
	answer[HEAD_SIZE + 1] = (fd & 0xff);

	memcpy(answer + HEAD_SIZE + 2, tmp, answer_size - 2);
	s_main_evpair->get_writer()->write(answer, answer_size + HEAD_SIZE);

	// conn->write(answer, answer_size + HEAD_SIZE + 2);
}

static void worker_thread(int thread_num) {
	int t_num = thread_num;
	char temp[512] = { 0 };
	sprintf_s(temp, "%s:%d %s thread_id:%d\n", "worker", t_num, "start", std::this_thread::get_id());
	std::cout << temp << std::endl;

	event_base* base = event_base_new();
	EVPair* evpair = new EVPair();
	evpair->init(base, t_num);
	evpair->get_reader()->set_callback(wt_evpair_read_complete, wt_evpair_error_callback);
	evpair->get_writer()->set_callback(NULL, wt_evpair_error_callback);
	s_evpair[t_num] = evpair;

	while (1) {
		event_base_dispatch(base);
		usleep(1);
	}

	evpair->release();
	delete evpair;

	event_base_free(base);
}

static void create_worker_threads() {
	for (int i = 0; i < WORKER_THREAD_NUM; i++) {
		s_thread[i] = new std::thread(std::bind(worker_thread, i));
	}
}

static void destory_worker_threads() {
	for (int i = 0; i < WORKER_THREAD_NUM; i++) {
		std::thread* th = s_thread[i];
		th->join();
		delete th;
	}
}

static void mt_evpair_error_callback(evutil_socket_t self_fd, const char* text) {
	std::cout << self_fd << " " << text << std::endl;
}

static void mt_evpair_read_complete(evutil_socket_t self_fd, void* data, size_t size, void* user_data) {
	unsigned char* byte = (unsigned char*)data;
	int client_fd = byte[0] << 8 | byte[1];

	Connection* conn = s_conn_mgr->get_by(client_fd);
	if (conn) {
		size_t send_size = size - 2;
		byte[0] = (send_size & 0xff00) >> 8;
		byte[1] = (send_size & 0xff);
		conn->write(byte, size);
	}
}

static void run_server() {
#ifdef WIN32
	evthread_use_windows_threads();
#else
	evthread_use_pthreads();
#endif

	event_base* base = event_base_new();

	s_main_evpair = new EVPair();
	s_main_evpair->init(base, -1);
	s_main_evpair->get_reader()->set_callback(mt_evpair_read_complete, mt_evpair_error_callback);
	s_main_evpair->get_writer()->set_callback(NULL, mt_evpair_error_callback);

	Listener* listener = new Listener();
	// listen and bind 127.0.0.1:8888
	listener->init(base, 8888, mt_listener_cb);

	s_conn_mgr = new ConnectionMgr();
	s_conn_mgr->init(base);

	create_worker_threads();

	event_base_dispatch(base);

	destory_worker_threads();

	s_conn_mgr->release();
	delete s_conn_mgr;

	listener->release();
	delete listener;

	s_main_evpair->release();
	delete s_main_evpair;

	event_base_free(base);
}

//-------------------------client---------------------------
static void client_error_callback(evutil_socket_t fd, const char* text) {
	std::cout << fd << " " << text << std::endl;
}

static void client_read_complete(evutil_socket_t fd, void* data, size_t size, void* user_data) {
	char* byte = (char*)data;
	char temp[512] = { 0 };
	memcpy(temp, data, size);
	temp[size] = '\0';

	std::cout << temp << std::endl;
}

static void run_client() {
	event_base* base = event_base_new();

	const char* host = "127.0.0.1";
	char portstr[16];
	sprintf_s(portstr, "%d", 8888);

	struct addrinfo ai_hints;
	struct addrinfo* ai_list = NULL;

	memset(&ai_hints, 0, sizeof(ai_hints));
	ai_hints.ai_family = AF_INET;
	ai_hints.ai_socktype = SOCK_STREAM;
	ai_hints.ai_protocol = IPPROTO_TCP;

	int status = getaddrinfo(host, portstr, &ai_hints, &ai_list);
	if (status != 0) {
		std::cout << "getaddrinfo failure" << std::endl;
		return;
	}

	evutil_socket_t conn_fd = socket(ai_list->ai_family, ai_list->ai_socktype, ai_list->ai_protocol);
	if (conn_fd < 0) {
		std::cout << "create socket failure" << std::endl;
		return;
	}

	if (connect(conn_fd, ai_list->ai_addr, ai_list->ai_addrlen) < 0) {
		std::cout << "can not connect to server " << errno << std::endl;
		return;
	}

	Connection* conn = new Connection();
	conn->init(base, conn_fd, BEV_OPT_CLOSE_ON_FREE, 0, 8888, -3);
	conn->set_callback(client_read_complete, client_error_callback);
	conn->enable(EV_READ | EV_WRITE);

	unsigned char request[512] = { 'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', '\0' };
	int req_size = strlen((const char*)request) + 1;
	char* temp = new char[req_size + HEAD_SIZE];
	int header = 0;
	header = htons(req_size);
	memcpy(temp, &header, HEAD_SIZE);
	memcpy(temp + HEAD_SIZE, request, req_size);
	conn->write(temp, req_size + 2);
	delete temp;

	event_base_dispatch(base);

	conn->release();
	delete conn;

	event_base_free(base);
}

// input -s to start server
// input -c to start client
int main(int argc, char** argv) {
	if (argc < 2) {
		std::cout << "input -s to start server" << std::endl;
		std::cout << "input -c to start client" << std::endl;
		return 0;
	}

	char opt = argv[1][0];
	if (opt != '-') {
		std::cout << "arg error" << std::endl;
		std::cout << "input -s to start server" << std::endl;
		std::cout << "input -c to start client" << std::endl;
		return 0;
	}

	char cmd = argv[1][1];

#ifdef WIN32
	WSADATA wsa_data;
	WSAStartup(0x0201, &wsa_data);
#endif

	if (cmd == 's') {
		run_server();
	}
	else if (cmd == 'c') {
		run_client();
	}
	else {
		std::cout << "input -s to run server and -c to run client" << std::endl;
	}

	return 1;
}
