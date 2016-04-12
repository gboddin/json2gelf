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
#include <argp.h>
#include "JSON_checker.h"


// Settings :
#define UDP_CHUNK_SIZE 1024 //UDP GELF CHUNK SIZE

// Fixed

#define GELF_HDR_SIZE 12  //FIXED GELF HEADER SIZE
#define true 1
#define false 0


const char *argp_program_version =
  "json2gelf 0.1.1";
const char *argp_program_bug_address =
  "<gregory@siwhine.net>";

/* The options we understand. */
static struct argp_option options[] = {
  {"no-validate-json", 'n',0 , 0,  "Skip JSON packet checks (default: no)" },
  {"host", 'h', "HOST",0 ,  "GELF remote server (default: 127.0.0.1)" },
  {"port", 'p', "PORT",0 ,  "GELF remote port (default: 12345)" },
  {"file", 'f', "FILE",0 , "Input file (default: /dev/stdin)" },
  {"verbose", 'v', 0, 0, "Increase verbosity" },
  { 0 }
};

/* Used by main to communicate with parse_opt. */
struct arguments
{
  int validate_json;
  int verbose;
  char *input_file;
  char *host;
  int port;
};

/* Program documentation. */
static char doc[] =
  "json2gelf -- Forward line separated JSON to a UDP GELF server";


/* Parse a single option. */
static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  /* Get the input argument from argp_parse, which we
     know is a pointer to our arguments structure. */
  struct arguments *arguments = state->input;

  switch (key)
    {
    case 'n':
      arguments->validate_json = 0;
      break;
    case 'f':
      arguments->input_file = arg;
      break;
    case 'h':
       arguments->host = arg;
       break;
    case 'v':
       arguments->verbose = 1;
       break;
    case 'p':
       if( (int) strtol(arg, NULL,10) > 65535)
         argp_usage (state);
       arguments->port = (int) strtol(arg, NULL,10);
       break;
    case ARGP_KEY_ARG:
      // no fixed argument nop
      argp_usage (state);
      break;
    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

static struct argp argp = { options, parse_opt, NULL, doc };

void die(char *s)
{
    printf(s);
    printf("\n");
    exit(EXIT_FAILURE);
}

int validate_json_string(char *json_string)
{
    JSON_checker jc = new_JSON_checker(20);
    size_t counter_i = 0 ;
    for(counter_i=0 ; counter_i < strlen(json_string) ; counter_i++) {
        if (!JSON_checker_char(jc, json_string[counter_i]))
            return 0;
    }
    if (!JSON_checker_done(jc)) {
       return 0;
    }
    return 1;
}

int main(int argc, char **argv)
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
    struct hostent *he;

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


    struct arguments arguments;

    /* Default values. */
    arguments.validate_json = 1;
    arguments.input_file = "/dev/stdin";
    arguments.host = "127.0.0.1";
    arguments.port = 12345;
    arguments.verbose = 0;



    /* Parse our arguments; every option seen by parse_opt will
     be reflected in arguments. */
    argp_parse (&argp, argc, argv, 0, 0, &arguments);


    // Open stdin :
    fp = fopen(arguments.input_file, "r");
    if (fp == NULL)
      die("Cannot fopen for reading !");

    memset((char *) &si_other, 0, sizeof(si_other));

    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(arguments.port);

    /* resolve hostname */
    if ( (he = gethostbyname(arguments.host) ) == NULL )
        die("Host cannot be reached");

    // store ip in socket srurct :
    memcpy(&si_other.sin_addr, he->h_addr_list[0], he->h_length);

    // open connection :
    if ( (s=socket(AF_INET, SOCK_DGRAM, 0)) == -1)
        die("socket error");

    printf("json2gelf : Reading from %s and sending packets to %s port %i %s JSON validation enabled...\n",arguments.input_file,arguments.host,arguments.port
           , arguments.validate_json ? "with" : "without" );

    // While we have data coming from stdin :
    while ((read = getline(&line, &len, fp)) != -1) {

        //chunk it :
        chunk_index = 0;

        //"Trim" last \n from line

        strtok(line, "\n");

        // is this packet is too big ?
        if(strlen(line) > (128 * UDP_CHUNK_SIZE))
            continue;

        // Check json

        if(arguments.validate_json && validate_json_string(line) == 0){
            if (arguments.verbose)
              printf("Skpping invalid JSON packet : %s\n", line);
            continue;
        }

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

        if(arguments.verbose)
          printf("Sent compressed GELF packet of %i bytes in %i chunk%s\n",line_size, total_chunks, total_chunks > 1  ? "s" : "");

    }

    // Close resources :
    fclose(fp);
    close(s);

    // Free line
    if (line)
      free(line);
    exit(EXIT_SUCCESS);
}

