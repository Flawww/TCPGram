#include <iostream>
#include <unordered_map>
#include "../TCPGram/tcpgram.h"
#include "../TCPGram/tcp_server.h"

#define IP "127.0.0.1"
#define PORT 42024

// CLIENT CODE
bool process_datagram_client(tcp::socket* sock, raw_message* msg) {
	printf("CLIENT Recieved datagram of %X bytes starting with %X\n", msg->m_size, msg->m_buf[0]);
	return true;
}

void start_client() {
	boost::asio::io_context ctx;
	auto socket = new tcp::socket(ctx);
	
	boost::system::error_code error;
	boost::array<uint8_t, MAX_BUFLEN> buf;

	socket->connect(tcp::endpoint(boost::asio::ip::address::from_string(IP, error), PORT), error);
	if (error) {
		printf("Failed to connect to server.\n");
	}

	tcpgram dgram = tcpgram(socket, process_datagram_client);

	auto msg = raw_message();
	msg.put<uint8_t>(10); // fill the message with random stuff
	msg.put_raw(0x6000);

	dgram.send(&msg);

	while (true) // just keep trying to recieve
		dgram.recieve();
}

// "SERVER" CODE
std::unordered_map<tcp::socket*, tcpgram*> g_clients;

// called to process a recieved datagram
bool process_datagram_server(tcp::socket* sock, raw_message* msg) {
	printf("SERVER Recieved datagram of %X bytes starting with %X, sending packet back...\n", msg->m_size, msg->m_buf[0]);
	g_clients[sock]->send(msg);

	return true;
}

// called when a new client is accepted
bool accept_client_callback(tcp::socket* sock) {
	g_clients[sock] = new tcpgram(sock, process_datagram_server, 0x8000);
	return true;
}

// called from tcp_server to recieve on a socket
bool read_socket_callback(tcp::socket* sock) {
	return g_clients[sock]->recieve();
}


void start_server() {
	tcp_server* server = new tcp_server(PORT, accept_client_callback, read_socket_callback);
	server->run();
	delete server;
}

int main()
{
	start_server();
	//start_client();

	std::getchar();
	return 0;
}