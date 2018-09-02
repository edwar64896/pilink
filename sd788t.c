#define _GNU_SOURCE
#include <stdio.h>
#include <ncurses.h>
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
#include <uev/uev.h>
#include <stdint.h>
#include <signal.h>
#include <simple2d.h>
#include "clink.h"


S2D_Text *s2d_timecode;
S2D_Text *s2d_temperature;
S2D_Window *s2d_window;
S2D_Text **s2d_channel_text;
S2D_Text **s2d_output_channel_text;
S2D_Text **s2d_input_channel_text;

S2D_Image *img_hdd_on;
S2D_Image *img_hdd_off;
S2D_Image *img_cf_on;
S2D_Image *img_cf_off;
S2D_Image *img_ext_on;
S2D_Image *img_ext_off;

pthread_t	update_thread;

int lastRead=0;
int verbose=0;
long tick=0;


struct currentstatus {
	uint8_t transport;
	uint16_t channelsarmed;
	char * samplerate;
	char * bitdepth;
	char * framerate;
	char * medselect;
	uint8_t medselect_bitmask;
	uint8_t vmajor,vminor;
	uint8_t hw1,hw2;
	uint8_t tcmajor,tcminor;
	uint8_t oxmajor,oxminor;
	uint16_t oxextended;
	char devicename[50];
	sd788t_meter_value * input_meters;
	sd788t_meter_value * track_meters;
	sd788t_meter_value * output_meters;
	sd788t_smpte regen_tc;
	sd788t_smpte ext_tc;
	sd788t_smpte file_tc;
	float temperature;
	float all_drives_utilization;
	float cf_utilization;
	float hdd_utilization;
	float ext_utilization;
	float adc_vext;
} currentstatus;

char *pTC,*pTemperature_text;

void renderSystemStatus();
void renderMediaSelects();

void 
sd788t_check_system_status() {
	uint8_t version,takeflags,parflags1,parflags2,parflags3;
	uint32_t takehandle;
	uint8_t size;
	int status;

	status=sd788t_get_parameter_change_status(&version,&takeflags,&takehandle,&parflags1,&parflags2,&parflags3);

	switch (takeflags) {
		case 0:
			break;
		case 1:
			break;
		case 2:
			break;
	}

	if (parflags1 & (0b01 << 0)) {
		// transport status changed
		sd788t_get_transport(&currentstatus.transport);
	}
	if (parflags1 & (0b01 << 1)) {
		// channel arming status changed??
		//sd788t_get_setting(124,&size,&currentstatus.channelsarmed);
		sd788t_user_get(124,&currentstatus.channelsarmed);
		printf("armed=%u\n",currentstatus.channelsarmed);
	}
	if (parflags1 & (0b01 << 6)) {
		//meter ballistics
	}
	if (parflags1 & (0b01 << 7)) {
		//meter peak hold change
	}
	if (parflags1 & (0b01 << 0)) {
		// input to tracks routing change
	}
	if (parflags2 & (0b01 << 1)) {
		// input to tracks pre/post fade change
	}
	if (parflags2 & (0b01 << 2)) {
		//output routing change
	}
	if (parflags2 & (0b01 << 4)) {
		//sample rate change
		sd788t_user_get_sample_rate(&currentstatus.samplerate);
	}
	if (parflags2 & (0b01 << 5)) {
		//bit depth change
		sd788t_user_get_bit_depth(&currentstatus.bitdepth);
	}
	if (parflags3 & (0b01 << 0)) {
		//Frame Rate Change
		sd788t_user_get_frame_rate(&currentstatus.framerate);
	}

	sd788t_get_temperature(&currentstatus.temperature);
	sd788t_get_sys_metric(TRANSPORT_UTILIZATION,INHDD,&currentstatus.hdd_utilization);
	sd788t_get_sys_metric(TRANSPORT_UTILIZATION,CF,&currentstatus.cf_utilization);
	sd788t_get_sys_metric(TRANSPORT_UTILIZATION,EXT,&currentstatus.ext_utilization);
	sd788t_get_sys_metric(TRANSPORT_UTILIZATION,ALL_DRIVES,&currentstatus.all_drives_utilization);
	sd788t_get_adc_vext(&currentstatus.adc_vext);
	sd788t_get_media_select(&currentstatus.medselect_bitmask,&currentstatus.medselect) ;
	sd788t_user_get_frame_rate(&currentstatus.framerate);
	sd788t_user_get_bit_depth(&currentstatus.bitdepth);
	sd788t_user_get_sample_rate(&currentstatus.samplerate);

	//printf("%s/%s/%s fps/%s\n",currentstatus.samplerate,currentstatus.bitdepth,currentstatus.framerate,currentstatus.medselect);
}

void process_port_list(uint8_t port_number, char* port_name,char* port_description) {
	printf("%01u: %s : %s\n",port_number,port_name, port_description);
}




void cleanup_exit(uev_t *w,void *arg, int events) {
	if (UEV_ERROR == events) {
		printf("Ignoring signal watcher error...\n");
	}
	printf("inside the signal handler and cleaning up\n");
	uev_exit(w->ctx);
}




void do_once() {

	sd788t_get_sw_version(&currentstatus.vmajor,&currentstatus.vminor);
	sd788t_get_hw_version(&currentstatus.hw1,&currentstatus.hw2);
	sd788t_get_timecode_version(&currentstatus.tcmajor,&currentstatus.tcminor);
	sd788t_get_oxford_version(&currentstatus.oxmajor,&currentstatus.oxminor,&currentstatus.oxextended);
	sd788t_get_devicename(currentstatus.devicename);
	sd788t_user_get_frame_rate(&currentstatus.framerate);
	sd788t_user_get_bit_depth(&currentstatus.bitdepth);
	sd788t_user_get_sample_rate(&currentstatus.samplerate);
	sd788t_get_temperature(&currentstatus.temperature);
	sd788t_get_sys_metric(TRANSPORT_UTILIZATION,INHDD,&currentstatus.hdd_utilization);
	sd788t_get_sys_metric(TRANSPORT_UTILIZATION,CF,&currentstatus.cf_utilization);
	sd788t_get_sys_metric(TRANSPORT_UTILIZATION,EXT,&currentstatus.ext_utilization);
	sd788t_get_sys_metric(TRANSPORT_UTILIZATION,ALL_DRIVES,&currentstatus.all_drives_utilization);
	sd788t_get_adc_vext(&currentstatus.adc_vext);
	sd788t_get_media_select(&currentstatus.medselect_bitmask,&currentstatus.medselect) ;

}

uint8_t processMeter(uint8_t rawMeter,uint8_t noise) {
	uint8_t t1;
	if (rawMeter>noise)
		rawMeter=noise;

	uint8_t fac=255/noise;
	return rawMeter*fac;

}

typedef struct du {
	float * paramValue;
	char * param;
	char * outtext;
	char * format;
	S2D_Text * s2dText;
	S2D_Text * s2dTextParam;

} params;

params paramsArray[]={
	{ .paramValue=&currentstatus.all_drives_utilization, .param="All Drives", .format="%.1f%%"},
	{ .paramValue=&currentstatus.hdd_utilization, .param="INHDD", .format="%.1f%%"},
	{ .paramValue=&currentstatus.cf_utilization, .param="CF", .format="%.1f%%"},
	{ .paramValue=&currentstatus.ext_utilization, .param="EXT", .format="%.1f%%"},
	{ .paramValue=&currentstatus.temperature, .param="TEMP", .format="%.1f Deg"},
	{ .paramValue=&currentstatus.adc_vext, .param="VEXT", .format="%.1f V"}

};

void renderSystemStatus() {
	for (int i=0;i<6;i++) {
		snprintf(paramsArray[i].outtext,40,paramsArray[i].format,paramsArray[i].param,*paramsArray[i].paramValue);
		S2D_SetText(paramsArray[i].s2dTextParam,paramsArray[i].outtext);
		S2D_DrawText(paramsArray[i].s2dText);
		S2D_DrawText(paramsArray[i].s2dTextParam);
	}
	renderMediaSelects();
}

void setupSystemStatus (uint16_t x,uint16_t y) {

	for (int i=0;i<6;i++) {

		paramsArray[i].s2dText=S2D_CreateText("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",paramsArray[i].param,40);
		paramsArray[i].s2dText->x=x;
		paramsArray[i].s2dText->y=y;
		paramsArray[i].s2dText->color.r=1;
		paramsArray[i].s2dText->color.g=1;
		paramsArray[i].s2dText->color.b=1;
		paramsArray[i].s2dText->color.a=1;

		paramsArray[i].s2dTextParam=S2D_CreateText("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",paramsArray[i].param,40);
		paramsArray[i].s2dTextParam->x=x+200;
		paramsArray[i].s2dTextParam->y=y;
		paramsArray[i].s2dTextParam->color.r=1;
		paramsArray[i].s2dTextParam->color.g=1;
		paramsArray[i].s2dTextParam->color.b=1;
		paramsArray[i].s2dTextParam->color.a=1;
		paramsArray[i].outtext=malloc(50);

		y+=40;
	}
	renderSystemStatus();
}
/*
   void setupInputMeter(uint16_t x, uint16_t y) {
   s2d_input_channel_text=calloc(12,sizeof(S2D_Text*));

   for (int i=0;i<8;i++) {
   s2d_input_channel_text[i]=S2D_CreateText("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf","",40);
   s2d_input_channel_text[i]->x=x;
   s2d_input_channel_text[i]->y=y;
   x+=50;
   }
   }
   void setupOutputMeter(uint16_t x, uint16_t y) {
   s2d_output_channel_text=calloc(12,sizeof(S2D_Text*));

   for (int i=0;i<6;i++) {
   s2d_output_channel_text[i]=S2D_CreateText("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf","",40);
   s2d_output_channel_text[i]->x=x;
   s2d_output_channel_text[i]->y=y;

   x+=50;
   }
   }
   */
void setupTrackMeter(uint16_t x, uint16_t y) {
	s2d_channel_text=calloc(12,sizeof(S2D_Text*));

	for (int i=0;i<12;i++) {
		s2d_channel_text[i]=S2D_CreateText("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",sd788t_get_channel_name(i),40);
		s2d_channel_text[i]->x=x+00;
		s2d_channel_text[i]->y=y;

		x+=50;
	}
}

/*
   void renderInputMeter(uint16_t x, uint16_t y) {
   uint16_t height=600;
   S2D_DrawLine(	x,y,x+height,y,5,
   1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1);
   S2D_DrawLine(	x+height,y,x+height,y-255,5,
   1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1);
   S2D_DrawLine(	x+height,y-255,x,y-255,5,
   1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1);
   S2D_DrawLine(	x,y-255,x,y,5,
   1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1);

   for (int i=0;i<8;i++) {
   S2D_DrawQuad(	(i*50)+20+x,height,0,1,0,1,
   (i*50)+60+x,height,0,1,0,1,
   (i*50)+60+x,345+processMeter(currentstatus.input_meters[i].vu,80),1,0,0,1,
   (i*50)+20+x,345+processMeter(currentstatus.input_meters[i].vu,80),1,0,0,1);
   S2D_DrawLine(	(i*50)+20+x,345+processMeter(currentstatus.input_meters[i].peak,80),
   (i*50)+60+x,345+processMeter(currentstatus.input_meters[i].peak,80),10,
   1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1);
   S2D_DrawText(s2d_output_channel_text[i]);
   }
   }
   void renderOutputMeter(uint16_t x, uint16_t y) {
   uint16_t height=600;
   S2D_DrawLine(	x,y,x+height,y,5,
   1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1);
   S2D_DrawLine(	x+height,y,x+height,y-255,5,
   1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1);
   S2D_DrawLine(	x+height,y-255,x,y-255,5,
   1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1);
   S2D_DrawLine(	x,y-255,x,y,5,
   1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1);

   for (int i=0;i<6;i++) {
   S2D_DrawQuad(	(i*50)+20+x,height,0,1,0,1,
   (i*50)+60+x,height,0,1,0,1,
   (i*50)+60+x,345+processMeter(currentstatus.output_meters[i].vu,80),1,0,0,1,
   (i*50)+20+x,345+processMeter(currentstatus.output_meters[i].vu,80),1,0,0,1);
   S2D_DrawLine(	(i*50)+20+x,345+processMeter(currentstatus.output_meters[i].peak,80),
   (i*50)+60+x,345+processMeter(currentstatus.output_meters[i].peak,80),10,
   1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1);
   S2D_DrawText(s2d_output_channel_text[i]);
   }
   }
   */

void renderTrackMeter(uint16_t x, uint16_t y) {
	uint16_t height=600;
	S2D_DrawLine(	x,y,x+height,y,5,
			1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1);
	S2D_DrawLine(	x+height,y,x+height,y-255,5,
			1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1);
	S2D_DrawLine(	x+height,y-255,x,y-255,5,
			1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1);
	S2D_DrawLine(	x,y-255,x,y,5,
			1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1);

	for (int i=0;i<12;i++) {
		S2D_DrawQuad(	(i*50)+x,height,0,1,0,1,
				(i*50)+40+x,height,0,1,0,1,
				(i*50)+40+x,345+processMeter(currentstatus.track_meters[i].vu,80),1,0,0,1,
				(i*50)+x,345+processMeter(currentstatus.track_meters[i].vu,80),1,0,0,1);
		S2D_DrawLine(	(i*50)+x,345+processMeter(currentstatus.track_meters[i].peak,80),
				(i*50)+40+x,345+processMeter(currentstatus.track_meters[i].peak,80),10,
				1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1);
		S2D_DrawText(s2d_channel_text[i]);
	}
}

void setupMediaSelects(uint16_t x,uint16_t y) {
	img_hdd_on=S2D_CreateImage("png/HDD_ON.png");
	img_hdd_off=S2D_CreateImage("png/HDD_OFF.png");
	img_cf_on=S2D_CreateImage("png/CF_ON.png");
	img_cf_off=S2D_CreateImage("png/CF_OFF.png");
	img_ext_on=S2D_CreateImage("png/EXT_ON.png");
	img_ext_off=S2D_CreateImage("png/EXT_OFF.png");

	img_hdd_on->x=x;
	img_hdd_on->y=y;
	img_hdd_off->x=x;
	img_hdd_off->y=y;

	img_cf_on->x=x+100;
	img_cf_on->y=y;
	img_cf_off->x=x+100;
	img_cf_off->y=y;

	img_ext_on->x=x+200;
	img_ext_on->y=y;
	img_ext_off->x=x+200;
	img_ext_off->y=y;
}

void renderMediaSelects() {

	if (currentstatus.medselect_bitmask & 0b001) {
		S2D_DrawImage(img_hdd_on);
	} else {
		S2D_DrawImage(img_hdd_off);
	}
	if (currentstatus.medselect_bitmask & 0b010) {
		S2D_DrawImage(img_cf_on);
	} else {
		S2D_DrawImage(img_cf_off);
	}
	if (currentstatus.medselect_bitmask & 0b100) {
		S2D_DrawImage(img_ext_on);
	} else {
		S2D_DrawImage(img_ext_off);
	}
}

void renderTimecode(uint16_t x, uint16_t y) {
	if (currentstatus.transport==0 || currentstatus.transport==2) {
		if (currentstatus.transport==2) {
			s2d_timecode->color.r=1;
			s2d_timecode->color.g=0;
			s2d_timecode->color.b=0;
			s2d_timecode->color.a=1;
		} else {
			s2d_timecode->color.r=1;
			s2d_timecode->color.g=1;
			s2d_timecode->color.b=1;
			s2d_timecode->color.a=1;
		}
	} else {
		s2d_timecode->color.r=0;
		s2d_timecode->color.g=1;
		s2d_timecode->color.b=0;
		s2d_timecode->color.a=1;
	}

	snprintf(pTC,12,"%02u:%02u:%02u:%02u",currentstatus.regen_tc.hours,currentstatus.regen_tc.minutes,currentstatus.regen_tc.seconds,currentstatus.regen_tc.frames);
	//printf("tc:%s\n",pTC);

	S2D_DrawText(s2d_timecode);
	S2D_SetText(s2d_timecode,pTC);

}

void render() {
	renderTimecode(10,10);
	renderTrackMeter(20,600);
	renderSystemStatus();
}


void *test_thread_function(void *args) {

	/* 
	 * setup CPU affinity
	 */

	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(3,&cpuset);
	pthread_setaffinity_np(pthread_self(),sizeof(cpuset),&cpuset);

	uint8_t nMeters;
	uint16_t result;
	int i=0;

	while(1) {

		sd788t_user_get(i,&result);
		printf("i=%u,r=%lu\n",i,result);
		i++;

		usleep(100000);
	}
}

void *update_thread_function(void *args) {

	/* 
	 * setup CPU affinity
	 */

	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(3,&cpuset);
	pthread_setaffinity_np(pthread_self(),sizeof(cpuset),&cpuset);

	uint8_t nMeters;

	do_once();

	while(1) {

		if ((tick % 2)==0) {
			//sd788t_get_timecode(&currentstatus.regen_tc,&currentstatus.ext_tc, &currentstatus.file_tc);
		}
		if ((tick % 3)==1) {
			//sd788t_get_track_meters(&nMeters,currentstatus.track_meters);
		}
		if ((tick % 20) ==0) {
			sd788t_check_system_status();
		}

		usleep(5000);
		tick++;
	}
}


void
start788test(int nsport){ 

	if (!sd788t_connect(nsport)) {
		printf("connection error.\n");
		exit(-1);
	}


	/*
	 * Allocate Objects
	 */
	pTC=malloc(20);

	currentstatus.track_meters=calloc(32,sizeof(sd788t_meter_value));
	currentstatus.output_meters=calloc(32,sizeof(sd788t_meter_value));
	currentstatus.input_meters=calloc(32,sizeof(sd788t_meter_value));

	memset(&currentstatus.regen_tc,0,sizeof(struct smpte));
	memset(&currentstatus.ext_tc,0,sizeof(struct smpte));
	memset(&currentstatus.file_tc,0,sizeof(struct smpte));

	memset(&currentstatus.track_meters,0xf1,sizeof(sd788t_meter_value));
	memset(&currentstatus.input_meters,0xf1,sizeof(sd788t_meter_value));
	memset(&currentstatus.output_meters,0xf1,sizeof(sd788t_meter_value));

	pthread_create (&update_thread,NULL,test_thread_function,NULL);
	
	void * retval;
	pthread_join(update_thread,&retval);

}
void
start788(int nsport){ 

	if (!sd788t_connect(nsport)) {
		printf("connection error.\n");
		exit(-1);
	}


	/*
	 * Allocate Objects
	 */
	pTC=malloc(20);

	currentstatus.track_meters=calloc(32,sizeof(sd788t_meter_value));
	currentstatus.output_meters=calloc(32,sizeof(sd788t_meter_value));
	currentstatus.input_meters=calloc(32,sizeof(sd788t_meter_value));

	memset(&currentstatus.regen_tc,0,sizeof(struct smpte));
	memset(&currentstatus.ext_tc,0,sizeof(struct smpte));
	memset(&currentstatus.file_tc,0,sizeof(struct smpte));

	memset(&currentstatus.track_meters,0xf1,sizeof(sd788t_meter_value));
	memset(&currentstatus.input_meters,0xf1,sizeof(sd788t_meter_value));
	memset(&currentstatus.output_meters,0xf1,sizeof(sd788t_meter_value));

	s2d_window=S2D_CreateWindow( "Hello Triangle", 1920,1080,NULL,render,0);
	s2d_timecode=S2D_CreateText("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf","00:00:00:00",110);
	s2d_timecode->x=20;
	s2d_timecode->y=30;


	//setupInputMeter(20,600);
	setupTrackMeter(20,600);
	//setupOutputMeter(1200,600);
	setupSystemStatus (1500,10);

	setupMediaSelects(1600,900);

	pthread_create (&update_thread,NULL,update_thread_function,NULL);

	S2D_Show(s2d_window);

	S2D_FreeWindow(s2d_window);

}


int main(int argc,char*argv[]) {
	int flags,opt;
	int nsecs,tfnd;
	int nsport;

	while ((opt=getopt(argc,argv,"lvd:t:")) != -1) {
		switch(opt) {
			case 'l':
				sd788t_list_ports(process_port_list);
				break;
			case 'd':
				nsport=atoi(optarg);
				start788(nsport);
				break;
			case 't':
				nsport=atoi(optarg);
				start788test(nsport);
				break;
			case 'v':
				verbose=1;
				break;
			default:
				fprintf(stderr,"Usage: %s [-l] [-d #]\n",argv[0]);
				break;
		}
	}

	void * status_ptr;

	exit(0);
}
