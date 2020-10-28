#pragma once
#include <functional>
#include <vector>
#include <boost/asio.hpp>
#include <boost/array.hpp>

using boost::asio::ip::tcp;

/**
* Non-blocking multi-client TCP server
*/
class tcp_server {
public:
	/**
	* @param listen_port port to start the server on
	* @param accept_callback callback function that is called when a client is accepted
	* @param process_socket_callback callback function that is called to recieve data on the socket in question
	*/
	tcp_server(int listen_port, std::function<bool(tcp::socket*)> accept_callback, std::function<bool(tcp::socket*)> process_socket_callback) : m_running(true), m_acceptor(tcp::acceptor(m_io_context, tcp::endpoint(tcp::v4(), listen_port))) {
		m_acceptor.non_blocking(true);

		m_accept_callback = accept_callback;
		m_process_socket_callback = process_socket_callback;
	};

	~tcp_server() {
		for (auto& it : m_connected_clients) {
			try {
				it->shutdown(boost::asio::socket_base::shutdown_both);
				it->close();
			}
			catch (boost::system::system_error& e) {
				printf("Error when closing socket: %s\n", e.what());
			}

			delete it;
		}

		m_connected_clients.clear();
	}

	/**
	* @brief main run loop which accept clients and reads sockets
	*/
	void run() {
		while (m_running) {
			accept_clients();
			read_sockets();
		}
	}

	/**
	* @brief loops all sockets and calls the recieve callback for each one
	*/
	void read_sockets() {
		std::vector<tcp::socket*> to_remove;

		// recieve all sockets
		for (auto& it : m_connected_clients) {
			if (!m_process_socket_callback(it)) {
				to_remove.push_back(it);
			}
		}

		boost::system::error_code error;
		for (auto& it : to_remove) {
			it->shutdown(boost::asio::socket_base::shutdown_both, error);
			it->close(error);

			// now remove it from our connected clients
			remove_client(it);
		}
	}

	/**
	* @brief routine which accepts clients
	*/
	void accept_clients() {
		auto close_socket = [](tcp::socket* socket) {
			boost::system::error_code error; // consume errors
			socket->shutdown(boost::asio::socket_base::shutdown_both, error);
			socket->close(error);
			delete socket;
		};

		boost::system::error_code error;

		bool accept = true;
		while (accept) {
			tcp::socket* sock = new tcp::socket(m_io_context);

			m_acceptor.accept(*sock, error);
			if (error) {
				delete sock;
				break;
			}

			if (!m_accept_callback(sock)) {
				close_socket(sock);
				continue;
			}

			sock->non_blocking(true, error);
			if (error) {
				close_socket(sock);
				continue;
			}

			m_connected_clients.push_back(sock);
		}
	}

	/**
	* @brief removes and frees a socket client from the connected clients vector
	* 
	* @param sock socket to free and remove
	*/
	void remove_client(tcp::socket* sock) {
		for (auto it = m_connected_clients.begin(); it != m_connected_clients.end(); it++) {
			if (*it == sock) {
				delete *it;
				m_connected_clients.erase(it);
				break;
			}
		}
	}

	bool m_running;
private:
	boost::asio::io_context m_io_context;
	tcp::acceptor m_acceptor;
	std::vector<tcp::socket*> m_connected_clients;

	std::function<bool(tcp::socket*)> m_accept_callback;
	std::function<bool(tcp::socket*)> m_process_socket_callback;
};