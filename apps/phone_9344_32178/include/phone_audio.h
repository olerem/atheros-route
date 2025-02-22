#ifndef __PHONE_AUDIO_H__
#define __PHONE_AUDIO_H__

#include "phone_control.h"
#include "common.h"
#include "si32178.h"
#include "speex/speex_echo.h"
#include "speex/speex_preprocess.h"
#include "signal_processing_library.h"
#include "echo_control_mobile.h"

//#define SAVE_OUT_DATE
//#define SAVE_FILE
//#define SAVE_INCOMING_FILE

#define SET_WRITE_TYPE      _IOW('N', 0x26, int)
#define SLIC_INIT			_IOW('N', 0x27, int)
#define SLIC_RELEASE		_IOW('N', 0x28, int)

#define PSTN 				1
#define TALKBACK 			0

#define PCM_RESET			0
#define PCM_NO_RESET		1

#define CALL				1
#define CALLED				0
#define SWITCH				0
#define RECONNECT			0
#define OTHER				0

typedef struct {
  unsigned short y[4];
  unsigned short x[2];
  const unsigned short* ba;
} HighPassFilterState;  

struct class_phone_audio
{
	int (*init_audio)(void);
	int phone_audio_sockfd;
	int phone_audio_pcmfd;
	int audio_base2pad_flag;
	int audio_pad2base_flag;
	int input_stream_wp;
	int input_stream_rp;
	int output_stream_wp;
	int output_stream_rp;
	int echo_stream_wp;
	int echo_stream_rp;
	int audio_reconnect_loops;
	int audio_reconnect_flag;
	int audio_send_thread_exit_flag;
	int audio_recv_thread_exit_flag;
	int audio_echo_thread_exit_flag;
	int audio_read_write_thread_exit_flag;
	int audio_send_thread_flag;
	int audio_recv_thread_flag;
	int audio_echo_thread_flag;
	int audio_read_write_thread_flag;
	int audio_incoming_thread_flag;
	int audio_talkbacked_recv_send_thread_flag;
	int audio_talkback_recv_thread_flag;
	int audio_talkback_send_thread_flag;
	int start_recv;	//拨号时，不接收数据
	int get_code; //解析到号码
	int get_code_timeout; //获取号码等待ring信号
	int get_code_timeout_times;
	int dtmf_over; //dtmf发送结束
	int dialup;
	char start_dtmf;
	int startaudio_mode;
	int time_to_reset;
	char key[8];
	SpeexEchoState *st;
	SpeexPreprocessState *den;
	SpeexPreprocessState *dn;
	pthread_mutex_t aec_mutex;
	HighPassFilterState hpf;
	void *aecmInst;
};

int init_audio(void);
void start_read_incoming();
void stop_read_incoming();
int startaudio(dev_status_t* devp,int flag,int action_mode);
void stopaudio(dev_status_t* devp,int flag,int reconnect_flag);
void* audio_loop_accept(void *argv);
int Encrypt(unsigned char *data,int len);
int Decrypt(unsigned char *data,int len);
int UlawEncode(unsigned char *pout,unsigned char *pin,int mSampleCount);
int UlawDecode(unsigned char *pout,unsigned char *pin,int inbytes);
int UlawEncodeEncrypt(unsigned char *pout,unsigned char *pin,int mSampleCount);
int UlawDecodeEncrypt(unsigned char *pout,unsigned char *pin,int inbytes);
void* AudioWriteThreadCallBack(void* argv);
void* AudioRecvThreadCallBack(void* argv);
void* AudioSendThreadCallBack(void* argv);
void* AudioReadThreadCallBack(void* argv);
void* AudioReadWriteThreadCallBack(void* argv);
void* AudioIncomingThreadCallBack(void* argv);
void* AudioEchoThreadCallBack(void* argv);

extern struct class_phone_audio phone_audio;
extern struct class_phone_control phone_control;
extern int pcm_ret;
extern int total_dtmf_ret;
extern int do_cmd_onhook(dev_status_t *dev);

#endif
