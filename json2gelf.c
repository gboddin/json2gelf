#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


// Settings :

#define SERVER "127.0.0.1" //IP to send data to
#define PORT 12345   //The port on which to send data
#define UDP_CHUNK_SIZE 1024 //UDP GELF CHUNK SIZE

// Fixed

#define GELF_HDR_SIZE 12  //FIXED GELF HEADER SIZE
#define true 1
#define false 0

void die(char *s)
{
    perror(s);
    exit(EXIT_FAILURE);
}

int main(void)
{
    /**
     * Init vars :
     * 
    **/
     
    FILE * fp;
    struct sockaddr_in si_other;
    int s, slen=sizeof(si_other);
    size_t len = 0;
    ssize_t read;


    uint8_t chunk_index = 0;
    uint8_t total_chunks = 1;
    uint32_t gelf_id = 1;

    int gelf_chunk_size = UDP_CHUNK_SIZE - GELF_HDR_SIZE;;
    int line_size;
    double left_overs;

    char * line = NULL;
    char gelf_header[GELF_HDR_SIZE];
    char udp_packet[UDP_CHUNK_SIZE];


   
 
      
    // Open stdin :
    fp = fopen("/dev/stdin", "r");
    if (fp == NULL)
        die("fopen error");
    
    if ( (s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        die("socket error");
    
    
    memset((char *) &si_other, 0, sizeof(si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(PORT);
    
    if (inet_aton(SERVER , &si_other.sin_addr) == 0) 
        die("inet_aton() failed");
    
    printf("json2gelf : Reading from stdin and sending packets to %s port %i ...\n",SERVER,PORT);
    
    //construct basic gelf_header :
    gelf_header[0] = 0x1e;
    gelf_header[1] = 0x0f;
    
    
    
    // While we have data coming from stdin :
    while ((read = getline(&line, &len, fp)) != -1) {

    //give it an id :
    
      gelf_id++;

    //chunk it :
    chunk_index = 0;
            
    line_size = strlen(line);
    
    total_chunks = ceil((float) line_size / gelf_chunk_size);

        while(chunk_index +1 <= total_chunks) {

            //by default we get max chunk size
            left_overs = gelf_chunk_size;

            //if it's the only chunk, left_overs is line_size
            if(total_chunks == 1)
                left_overs =  line_size;
            // else if we're at the last chunk
            else if(chunk_index + 1 == total_chunks)
                //remaining chars = strlen(line) % total_chunks
                left_overs =  fmod((float) line_size, (float) gelf_chunk_size);

            // build our gelf header (32bit) :
            // ( Endian conversion from local to network )
            uint32_t gelf_bw_id = htonl(gelf_id);

            // 32 bits will be enough, copy to our packet 2 times (64bits by gelf_specs)
            memcpy(gelf_header+2, &gelf_bw_id, 4);
            memcpy(gelf_header+6, &gelf_bw_id, 4);

            // Add chunk index and total chunk
            memcpy(gelf_header+10, &chunk_index ,1);
            memcpy(gelf_header+11, &total_chunks, 1);

            // Forge our UDP Packet :
            memcpy(udp_packet, gelf_header, GELF_HDR_SIZE);

              // Here we substr :
            memcpy(udp_packet + GELF_HDR_SIZE, &line[chunk_index * gelf_chunk_size], left_overs);

            // And send it :
            if (sendto(s, udp_packet, (left_overs + GELF_HDR_SIZE), 0, (struct sockaddr *) &si_other, slen)==-1)
              die("sendto() failed");

            // Increase our chunk index
            chunk_index++;
        }
    }
    
    fclose(fp);
  
    if (line)
        free(line);
    exit(EXIT_SUCCESS);
}

