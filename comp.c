#include "comp.h"

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
    *dst = packet;
    *dlen = len;
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
    *dst = packet;
    *dlen = len;
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
}
