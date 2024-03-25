#define HOST "127.0.0.1"
#define PORT 8080
#define BACKLOG 20
#define SERV_BUFF 4096
#define POOL_T 4

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>

using namespace std;
typedef int SOCKET;
class sock_queue
{
	struct node
	{
		SOCKET* sock;
		node* next;
	};
	node* head = nullptr;
	node* tail = nullptr;
public:
	int size = 0;
    void enqueue(SOCKET* s)
    {
        node *new_node = new node;
        new_node->sock = s;
        new_node->next = nullptr;
        if (tail == nullptr)
            head = new_node;
        else
            tail->next = new_node;

        size++;
        tail = new_node;
    }

    SOCKET* dequeue()
    {
        if (head == nullptr)
        {
            return nullptr;
        }
        else
        {
            SOCKET *sock = head->sock;

            node *temp = head;
            head = head->next;
            if (head == nullptr)
                tail = nullptr;

            delete temp;
            size--;
            return sock;
        }
    }
};

std::mutex thread_mtx;
std::condition_variable_any cv;
sock_queue pending;

unsigned char* ws_accept_key_hash(char* key_src)
{
	const char* magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	char key[128];
	strcpy(key, key_src);
	strcat(key, magic);
	unsigned char* sha_result = new unsigned char[SHA_DIGEST_LENGTH];
	SHA1(reinterpret_cast<const unsigned char*>(key), strlen(key), sha_result);

	return sha_result;
}

static const std::string base64_chars =
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";

std::string base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len) {
	std::string ret;
	int i = 0;
	int j = 0;
	unsigned char char_array_3[3];
	unsigned char char_array_4[4];

	while(in_len--)
	{
		char_array_3[i++] = *(bytes_to_encode++);
		if(i == 3)
		{
			char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
			char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
			char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
			char_array_4[3] = char_array_3[2] & 0x3f;

			for(i = 0; (i < 4); i++)
				ret += base64_chars[char_array_4[i]];
			i = 0;
		}
	}

	if(i)
	{
		for(j = i; j < 3; j++)
			char_array_3[j] = '\0';

		char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
		char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
		char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
		char_array_4[3] = char_array_3[2] & 0x3f;

		for(j = 0; (j < i + 1); j++)
			ret += base64_chars[char_array_4[j]];

		while((i++ < 3))
			ret += '=';
	}

	return ret;
}

void handle_conn(SOCKET* pclient)
{
	SOCKET pclient_socket = *pclient;
	delete pclient;
	char* buff = new char[SERV_BUFF] {};
	int bytes = recv(pclient_socket, buff, SERV_BUFF, 0);
	if (bytes < 0)
		cout << "Failed to read client request\n";

    const char* http_connection = "Connection: Upgrade";
    const char* http_upgrade = "Upgrade: websocket";

	std::string serverMessage = "";
	std::string response = "";

    if(strstr(buff, http_connection) && strstr(buff, http_upgrade))
    {
        cout << "Websocket connection!\n";
        

        char* ws_key_start = strstr(buff, "Sec-WebSocket-Key: ");
        ws_key_start += 19;

		char* ws_key_end = strchr(ws_key_start, '\r');
		if(!ws_key_end) ws_key_end = strchr(ws_key_start, '\n');
		if(!ws_key_end) cout << "No found websocket handshake key"; 
		size_t ws_key_len = ws_key_end - ws_key_start;

		char key[64];
		memcpy(key, ws_key_start, ws_key_len);
		key[ws_key_len] = '\0';

		unsigned char* sha1 = ws_accept_key_hash(key);

		cout << "Input: " << key << endl;
		std::stringstream ss;
    	for (int i = 0; i < SHA_DIGEST_LENGTH; ++i)
		{
    	    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(sha1[i]);
    	}
		cout << "Hash: " << ss.str() << endl;
		string base64 = base64_encode(sha1, 20);

        serverMessage = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " + base64 + "\r\nSec-WebSocket-Protocol: lws-minimal\r\nAccess-Control-Allow-Origin: *\r\n\n";
		serverMessage.append("\r\n");
		
		delete[] sha1;
    }
	else
	{
		serverMessage = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: ";
	    response = "<html><h1>Websocket server</h1></html>";
		serverMessage.append(std::to_string(response.size()));
		serverMessage.append("\r\n\r\n");
		serverMessage.append(response);
	}

	int bytes_send = 0;
	int total_send = 0;
	while (total_send < serverMessage.size())
	{
		bytes_send = send(pclient_socket, serverMessage.c_str(), serverMessage.size(), 0);

		if (bytes < 0)
        {
			cout << "Failed to send response\n";
            break;
        }
		total_send += bytes_send;
	}
    //close(pclient_socket);
	delete[] buff;
}

void thread_func()
{
	while (1)
	{
		SOCKET* pclient;
		thread_mtx.lock();
		if ((pclient = pending.dequeue()) == NULL)
		{
			cv.wait(thread_mtx);
			pclient = pending.dequeue();
		}
		thread_mtx.unlock();
		if (pclient != NULL)
			handle_conn(pclient);
	}
}

int main(int argc, char* argv[])
{
	SOCKET server_socket, client_socket;
	struct sockaddr_in serv_info {};
	std::thread pool[POOL_T];

	for (size_t i = 0; i < POOL_T; i++)
		pool[i] = std::thread(thread_func);

	server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (server_socket == -1) 
		cout << "Failed to create socket\n";

	serv_info.sin_addr.s_addr = inet_addr(HOST);
	serv_info.sin_addr.s_addr = INADDR_ANY;
	serv_info.sin_port = htons(PORT);
	size_t serv_size = sizeof(serv_info);

	if (bind(server_socket, (sockaddr*)&serv_info, serv_size) != 0) cout << "Failed to bind socket\n";
		
	if (listen(server_socket, BACKLOG) != 0) cout << "Failed to start listening\n";

    printf("Starting HTTP server\n");
	while (1)
	{
		client_socket = accept(server_socket, (sockaddr*)&serv_info, (socklen_t*)&serv_size);
		if (client_socket == -1)
			cout << "Failed to accept\n";
		SOCKET* pclient = new SOCKET;
		*pclient = client_socket;

		thread_mtx.lock();
		pending.enqueue(pclient);
		cv.notify_one();
		thread_mtx.unlock();
	}
	close(server_socket);
    close(client_socket);
	return 0;
}
