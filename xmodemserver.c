#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/signal.h>
#include <unistd.h>
#include "xmodemserver.h"
#include "crc16.h"

#ifndef PORT
#define PORT 50429
#endif

void *top = NULL;
int listenfd = 0;


void bindandlisten(){
    
    struct sockaddr_in socket_address;
    int yes = 1;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        perror("socket");
        exit(1);
    }
    
    memset(&socket_address, '\0', sizeof socket_address);
    socket_address.sin_family = AF_INET;
    socket_address.sin_addr.s_addr = INADDR_ANY;
    socket_address.sin_port = htons(PORT);
    
    if(bind(listenfd, (struct sockaddr *)&socket_address, sizeof(socket_address)) == -1){
        perror("bind");
        exit(1);
    }
    
    if (listen(listenfd, 5) == -1){
        perror("listen");
        exit(1);
    }
    if((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) == -1) {
        perror("setsockopt");
    }
}

FILE *open_file_in_dir(char *filename, char *dirname) {//file helper
    char buffer[1024];
    strncpy(buffer, "./", 1024);
    strncat(buffer, dirname, 1024 - strlen(buffer));
    
    // create the directory dirname. Fail silently if directory exists
    if(mkdir(buffer, 0700) == -1) {
        if(errno != EEXIST) {
            perror("mkdir");
            exit(1);
        }
    }
    strncat(buffer, "/", 1024 - strlen(buffer));
    strncat(buffer, filename, 1024 - strlen(buffer));
    
    return fopen(buffer, "wb");
}
/* Search the first inbuf characters of buf for a network newline
 * Return the location of the '\r' if the network newline is found,
 * or -1 otherwise.
 */
int find_network_newline(char *buf, int filename_length) //taken from exercise 7
{
    int i;
    for (i = 0; i <= filename_length; i++){
        if (buf[i] == '\r' && buf[i+1] == '\n'){
            return i;
        }
    }
    return -1;
}

/* Remove the client from the linked list and also clear the descriptor
 from fd_set
 */
static void removeclient(int fd) // taken muffin man
{   printf("Removing Client Removed");
    struct client **p;
    for (p = (struct client**)&top; *p && (*p)->fd != fd; p = &(*p)->next)
        ;
    if (*p) {
        struct client *t = (*p)->next;
        fflush(stdout);
        free(*p);
        *p = t;
    } else {
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n", fd);
        fflush(stderr);
    }
    return;
}

void newconnection(int descriptor, struct client *p, fd_set fdlist){//this is my inital state, never directly set p->state to inital there is no need in my implementation
    char *dir = "filestore";
    int filename_length = 0;
    p = malloc(sizeof(struct client)); // make new client
    printf("Adding Client with FD %d\n",p->fd);
    int fd;
    struct sockaddr_in peer;
    socklen_t socklen;
    socklen = sizeof(peer);
    p->state = initial;
    p->inbuf = 0;
    p->current_block = 1;
    
    if((fd = accept(descriptor, (struct sockaddr *)&peer, &socklen)) < 0){
        perror("accept");
    } else {
        p->fd = fd;
        char filename[22];
        int temp = sizeof(filename);
        int read_bytes;
        char *tempbuff = filename;
        while ((read_bytes = read(fd, tempbuff, temp)) != 0){
                filename_length = filename_length + read_bytes;
                int network_char = find_network_newline(filename, filename_length);// look for network newline
            
                if (filename_length > 20){ // file name too long
                printf("Filename Too Long\n");
                    p->state = finished; // change state to finished and remove client
                }
            
                if (network_char >= 0) {// if network char is found setup client and jump to preblock
                    p->state = pre_block;
                    filename[network_char] = '\0';
                    strncpy(p->filename, (const char *)filename, 20);
                    FILE *fp;
                    fp = open_file_in_dir(p->filename, dir);
                    p->fp = fp;
                    char message = 'C';
                    write(fd, &message, 1);
                    printf("File and Client Added, C sent to client\n");
                    break;
                }
                tempbuff = filename + filename_length;
                temp = sizeof(filename) - filename_length;
            }
        }
        p->next = top;
        top = p;
}

//checks the crc based off computed crc from payload.
void checkblock(struct client *p, fd_set fdlist)
{
    unsigned char current_block = p->buf[0];
    unsigned char inverse_block = p->buf[1];
    unsigned char payload[p->blocksize - 4];
    if ((current_block != 255 - inverse_block)|| (p->current_block != current_block)){
        printf("issue with inverse or unexpected block. Removing client.\n"); // if the blocknumbers dont match drop the client
        removeclient(p->fd);
    }
      else if (p->current_block == current_block + 1) { // dup block send ack
        printf("Server got a duplicate block, ACK Send to client.\n");
        char message = ACK;
        write(p->fd, &message, 1);
        p->state = pre_block;
    } else { // everything is fine with the block check crc
        int counter;
        for (counter = 2; counter < p->blocksize-2; counter++){
            payload[counter-2] = p->buf[counter];
        }
        unsigned short server_crc = crc_message(XMODEM_KEY, payload, p->blocksize - 4); // compute crc
        if(((p->buf[p->blocksize - 2] & 0xFF) != server_crc >> 8) || ((p->buf[p->blocksize - 1] & 0xFF) != (server_crc & 0xFF))){//crc doesnt match drop client
            printf("CRC not matching NAK sent to Client.\n");
            char message = NAK;
            write(p->fd, &message, 1);
        } else {
            p->state = pre_block;
            p->inbuf = 0;
            printf("All Checks Passed Wrting Block to File\n"); // otherwise write
            fwrite(p->buf + 2, sizeof(char), p->blocksize - 4, p->fp);
            //fflush(p->fp);
            p->current_block += 1;
            if (p->current_block > 255){
                p->current_block = 0;
            }
            char message = ACK;
            write(p->fd, &message, 1);
        }
    }
}

int main(int argc, char *argv[])
{
    printf("Welcome to the Matrix... I mean my XModem Server\n");
    printf("Waiting For Connection(s)\n");
    struct client *p;
    bindandlisten();
    int maxfd = listenfd;
    
    while(1){
        fd_set fdlist;
        FD_ZERO(&fdlist);
        FD_SET(listenfd, &fdlist);
        
        //adding the connected clients to fd_set and getting maxfd
        for (p = top; p; p = p->next){
            FD_SET(p->fd, &fdlist);
            if(p->fd > maxfd){
                maxfd = p->fd;
            }
        }
        
        if (select(maxfd + 1, &fdlist, NULL, NULL, NULL) < 0){
            perror("select");
            exit(1);
        } else {
            
            if (FD_ISSET(listenfd, &fdlist)) // if there is an action on the master socket there a new connection
                newconnection(listenfd, p, fdlist);
            
            //getting the first selected client
            for (p = top; p; p = p->next) // after master socket is checked up into existing clients
                if (FD_ISSET(p->fd, &fdlist))
                    break;

            
            if(p){//exisiting connection, jump into states
                if(p->state == pre_block){   //states
                    printf("Pre Block with block num %d.\n", p->current_block);
                    char recieved_message;
                    read(p->fd, &recieved_message, 1);
                    if (recieved_message == SOH || recieved_message == STX || recieved_message == EOT) {
                    
                    if (recieved_message == SOH){
                        printf("Received SOH\n");
                        p->blocksize = 132;
                        p->state = get_block;
                    }
                    if (recieved_message == STX) {
                        printf("Received STX\n");
                        p->blocksize = 1028;
                        p->state = get_block;
                    }
                    if (recieved_message == EOT){
                        char message = ACK;
                        write(p->fd, &message, 1);
                        printf("Received EOT\n");
                        p->state = finished;
                    }
                    }
                    else{
                        printf("Invalid block code");
                        p->state = finished;
                    }
                        
                }
                
                if(p->state == get_block){
                    int nbytes;
                    if ((nbytes = read(p->fd, p->buf + p->inbuf, sizeof(p->buf) - p->inbuf)) > 0){ //read from socket what we can
                        p->inbuf = p->inbuf + nbytes;
                        if(p->inbuf == p->blocksize){//if we have read the blocksize specificed continue
                            strncat((char *)p->buf, (const char *)p->buf + p->inbuf, sizeof(p->buf) - strlen((const char *)p->buf));
                            printf("Block Recieved.\n");
                            p->state = check_block; //jump to check block
                        }
                    }
                }
                
                if (p->state == check_block){
                    printf("Checking Block\n");
                    checkblock(p, fdlist); //checkblock helper call
                }
                
                if (p->state == finished){
                    fclose(p->fp);
                    close(p->fd);
                    removeclient(p->fd); // finally remove client and free memory

                }
            }
        }
    }
    return 0;
}
