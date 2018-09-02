#if !defined _clink

#define _GNU_SOURCE
#include <stdio.h>
#include <sys/types.h>
#include <assert.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <sched.h>
#include <assert.h>
#include <pthread.h>
#include <libserialport.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>

#define ewrap(...)	if (__VA_ARGS__ < SP_OK) {printf("lost connection...\n");exit(0);}
#define ewrap2(...)	int j;j=__VA_ARGS__;if (j < SP_OK) {printf("lost connection...\n");return NULL;}

typedef struct sd788t_rxbuf {
	uint8_t size;
	uint8_t status;
	uint8_t *readBuf;
} sd788t_rxbuf;

typedef struct sd788t_meter_value {
	uint8_t vu;
	uint8_t peak;
} sd788t_meter_value;

typedef struct smpte {
	uint8_t hours;
	uint8_t minutes;
	uint8_t seconds;
	uint8_t frames;
} sd788t_smpte;

/*
 * this pointer is used to bring a return data buffer from the 
 * receive thread to the active transmitting thread where
 *  the data will be returned to the calling function.
 */
sd788t_rxbuf * pReturnBuffer;
typedef struct sp_port sd788t_sp_port;
sd788t_sp_port * psd788t_sp_port;

/*
 * some function prototypes
 */

#define CALLHEADER	\
	sd788t_sp_port * sp_port; \
sd788t_rxbuf * rxbuf; \
uint8_t retval; \
\
if (sd788t_get_connected(&sp_port)!=SD788T_OK) { \
	return SD788T_CONNECTION_ERROR; \
} \



uint8_t sendBuf[1000];
uint8_t readBuf[1000];
void *readbuf;

enum memory_enum {
	L1,L2,SDRAM
};

enum drives_enum {
	ALL_DRIVES,CF,INHDD,EXT
};

enum metrics_enum {
	PROCESS_UTILIZATION,TRANSPORT_UTILIZATION,HEAP_USAGE,STACK_USAGE,MEMORY_USAGE
};

enum transport_control_enum {
	TRANSPORT_STOP,TRANSPORT_PLAY,TRANSPORT_RECORD
};


enum sd788t_error_enum {
	SD788T_BAD_ACK=-2, SD788T_CONNECTION_ERROR=-1, SD788T_OK=0
};

enum sd788t_rxbuf_status{
	SD788T_RXBUF_NULL=-2, SD788T_RXBUF_CHECKSUM_ERROR=-1, SD788T_RXBUF_OK=0
};



/*
 * exception is for use when handling meter retrieval which isn't really up to spec.
 */
uint8_t sd788t_user_get_limiters(uint8_t * limiters_bitmask) ;
uint8_t sd788t_get_media_select(uint8_t *, char**);
uint8_t sd788t_user_get_frame_rate(char ** framerate) ;
struct sd788t_rxbuf * sd788t_checkAndSend(int exception, sd788t_sp_port* sp_port,uint8_t * buf, uint8_t bufLen);
uint8_t sd788t_user_get_sample_rate(char ** samplerate) ;
uint8_t sd788t_user_get_bit_depth(char ** bitdepth) ;
void sd788t_free_rxbuf(struct sd788t_rxbuf * rxbuf);
int sd788t_get_connected(sd788t_sp_port ** psp_port);
int sd788t_get_parameter_change_status(uint8_t *version, uint8_t *takeflags, uint32_t *takehandle, uint8_t *parflags1, uint8_t *parflags2, uint8_t *parflags3 );
uint8_t * sd788t_createCommandBuffer(uint8_t command,uint8_t cmddata[],uint8_t cmddatasize,uint8_t *bufsize);
uint8_t sd788t_get_transport(uint8_t * pTransportStatus);
uint8_t sd788t_get_setting(uint16_t setting, uint8_t *size, uint16_t *result);
uint8_t sd788t_get_temperature(float * pTemperature);
uint8_t sd788t_get_sys_metric(uint8_t metric,uint8_t param,float *result);
uint8_t sd788t_user_get(uint16_t setting,uint16_t *result);
uint8_t sd788t_get_timecode( sd788t_smpte * regen_tc, sd788t_smpte * ext_tc, sd788t_smpte * file_tc) ;
uint8_t sd788t_get_sw_version(uint8_t *vmajor,uint8_t *vminor) ;
uint8_t sd788t_get_hw_version(uint8_t *tcmodule,uint8_t *jumpers) ;
uint8_t sd788t_get_productid(uint8_t *productid) ;
uint8_t sd788t_get_devicename(char *devicename) ;
uint8_t sd788t_get_timecode_version(uint8_t *vmajor,uint8_t *vminor) ;
uint8_t sd788t_get_oxford_version(uint8_t *vmajor,uint8_t *vminor, uint16_t *vextended) ;
uint8_t sd788t_get_adc_vext(float *result) ;
uint8_t sd788t_get_adc(uint8_t adc,uint16_t *result) ;

/*
 * 
 */
uint8_t sd788t_get_track_meters(uint8_t *nMeters,sd788t_meter_value * meter_values) ;
uint8_t sd788t_get_output_meters(uint8_t *nMeters,sd788t_meter_value * meter_values) ;
uint8_t sd788t_get_input_meters(uint8_t *nMeters,sd788t_meter_value * meter_values) ;

/*
 * Params: pointer to uint8_t  to hold resulting transport status.
 */
uint8_t sd788t_get_transport(uint8_t * pTransportStatus) ;

/*
 * main internal stuff
 */
void sd788t_list_ports(void (* process_ports)(uint8_t port_number, char * port_name, char * port_description)) ;
sd788t_sp_port* sd788t_sp_get_port_by_number(int nsport) ;
void sd788t_send40(sd788t_sp_port* sp_port);
int sd788t_connect(int nsport);
int sd788t_get_connected(sd788t_sp_port ** psp_port) ;
struct sd788t_rxbuf * sd788t_checkAndSend(int exception, sd788t_sp_port* sp_port,uint8_t * sendBuf, uint8_t bufLen);
uint8_t * sd788t_createCommandBuffer(uint8_t command, uint8_t cmddata[],uint8_t cmddatasize,uint8_t *bufsize) ;
char *sd788t_get_channel_name(uint8_t channel) ;
struct sd788t_rxbuf * sd788t_create_rxbuf(int size) ;
struct sd788t_rxbuf * sd788t_getRxBuf();
void sd788t_free_rxbuf(struct sd788t_rxbuf * rxbuf) ;
int sd788t_checkIncoming(uint8_t *buf,int bufLen) ;
struct sd788t_rxbuf * sd788t_sendCommand(sd788t_sp_port *sp_port,uint8_t command,uint8_t cmddata[],uint8_t cmddatasize) ;
#endif
