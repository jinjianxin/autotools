/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * lib.c
 * Copyright (C) jjx 2012 <jianxin.jin@asianux.com>
 *
 * phone-daemon is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * phone-daemon is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>      /*标准输入输出定义*/
#include <stdlib.h>     /*标准函数库定义*/
#include <unistd.h>     /*Unix 标准函数定义*/
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>      /*文件控制定义*/
#include <termios.h>    /*PPSIX 终端控制定义*/
#include <errno.h>
#include <string.h>
#include <glib.h>
#include <glib-object.h>
#include <iostream>

using namespace std;

#include "unicode.h"
#include "blueHFPApi.h"
#include "blueutils.h"

//#define FALSE  -1
//#define TRUE   0
#define BUF_LEN 1024
#define CHINA_SET_MAX 8

//static  int serial_fd;
//static gboolean lock = TRUE;
//static char *center = "8613010331500";

static int read_id= 0;
static int delete_id = 0;
static int sim_id = 0;
static int process_id = 0;
static int sms_id = 0;
static GQueue *smsQueue;
static gboolean send_sms = FALSE;

typedef struct
{
    int serial_fd;
    gboolean lock;
    char *centerNumber;
    HFPCallback hfpCallback;
} BlueHfp;

BlueHfp *blueHfp = NULL;

int speed_arr[] = { B38400, B19200, B9600, B4800, B2400, B1200, B300,
                    B38400, B19200, B9600, B4800, B2400, B1200, B300,
                  };

int name_arr[] = {38400,  19200,  9600,  4800,  2400,  1200,  300, 38400,
                  19200,  9600, 4800, 2400, 1200,  300,115200,
                 };

void *serial_read_data(gpointer data);
char *blue_hfp_utf8_tounicode(const char *content);
char *blue_hfp_unicode_uth8(char *content);
char *blue_hfp_get_center_number(char *num);
char *blue_hfp_get_address_number(const char *num);
char *blue_hfp_process_string(char *msg);

gboolean blue_hfp_read_sms_timeout(gpointer data);
gboolean blue_hfp_send_sms_timeout(gpointer data);
gboolean blue_hfp_delete_sms_timeout(gpointer data);
gboolean blue_hfp_delete_sim_sms(gpointer data);
gboolean blue_hfp_get_at_zpas(gpointer data);
gboolean blue_hfp_process_string_timeout(gpointer data);


int initBlueHFP(unsigned int  heartbeatInterval)
{
    g_type_init();

    blueHfp = (BlueHfp *)malloc(sizeof(BlueHfp *)*1);

    smsQueue = g_queue_new();

    blueHfp->centerNumber = "8613010331500";

    blueHfp->serial_fd = openDev("/dev/ttyUSB1");

    if(blueHfp->serial_fd == -1)
    {
        exit(0);
    }

    set_speed(blueHfp->serial_fd,115200);

    if (set_Parity(blueHfp->serial_fd,8,1,'N') == FALSE)  {
        printf("Set Parity Error\n");
        exit (0);
    }

    g_thread_create(serial_read_data,(void *)blueHfp->serial_fd,NULL,NULL);

    char *commond = "AT+CSCA?\r";
    int ret = strlen(commond);

    if(ret == write(blueHfp->serial_fd,commond,strlen(commond)))
    {
        printf("write success\n");
    }

    /*设置短信发送格式 0为pdu 1为英文字符*/
    commond = "AT+CMGF=0\r";
    ret = strlen(commond);

    if(ret == write(blueHfp->serial_fd,commond,strlen(commond)))
    {
        printf("write AT+CMGF=0 success\n");
    }

	commond = "AT+ZCSQ=5\r";
    ret = strlen(commond);

    if(ret == write(blueHfp->serial_fd,commond,strlen(commond)))
    {
        printf("write AT+ZCSQ=5 success\n");
    }

	commond = "AT+CSQ\r";
    ret = strlen(commond);

    if(ret == write(blueHfp->serial_fd,commond,strlen(commond)))
    {
        printf("write AT+CSQ=0 success\n");
    }

//    read_id = g_timeout_add_seconds(5,G_CALLBACK(blue_hfp_read_sms_timeout),NULL);
	read_id = g_timeout_add_seconds(5,blue_hfp_read_sms_timeout,NULL);

//    g_timeout_add_seconds(3,G_CALLBACK(blue_hfp_get_at_zpas),NULL);

    blueHfp->lock = FALSE;

    /*	commond = "AT+CMGL=4\t";

        int size = strlen(commond);

        if(size == write(blueHfp->serial_fd,commond,strlen(commond)))
        {
            printf("read sms  success\n");
        }

        printf("%s\n",commond);

    	blueHfp->lock = TRUE;
    	*/


    /*	char *str = "+CSCA: \"+8613010331500\",145";

    	char *tmp = blue_utils_get_center_number(str);
    	printf("%s\n",tmp);

    	char *str = "+CLIP: \"18661018857\",128,,,,0";

    	char *tmp = blue_utils_get_ring_number(str);
    	printf("%s\n",tmp);


    	char *str = "+CMGL: 0,1,,25\
    				  0891683110406505F0240BA15161512770F1000821010221749523066D4B8BD57684";

    	char *tmp = blue_utils_get_sms_msg(str);

    	printf("tmp=%s\n",tmp);

    	if(tmp !=NULL)
    	{
     		if(blueHfp->hfpCallback)
    		{
    			(blueHfp->hfpCallback)(SMMT_EVENT,NULL,tmp);
    		}
    	}
    */
    /*
    	char *str = "+CMTI: \"ME\",12";

    	char *index = blue_utils_notify_sms(str);

    	printf("index=%s\n",index);
    	*/
    /*
        char *str = "+CMGR: 0,,31\
        		0891683110406505F0240BA15161512770F10008210122418232230C6D4B8BD58D1F62C55F975230";

    */

    /*	char *str = "+CMGR: 0,,159\
    				 \
    		0891683110301305F0400BA18166018158F70008210142111174238C050003F5030200300030524DFF0C56DE590D0057005800580058004AFF08514D8D39FF095C316709673A4F1A514D8D3954C15C1D4EF7503C0033003400315143768456DB4EBA59579910FF0C88AB62BD4E2D4F1A545853EF518D5E260031540D4EB253CBFF084EB253CB97004E3A554676DF4F1A5458621673B0573A5F00901AFF0930024E5F53EF51736CE8";
    */

/*    	char *str1 = "+CMGR: 0,,159\
    		\
    		0891683110301305F0400BA18166018158F70008210142618285238C050003B0030170ED8FA398CE5E2D53778EAB5FC3FF0C6E58571F60C56ECB54737EF5957FFF0100310030002F00320033002000310038003A00300030FF0C0031003200350038003055466237805476DF908060A867655B096E588BB0FF0891D1592A6E565E97FF09514D8D394EAB752899998FA359579910FF0100310030002F00320031002000320034003A";

        char *p = NULL;
        p = strstr(str1,"CMGR");
        if(p !=NULL)
        {
            char *tmp = blue_utils_get_sms_msg(str1);
            printf("%s\n",tmp);
    	}

    	char *str2 = "+CMGR: 0,,159\
    		\
    		0891683110301305F0400BA18166018158F70008210142618285238C050003B0030200300030524DFF0C56DE590D0057005800580058004AFF08514D8D39FF095C316709673A4F1A514D8D3954C15C1D4EF7503C0033003400315143768456DB4EBA59579910FF0C88AB62BD4E2D4F1A545853EF518D5E260031540D4EB253CBFF084EB253CB97004E3A554676DF4F1A5458621673B0573A5F00901AFF0930024E5F53EF51736CE8";

        p = NULL;
        p = strstr(str2,"CMGR");
        if(p !=NULL)
        {
            char *tmp = blue_utils_get_sms_msg(str2);
            printf("%s\n",tmp);
    	}
*/
/*    	char *str3 = "+CMGR: 0,,27\
			\
			0891683110301305F0040BA18166018158F70000210162118365230931D98C56B3DD7239";*/


		char *str3 = "+CMGR: 0,,53\
			\
			0891683110301305F0040BA18166018158F700002101130175452326F4F05C1783C5743119CC25CBE56430DD2CE6AAD9703098AE9D07B1DFE3303DFD7603";
		
		char *p = NULL;
        p = strstr(str3,"CMGR");
        if(p !=NULL)
        {
            //char *tmp = blue_utils_get_sms_msg(str3);
    	}

		string str4("+CMGR: 0,,53\
			\
			0891683110301305F0040BA18166018158F700002101130175452326F4F05C1783C5743119CC25CBE56430DD2CE6AAD9703098AE9D07B1DFE3303DFD7603");

		unsigned found = str4.find("CMGR");

		if(found>0)
		{
			cout<<found<<endl;
			string *tmp = blue_utils_get_sms_msg(str4);
		}
		

 
/*		char *str4 ="+CMGL: 1,0,,136\
			\
			0891683110300405F12405A10110F0000821019290253423786E2999A863D0793AFF1A81F30032003965E5003165F600330034520660A85F5367085DF275286D4191CF0036002E00350036004D0042FF0C5957991052694F5956FD518500350033002E00340034004D00423002672C6B2167E58BE27ED3679C5B5857285EF665F6FF0C8BF74EE551FA8D264E3A51C63002";

		p = NULL;
        p = strstr(str4,"CMGL");
        if(p !=NULL)
        {
            char *tmp = blue_utils_get_sms_msg(str4);
            printf("%s\n",tmp);
    	}
 */

/*
	char *str3 = "+CMGR: 0,,95\
    		\
    		0891683110301305F0440BA18166018158F70008210142618295234C050003B00303201C65E0952179FB52A800310032003500380030554676DF201D65B06D6A5FAE535A53C24E0E76F851736D3B52A8300230100031003200350038003055466237805476DF3011";

	char *p = NULL;
	p = NULL;
	p = strstr(str3,"CMGR");
	if(p !=NULL)
	{
		int i =0 ;

		for(i = 0;i<5;i++)
		{
			g_queue_push_tail(smsQueue,str3);
		}
	}

	if(process_id == 0)
	{
		process_id = g_timeout_add_seconds(3,G_CALLBACK(blue_hfp_process_string_timeout),NULL);
	}
	*/

/*	char *str3 = "+CSQ: 17,99";
	char *p = NULL;
	p = NULL;
	p = strstr(str3,"CSQ");
	if(p !=NULL)
	{
		char *tmp = blue_utils_get_signal(str3);
		printf("tmp = %s\n",tmp);
	}
*/

    return HFP_SUCCESS;
}

int openDev(const char *dev)
{
    int	fd = open( dev,O_RDWR|O_NOCTTY );

    if (-1 == fd)
    {
        perror("Can't Open Serial Port");
        return -1;
    }
    else
        return fd;
}


void set_speed(int fd, int speed)
{
    int   i;
    int   status;
    struct termios   Opt;

    tcgetattr(fd, &Opt);
    for ( i= 0;  i < sizeof(speed_arr) / sizeof(int);  i++) {
        if  (speed == name_arr[i]) {
            tcflush(fd, TCIOFLUSH);
            cfsetispeed(&Opt, speed_arr[i]);
            cfsetospeed(&Opt, speed_arr[i]);
            status = tcsetattr(fd, TCSANOW, &Opt);
            if  (status != 0) {
                perror("tcsetattr fd1");
                return;
            }
            tcflush(fd,TCIOFLUSH);
        }
    }
}

int set_Parity(int fd,int databits,int stopbits,int parity)
{

    struct termios options;
    if  ( tcgetattr( fd,&options)  !=  0) {
        perror("SetupSerial 1");
        return(FALSE);
    }
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CLOCAL | CREAD;
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

    switch (databits) /*设置数据位数*/
    {
    case 7:
        options.c_cflag |= CS7;
        break;
    case 8:
        options.c_cflag |= CS8;
        break;
    default:
        fprintf(stderr,"Unsupported data size\n");
        return (FALSE);
    }
    switch (parity)
    {
    case 'n':
    case 'N':
        options.c_cflag &= ~PARENB;   /* Clear parity enable */
        options.c_iflag &= ~INPCK;     /* Enable parity checking */
        break;
    case 'o':
    case 'O':
        options.c_cflag |= (PARODD | PARENB); /* 设置为奇效验*/
        options.c_iflag |= INPCK;             /* Disnable parity checking */
        break;
    case 'e':
    case 'E':
        options.c_cflag |= PARENB;     /* Enable parity */
        options.c_cflag &= ~PARODD;   /* 转换为偶效验*/
        options.c_iflag |= INPCK;       /* Disnable parity checking */
        break;
    case 'S':
    case 's':  /*as no parity*/
        options.c_cflag &= ~PARENB;
        options.c_cflag &= ~CSTOPB;
        break;
    default:
        fprintf(stderr,"Unsupported parity\n");
        return (FALSE);
    }
    /* 设置停止位*/
    switch (stopbits)
    {
    case 1:
        options.c_cflag &= ~CSTOPB;
        break;
    case 2:
        options.c_cflag |= CSTOPB;
        break;
    default:
        fprintf(stderr,"Unsupported stop bits\n");
        return (FALSE);
    }
    /* Set input parity option */
    if (parity != 'n')
        options.c_iflag |= INPCK;
    //tcflush(fd,TCIFLUSH);
    tcflush(fd, TCIOFLUSH);
    //options.c_cc[VTIME] = 150; /* 设置超时15 seconds*/
    options.c_cc[VTIME] = 10;
    options.c_cc[VMIN] = 0; /* Update the options and do it NOW */
    if (tcsetattr(fd,TCSANOW,&options) != 0)
    {
        perror("SetupSerial 3");
        return (FALSE);
    }
    return (TRUE);

}

int unInitBlueHFP()
{
    close(blueHfp->serial_fd);
    free(blueHfp);
    blueHfp = NULL;
}

int dial(const char* number)
{
    blueHfp->lock = FALSE;
    char *commond = (char *)malloc(sizeof(char *)*128);
    memset(commond,0,strlen(commond));

    sprintf(commond,"ATD%s;\r",number);

    int size = strlen(commond);

    if(size == write(blueHfp->serial_fd,commond,strlen(commond)))
    {
        printf("make call success\n");
    }

    printf("%s\n",commond);

    free(commond);
    commond = NULL;
    blueHfp->lock = TRUE;
}

int dialExtNumber(const char* number)
{
	blueHfp->lock = FALSE;
    char *commond = (char *)malloc(sizeof(char *)*128);
    memset(commond,0,strlen(commond));

    sprintf(commond,"AT+VTS=%s\r",number);

    int size = strlen(commond);

    if(size == write(blueHfp->serial_fd,commond,strlen(commond)))
    {
        printf("dial extension success\n");
    }

    printf("%s\n",commond);

    free(commond);
    commond = NULL;
    blueHfp->lock = TRUE;


}

int answerCall()
{
    char *commond = "ATA\r";
    int size = strlen(commond);

    if(size == write(blueHfp->serial_fd,commond,strlen(commond)))
    {
        printf("%s\n",commond);
    }

}

int rejectCall()
{
    char *commond = "AT+CHUP\r";
    int size = strlen(commond);

    if(size == write(blueHfp->serial_fd,commond,strlen(commond)))
    {
        printf("%s\n",commond);
    }
}

int handupCall()
{
    char *commond = "AT+CHUP\r";
    int size = strlen(commond);

    if(size == write(blueHfp->serial_fd,commond,strlen(commond)))
    {
        printf("%s\n",commond);
    }

}

int sendSM(const char *content,const char *number)
{
    char *addressNumber = blue_hfp_get_address_number(number);

    char *size = blue_utils_number_to16(strlen(number));

    char *sendMsg  = (char *)malloc(sizeof(char *)*1024);
    memset(sendMsg,'\0',strlen(sendMsg));

    sprintf(sendMsg,"1100%s91%s",size,addressNumber);

    char *centerNumber = blue_hfp_get_center_number(blueHfp->centerNumber);

    char *msg = blue_hfp_utf8_tounicode(content);

    printf("%s\n",msg);

    char *commond  = (char *)malloc(sizeof(char *)*512);
    memset(commond,'\0',strlen(commond));

    sprintf(commond,"%s%s000800%s\x1A",centerNumber,addressNumber,msg);

    printf("%d\n",strlen(addressNumber)+6+strlen(msg));


    char * commond1 = (char *)malloc(sizeof(char *)*64);
    memset(commond1,'\0',strlen(commond1));

    char *commond2 = (char *)malloc(sizeof(char *)*1024);
    memset(commond2,'\0',strlen(commond2));

    sprintf(commond1,"AT+CMGS=%d\r",(strlen(addressNumber)+6+strlen(msg))/2);

    sprintf(commond2,"%s",commond);
    printf("%s\n%s\n",commond1,commond2);

    blueHfp->lock = FALSE;

    int ret = strlen(commond1);
    if(ret == write(blueHfp->serial_fd,commond1,strlen(commond1)))
    {
        printf("commond1 success\n");
    }

    ret = strlen(commond2);
    if(ret == write(blueHfp->serial_fd,commond2,strlen(commond2)))
    {
        printf("commond2 success\n");
    }


    blueHfp->lock = TRUE;

	sms_id = g_timeout_add_seconds(15,blue_hfp_send_sms_timeout,NULL);
	send_sms = TRUE;

	return HFP_SUCCESS;
}

int deletesendSM(unsigned short choice)
{
}

int readSM(unsigned short choice)
{
}

char *blue_hfp_get_center_number(char *num)
{
    char *str1 = blue_utils_switch_number(num);
    char str2[32];
    sprintf(str2,"91%s",str1);
    char *size = blue_utils_number_to16(strlen(str2)/2);

    char *str3 = (char *)malloc(sizeof(char *)*32);
    memset(str3,'\0',strlen(str3));

    sprintf(str3,"%s%s",size,str2);

    char *number = strdup(str3);

    free(str3);
    str3 = NULL;

    return number;
}

char *blue_hfp_get_address_number(const char *num)
{
    char *str1 = blue_utils_switch_number(num);
    char *size = blue_utils_number_to16(strlen(num));

    char *str3 = (char *)malloc(sizeof(char *)*64);
    memset(str3,'\0',strlen(str3));

    sprintf(str3,"1100%s91%s",size,str1);

    char *number = strdup(str3);

    free(str3);
    str3 = NULL;

    return number;
}

char *blue_hfp_utf8_tounicode(const char *content)
{
    unsigned char ch[CHINA_SET_MAX], *p = NULL, *e = NULL, *s = NULL;
    int i = 0, n = 0, unicode = 0;

    char *msg = (char*)malloc(sizeof(char *)*512);
    memset(msg,'\0',strlen(msg));

    char *name = (char *)malloc(strlen(content)*sizeof(char *));
    name = strdup(content);

    i=0;
    while(name[i] !='\0')
    {
        if(name[i] >=0 && name[i]<=127)
        {
            if(name[i] !=' ')
            {
                char zimu[2];
                zimu[0]=name[i];
                zimu[1]='\0';
                char *str = strdup(zimu);

                n = UTF8toUnicode((unsigned char *)str,&unicode);
                if(unicode<255)
                {
/*                    char str[16] ;
                    sprintf(str,"00%x",unicode);
                    strcat(msg,str);*/

					char str[16] ;
					sprintf(str,"00%x",unicode);
					if(strlen(str) == 3)	
					{
						char tmp[16];
						sprintf(tmp,"0%s",str);
						strcat(msg,tmp);
					}
					else	
					{
						strcat(msg,str);
					}
                }
                else
                {
                    char str[16] ;
                    sprintf(str,"%x",unicode);
                    strcat(msg,str);
                }

            }

            i++;
        }
        else
        {
//			printf("这个是一个汉字\n");
            char hanzi[4];
            hanzi[0]=name[i];
            hanzi[1]=name[i+1];
            hanzi[2]=name[i+2];
            hanzi[3]='\0';
            char *str = strdup(hanzi);

            n = UTF8toUnicode((unsigned char *)str,&unicode);
            if(unicode<255)
            {
                char str[16];
                sprintf(str,"00%x",unicode);
                strcat(msg,str);
            }
            else
            {
                char str[16];
                sprintf(str,"%x",unicode);
                strcat(msg,str);

            }

            i+=3;
        }
    }

    char *str = blue_utils_number_to16(strlen(msg)/2);

    char tmp[1024];
    sprintf(tmp,"%s%s",str,msg);

    free(msg);
    msg = NULL;

    free(name);
    name = NULL;

    char *message = strdup(tmp);

    return message;
}

char *blue_hfp_unicode_utf8(char *content)
{
}

void *serial_read_data(gpointer data)
{
    fd_set rfd;
    int ret;
    struct timeval timeout;
    char buf[BUF_LEN];

    while(1)
    {
        /*        if(lock)
                {
        			*/
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        FD_ZERO(&rfd);
        FD_SET(blueHfp->serial_fd, &rfd);
        ret = select(blueHfp->serial_fd+1, &rfd, NULL, NULL, &timeout);

        if(FD_ISSET(blueHfp->serial_fd, &rfd))
        {
            do
            {
                memset(buf, '\0', BUF_LEN);
                ret = read(blueHfp->serial_fd, buf, BUF_LEN);
                if(ret > 0)
                {
                    char *str = strdup(buf);
                    printf("%s\n",str);
                    blue_hfp_process_string(str);
                }
            } while(ret > 0);
        }
//        }
    }
}

char *blue_hfp_process_string(char *msg)
{
    printf("%s\n",msg);

    char *p = NULL;

    if(p = strstr(msg,"+CSCA"))
    {
        if(p !=NULL)
        {
//			printf("=========%s\n",p);
            char *center = blue_utils_get_center_number(msg);
            blueHfp->centerNumber = strdup(center);
//			printf("%s\n",center);
        }
    }
    else if(p = strstr(msg,"STOPRING"))
    {
        printf("hangup\n");

        if(p !=NULL)
        {
            if(blueHfp->hfpCallback)
            {
                (blueHfp->hfpCallback)(PHONESTATUS_EVENT,NULL,CALLINACTIVE);
            }
        }
    }
    else if(p = strstr(msg,"HANGUP"))
    {
        if(p !=NULL)
        {
            if(blueHfp->hfpCallback)
            {
                (blueHfp->hfpCallback)(PHONESTATUS_EVENT,NULL,CALLINACTIVE);
            }
        }
    }
	else if(p = strstr(msg,"NO CARRIER"))
	{
		if(p !=NULL)
        {
            if(blueHfp->hfpCallback)
            {
                (blueHfp->hfpCallback)(PHONESTATUS_EVENT,NULL,CALLINACTIVE);
            }
        }

	}
    else if(p = strstr(msg,"+CLIP"))
    {
        if(p = strstr(msg,"+CLIP"))
        {
            char *ringNumber = blue_utils_get_ring_number(msg);

            if(blueHfp->hfpCallback)
            {
                (blueHfp->hfpCallback)(CALLERNUMBER_EVENT,(void*)CALLACTIVE,(unsigned long )ringNumber);
            }
        }
    }
    else if(p = strstr(msg,"CMTI"))
    {
        if(p = strstr(msg,"CMTI"))
        {
            char *index = blue_utils_notify_sms(msg);
            if(index !=NULL)
            {
                char *commond = (char *)malloc(sizeof(char *)*512);
                memset(commond,'\0',strlen(commond));

                sprintf(commond,"AT+CMGR=%s\r",index);

                blueHfp->lock = FALSE;

                int size = strlen(commond);

                if(size == write(blueHfp->serial_fd,commond,strlen(commond)))
                {
                    printf("make call success\n");
                }
                printf("%s\n",commond);

                free(commond);
                commond = NULL;
                blueHfp->lock = TRUE;

                sim_id = g_timeout_add_seconds(3,blue_hfp_delete_sim_sms,index);
            }
        }
    }
    else if(p = strstr(msg,"ANSWER"))
    {
        printf("answer\n");

        if(blueHfp->hfpCallback)
        {
            (blueHfp->hfpCallback)(PHONESTATUS_EVENT,NULL,CALLACTIVE);
        }
    }
    else if(p = strstr(msg,"+CMGL"))
    {
        printf("message\n");
        if(p !=NULL)
        {
            /*char *str = strdup(msg);
            printf("***********%s\n",msg);
            char *tmp= blue_utils_get_sms_msg(str);
            if(tmp !=NULL)
            {
                if(blueHfp->hfpCallback)
                {
                    (blueHfp->hfpCallback)(SMMT_EVENT,NULL,tmp);
                }
            }
            */

            if(process_id == 0)
            {
                //process_id = g_timeout_add_seconds(3,G_CALLBACK((void *)blue_hfp_process_string_timeout),NULL);
				process_id = g_timeout_add_seconds(3,blue_hfp_process_string_timeout,NULL);
            }

            g_queue_push_tail(smsQueue,msg);
        }
    }
    else if(p = strstr(msg,"CMGR"))
    {
        if(p !=NULL)
        {
            char *str = strdup(msg);
/*            printf("***********%s\n",msg);
            char *tmp= blue_utils_get_sms_msg(str);

            if(tmp !=NULL)
            {
                if(blueHfp->hfpCallback)
                {
                    (blueHfp->hfpCallback)(SMMT_EVENT,NULL,tmp);
                }
            }
			*/

			if(process_id == 0)
            {
                process_id = g_timeout_add_seconds(3,blue_hfp_process_string_timeout,NULL);
            }

            g_queue_push_tail(smsQueue,msg);

        }
    }
	else if(p = strstr(msg,"CSQ"))
	{
		if(p !=NULL)
		{
			char *tmp = blue_utils_get_signal(msg);
			printf("singal = %s\n",tmp);
			if(blueHfp->hfpCallback)
			{
				(blueHfp->hfpCallback)(SINGAL_EVENT,NULL,(unsigned long)tmp);
			}
		}
	}
	else if(p = strstr(msg,"+CMGS:"))
	{
		if(p !=NULL)
		{
			if(blueHfp->hfpCallback)
			{
				(blueHfp->hfpCallback)(SMS_SUCCESS,NULL,NULL);
				if(sms_id !=0)
				{
					g_source_remove(sms_id);
					sms_id = 0;
				}

				send_sms = FALSE;
			}
		}
	}
}

void setMsgCallBack(HFPCallback pFunc,void *data)
{
    blueHfp->hfpCallback = pFunc;

    /*	char *str = "+CSCA: \"+8613010331500\",145";

    	char *tmp = blue_utils_get_center_number(str);
    	printf("%s\n",tmp);

    	if(blueHfp->hfpCallback)
    	{
    		(blueHfp->hfpCallback)(RING_EVENT,CALLACTIVE,tmp);
    	}

    	char *str = "+CLIP: \"18661018857\",128,,,,0";

    	char *tmp = blue_utils_get_ring_number(str);
    	printf("%s\n",tmp);

    */

}

gboolean blue_hfp_send_sms_timeout(gpointer data)
{
	if(sms_id !=0)
	{
		g_source_remove(sms_id);
        sms_id = 0;
	}

	if(blueHfp->hfpCallback)
	{
		(blueHfp->hfpCallback)(SMS_FAILED,NULL,NULL);
	}

	send_sms = FALSE;

	return FALSE;
}

gboolean blue_hfp_read_sms_timeout(gpointer data)
{
    printf("read sms success\n");
    if(read_id !=0)
    {
        g_source_remove(read_id);
        read_id = 0;
    }

    blueHfp->lock = FALSE;

    char *commond = "AT+CMGL=4\r";

    int size = strlen(commond);

    if(size == write(blueHfp->serial_fd,commond,strlen(commond)))
    {
        printf("read sms  success\n");
    }

    printf("%s\n",commond);

    blueHfp->lock = TRUE;


    delete_id = g_timeout_add_seconds(10,blue_hfp_delete_sms_timeout,NULL);

    printf("***********************\n");

    return FALSE;
}

gboolean blue_hfp_delete_sms_timeout(gpointer data)
{
    printf("delete sms timeout\n");
    if(delete_id !=0)
    {
        g_source_remove(delete_id);
        delete_id = 0;
    }

    blueHfp->lock = FALSE;

    char *commond = "AT+CMGD=1,3\r";

    int size = strlen(commond);

    if(size == write(blueHfp->serial_fd,commond,strlen(commond)))
    {
        printf("read sms  success\n");
    }

    printf("%s\n",commond);

    blueHfp->lock = TRUE;

    return FALSE;
}

gboolean blue_hfp_delete_sim_sms(gpointer data)
{
    if(sim_id !=0)
    {
        g_source_remove(sim_id);
        sim_id = 0;
    }

    char *commond = (char *)malloc(sizeof(char *)*512);

    memset(commond,'\0',strlen(commond));

    sprintf(commond,"AT+CMGD=%s\r",(char *)data);
    blueHfp->lock = FALSE;

    int size = strlen(commond);
    if(size == write(blueHfp->serial_fd,commond,strlen(commond)))
    {
        printf("delete sms for sim\n");
    }
    printf("%s\n",commond);

    free(commond);
    commond = NULL;

    blueHfp->lock = TRUE;
}

gboolean blue_hfp_get_at_zpas(gpointer data)
{
    char *commond = "AT+ZPAS?\r";
    int ret = strlen(commond);

    if(ret == write(blueHfp->serial_fd,commond,strlen(commond)))
    {
    }

    return TRUE;
}


gboolean blue_hfp_process_string_timeout(gpointer data)
{
    if(g_queue_is_empty(smsQueue))
    {
        printf("smsQueue is empty\n");
        g_source_remove(process_id);
        process_id = 0;

	    char *commond = "AT+CMGD=1,3\r";

		int size = strlen(commond);

		if(size == write(blueHfp->serial_fd,commond,strlen(commond)))
		{
			printf("read sms  success\n");
		}

	}
    else
    {

		printf("-----------------------\n");
        gpointer str= g_queue_pop_head(smsQueue);
            
		char *tmp= blue_utils_get_sms_msg((char *)str);

		printf("*********************%s\n",tmp);
        
		if(tmp !=NULL)
		{
			if(blueHfp->hfpCallback)
			{
				(blueHfp->hfpCallback)(SMMT_EVENT,NULL,(int)tmp);
			}
		}
	}



    return TRUE;
}

void test()
{
    char *tmp = "test";
//	(blueHfp->hfpCallback)(PHONESTATUS_EVENT,NULL,CALLINACTIVE);
//	(blueHfp->hfpCallback)(PHONESTATUS_EVENT,NULL,CALLACTIVE);
//	(blueHfp->hfpCallback)(SMMT_EVENT,NULL,tmp);

    /*
    char *str = "+CMGR: 0,,143\
    	0891683110406505F0240BA13129510233F80008210132415330237C002E002C003F0027003A20260040002F002800210022003B0029002A0026005B007E0060005C005D002300240025005E003D002D002B005F007D007C003CFF07FF0EFF0C003EFF1FFF0FFF08FF1AFF06FF0AFF1BFF09FF02FF01FF3BFF03FF5EFF40FF3CFF04FF05FE3FFF3FFF5BFF1DFF0DFF0BFF5DFF5CFF1CFF1E";

    char *p = NULL;
    p = strstr(str,"CMGR");
    if(p !=NULL)
    {
        char *tmp = blue_utils_get_sms_msg(str);

    	if(blueHfp->hfpCallback)
    	{
    		(blueHfp->hfpCallback)(SMMT_EVENT,NULL,tmp);
    	}


    }*/

/*		char *str3 = "+CMGR: 0,,53\
			\
			0891683110301305F0040BA18166018158F700002101130175452326F4F05C1783C5743119CC25CBE56430DD2CE6AAD9703098AE9D07B1DFE3303DFD7603";

		char *p = NULL;
        p = strstr(str3,"CMGR");
        if(p !=NULL)
        {
            char *tmp = blue_utils_get_sms_msg(str3);
            printf("%s\n",tmp);
			(blueHfp->hfpCallback)(SMMT_EVENT,NULL,tmp);
    	}

*/
}
