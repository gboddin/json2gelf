#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#define SERVER "127.0.0.1" //IP to send data to
#define PORT 12345   //The port on which to send data
#define CHUNK_SIZE 1024 //UDP GELF CHUNK SIZE
#define GELF_HDR_SIZE 12  //FIXED GELF HEADER SIZE

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
    char * line = NULL;
    char buffer[CHUNK_SIZE - GELF_HDR_SIZE];
    char udp_packet[CHUNK_SIZE];
    char gelf_header[GELF_HDR_SIZE];
    size_t len = 0;
    ssize_t read;
    struct sockaddr_in si_other;
    int s, slen=sizeof(si_other);
    int chunk_size;
    uint8_t chunk_index = 0;
    uint8_t total_chunks = 1;
    int line_size;
    uint32_t gelf_id = 1;
 
    double left_overs;
    
    //final chunksize, removing HDR_SIZE
    chunk_size = CHUNK_SIZE - GELF_HDR_SIZE;
    
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
    
    
    //construct basic gelf_header :
    gelf_header[0] = 0x1e;
    gelf_header[1] = 0x0f;
    
    // While we have data coming from stdin :
    while ((read = getline(&line, &len, fp)) != -1) {
		
		//chunk it:
		total_chunks = ceil((float) strlen(line) / chunk_size);
		line_size = strlen(line);
		printf("+ Loaded line of %i bytes\n", line_size );
	    gelf_id++;

		while(chunk_index +1 <= total_chunks) { 
		  
		  printf("++ CHUNK %i of %i\n", chunk_index + 1, total_chunks);
		  
		  left_overs = chunk_size;
		  
		  if(chunk_index + 1 == total_chunks) {
			
			//remaining chars = strlen(line) % total_chunks
			left_overs =  fmod((float) line_size, (float) chunk_size);

			if(total_chunks == 1)
			   left_overs =  line_size;
			
		  };
	
		  printf("+++ Start at : %i\n", chunk_index * chunk_size);
		  printf("+++ For : %i bytes \n", (int) left_overs);
		  
		  // build our gelf header (32bit) :
		  
		  uint32_t gelf_bw_id = htonl(gelf_id);
		  
          memcpy(gelf_header+2, &gelf_bw_id, 4);
          memcpy(gelf_header+6, &gelf_bw_id, 4);

		  // Chunks :
		  memcpy(gelf_header+10, &chunk_index ,1);
		  memcpy(gelf_header+11, &total_chunks,1);
		  
		
		  
		  // Push the line part in the first buffer
		  memcpy(buffer, &line[chunk_index * chunk_size] , left_overs);
		 
		  // Forge our UDP Packet :
	      memcpy(udp_packet,gelf_header, GELF_HDR_SIZE);
		  memcpy(udp_packet + GELF_HDR_SIZE, buffer, CHUNK_SIZE);
		  
		  // And send it :
		  if (sendto(s, udp_packet, left_overs + GELF_HDR_SIZE, 0 , (struct sockaddr *) &si_other, slen)==-1)
			  die("sendto() failed");
		
		  chunk_index++;
		}
	
		printf("+++++ Sent %zu bytes to %s ! +++++++++\n\n", read,"locahost");
		chunk_index = 0;        
    }
    
    fclose(fp);
	
    if (line)
        free(line);
    exit(EXIT_SUCCESS);
}

