#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <stdlib.h>
#include <stdio.h>

enum {
    MAX_CLIENT = 2000,
	MAX_BUFFER = 4094,
	MSG_BUFFER = 100
};

int clients[MAX_CLIENT] = {-1};
char *messages[MAX_CLIENT];

fd_set fd_pool, fd_pool_write, fd_pool_read;

void ft_error()
{
	write(2,"Fatal error\n",strlen("Fatal error\n"));
	exit(1);
}

void ft_send(int fd, char *str)
{
	for(int i = 0; i < MAX_CLIENT;i++)
		if(clients[i] != -1 && i != fd && FD_ISSET(i,&fd_pool_write))
			send(i,str,strlen(str),0);
}

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

int main(int ac , char **av) {
	if (ac == 2)
	{
		int sockfd;
		struct sockaddr_in servaddr;

		// socket create and verification 
		sockfd = socket(AF_INET, SOCK_STREAM, 0); 
		if (sockfd == -1) ft_error();
		bzero(&servaddr, sizeof(servaddr));

		// assign IP, PORT 
		servaddr.sin_family = AF_INET; 
		servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
		servaddr.sin_port = htons(atoi(av[1]));

		// Binding newly created socket to given IP and verification and listen
		if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) 
			ft_error();
		if (listen(sockfd, 128) != 0) ft_error();

		// init fd_pool
		FD_SET(sockfd,&fd_pool);
		int max = sockfd;
		int index = 0;

		// main loop
		while(1)
		{
			fd_pool_read = fd_pool_write = fd_pool; // copy fd_pool

			if(select(max+1,&fd_pool_read,&fd_pool_write,NULL,NULL) < 0) continue;

			for(int fd = 0; fd <= max;fd++)
			{
				if(FD_ISSET(fd,&fd_pool_read))
				{
					if (fd == sockfd) // server socket, listens for new connections
					{
						// accept new connection and adding to fd_pool
						int newClient = accept(sockfd,NULL,NULL); // accept new connection
						if (newClient <= 0) continue;
						FD_SET(newClient,&fd_pool);
						if(newClient > max) max = newClient; // update max

						// init client and message
						clients[newClient] = index++; // storing the index of the client

						// init message string
						//messages[newClient] = malloc(2);
						//messages[newClient][0] = 0;

						// communicate arrival to all clients
						char str[MSG_BUFFER];
						sprintf(str,"server: client %d just arrived\n",index-1);
						ft_send(newClient,str);
					}
					else // client socket, reads and sends messages
					{
						char buffer[MAX_BUFFER + 1];
						int bytes_read = recv(fd,buffer,MAX_BUFFER,0); // read from client
						if(bytes_read <= 0) // client disconnected
						{
							FD_CLR(fd,&fd_pool);
							char str[MSG_BUFFER];
							sprintf(str,"server: client %d just left\n",clients[fd]);
							ft_send(fd,str);
							clients[fd] = -1; // maybe would work even without this
							free(messages[fd]);
							close(fd);
 						}
						else
						{
							buffer[bytes_read] = 0;
							messages[fd] = str_join(messages[fd],buffer);
							char *tmp;
							while(extract_message(&messages[fd],&tmp)) // extract messages from buffer, separated from newlines
							{
								char str[strlen(tmp) + MSG_BUFFER];
								sprintf(str,"client %d: %s",clients[fd],tmp);
								ft_send(fd,str);
								free(tmp);
							}
						}
					}
				}
			}
		}
	}
    else
	{
		write(2, "Wrong number of arguments\n",strlen("Wrong number of arguments\n"));
		exit(1);
	}
}