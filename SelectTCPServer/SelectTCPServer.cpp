#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>

#define SERVERPORT 9000
#define BUFSIZE    20

enum PROTOCOL
{
	LOGIN = 0,
};

// ���� ���� ������ ���� ����ü�� ����
struct SOCKETINFO
{
	SOCKET sock;
	char buf[BUFSIZE+1];
	char tempbuf[512];
	char* tempPtr;
	char loginresult[10];
	int recvbytes;
	int sendbytes;
	int totalbytes = 0;		// �޾ƾ��� �� ����
};

int nTotalSockets = 0;
SOCKETINFO *SocketInfoArray[FD_SETSIZE];

typedef struct _userinfo
{
	char id[20];
	char password[20];
}UserInfo;

UserInfo UserInfoArray[3] = 
{
	{"aa","11"},
	{"bbbb","2222"},
	{"cc","33"}
};

// ���� ���� �Լ�
BOOL AddSocketInfo(SOCKET sock);
void RemoveSocketInfo(int nIndex);

// ���� ��� �Լ�
void err_quit(char *msg);
void err_display(char *msg);

PROTOCOL GetProtocol(const char* _ptr)
{
	PROTOCOL protocol;
	memcpy(&protocol, _ptr, sizeof(PROTOCOL));

	return protocol;
}

void UnPack_userInfo(const char* _buf, UserInfo* user)
{
	const char* ptr = _buf + sizeof(PROTOCOL);
	int strsize;

	memcpy(&strsize, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(user->id, ptr, strsize);
	ptr = ptr + strsize;

	memcpy(&strsize, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(user->password, ptr, strsize);
	ptr = ptr + strsize;
}

int main(int argc, char *argv[])
{
	int retval;

	// ���� �ʱ�ȭ
	WSADATA wsa;
	if(WSAStartup(MAKEWORD(2,2), &wsa) != 0)
		return 1;

	// socket()
	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if(listen_sock == INVALID_SOCKET) err_quit("socket()");

	// bind()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = bind(listen_sock, (SOCKADDR *)&serveraddr, sizeof(serveraddr));
	if(retval == SOCKET_ERROR) err_quit("bind()");

	// listen()
	retval = listen(listen_sock, SOMAXCONN);
	if(retval == SOCKET_ERROR) err_quit("listen()");

	// �ͺ���ŷ �������� ��ȯ
	u_long on = 1;
	retval = ioctlsocket(listen_sock, FIONBIO, &on);
	if(retval == SOCKET_ERROR) err_display("ioctlsocket()");

	// ������ ��ſ� ����� ����
	FD_SET rset, wset;
	SOCKET client_sock;
	SOCKADDR_IN clientaddr;
	int addrlen, i;
	PROTOCOL protocol;

	UserInfo* user = new UserInfo();

	while(1){
		// ���� �� �ʱ�ȭ
		FD_ZERO(&rset);
		FD_ZERO(&wset);
		FD_SET(listen_sock, &rset);
		for(i=0; i<nTotalSockets; i++)
		{
			if(SocketInfoArray[i]->recvbytes > SocketInfoArray[i]->sendbytes)
				FD_SET(SocketInfoArray[i]->sock, &wset);
			else
				FD_SET(SocketInfoArray[i]->sock, &rset);
		}

		// select()
		retval = select(0, &rset, &wset, NULL, NULL);
		if(retval == SOCKET_ERROR) err_quit("select()");

		// ���� �� �˻�(1): Ŭ���̾�Ʈ ���� ����
		if(FD_ISSET(listen_sock, &rset))
		{
			addrlen = sizeof(clientaddr);
			client_sock = accept(listen_sock, (SOCKADDR *)&clientaddr, &addrlen);
			if(client_sock == INVALID_SOCKET)
			{
				err_display("accept()");
			}
			else
			{
				printf("\n[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n",
					inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
				// ���� ���� �߰�
				AddSocketInfo(client_sock);
			}
		}


		// ���� �� �˻�(2): ������ ���
		for(i=0; i<nTotalSockets; i++){
			SOCKETINFO *ptr = SocketInfoArray[i];
			if(FD_ISSET(ptr->sock, &rset))
			{
				// ������ �ޱ�
				retval = recv(ptr->sock, ptr->buf, BUFSIZE, 0);
				if(retval == SOCKET_ERROR)
				{
					err_display("recv()");
					RemoveSocketInfo(i);
					continue;
				}
				else if(retval == 0)
				{
					RemoveSocketInfo(i);
					continue;
				}				

				// �� ���� 0�϶� ���ο� ��Ŷ �޴°���
				if (ptr->totalbytes == 0)
				{
					memcpy(&ptr->totalbytes, ptr->buf, sizeof(int));		// �� ���� ����
					ptr->totalbytes += sizeof(int);
					//printf("���� ������ �� ����: %d\n", ptr->totalbytes);
					ptr->tempPtr = ptr->tempbuf;
					memcpy(ptr->tempbuf, ptr->buf, retval);
				}

				if (ptr->totalbytes > ptr->recvbytes)		// �� ���� ���� ���
				{
					ptr->recvbytes += retval;
					ptr->sendbytes = ptr->recvbytes;
					memcpy(ptr->tempPtr, ptr->buf, retval);			// �ӽù��ۿ� �̾���̱�
					ptr->tempPtr += retval;
					//printf("�̾���̱� %d ����Ʈ\n", retval);
				}
				
				if(ptr->totalbytes <= ptr->recvbytes)// �� ���� ���
				{
					ptr->tempPtr = ptr->tempbuf + sizeof(int);
					protocol = GetProtocol(ptr->tempPtr);	// �������� �и�

					UnPack_userInfo(ptr->tempPtr, user);

					for (int i = 0; i < 3; i++)
					{
						strcpy(ptr->loginresult, "�α��ν���");

						if (strcmp(UserInfoArray[i].id, user->id) == 0 &&
							strcmp(UserInfoArray[i].password, user->password) == 0)
						{
							strcpy(ptr->loginresult,"�α��μ���");
							break;
						}
					}

					ZeroMemory(user, sizeof(UserInfo));
					ptr->sendbytes = 0;
				}

				// ���� ������ ���
				//addrlen = sizeof(clientaddr);
				//getpeername(ptr->sock, (SOCKADDR *)&clientaddr, &addrlen);
				//ptr->buf[retval] = '\0';
				//printf("[TCP/%s:%d] %s\n", inet_ntoa(clientaddr.sin_addr),
					//ntohs(clientaddr.sin_port), ptr->buf);
			}
			if(FD_ISSET(ptr->sock, &wset))
			{
				printf("�����ͺ���\n");
				// ������ ������
				strcpy(ptr->buf, ptr->loginresult);
				retval = send(ptr->sock, ptr->buf + ptr->sendbytes, 
					ptr->recvbytes - ptr->sendbytes, 0);
				if(retval == SOCKET_ERROR)
				{
					err_display("send()");
					RemoveSocketInfo(i);
					continue;
				}
				ptr->sendbytes += retval;
				if(ptr->recvbytes == ptr->sendbytes){
					ptr->recvbytes = ptr->sendbytes = 0;
				}
			}
		}
	}

	// ���� ����
	WSACleanup();
	return 0;
}

// ���� ���� �߰�
BOOL AddSocketInfo(SOCKET sock)
{
	if(nTotalSockets >= FD_SETSIZE){
		printf("[����] ���� ������ �߰��� �� �����ϴ�!\n");
		return FALSE;
	}

	SOCKETINFO *ptr = new SOCKETINFO;
	if(ptr == NULL){
		printf("[����] �޸𸮰� �����մϴ�!\n");
		return FALSE;
	}

	ptr->sock = sock;
	ptr->recvbytes = 0;
	ptr->sendbytes = 0;
	ptr->totalbytes = 0;
	ptr->tempPtr = nullptr;
	ZeroMemory(ptr->tempbuf, sizeof(ptr->tempbuf));
	ZeroMemory(ptr->loginresult, sizeof(ptr->loginresult));
	SocketInfoArray[nTotalSockets++] = ptr;

	return TRUE;
}

// ���� ���� ����
void RemoveSocketInfo(int nIndex)
{
	SOCKETINFO *ptr = SocketInfoArray[nIndex];

	// Ŭ���̾�Ʈ ���� ���
	SOCKADDR_IN clientaddr;
	int addrlen = sizeof(clientaddr);
	getpeername(ptr->sock, (SOCKADDR *)&clientaddr, &addrlen);
	printf("[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n", 
		inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));

	closesocket(ptr->sock);
	delete ptr;

	if(nIndex != (nTotalSockets-1))
		SocketInfoArray[nIndex] = SocketInfoArray[nTotalSockets-1];

	--nTotalSockets;
}

// ���� �Լ� ���� ��� �� ����
void err_quit(char *msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(1);
}

// ���� �Լ� ���� ���
void err_display(char *msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	printf("[%s] %s", msg, (char *)lpMsgBuf);
	LocalFree(lpMsgBuf);
}