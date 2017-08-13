/*
** selectserver.c -- a cheezy multiperson chat server
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PORT "9034"   // port we're listenering on
#define SOCK_PATH "echo_socket"
#define BUF_SIZE 1024

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa);

/* the code of linked list based on example from 
** http://www.thegeekstuff.com/2012/08/c-linked-list-example
*/
typedef struct clientList;
typedef struct	{
	int sockfd, clientid, clientStatus;
}Client;

char* buf_client(Client data);
void print_client(Client data);

typedef struct	{
	Client data;
	struct clientList* next;
}clientList;

clientList *head = NULL;
clientList *curr = NULL;
static int sizeOfList = 0;

clientList* create_list(Client val);
clientList* add_to_list(Client val, bool add_to_end);
clientList* search_in_list(int id, clientList **prev);
int delete_from_list(Client val);
void print_list(void);
void buf_list(char** buff);

int main(void)	{
    fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    int fdmax;        // maximum file descriptor number

    int listener, listener2, uds=0;		// listenering socket descriptors
    int newfd;        // newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // client address
    socklen_t addrlen;
	char *str[BUF_SIZE];
    char buf[BUF_SIZE];    // buffer for client data

    char buffer[BUF_SIZE];     
	int nbytes;
	int id, status=0;
	char remoteIP[INET6_ADDRSTRLEN];
    int yes=1;        // for setsockopt() SO_REUSEADDR, below
    int fd, fd2, j, rv, len, done, n, t;
	struct addrinfo hints, *ai, *p;
	Client val, *updt=NULL;
	struct sockaddr_un local, remote;	
    int i = 0, ret = 0;
    clientList *ptr = NULL;

    FD_ZERO(&master);    // clear the master and temp sets
    FD_ZERO(&read_fds);

	// get us a socket and bind it
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
		fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
		exit(1);
	}
	if ((listener2 = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}
	local.sun_family = AF_UNIX;
	strcpy(local.sun_path, SOCK_PATH);
	unlink(local.sun_path);
	len = strlen(local.sun_path) + sizeof(local.sun_family);
	if (bind(listener2, (struct sockaddr *)&local, len) == -1) {
		perror("bind");
		exit(1);
	}

	if (listen(listener2, 5) == -1) {
		perror("listen");
		exit(1);
	}
	FD_SET(listener2, &master);

	for(p = ai; p != NULL; p = p->ai_next) {
    	listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (listener < 0)	{
			continue;	
		}

		// lose the pesky "address already in use" error message
		setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
		if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
			close(listener);
			continue;
		}
		break;
	}

	// if we got here, it means we didn't get bound
	if (p == NULL) {
		fprintf(stderr, "selectserver: failed to bind\n");
		exit(2);
	}

	freeaddrinfo(ai); // all done with this

    // listen
    if (listen(listener, 10) == -1) {
        perror("listen");
        exit(3);
    }

    // add the listener to the master set
    FD_SET(listener, &master);

    // keep track of the biggest file descriptor
    fdmax = listener; // so far, it's this one

    // main loop
    for(;;) {
        read_fds = master; // copy it

		printf("fdmax=%d\n",fdmax);
		printf("Waiting...\n");
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }
		printf("Got something\n");
        // run through the existing connections looking for data to read
        for(i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // we got one!!
				if(i == listener)	{
	 	           // handle new connections
	 	           addrlen = sizeof remoteaddr;
					newfd = accept(listener,
					(struct sockaddr *)&remoteaddr,
					&addrlen);

					if (newfd == -1)
	                	perror("accept");
					else {
	            		FD_SET(newfd, &master); // add to master set
          	        	if (newfd > fdmax) {    // keep track of the max
	      	 	        	fdmax = newfd;
             	       	}
		  		       	printf("selectserver: new connection from %s on "
                         "socket %d\n",
						inet_ntop(remoteaddr.ss_family,
						get_in_addr((struct sockaddr*)&remoteaddr),
						remoteIP, INET6_ADDRSTRLEN),
						newfd);
				        // recv id from a client
	    	            if ((nbytes = recv(newfd, &id, 4, 0)) <= 0) {
	    		            // got error or connection closed by client
			                if (nbytes == 0) {
				                // connection closed
				                printf("selectserver: socket %d hung up\n", i);
		    	            } 
							else {
				                perror("recv");
		    	            }
	        	            close(i); // bye!
	        	            FD_CLR(i, &master); // remove from master set
						}
						else	{
							//we got if from the client
							printf("id = %d\n", id);	
							val.sockfd = newfd;
							val.clientid = id;
							val.clientStatus = 0;
							print_client(val);
							add_to_list(val, true);
				        }
					}
				}
				else if (i == listener2)	{
	 	          	t = sizeof(remote);
					if ((uds = accept(listener2, (struct sockaddr *)&remote, &t)) == -1) {
						perror("accept");
					}
					else {
						printf("UDS socket = %d\n", uds);
	            		FD_SET(uds, &master); // add to master set
          	        	if (uds > fdmax) {    // keep track of the max
	      	 	        	fdmax = uds;
             	       	}
					}
					printf("UDS Connected.\n");
					listener2 = 0;
        	        close(i); // bye!
        	        FD_CLR(i, &master); // remove from master set
				}
				else if (i == uds) {
					printf("hello uds\n");
					nbytes = recv(i, buf, BUF_SIZE, 0);
					if (nbytes <= 0) {
						// got error or connection closed by client
			            if (nbytes == 0) {
				        	// connection closed
				            printf("selectserver: socket %d hung up\n", i);
		    	        } 
						else {
				        	perror("recv");
		    	        }
						uds = 0;
	        	        close(i); // bye!
	        	        FD_CLR(i, &master); // remove from master set
					}
					printf("recv %s from uds\n", buf);
					if (0 == strncmp(buf, "whois", 5))	{
						for(i = 0; str[i] != NULL; i++)	{
							free(str[i]);
							str[i] = NULL;
						}
						buf_list(str);	
						for(i = 0; str[i] != NULL; i++)	{
							send(i, str[i], sizeof(str[i]), 0);
						}
					}
					else if (0 == strncmp(buf, "crit", 4))	{
						for(i = 0; str[i] != NULL; i++)	{
							free(str[i]);
							str[i] = NULL;
						}					
						for(j = 1; j <= 5; j++)	{
							ptr = search_in_list(i, NULL);
							updt = &(ptr->data);
							if(updt->clientStatus == j)	{
								str[0] = buf_client(*updt);
							 }
						}							
						for(i = 0; str[i] != NULL; i++)	{
							free(str[i]);
							str[i] = NULL;
						}
						buf_list(str);
						for(i = 0; str[i] != NULL; i++)	{
							send(i, str[i], sizeof(str[i]), 0);
						}
					}
					else if (0 == strncmp(buf, "grep ", 5))	{
						if((fd=open("db.txt",  O_WRONLY | O_CREAT | O_TRUNC))<0)	{
							perror("open");
							exit(1);
						}
						for(i = 0; str[i] != NULL; i++)	{
							free(str[i]);
							str[i] = NULL;
						}
						buf_list(str);
						for(i = 0; str[i] != NULL; i++)	{
							write(fd, str[i], sizeof(str[i]));
						}
						sprintf(buffer, "%s db.txt > grep.txt", buf);
						system(buffer);
						fd2=open("grep.txt",  O_RDONLY);
						while((n=read(fd2, buf, BUF_SIZE))>0)	{
							send(i, buf, n, 0);
						}
						close(fd);
						close(fd2);
					}
				}
				else	{ 
	                // handle data from a client
					if ((nbytes = recv(i, &status, 4, 0)) <= 0) {
    	            	// got error or connection closed by client
    	                if (nbytes == 0) {
    	             	   // connection closed
    	                   printf("selectserver: socket %d hung up\n", i);
							ptr = search_in_list(i, NULL);
							val = ptr->data;
							delete_from_list(val);
    	                } 
						else	{
    	                	perror("recv");
    	                }
    	                close(i); // bye!
    	                FD_CLR(i, &master); // remove from master set
    	            }
					else	{
						printf("status = %d\n", status);
						ptr = search_in_list(i, NULL);
						updt = &(ptr->data);
						updt->clientStatus = status;		
						print_client(ptr->data);
    	            } // END handle data from client
    	        } // END got new incoming connection
    	    } // END looping through file descriptors
    	} // END for(;;)--and you thought it would never end!
	}
    return 0;
}
void *get_in_addr(struct sockaddr *sa)	{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
char* buf_client(Client data)	{
	char* buff;
	buff = (char*)malloc(52);
	sprintf(buff, "sockfd = %d, clientid = %d, status = %d\n",
			data.sockfd, data.clientid, data.clientStatus);
	return buff;
}
void print_client(Client data)	{
//	printf("sockfd = %d, clientid = %d, status = %d\n",
//			data.sockfd, data.clientid, data.clientStatus);
	printf("%s", buf_client(data));
}
clientList* create_list(Client val)
{
    printf("\n creating list with headnode as [%d]\n", val.clientid);
    clientList *ptr = (clientList*)malloc(sizeof(clientList));
    if(NULL == ptr)
    {
        printf("\n Node creation failed \n");
        return NULL;
    }
    ptr->data = val;
    ptr->next = NULL;

    head = curr = ptr;
	sizeOfList++;
    return ptr;
}

clientList* add_to_list(Client val, bool add_to_end)	{
	printf("current size of list = %d\n", sizeOfList);
    if(NULL == head)	{
		printf("head = NULL\n");
        return (create_list(val));
    }

    if(add_to_end)
        printf("\n Adding node to end of list with value [%d]\n", val.clientid);
    else
        printf("\n Adding node to beginning of list with value [%d]\n", val.clientid);

    clientList *ptr = (clientList*)malloc(sizeof(clientList));
    if(NULL == ptr)	{
        printf("\n Node creation failed \n");
        return NULL;
    }
    ptr->data = val;
    ptr->next = NULL;

    if(add_to_end)	{
        curr->next = ptr;
        curr = ptr;
    }
    else	{
        ptr->next = head;
        head = ptr;
    }
	sizeOfList++;
	print_list();
    return ptr;
}

clientList* search_in_list(int id, clientList **prev)	{
    clientList *ptr = head;
    clientList *tmp = NULL;
    bool found = false;
	Client temp;

    printf("\n Searching the list for value [%d] \n", id);

    while(ptr != NULL)	{
		temp = ptr->data;
	    if(temp.sockfd == id || temp.clientid == id)	{
            found = true;
            break;
        }
        else	{
            tmp = ptr;
            ptr = ptr->next;
        }
    }

    if(true == found)	{
        if(prev)
            *prev = tmp;
        return ptr;
    }
    else	{
        return NULL;
    }
}

int delete_from_list(Client val)	{
    clientList *prev = NULL;
    clientList *del = NULL;

    printf("\n Deleting value [%d] from list\n", val.clientid);

    del = search_in_list(val.clientid, &prev);
    if(del == NULL)	{
        return -1;
		printf("\n delete [val = %d] failed, no such element found\n",val.clientid);
    }
    else	{
        if(prev != NULL)
            prev->next = del->next;
        if(del == curr)
            curr = prev;
        else if(del == head)
            head = del->next;
    }

    free(del);
    del = NULL;
	printf("\n delete [val = %d] passed \n", val.clientid);
	sizeOfList--;
	if(sizeOfList == 0)
		curr = head = NULL;
    return 0;
}

void print_list(void)	{
    clientList *ptr = head;

    printf("\n -------Printing list Start------- \n");
    while(ptr != NULL)	{
        print_client(ptr->data);
        ptr = ptr->next;
    }
    printf("\n -------Printing list End------- \n");

    return;
}

void buf_list(char** buff)	{
	clientList *ptr = head;
    clientList *tmp = NULL;
	Client temp;
	int i = 0;

    printf("\n Buffering the list \n");

    while(ptr != NULL)	{
		temp = ptr->data;
		buff[i] = buf_client(temp);

        tmp = ptr;
        ptr = ptr->next;
 		i++;
    }
}
