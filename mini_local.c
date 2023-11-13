#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>

enum {
    MAX_CLIENTS = 128,
	MAX_BUFFER = 4094,
	MSG_BUFFER = 100
};

int clients_indexes[MAX_CLIENTS] = {-1};
char *messages[MAX_CLIENTS];

fd_set fd_pool, fd_pool_write, fd_pool_read;

void ft_error()
{
	write(2,"Fatal error\n",strlen("Fatal error\n"));
	exit(1);
}

void send_message(int fd, char *str)
{
	for(int i = 0; i < MAX_CLIENTS;i++)
		if(clients_indexes[i] != -1 && i != fd && FD_ISSET(i,&fd_pool_write))
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

// Function to get the IP address as a string
char* getIPAddress() {
    // Run ifconfig and capture the output using popen
    FILE* ifconfig_output = popen("ifconfig eno2 | grep -oP 'inet \\K\\S+'", "r");
    if (ifconfig_output == NULL) {
        perror("Error running ifconfig");
        exit(1);
    }

    // Read the IP address from the output
    char ip_address[16];  // Assuming IPv4 address, adjust accordingly
    if (fgets(ip_address, sizeof(ip_address), ifconfig_output) == NULL) {
        perror("Error reading IP address");
        exit(1);
    }

    // Close the ifconfig process
    pclose(ifconfig_output);

    // Remove leading or trailing whitespaces
    size_t len = strlen(ip_address);
    if (len > 0 && ip_address[len - 1] == '\n') {
        ip_address[len - 1] = '\0';
    }

    // Allocate memory for the IP address and copy the value
    char* result = strdup(ip_address);
    if (result == NULL) {
        perror("Error allocating memory");
        exit(1);
    }

    return result;
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

		// exam Version
		// servaddr.sin_addr.s_addr = htonl(2130706433); // 127.0.0.1

		// Call the function to get the IP address
		char* ip_address = getIPAddress();

		// Convert the IP address to binary form
		if (inet_pton(AF_INET, ip_address, &(servaddr.sin_addr)) <= 0) {
			perror("Error converting IP address");
			free(ip_address);  // Free memory before exiting
			exit(1);
		}

		// Print the obtained IP address
		printf("IP Address: %s\n", ip_address);

		// Free memory
		free(ip_address);

		servaddr.sin_port = htons(atoi(av[1]));

		// Binding newly created socket to given IP and verification and listen
		if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) 
			ft_error();
		if (listen(sockfd, MAX_CLIENTS) != 0) ft_error();

		// init fd_pool
		FD_SET(sockfd,&fd_pool);
		int max_fd = sockfd;
		int index = 0;

		// main loop
		while(1)
		{
			fd_pool_read = fd_pool_write = fd_pool; // copy fd_pool

			if(select(max_fd+1,&fd_pool_read,&fd_pool_write,NULL,NULL) < 0) continue;

			for(int fd = 0; fd <= max_fd;fd++)
			{
				if(FD_ISSET(fd,&fd_pool_read))
				{
					if (fd == sockfd) // server socket, listens for new connections
					{
						// accept new connection and adding to fd_pool
						int new_client_fd = accept(sockfd,NULL,NULL); // accept new connection
						if (new_client_fd <= 0) continue;
						FD_SET(new_client_fd,&fd_pool);
						if(new_client_fd > max_fd) max_fd = new_client_fd; // update max

						// init client and message
						clients_indexes[new_client_fd] = index++; // storing the index of the client

						// communicate arrival to all clients
						char str[MSG_BUFFER];
						sprintf(str,"server: client %d just arrived\n",index-1);
						send_message(new_client_fd,str);
					}
					else // client socket, reads and sends messages
					{
						char buffer[MAX_BUFFER + 1];
						int bytes_read = recv(fd,buffer,MAX_BUFFER,0); // read from client

						if(bytes_read <= 0) // client disconnected
						{
							FD_CLR(fd,&fd_pool);
							char str[MSG_BUFFER];
							sprintf(str,"server: client %d just left\n",clients_indexes[fd]);
							send_message(fd,str);
							clients_indexes[fd] = -1;
							free(messages[fd]);
							close(fd);
 						}
						else // client sent a message, sharing it with all clients
						{
							buffer[bytes_read] = 0;
							messages[fd] = str_join(messages[fd],buffer);
							char *tmp;
							while(extract_message(&messages[fd],&tmp)) // extract messages from buffer, separated from newlines
							{
								char str[strlen(tmp) + MSG_BUFFER];
								sprintf(str,"client %d: %s",clients_indexes[fd],tmp);
								send_message(fd,str);
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

// int clients_indexes[]
// char *messages[]
// fd_set fd_pool

// if ac != 2 -> error

// int sockfd = socket()
// bind()
// listen()

// FD_SET(sockfd,fd_pool)
// int max_fd = sockfd
// int index = 0

// while(1)

// fd_pool_read = fd_pool_write = fd_pool
// select()

// for loop fd

// server socket
	// accept()
	// FD_SET(new_client_fd,fd_pool)
	// clients_indexes[new_client_fd] = index++
	// update max_fd

	// send message

// client socket

	// char buffer[]
	// recv()

	// if bytes_read <= 0

		// FD_CLR(fd,fd_pool)
		// clients_indexes[fd] = -1
		// close(fd)

	// else
	
		// messages[fd] = str_join(messages[fd],buffer)
		// while(extract_message(&messages[fd],&tmp))
			// send_message(fd,tmp)
			// free(tmp)





