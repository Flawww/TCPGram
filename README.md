# TCPGram
TCPGram is a library that offers UDP-like datagrams (individual packets) whilst using TCP.
This lets you do packet-based networking with the reliability that TCP offers without having to implement reliability for UDP.

## Requirements
TCPGram requires C++11 or later to compile, along with [boost ASIO](https://www.boost.org/doc/libs/1_74_0/doc/html/boost_asio.html)
It should compile on most platforms without problems.

## Usage and example
Including *tcp_server.h* and *tcpgram.h* setting up a a server is as easy as
```c++
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
	tcp_server server = tcp_server(PORT, accept_client_callback, read_socket_callback);
	server.run();
}
```

Likewise, using TCPGram for the client (Using boost ASIO for the socket)
```c++
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
```