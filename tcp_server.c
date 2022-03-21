#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <conio.h>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN 4096
#define DEFAULT_PORT "1030"

enum {
	m_swallow = 0,
	m_mirror = 1
}e_mode;

int mode = m_swallow;
int port = 1030;

//------------------------------------------------------------------------------------------------------------------


int main(int argc, char *argv[])
{
    WSADATA wsaData;
    int iResult;

    SOCKET ListenSocket = INVALID_SOCKET;
    SOCKET ClientSocket = INVALID_SOCKET;

    struct addrinfo *result = NULL;
    struct addrinfo hints;

    char recvbuf[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;
	int wasParamError = 0;

	FD_SET ReadSet;

	//get params
	if (argc > 1){
		int i;
		for(i = 1; i < argc; i++){
			if(memcmp(argv[i], "-ms", 3) == 0){
				mode = m_swallow;
			}
			else if (memcmp(argv[i], "-mm", 3) == 0){
				mode = m_mirror;
			}
			else if (memcmp(argv[i], "-p=", 3) == 0){
				port = atoi(&argv[i][3]);
				if (port <= 0){
					printf("!invalid port value, using default 1030 \"%s\"\n", argv[i]);	
					port = 1030;
					wasParamError = 1;
				}
			}
			else {
				printf("!invalid parameter \"%s\"\n", argv[i]);
				wasParamError = 1;
			}
		}
		
	}
	if (argc == 1 || wasParamError){
		printf("*** usage: %s -ms/-mm(mode:swallow/mirror) -p=%d(listen port)\n", argv[0], port);
		if (wasParamError){
			return 1;
		}
	}

	printf("*** simple non-MT TCP server! Setup -> mode=%s, port=%d (ESC to exit)\n", mode == m_swallow ? "swallow" : "mirror", port);

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
	{
		char portStr[10];
		sprintf(portStr, "%d", port);
		iResult = getaddrinfo(NULL, portStr, &hints, &result);
		//(getaddrinfo output parameter is a linked list as result, which can have more results. Try until connect is successful!)
		if ( iResult != 0 ) {
			printf("getaddrinfo failed with error: %d\n", iResult);
			WSACleanup();
			return 1;
		}
	}

/*
	//testing result
	{
		struct addrinfo *next = result;
		while(next){
			printf("%s\n","(***test) getaddrinfo result check");
			printf("(***test) ai_flags:%d\n", next->ai_flags);
			printf("(***test) ai_family:%d\n", next->ai_family);
			printf("(***test) ai_socktype:%d\n", next->ai_socktype);
			printf("(***test) ai_protocol:%d\n", next->ai_protocol);
			printf("(***test) ai_addrlen:%d\n", next->ai_addrlen);
			printf("(***test) ai_canonname:%s\n", next->ai_canonname);
			printf("(***test) ai_addr->sa_family:%d\n", next->ai_addr->sa_family);
			printf("(***test) ai_addr->sa_data:%s\n", next->ai_addr->sa_data);
			next = next->ai_next;
		}
	}
*/
    // Create a SOCKET for connecting to server
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result);

	//-------------------------
	// Set the socket I/O mode: In this case FIONBIO
	// enables or disables the blocking mode for the 
	// socket based on the numerical value of iMode.
	// If iMode = 0, blocking is enabled; 
	// If iMode != 0, non-blocking mode is enabled.
	{
		u_long iMode = 1;
		iResult = ioctlsocket(ListenSocket, FIONBIO, &iMode);
		if (iResult != NO_ERROR){
			printf("ioctlsocket failed with error: %ld\n", iResult);
	        freeaddrinfo(result);
        	WSACleanup();
        	return 1;
		}
	}
  

    // Setup the TCP listening socket
    iResult = bind( ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

	freeaddrinfo(result);

    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

	printf("entering cycle\n");

    // Receive until the peer shuts down the connection
	fflush(stdin);
    do {
		ClientSocket = INVALID_SOCKET;
		Sleep(100);	//to have some free time to CPU
		if ((ClientSocket = accept(ListenSocket, NULL, NULL)) != INVALID_SOCKET){	//non-blocking accept
			printf("incoming\n");
			iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
			if (iResult > 0) {
				printf("Bytes received: %d\n", iResult);

				if (mode == m_mirror){
					int iSendResult;
					// Echo the buffer back to the sender
					iSendResult = send( ClientSocket, recvbuf, iResult, 0 );
					if (iSendResult == SOCKET_ERROR) {
						printf("send failed with error: %d\n", WSAGetLastError());
						break;
					}
					printf("Bytes sent: %d\n", iSendResult);
				}
			}
			else if (iResult == 0)
				printf("Connection closing...\n");
			else  {
				printf("recv failed with error: %d\n", WSAGetLastError());
				break;
			}

			// shutdown the connection since we're done
			iResult = shutdown(ClientSocket, SD_SEND);
			if (iResult == SOCKET_ERROR) {
				printf("shutdown #1 failed with error: %d\n", WSAGetLastError());
				break;
			}
			//close client socket
			closesocket(ClientSocket);
			
		}

    } while ( !(_kbhit() && _getch() == 27) /*iResult > 0*/);

	// cleanup
	if (ClientSocket != INVALID_SOCKET && ClientSocket != 0){
		iResult = shutdown(ClientSocket, SD_SEND);
		if (iResult == SOCKET_ERROR) {
			printf("shutdown #2 failed with error: %d\n", WSAGetLastError());
		}

		closesocket(ClientSocket);
	}

    closesocket(ListenSocket);
	WSACleanup();

    return 0;

}

//------------------------------------------------------------------------------------------------------------------
