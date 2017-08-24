/* simple-server.c
 *
 * Copyright (c) 2000 Sean Walton and Macmillan Publishers.  Use may be in
 * whole or in part in accordance to the General Public License (GPL).
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
*/

/*****************************************************************************/
/*** simple-server.c                                                       ***/
/***                                                                       ***/
/*****************************************************************************/

/**************************************************************************
*	This is a simple echo server.  This demonstrates the steps to set up
*	a streaming server.
**************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <resolv.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#define MY_PORT		9999
#define MAXBUF		1024
#define MAXCHUNKS	32
#define SIZEOFMALLOCS	0x100-8

const char *hellomessage = "***** Welcome to the Heap challenge 1 - UAF *****\n\0";
const char *optionsmessage = "\n[1] Allocate a chunk \n"
			     "[2] Free last chunk\n"
			     "[3] Set contents of last chunk\n"
			     "[4] Print contents of first chunk\n"
			     "[5] Exit and reinitialize everything\n\0";
const char *allfreemessage = "[-] Sorry d00d, all chunks are free...\n\0";
const char *allocmessage = "[+] Allocation done\n\0";
const char *freemessage = "[+] Free'd chunk\n\0";
const char *reqinputmessage = "[+] Enter your payload: \0";
const char *memcpymessage = "[+] Contents copied into chunk\n\0";
const char *nopemessage = "[+] You can't set the contents of the first chunk...\n\0";
const char *byemessage = "\n[+] Bye bye!\n\0";


void zechallenge(int clientfd)
{
	const char *f14g = "SP{nottheflag_d000000000000000000d}\n\0";
	int chunkpos = 0;
	char *flagmessage = malloc(SIZEOFMALLOCS);
	char buffer[MAXBUF];
	char ** strchunks[MAXCHUNKS]; // Pointer to pointer

	strchunks[0] = malloc(SIZEOFMALLOCS);
	*strchunks[0] = flagmessage;

	sprintf(flagmessage, "The flag is at %p\n", f14g);

	// Send hello message
	send(clientfd, hellomessage, strlen(hellomessage), 0); 

	int loopout = 0;
	while (!loopout)
	{
		if (send(clientfd, optionsmessage, strlen(optionsmessage), 0) < 0) break; 
		if (recv(clientfd, buffer, MAXBUF, 0))
		{
			// [1] Allocate a chunk 
			// [2] Free last chunk
			// [3] Set contents of last chunk
			// [4] Print contents of first allocated chunk - Hint: printf("%s\n", **strchunks[0])
			switch (buffer[0])
			{
				case '1':
					if (chunkpos < MAXCHUNKS)
					{// If we have free()'d 0, now we use the ones starting from 1
					 // Note that this will cause double free if someone free()'s 0 again
						if (chunkpos == -1) ++chunkpos; 
						strchunks[++chunkpos] = malloc(SIZEOFMALLOCS);
						send(clientfd, allocmessage, strlen(allocmessage), 0); 
					}
					break;
				case '2':
					if (chunkpos > -1)
					{
						free(strchunks[chunkpos--]);
						send(clientfd, freemessage, strlen(freemessage), 0); 
						break;
					}
				case '3':
					if (chunkpos > -1)
					{
						if (chunkpos == 0)
						{
							send(clientfd, nopemessage, strlen(nopemessage), 0); 
							break;
						}
						send(clientfd, reqinputmessage, strlen(reqinputmessage), 0); 
						if (recv(clientfd, buffer, 8, 0) > 0)
							memcpy(strchunks[chunkpos], (char*)buffer, 8);
						send(clientfd, memcpymessage, strlen(memcpymessage), 0); 
						break;
					}
				case '4':
					if (chunkpos > -1)
					{
						// Next line will crash on bad pointers
						char *maybeflag = *strchunks[0]; 
						send(clientfd, maybeflag, strlen(f14g), 0);
						break;
					} 
					// If we didn't break in 3 or 4 it means that all chunks are free 
					send(clientfd, allfreemessage, strlen(allfreemessage), 0);	
					break;
				case '5':
					loopout = 1;
					send(clientfd, byemessage, strlen(byemessage), 0); 
					exit(0);
					break;
			}
		} else break;
	}
	close(clientfd);
}

int main(int argc, char *argv[])
{
	errno = 0;
	char *p;
	struct sockaddr_in self;
	int port = MY_PORT;
	int clientfd;
	int option = 1;
	struct sockaddr_in client_addr;
	int addrlen=sizeof(client_addr);
	pid_t parent = getpid();
	pid_t child;

	if (argc == 2)
		port = strtol(argv[1], &p, 10);
	printf("[+] Listening port: %d\n", port);

	int sockfd;
	/*---Create streaming socket---*/
    if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
	{
		perror("Socket");
		exit(errno);
	}
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

	/*---Initialize address/port structure---*/
	bzero(&self, sizeof(self));
	self.sin_family = AF_INET;
	self.sin_port = htons(port);
	self.sin_addr.s_addr = INADDR_ANY;

	/*---Assign a port number to the socket---*/
    if ( bind(sockfd, (struct sockaddr*)&self, sizeof(self)) != 0 )
	{
		perror("socket--bind");
		exit(errno);
	}

	/*---Make it a "listening socket"---*/
	if ( listen(sockfd, 20) != 0 )
	{
		perror("socket--listen");
		exit(errno);
	}

	/*---Forever... ---*/
	/*---accept a connection (creating a data pipe)---*/
	while (1)
	{
		clientfd = accept(sockfd, (struct sockaddr*)&client_addr, &addrlen);
		printf("%s:%d connected\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
		child = fork();
		if (getpid() == parent)
		{
			close(clientfd);
		}
		else
		{
			zechallenge(clientfd);
		}
	}

	/*---Clean up (should never get here!)---*/
	close(sockfd);
	return 0;
}
