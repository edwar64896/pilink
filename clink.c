#include "clink.h"

char *sd788t_channels[]={"L","R","A","B","C","D","E","F","G","H","x1","x2"};
char* sTransportStatusArray[]={"STOP","PLAY","RECORD","PAUSE"};
int connected=0;

typedef struct settings788 {
	uint8_t setting_id;
	char * setting_description;
	char * setting_params[];
} s_settings788;

s_settings788 s0={ .setting_id=0, .setting_description="Sample Rate", .setting_params={"32kHz","44.1kHz","47.952kHz","47.952FkHz","48kHz","48.048kHz","48.048FkHz","88.2kHz","96kHz","96.096kHz","96.096FkHz","176.4kHz","192kHz" }};
s_settings788 s1={ .setting_id=1, .setting_description="Bit Depth", .setting_params={"24 Bits","16 Bits","16 Bits No Dither" }};
s_settings788 s2={ .setting_id=2, .setting_description="Input Linking", .setting_params={"Unlinked","1-2 S","1-4 M","1-6 M","1-8 M","3-4 S","5-6 S","7-8 S","5-6,7-8 S","1-2,3-4 S","1-2,3-4,5-6 S","1-2,3-4,5-6,7-8 S","1-2 MS","3-4 MS","5-6 MS","7-8 MS","1-2,3-4 MS","1-2,3-4,5-6 MS","1-2,3-4,5-6,7-8 MS" }};

char *sd788t_get_channel_name(uint8_t channel) {
	if (channel>=12) 
		return "";
	else
		return sd788t_channels[channel];
}

void print_hex_memory(void *mem) {
	int i;
	unsigned char *p = (unsigned char *)mem;
	for (i=0;i<48;i++) {
		printf("0x%02X ", p[i]);
	}
	printf("\n");
}

/*
 * print a list of available serial ports to the screen.
 * The user will select one of these
 * in order to connect the application
 */
void sd788t_list_ports(void (* port_function)(uint8_t , char * , char * )) {
	int sport_array_counter=0;
	struct sp_port* sport;
	struct sp_port** sports;
	struct sp_port** sports1;
	char* sPortDescription;
	char* sPortName;


	ewrap (sp_list_ports(&sports));
	sports1=sports;

	while (1) {
		if (*(sports1)==NULL) {
			break;
		}
		sPortDescription=sp_get_port_description(*(sports1));
		sPortName=sp_get_port_name(*(sports1++));
		if (port_function!=NULL)
			(*port_function)(sport_array_counter++,sPortName,sPortDescription);
	}

	sp_free_port_list(sports);
}

sd788t_sp_port * sd788t_sp_get_port_by_number(int nsport) {

	int sport_array_counter=0;
	struct sp_port* sport;
	struct sp_port** sports;
	struct sp_port** sports1;
	char* sPortDescription;
	char* sPortName;

	struct sp_port* copy_ptr;

	ewrap (sp_list_ports(&sports));
	ewrap (sp_copy_port(sports[nsport],&copy_ptr));

	sp_free_port_list(sports);

	return copy_ptr;

}

/*
 * function unverified - seems to be duplicating the result of the track meters
 * also - the return bytes don't match spec.
 *
 * same for output meters.
 */
uint8_t sd788t_get_input_meters(uint8_t *nMeters,sd788t_meter_value * meter_values) {

	CALLHEADER;

	uint8_t rxstatus;
	uint8_t cmddata[]={0};
	rxbuf=sd788t_sendCommand(sp_port,62,cmddata,1);
	printf("size=%u\n",rxbuf->size);
	if (rxbuf == NULL) {
		printf("inside\n");
		return SD788T_RXBUF_NULL;
	}

	if (rxbuf->status==SD788T_RXBUF_OK) {
		(*nMeters)=(rxbuf->size)/2;
		memcpy(meter_values,rxbuf->readBuf+1,rxbuf->size);
	}
	rxstatus=rxbuf->status;
	sd788t_free_rxbuf(rxbuf);
	return rxstatus;
}

uint8_t sd788t_get_track_meters(uint8_t *nMeters,sd788t_meter_value * meter_values) {

	CALLHEADER;

	uint8_t rxstatus;
	uint8_t cmddata[]={1};
	rxbuf=sd788t_sendCommand(sp_port,62,cmddata,1);
	if (rxbuf == NULL) {
		return SD788T_RXBUF_NULL;
	}

	if (rxbuf->status==SD788T_RXBUF_OK) {
		(*nMeters)=(rxbuf->size-3)/2;
		memcpy(meter_values,rxbuf->readBuf+1,rxbuf->size-3);
	}
	rxstatus=rxbuf->status;
	sd788t_free_rxbuf(rxbuf);
	return rxstatus;
}

uint8_t sd788t_get_output_meters(uint8_t *nMeters,sd788t_meter_value * meter_values) {

	CALLHEADER;

	uint8_t rxstatus;
	uint8_t cmddata[]={2};
	rxbuf=sd788t_sendCommand(sp_port,62,cmddata,1);
	if (rxbuf == NULL) {
		return SD788T_RXBUF_NULL;
	}

	if (rxbuf->status==SD788T_RXBUF_OK) {
		(*nMeters)=(rxbuf->size)/2;
		memcpy(meter_values,rxbuf->readBuf+1,rxbuf->size);
	}
	rxstatus=rxbuf->status;
	sd788t_free_rxbuf(rxbuf);
	return rxstatus;
}


uint32_t shift8(uint32_t destination, uint8_t incoming) {
	return (((destination << 8) & 0xFFFFFF00) | (incoming & 0xFF));
}

int sd788t_get_parameter_change_status(uint8_t *version, uint8_t *takeflags, uint32_t *takehandle, uint8_t *parflags1, uint8_t *parflags2, uint8_t *parflags3 ) {

	CALLHEADER;

	rxbuf=sd788t_sendCommand(sp_port,85,NULL,0);
	if (rxbuf == NULL) {
		return SD788T_RXBUF_NULL;
	}

	if (rxbuf->readBuf[0] != 0xF1 ){
		sd788t_free_rxbuf(rxbuf);
		return SD788T_BAD_ACK;
	}

	(*version)=rxbuf->readBuf[1];
	(*takeflags)=rxbuf->readBuf[2];

	(*takehandle)=shift8(0,rxbuf->readBuf[6]); // little-endian!
	(*takehandle)=shift8((*takehandle),rxbuf->readBuf[5]);
	(*takehandle)=shift8((*takehandle),rxbuf->readBuf[4]);
	(*takehandle)=shift8((*takehandle),rxbuf->readBuf[3]);

	(*parflags1)=rxbuf->readBuf[7];
	(*parflags2)=rxbuf->readBuf[8];
	(*parflags3)=rxbuf->readBuf[9];

	//printf("version=%u,takeflags=%u,takehandle=%u,parflags1=%u,parflags2=%u,parflags3=%u\n",*version,*takeflags,*takehandle,*parflags1,*parflags2,*parflags3);

	sd788t_free_rxbuf(rxbuf);
	return SD788T_OK;
}

uint8_t sd788t_ctrl_transport(uint8_t uTransport) {

	CALLHEADER;

	uint8_t cmddata[]={uTransport};
	rxbuf=sd788t_sendCommand(sp_port,17,cmddata,1);
	if (rxbuf == NULL) {
		return SD788T_RXBUF_NULL;
	}

	sd788t_free_rxbuf(rxbuf);
	return SD788T_OK;
}

/*
 * returns bitmask of which media options are in use:
 * Bit 1: INHDD
 * Bit 2: CF
 * Bit 3: EXHDD
 */
uint8_t sd788t_get_media_select(uint8_t * media_select_bitmask,char ** medselect) {
	uint16_t result=0;
	sd788t_user_get(74,&result);
	(*media_select_bitmask)=0;
	switch (result) {
		case 0:
			*medselect="INHDD";
			(*media_select_bitmask)=0b001;
			break;
		case 1:
			*medselect="CF";
			(*media_select_bitmask)=0b010;
			break;
		case 2:
			*medselect="EXHDD";
			(*media_select_bitmask)=0b100;
			break;
		case 3:
			*medselect="INHDD/CF";
			(*media_select_bitmask)=0b011;
			break;
		case 4:
			*medselect="INHDD/EXHDD";
			(*media_select_bitmask)=0b101;
			break;
		case 5:
			*medselect="EXHDD/CF";
			(*media_select_bitmask)=0b110;
			break;
		case 6:
			*medselect="INHDD/EXHDD/CF";
			(*media_select_bitmask)=0b111;
			break;
		default:
			//*medselect="---";
			//(*media_select_bitmask)=0b000;
			break;
	}
}

uint8_t sd788t_user_get_tc_mode(char ** tcmode) {
	uint16_t result;
	sd788t_user_get(52,&result);
	switch (result) {
		case 0:
			*tcmode="Off";
			break;
		case 1:
			*tcmode="FR";
			break;
		case 2:
			*tcmode="FR-JAM";
			break;
		case 3:
			*tcmode="RECRUN";
			break;
		case 4:
			*tcmode="24H=RUN";
			break;
		case 5:
			*tcmode="EXTTC-HLT";
			break;
		case 6:
			*tcmode="EXTTC-FWHEEL";
			break;
		case 7:
			*tcmode="EXTTC-HLT";
			break;
		case 8:
			*tcmode="EXTTC-FWHEEL-REC";
			break;
		default:
			*tcmode="---";
			break;
	}
}

/*
 * mute bitmask
 *
 * active bit shows inverted mute
 */
uint8_t sd788t_user_get_mute(uint8_t * mute_bitmask) {
	uint16_t result;
	uint8_t bitmask=0;

	for (int i=247;i<255;i++) {
		sd788t_user_get(i,&result);
		bitmask >> 1;
		if (result)
			bitmask |= 0b10000000;
	}
	(*mute_bitmask)=bitmask;
}

/*
 * phase bitmask
 *
 * active bit shows inverted phase
 */
uint8_t sd788t_user_get_phase(uint8_t * phase_bitmask) {
	uint16_t result;
	uint8_t bitmask=0;

	for (int i=190;i<198;i++) {
		sd788t_user_get(i,&result);
		bitmask >> 1;
		if (result)
			bitmask |= 0b10000000;
	}
	(*phase_bitmask)=bitmask;
}

uint8_t sd788t_user_get_phantom(uint8_t * phantom_bitmask) {
	uint16_t result;
	uint8_t bitmask=0;

	for (int i=129;i<137;i++) {
		sd788t_user_get(i,&result);
		bitmask >> 1;
		if (result)
			bitmask |= 0b10000000;
	}
	(*phantom_bitmask)=bitmask;
}

/*
 * params:
 * 1: input number (1-8)
 * 2. Ptr to uint8_t to hold source:
 * 	1: Mic
 * 	2: Line
 * 	3: Digital
 */
uint8_t sd788t_user_get_input_source(uint8_t input, uint16_t * source) {
	uint8_t result;

	if (input<1 || input > 8)
		return -1;
	return sd788t_user_get(8+input,source);
}

/*
 * returns bitmask showing whether lowcut is active
 *
 * bit1: track 1 ....
 * bit8: track 8
 */
uint8_t sd788t_user_get_lowcut(uint8_t * lowcut_bitmask) {
	uint16_t result;
	uint8_t bitmask=0;

	for (int i=25;i<33;i++) {
		sd788t_user_get(i,&result);
		bitmask >> 1;
		if (result)
			bitmask |= 0b10000000;
	}
	(*lowcut_bitmask)=bitmask;
}

/*
 * returns bitmask showing whether limiters are active
 *
 * bit1: track 1....
 * bit8: track 8
 */
uint8_t sd788t_user_get_limiters(uint8_t * limiters_bitmask) {
	uint16_t result;
	uint8_t bitmask=0;

	for (int i=33;i<41;i++) {
		sd788t_user_get(i,&result);
		bitmask >> 1;
		if (result)
			bitmask |= 0b10000000;
	}
	(*limiters_bitmask)=bitmask;
}

uint8_t sd788t_user_get_frame_rate(char ** framerate) {
	uint16_t result;
	sd788t_user_get(51,&result);
	switch (result) {
		case 0:
			*framerate="23.967";
			break;
		case 1:
			*framerate="24";
			break;
		case 2:
			*framerate="25";
			break;
		case 3:
			*framerate="29.97";
			break;
		case 4:
			*framerate="29.97DF";
			break;
		case 5:
			*framerate="30";
			break;
		case 6:
			*framerate="30DF";
			break;
		case 7:
			*framerate="30+";
			break;
		default:
			*framerate="---";
			break;
	}
}
uint8_t sd788t_user_get_bit_depth(char ** bitdepth) {
	uint16_t result;
	sd788t_user_get(1,&result);
	switch (result) {
		case 0:
			*bitdepth="24b";
			break;
		case 1:
			*bitdepth="16b";
			break;
		case 2:
			*bitdepth="16bND";
			break;
		default:
			*bitdepth="---";
			break;
	}
}

uint8_t sd788t_user_get_sample_rate(char ** samplerate) {
	uint16_t result;
	sd788t_user_get(0,&result);
	switch (result) {
		case 0:
			*samplerate="32 kHz";
			break;
		case 1:
			*samplerate="44.1 kHz";
			break;
		case 2:
			*samplerate="47.952 kHz";
			break;
		case 3:
			*samplerate="47.952F kHz";
			break;
		case 4:
			*samplerate="48 kHz";
			break;
		case 5:
			*samplerate="48.048 kHz";
			break;
		case 6:
			*samplerate="48.048F kHz";
			break;
		case 7:
			*samplerate="88.2 kHz";
			break;
		case 8:
			*samplerate="96 kHz";
			break;
		case 9:
			*samplerate="96.096 kHz";
			break;
		case 10:
			*samplerate="96.096F kHz";
			break;
		case 11:
			*samplerate="176.4 kHz";
			break;
		case 12:
			*samplerate="192 kHz";
			break;
		default:
			*samplerate="---";
			break;
	}
}
uint8_t sd788t_user_get(uint16_t setting,uint16_t *result) {

	CALLHEADER;

	uint8_t s1=(uint8_t)(setting & 0x00FF);
	uint8_t s2=(uint8_t)((setting & 0xFF00)>>8);

	uint8_t cmddata[]={s1,s2};
	rxbuf=sd788t_sendCommand(sp_port,10,cmddata,2);

	if (rxbuf == NULL) {
		return SD788T_RXBUF_NULL;
	}

	(*result)=shift8(0,rxbuf->readBuf[1]);
	(*result)=shift8((*result),rxbuf->readBuf[0]);

	sd788t_free_rxbuf(rxbuf);
	return SD788T_OK;

}

uint8_t sd788t_get_setting(uint16_t setting, uint8_t *size, uint16_t *result) {

	CALLHEADER;

	uint8_t s1=(uint8_t)(setting & 0x00FF);
	uint8_t s2=(uint8_t)((setting & 0xFF00)>>8);

	uint8_t cmddata[]={s1,s2};
	rxbuf=sd788t_sendCommand(sp_port,10,cmddata,2);
	if (rxbuf == NULL) {
		printf("no setting :-(\n");
		return SD788T_RXBUF_NULL;
	}

	(*size)=rxbuf->size;

	(*result)=shift8(0,rxbuf->readBuf[1]);
	(*result)=shift8((*result),rxbuf->readBuf[0]);

	printf("s1=%u, s2=%u ",s1,s2);
	printf("setting=%u ",setting);
	printf("rxstatus=%u ",rxbuf->status);
	printf("size=%u ",*size);
	printf("result=%u\n",*result);

	sd788t_free_rxbuf(rxbuf);
	return SD788T_OK;
}
uint8_t sd788t_get_transport(uint8_t * pTransportStatus) {
	CALLHEADER;

	rxbuf=sd788t_sendCommand(sp_port,18,NULL,0);
	if (rxbuf == NULL) {
		return SD788T_RXBUF_NULL;
	}


	(*pTransportStatus) = rxbuf->readBuf[0];
	printf("transportstatus=%u\n",*pTransportStatus);
	sd788t_free_rxbuf(rxbuf);
	return SD788T_OK;
}

uint8_t sd788t_get_timecode_version(uint8_t *vmajor,uint8_t *vminor) {

	CALLHEADER;

	uint8_t cmddata[]={6};
	rxbuf=sd788t_sendCommand(sp_port,65,cmddata,1);
	if (rxbuf == NULL) {
		return SD788T_RXBUF_NULL;
	}


	(*vminor) = rxbuf->readBuf[1];
	(*vmajor) = rxbuf->readBuf[2];

	printf("tcversion=%u.%u\n",*vmajor,*vminor);
	sd788t_free_rxbuf(rxbuf);
	return SD788T_OK;
}
uint8_t sd788t_get_oxford_version(uint8_t *vmajor,uint8_t *vminor, uint16_t *vextended) {

	CALLHEADER;

	uint8_t cmddata[]={5};
	rxbuf=sd788t_sendCommand(sp_port,65,cmddata,1);
	if (rxbuf == NULL) {
		return SD788T_RXBUF_NULL;
	}


	(*vminor) = rxbuf->readBuf[1];
	(*vmajor) = rxbuf->readBuf[2];
	(*vextended)=shift8(0,rxbuf->readBuf[4]);
	(*vextended)=shift8((*vextended),rxbuf->readBuf[3]);
	//(*vextended) = rxbuf->readBuf[3] | ((rxbuf->readBuf[4] & 0x00FF) << 8);

	printf("oxversion=%u.%u.%u\n",*vmajor,*vminor,*vextended);
	sd788t_free_rxbuf(rxbuf);
	return SD788T_OK;
}
uint8_t sd788t_get_devicename(char *devicename) {

	CALLHEADER;

	uint8_t cmddata[]={3};
	rxbuf=sd788t_sendCommand(sp_port,65,cmddata,1);
	if (rxbuf == NULL) {
		return SD788T_RXBUF_NULL;
	}


	strcpy(devicename,rxbuf->readBuf+1);

	sd788t_free_rxbuf(rxbuf);
	return SD788T_OK;
}
uint8_t sd788t_get_productid(uint8_t *productid) {

	CALLHEADER;

	uint8_t cmddata[]={2};
	rxbuf=sd788t_sendCommand(sp_port,65,cmddata,1);
	if (rxbuf == NULL) {
		return SD788T_RXBUF_NULL;
	}


	(*productid) = rxbuf->readBuf[1];

	sd788t_free_rxbuf(rxbuf);
	return SD788T_OK;
}
uint8_t sd788t_get_hw_version(uint8_t *tcmodule,uint8_t *jumpers) {

	CALLHEADER;

	uint8_t cmddata[]={1};
	rxbuf=sd788t_sendCommand(sp_port,65,cmddata,1);
	if (rxbuf == NULL) {
		return SD788T_RXBUF_NULL;
	}


	(*tcmodule) = rxbuf->readBuf[2];
	(*jumpers) = rxbuf->readBuf[1];

	printf("hwversion=%u.%u\n",*tcmodule,*jumpers);
	sd788t_free_rxbuf(rxbuf);
	return SD788T_OK;
}
uint8_t sd788t_get_sw_version(uint8_t *vmajor,uint8_t *vminor) {

	CALLHEADER;

	uint8_t cmddata[]={0};
	rxbuf=sd788t_sendCommand(sp_port,65,cmddata,1);

	if (rxbuf == NULL) {
		return SD788T_RXBUF_NULL;
	}

	(*vminor) = rxbuf->readBuf[1];
	(*vmajor) = rxbuf->readBuf[2];

	printf("swversion=%u.%u\n",*vmajor,*vminor);
	sd788t_free_rxbuf(rxbuf);
	return SD788T_OK;
}

uint8_t sd788t_get_adc_vext(float *result) {
	uint16_t iResult;
	int retval;
	retval= sd788t_get_adc(12,&iResult);
	if (retval==0) {
		(*result)=(float)iResult/4096.0*6.0*3.3;
	} else {
		(*result)=0;
	} 
	return retval;
}

uint8_t sd788t_get_adc(uint8_t adc,uint16_t *result) {

	CALLHEADER;

	uint8_t cmddata[]={adc};
	rxbuf=sd788t_sendCommand(sp_port,23,cmddata,1);


	if (rxbuf == NULL) {
		return SD788T_RXBUF_NULL;
	}

	//note this is a 12-bit value packed into a 16 bit word, LE
	(*result) = ((rxbuf->readBuf[1] & 0x000F) << 8) | rxbuf->readBuf[2];

	sd788t_free_rxbuf(rxbuf);
	return SD788T_OK;
}
uint8_t sd788t_get_sys_metric(uint8_t metric,uint8_t param,float *result) {

	CALLHEADER;

	uint8_t cmddata[]={metric,param};
	uint16_t uresult=0;
	rxbuf=sd788t_sendCommand(sp_port,76,cmddata,2);
	if (rxbuf == NULL) {
		return SD788T_RXBUF_NULL;
	}


	uresult = rxbuf->readBuf[1]|((rxbuf->readBuf[2] & 0x00FF) << 8);
	(*result)=uresult/10;

	sd788t_free_rxbuf(rxbuf);
	return SD788T_OK;
}
uint8_t sd788t_get_temperature(float * pTemperature) {

	CALLHEADER;

	rxbuf=sd788t_sendCommand(sp_port,15,NULL,0);

	if (rxbuf == NULL) {
		return SD788T_RXBUF_NULL;
	}

	(*pTemperature) = (float)rxbuf->readBuf[0];
	sd788t_free_rxbuf(rxbuf);
	return SD788T_OK;
}

uint8_t sd788t_get_timecode( sd788t_smpte * regen_tc, sd788t_smpte * ext_tc, sd788t_smpte * file_tc) {

	CALLHEADER;

	rxbuf=sd788t_sendCommand(sp_port,19,NULL,0);

	if (rxbuf == NULL) {
		return SD788T_RXBUF_NULL;
	}

	if (rxbuf->status==SD788T_RXBUF_OK) {

		file_tc->hours = rxbuf->readBuf[11];
		file_tc->minutes = rxbuf->readBuf[10];
		file_tc->seconds = rxbuf->readBuf[9];
		file_tc->frames = rxbuf->readBuf[8];
		ext_tc->hours = rxbuf->readBuf[7];
		ext_tc->minutes = rxbuf->readBuf[6];
		ext_tc->seconds = rxbuf->readBuf[5];
		ext_tc->frames = rxbuf->readBuf[4];
		regen_tc->hours = rxbuf->readBuf[3];
		regen_tc->minutes = rxbuf->readBuf[2];
		regen_tc->seconds = rxbuf->readBuf[1];
		regen_tc->frames = rxbuf->readBuf[0];

		sd788t_free_rxbuf(rxbuf);
		return SD788T_RXBUF_OK;
	} else {
		sd788t_free_rxbuf(rxbuf);
		return SD788T_RXBUF_CHECKSUM_ERROR;
	}
}

/*
 * create and instance of the return data buffer
 * 
 * This will have to be freed once the data has been processed.
 */
struct sd788t_rxbuf * sd788t_create_rxbuf(int size) {
	struct sd788t_rxbuf * rxbuf;
	rxbuf=calloc(1,sizeof(struct sd788t_rxbuf));
	rxbuf->size=size;
	rxbuf->readBuf=calloc(size,sizeof(uint8_t));
	memset(rxbuf->readBuf,0,size);
	return rxbuf;
}

/*
 * free up the rxbuffer
 */
void sd788t_free_rxbuf(struct sd788t_rxbuf * rxbuf) {
	free(rxbuf->readBuf);
	free(rxbuf);
}

/*
 * are we connected?
 */
int sd788t_get_connected(struct sp_port ** psp_port) {
	if (connected) {
		(*psp_port)=psd788t_sp_port;
		return SD788T_OK ;
	} else {
		return SD788T_CONNECTION_ERROR;
	}

}

/* 
 * helper function to send a command
 * and return the data buffer
 *
 * 
 * The function will block during sd788t_checkAndSend
 */
struct sd788t_rxbuf * sd788t_sendCommand(struct sp_port *sp_port,uint8_t command,uint8_t cmddata[],uint8_t cmddatasize) {

	uint8_t *buffer;
	uint8_t bufsize;
	int exception=0; // for use when handling meter retrieval which doesn't appear to match the spec :-(
	struct sd788t_rxbuf *retbuffer;
	buffer=sd788t_createCommandBuffer(command,cmddata,cmddatasize,&bufsize);
	
	if (command==62 && cmddatasize==1 && cmddata[0]==0)  // Get Input Meters
		exception=1;
	if (command==62 && cmddatasize==1 && cmddata[0]==2)  // Get Output Meters
		exception=2;
	retbuffer=sd788t_checkAndSend(exception, sp_port,buffer,bufsize);
	return retbuffer;
} 
/*
 * utility to assist with changing baud rates
 *
 * assumes we are connected
 */
void sd788t_send40(struct sp_port* sp_port)
{
	uint8_t c40=0x40;

	for (int i = 0; i < 255; i++)
	{
		ewrap (sp_blocking_write(sp_port,&c40,1,500));
	}
}

/*
 * function to connect usb serial to the 788
 */
int sd788t_connect(int nsport) {

	readbuf=malloc(512);

	sd788t_sp_port * sp_port;

	sp_port=sd788t_sp_get_port_by_number(nsport);

	ewrap (sp_open(sp_port,SP_MODE_READ_WRITE));
	ewrap (sp_set_baudrate(sp_port,9600));
	ewrap (sp_set_bits(sp_port,8));
	ewrap (sp_set_parity(sp_port,SP_PARITY_NONE));
	ewrap (sp_set_stopbits(sp_port,1));
	ewrap (sp_start_break(sp_port));
	usleep (300);
	ewrap (sp_end_break(sp_port));
	ewrap (sp_set_baudrate(sp_port,115200));
	sd788t_send40(sp_port);

	connected=1;
	psd788t_sp_port=sp_port;

	return connected;
}

/*
 * check incoming checksums
 */
int sd788t_checkIncoming(uint8_t *buf,int bufLen) {

	uint8_t sum1=0;
	uint8_t sum2=0;

	for (int i = 0; i < bufLen; i++)
	{
		sum1=(sum1+buf[i]) % 255;
		sum2=(sum2+sum1) % 255;
	}
	return (sum1+sum2) % 255;
}

struct sd788t_rxbuf * sd788t_getRxBuf(){
	return pReturnBuffer;
}

/*
 * Initialize and populate the command buffer headers
 */
uint8_t * sd788t_createCommandBuffer(uint8_t command, uint8_t cmddata[],uint8_t cmddatasize,uint8_t *bufsize) {

	/*
	 * adding six to the cmddatasize:
	 * header,id,length,command,cksum1,cksum2
	 */
	char checksum1=0;
	char checksum2=0;
	char sum1=0;
	char sum2=0;
	uint8_t j=0;
	uint8_t *sendBuf=calloc(cmddatasize+10,sizeof(uint8_t));
	memset(sendBuf,0,cmddatasize+6);

	sendBuf[j++]=(uint8_t)0xA5;
	sendBuf[j++]=(uint8_t)0x00;
	sendBuf[j++]=(uint8_t)cmddatasize+3;
	sendBuf[j++]=(uint8_t)command;
	if (cmddatasize && cmddata!=NULL) {
		for (int i=0;i<cmddatasize;i++) {
			sendBuf[j++]=(uint8_t)cmddata[i];
		}
	}

	for (int i = 0; i < j+2; i++)
	{
		if (i == j)
		{
			checksum1 = 255 - ((sum1+sum2) % 255); sendBuf[j] = (char) checksum1;
		}
		if (i == j+1)
		{
			checksum2 = 255 - sum1; sendBuf[j+1] = (char) checksum2;
		}
		sum1=(sum1+sendBuf[i]) % 255; sum2=(sum2+sum1) % 255;
	}

	(*bufsize)=j+2;
	return sendBuf;

}

struct sd788t_rxbuf * sd788t_checkAndSend(int exception, struct sp_port* sp_port,uint8_t * sendBuf, uint8_t bufLen)
{
	int size=0;

	struct sd788t_rxbuf * rxbuf;

	ewrap (sp_flush(sp_port,SP_BUF_BOTH ));
	ewrap (sp_nonblocking_write(sp_port,sendBuf,bufLen));
	ewrap (sp_drain(sp_port));

	free(sendBuf);


	int nBytes;

	memset(readbuf,0,512);
	//ewrap2 (nBytes=sp_blocking_read(sp_port,readbuf,100,50));
	ewrap (nBytes=sp_nonblocking_read(sp_port,readbuf,100));

	//printf("nBytes=%u\n",nBytes);
	// check response
	//
	
	if ((uint8_t)(*((uint8_t *)readbuf)) != 0xA5) {
		return NULL;
	}

	/*
	 * not Required due Input and Output metering 
	 * not really doing what you would expect.
	 *
	switch (exception) {
		// handling GetMeter with a parameter of 0 (Inputs)
		//
		// returned sequence is A5 FE F1 etc... there is no "length" parameter
		// so this needs to be inserted.
		case 1:
			memcpy(readbuf+50,readbuf+2,nBytes-2);
			memcpy(readbuf+3,readbuf+50,nBytes-2);
			(*((uint8_t *)(readbuf+2)))=16;
			break;
		// handling GetMeter with a parameter of 1 (Outputs)
		//
		// returned sequence is A5 FE F1 etc... there is no "length" parameter
		// so this needs to be inserted.
		case 2:
			memcpy(readbuf+50,readbuf+2,nBytes-2);
			memcpy(readbuf+3,readbuf+50,nBytes-2);
			(*((uint8_t *)(readbuf+2)))=12;
			break;
		default:
			break;
	}
	*/

	//nBytes=sp_input_waiting(sp_port);

	size=(uint8_t)(*(uint8_t*)(readbuf+2)); // amount of data we are going to read

	ewrap (sp_flush(sp_port,SP_BUF_INPUT));

	rxbuf=sd788t_create_rxbuf(size);

	if (!sd788t_checkIncoming(readbuf,size+3)) {
		memcpy(rxbuf->readBuf,readbuf+3,size);
		rxbuf->size=size;
		rxbuf->status=SD788T_RXBUF_OK;
	} else {
		rxbuf->status=SD788T_RXBUF_CHECKSUM_ERROR;
	}

	return rxbuf;
}
