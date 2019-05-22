#include "comp.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <chord.h>
#include <rohc/rohc.h>
#include <rohc/rohc_decomp.h>
#include <rohc/rohc_comp.h>
#include <rohc/rohc_buf.h> /* for the rohc_buf_*() functions */
#include <netinet/ip.h> /* for the IPv4 header */
#define BUFFER_SIZE 2048

static struct rohc_comp *compressor; /*the ROHC compressor */
static struct rohc_decomp *decompressor;  /* the ROHC decompressor */
static char compressedPacket[MAX_PACKET_SIZE]; //MAXPACKETSIZE defined in chord.h
static char unCompressedPacket[MAX_PACKET_SIZE];

//generate a random number, used for generating number of contexts
//basically hack the arguments to fit the rohc_library's requirements
static int gen_random_num(const struct rohc_comp *const comp,
        void *const user_context)
{
    return rand();
}


/*
 * This function is invoked whenever a packet that needs to be
 * compressed is received over the TUN interface. Argument packet
 * points to a buffer that contains the whole packet (starting with
 * the IP header). Argument contains the size of the packet in bytes.
 *
 * Use the return arguments dst and dlen to return a compressed
 * version of the packet to the caller. The buffer pointed to by dst
 * should be either global buffer or a static buffer allocated in the
 * body of this function.
 *
 * The default (null) implementation does not perform any compression.
 * Hence, it just copies the pointers and lengths.
 *
 * Return value 0 indicates success. A negative value indicates
 * serious compression error. If the function returns a negative
 * value, the program terminates.
 */
    int
comp_shrink(char **dst, size_t *dlen, char *packet, size_t len)
{

    if(!len) //empty packet
        return 1;

    //this section is to check if we are dealing with ICMP packets
    bool isICMP;
    rohc_status_t rohc_status;
    /* the header of the IPv4 packet */
    static struct iphdr *ip_header;     //IP header struct, not from ROHC
    ip_header = (struct iphdr *) (packet); 
    isICMP = (ip_header->protocol == 1); //1 is the protocol number of ICMP
    if(!isICMP) //only working with ICMP protocol for now
    { 
        fprintf(stderr, "Packet is not ICMP\n");
        *dst = packet;
        *dlen = len;
        return 2;
    }

    /* the buffer that will contain the IPv4 packet to compress */
    static uint8_t ip_buffer[MAX_PACKET_SIZE]; 
    /* the packet that will contain the IPv4 packet to compress */
    struct rohc_buf ip_packet = rohc_buf_init_empty(ip_buffer, MAX_PACKET_SIZE); 
    //make the struct rohc_buf which will contain all the informaton about the packet including ip_buffer which will be stored in the structs data field

    /* the buffer that will contain the resulting ROHC packet */
    static uint8_t rohc_buffer[MAX_PACKET_SIZE];
    /* the packet that will contain the resulting ROHC packet */
    struct rohc_buf rohc_packet = rohc_buf_init_empty(rohc_buffer, MAX_PACKET_SIZE); //initialize to an empty struct

    /* copy the given packet to the IP packet */
    rohc_buf_append(&ip_packet, (uint8_t *) packet, len);

    //compress the packet
    rohc_status = rohc_compress4(compressor, ip_packet, &rohc_packet);
    if(rohc_status == ROHC_STATUS_NO_CONTEXT)
    {
        fprintf(stderr, "No context\n");
        *dst = packet;
        *dlen = len;
        return 3;
    }
    if(rohc_status != ROHC_STATUS_OK)
    {
        fprintf(stderr, "compression of IP packet failed: ROHC_strerror: %s, rohc_status: %d\n",
                rohc_strerror(rohc_status), rohc_status);
        return -6;
    }

    //move data to return locations
    memcpy(compressedPacket, rohc_buf_data(rohc_packet), rohc_packet.len); //copy the packet from the stack into static memory for access outside functin
    *dst = compressedPacket;
    *dlen = rohc_packet.len;

    return 0;
}


/*
 * This function is invoked whenever a packet that needs to be
 * decompressed is received over the serial port. The compressed
 * packet will be in the buffer pointed to by argument packet.
 * Argument len contains the size of the compressed packet.
 *
 * Return the decompressed version via return arguments dst and dlen.
 * Argument dst should point to a statically allocated buffer so that
 * it remains valid after this function has returned.
 *
 * The default (null) implementation does not perform any
 * decompression and just copies the pointers and lengths.
 *
 * Return 0 on success and a negative value on error. The program
 * terminates if the function returns a negative value.
 */
    int
comp_expand(char **dst, size_t *dlen, char *packet, size_t len)
{
    /* the buffer that will contain the ROHC packet to decompress */
    static unsigned char rohc_buffer[BUFFER_SIZE];
    /* the packet that will contain the ROHC packet to decompress */
    struct rohc_buf rohc_packet = rohc_buf_init_empty(rohc_buffer, BUFFER_SIZE);

    /* the buffer that will contain the resulting IPv4 packet */
    static unsigned char ip_buffer[BUFFER_SIZE];
    /* the packet that will contain the resulting IPv4 packet */
    struct rohc_buf ip_packet = rohc_buf_init_empty(ip_buffer, BUFFER_SIZE); //initialize to an empty struct


    /* copy the given packet to the ROHC packet */
    rohc_buf_append(&rohc_packet, (uint8_t *) packet, len);

    struct rohc_buf *rcvd_feedback = NULL;
    struct rohc_buf *feedback_send = NULL;

    rohc_status_t status;

    status = rohc_decompress3(decompressor, rohc_packet, &ip_packet, rcvd_feedback, feedback_send); //decompress the packet
    if(status == ROHC_STATUS_NO_CONTEXT)
    {
        fprintf(stderr, "No context yet\n");
        *dst = packet;
        *dlen = len;
        return 1;
    }
    if(status != ROHC_STATUS_OK)
    {
        fprintf(stderr, "decompression of ROHC packet failed\n");
        fprintf(stderr, "\trohc_status: %d\n", status);
        fprintf(stderr, "\tstrerror: %s\n", strerror(status));
        return -7;
    }

    if(!(rohc_comp_deliver_feedback2(compressor, *feedback_send)))
    {
        fprintf(stderr, "Feedback didn't work");
    }

    memcpy(unCompressedPacket, rohc_buf_data(ip_packet), ip_packet.len); //copy uncompressed data into static memory for return
    *dst = unCompressedPacket;
    *dlen = ip_packet.len;


    return 0;
}


/*
 * Perform one-time initialization. This function will be called once
 * when the program is starting up. You can perform any necessary
 * initializations of the RoHC library here. Return 0 to indicate that
 * everything went well and a negative value to indicate an error. If
 * the function returns a negative value, the program will terminate.
 */
    int
comp_init()
{
    //compressor section
    srand((unsigned int) time(NULL));
    compressor = rohc_comp_new2(ROHC_LARGE_CID, ROHC_LARGE_CID_MAX, gen_random_num, NULL); //constructor for compressor
    if(compressor == NULL)
    {
        fprintf(stderr, "failed create the ROHC compressor\n");
        return -1;
    }
    /*"The ROHC compressor does not use the compression profiles that are not enabled. Thus not enabling a profile might affect compression performances."*/
    if(!rohc_comp_enable_profile(compressor, ROHC_PROFILE_IP)) //only compress the IP header section of the packet
    {
        fprintf(stderr, "failed to enable the IP-only profile\n");
        return -2;
    }


    //decompressor section
    decompressor = rohc_decomp_new2(ROHC_LARGE_CID, ROHC_LARGE_CID_MAX, ROHC_U_MODE); //constructor for decompressor
    if(decompressor == NULL)
    {
        fprintf(stderr, "failed create the ROHC decompressor\n");
        return -3;
    }
    if(!rohc_decomp_enable_profile(decompressor, ROHC_PROFILE_IP)) //only compress the IP header section of the packet
    {
        fprintf(stderr, "failed to enable the IP-only profile\n");
        return -4;
    }
    if(!rohc_decomp_enable_profile(decompressor, ROHC_PROFILE_UNCOMPRESSED)) //testing
    {
        fprintf(stderr, "failed to enable the Uncompressed profile\n");
        return -5;
    }
    return 0;
}


/*
 * Perform one-time innitialization just before the program
 * terminates. This function can be used to perform any necessary
 * cleanup before the program terminates, e.g., to free allocated
 * memory.
 */
    void
comp_cleanup()
{
    rohc_comp_free(compressor);
    rohc_decomp_free(decompressor);
}


