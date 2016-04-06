#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <zlib.h>



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

    int gelf_chunk_size = UDP_CHUNK_SIZE - GELF_HDR_SIZE;;
    int line_size;
    double left_overs;

    char * line = NULL;
    char gelf_header[GELF_HDR_SIZE];
    char udp_packet[UDP_CHUNK_SIZE];
    struct timeval tv;
    z_stream defstream;
    // Allocating uncompressed size for compressed
    char compressed_line[128 * UDP_CHUNK_SIZE];



    // Open stdin :
    fp = fopen("/dev/stdin", "r");
    if (fp == NULL)
    die("fopen error");

    memset((char *) &si_other, 0, sizeof(si_other));

    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(PORT);


    if (inet_aton(SERVER , &si_other.sin_addr) == 0)
        die("inet_aton() failed");

    // open connection :
    if ( (s=socket(AF_INET, SOCK_DGRAM, 0)) == -1)
        die("socket error");

    printf("json2gelf : Reading from stdin and sending packets to %s port %i ...\n",SERVER,PORT);

    // While we have data coming from stdin :
    while ((read = getline(&line, &len, fp)) != -1) {

        //chunk it :
        chunk_index = 0;

        //"Trim" last \n from line

        strtok(line, "\n");

        // is this packet is too big ?
        if(strlen(line) > (128 * UDP_CHUNK_SIZE))
            continue;

        // nop, compress it :

        defstream.zalloc = Z_NULL;
        defstream.zfree = Z_NULL;
        defstream.opaque = Z_NULL;

        defstream.avail_in = (uInt) strlen(line); // size of input, string - terminator
        defstream.next_in = (Bytef *)line; // input char array
        defstream.avail_out = (uInt)sizeof(compressed_line); // size of output
        defstream.next_out = (Bytef *)compressed_line; // output char array

        // the actual compression work.
        deflateInit(&defstream, Z_BEST_SPEED);
        deflate(&defstream, Z_FINISH);
        deflateEnd(&defstream);

        //The size
        line_size = (char*)defstream.next_out - compressed_line;

        printf("Compressed line : %i\n", line_size);

        total_chunks = ceil((float) line_size / gelf_chunk_size);


        // This byte here should be is a 9c when using gzcompress from php but here it's not.
        // logstash checks for this magic byte sequence to detect compression ( 78,9c ), 78 is good :
        compressed_line[1] = 0x9c;

        // get microtime for id :
        gettimeofday(&tv,NULL);

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

            // build our gelf header (12 bytes) :

            gelf_header[0] = 0x1e;
            gelf_header[1] = 0x0f;

            // id is from tv_usec.tv_sec
            memcpy(gelf_header+2, &tv.tv_usec, 4);
            memcpy(gelf_header+6, &tv.tv_usec, 4);

            // Add chunk index and total chunk
            memcpy(gelf_header+10, &chunk_index ,1);
            memcpy(gelf_header+11, &total_chunks, 1);

            // Forge our UDP Packet :
            memcpy(udp_packet, gelf_header, GELF_HDR_SIZE);

            // Here we substr :
            memcpy(udp_packet + GELF_HDR_SIZE, &compressed_line[chunk_index * gelf_chunk_size], left_overs+1);

            // And send it :
            if (sendto(s, udp_packet, ((int) left_overs + GELF_HDR_SIZE), 0, (struct sockaddr *) &si_other, slen)==-1)
                die("sendto() failed");

            // Increase our chunk index
            chunk_index++;
        }
    }

    // Close resources :
    fclose(fp);
    close(s);

    // Free line
    if (line)
      free(line);
    exit(EXIT_SUCCESS);
}

