/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * main.cc
 * Copyright (C) jinjianxin 2010 <jianxin.jin@asianux.com>
 *
 * listen is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * listen is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <mcheck.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <sys/stat.h>
#include <error.h>
#include <sqlite3.h>
#include <ctype.h>
#include <signal.h>
#include <term.h>
#include <curses.h>
#include <unistd.h>

#include <glib.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <libintl.h>

#include "blueHFPApi.h"


static DBusConnection *dbus_conn;
DBusError dbus_error;
pthread_t thread[2];
pthread_mutex_t mut;
sqlite3 *db;
gboolean end = TRUE;
gboolean is_calling = FALSE;
gboolean is_icall = FALSE;
gboolean is_ecall = FALSE;
gboolean is_begin = FALSE;
gboolean is_new = FALSE;
double lon = 0.;
double lat = 0.;
time_t sms_time;

void cmdResult(const char* cmdstr, int result);
void sendmessage(const char* cmdstr, int result);
void sendfom(const char* cmdstr, int result);
void SendRegister(const char* cmdstr, int result);
void send_panel_message();
void send_panel_callinginactive();
gboolean sms_database_init();
gboolean change_volume(gpointer data);
gboolean read_sm(gpointer data);
gboolean delete_sm(gpointer data);


/*发送紧急短信*/
void send_danger_message(char *number)
{
    printf("send danger number =%s\n ",number);

//	system("dbus-send --dest=com.asianux.gps / com.asianux.gps.mlr");

}

static gboolean dbus_name_has_owner(gchar * name)
{
    gboolean ret;
    ret = dbus_bus_name_has_owner(dbus_conn, name, NULL);

    return ret;
}

static DBusHandlerResult mking_signal_filter(DBusConnection *connection,
        DBusMessage *message, void *user_data)
{
    DBusMessageIter arg;
    char *str;
    char *contents;

    if (dbus_message_is_signal(message,"com.asianux.pdaemon","call"))
    {
        if (!dbus_message_iter_init(message,&arg))
        {
            fprintf(stderr,"Message has no Param\n");
        }
        else if (dbus_message_iter_get_arg_type(&arg) != DBUS_TYPE_STRING)
        {
            g_printerr("Param is not string\n");
        }
        else
        {
            dbus_message_iter_get_basic(&arg,&str);
//			app_agent_set_play(appAgent);

            cmdResult("拨打电话 dial",dial(str));
            is_calling = TRUE;
        }
    }
    else if (dbus_message_is_signal(message,"com.asianux.pdaemon","hook"))
    {
        if (!dbus_message_iter_init(message,&arg))
        {
            fprintf(stderr,"Message has no Param\n");
        }
        else if (dbus_message_iter_get_arg_type(&arg) != DBUS_TYPE_STRING)
        {
            g_printerr("Param is not string\n");
        }
        else
        {
            dbus_message_iter_get_basic(&arg,&str);

            cmdResult("挂机 hangup",handupCall());
            send_panel_callinginactive();
            is_calling = FALSE;
        }
    }
    else if (dbus_message_is_signal(message,"com.asianux.pdaemon","answer"))
    {
		printf("\n-------answer the call ------\n");
        cmdResult("接听电话 answer call",answerCall());
        is_calling = TRUE;

    }
    else if (dbus_message_is_signal(message,"com.asianux.pdaemon","reject"))
    {
		printf("\n---------reject the call ------\n");
        cmdResult("拒听电话 reject call",rejectCall());
		is_calling = FALSE;
    }
    else if (dbus_message_is_signal(message,"com.asianux.pdaemon","sms"))
    {
        if (!dbus_message_iter_init(message,&arg))
        {
            fprintf(stderr,"Message has no Param\n");
        }
        else if (dbus_message_iter_get_arg_type(&arg) != DBUS_TYPE_STRING)
        {
            g_printerr("Param is not string\n");
        }
        else
        {
            dbus_message_iter_get_basic(&arg,&str);
            printf("Get phone number value:%s\n",str);

            dbus_message_iter_next(&arg);

            dbus_message_iter_get_basic(&arg,&contents);
            printf("get sms contents value;%s\n",contents);
		
//            sendmessage("send message",sendSM(contents,str));	
			sendSM(contents,str);
			sendSM(string(contents),string(str));
        }
    }
    else if (dbus_message_is_signal(message,"com.asianux.pdaemon","fom"))
    {
        if (!dbus_message_iter_init(message,&arg))
        {
            fprintf(stderr,"Message has no Param\n");
        }
        else if (dbus_message_iter_get_arg_type(&arg) != DBUS_TYPE_STRING)
        {
            g_printerr("Param is not string\n");
        }
        else
        {
            dbus_message_iter_get_basic(&arg,&str);
            printf("Get phone number value:%s\n",str);

            dbus_message_iter_next(&arg);

            dbus_message_iter_get_basic(&arg,&contents);
            printf("get sms contents value;%s\n",contents);

         //   sendfom("send fom",sendSM(contents,str));
			sendSM(contents,str);

        }


    }
    else if (dbus_message_is_signal(message,"com.asianux.pdaemon","register"))
    {
        if (!dbus_message_iter_init(message,&arg))
        {
            fprintf(stderr,"Message has no Param\n");
        }
        else if (dbus_message_iter_get_arg_type(&arg) != DBUS_TYPE_STRING)
        {
            g_printerr("Param is not string\n");
        }
        else
        {
            dbus_message_iter_get_basic(&arg,&str);
            printf("Get phone number value:%s\n",str);

            dbus_message_iter_next(&arg);
            dbus_message_iter_get_basic(&arg,&contents);
            printf("get sms contents value;%s\n",contents);
            SendRegister("send register",sendSM(contents,str));

            printf("---------------------------------------send register\n");
        }
    }
    else if (dbus_message_is_signal(message,"com.asianux.pdaemon","extension"))
    {
        if (!dbus_message_iter_init(message,&arg))
        {
            fprintf(stderr,"Message has no Param\n");
        }
        else if (dbus_message_iter_get_arg_type(&arg) != DBUS_TYPE_STRING)
        {
            g_printerr("Param is not string\n");
        }
        else
        {
            dbus_message_iter_get_basic(&arg,&str);
            printf("############Get extension  value:%s\n",str);

            dialExtNumber(str);
        }
    }
    else if (dbus_message_is_signal(message,"com.asianux.pdaemon","read"))
    {
        //cmdResult("读取短信",readSM(UNREAD_SM));
    }

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

void send_panel_message()
{
    DBusMessage *message;
    message = dbus_message_new_signal("/mid/panel/Object","mid.panel.Type","calling");

    dbus_connection_send(dbus_conn,message,NULL);

    dbus_connection_flush(dbus_conn);
    dbus_message_unref(message);

    printf("send calling \n");

}

gboolean change_volume(gpointer data)
{
    printf("---------------------------change volume\n");
    send_panel_message();
    printf("---------------------------change end\n");

    return FALSE;
}

gboolean delete_sm(gpointer data)
{
//	cmdResult("删除短信",deletesendSM(READED_SM | SENT_SM | UNSENDT_SM | UNREAD_SM));
	return FALSE;
}

gboolean read_sm(gpointer data)
{
  //  cmdResult("读取短信",readSM(READED_SM | SENT_SM | UNSENDT_SM | UNREAD_SM));
//	g_timeout_add(15000,delete_sm,NULL);
    return FALSE;
}

void send_panel_callinginactive()
{
    DBusMessage *message;
    message = dbus_message_new_signal("/mid/panel/Object","mid.panel.Type","callinactive");

    dbus_connection_send(dbus_conn,message,NULL);

    dbus_connection_flush(dbus_conn);
    dbus_message_unref(message);

    printf("send end call \n");

}

gboolean listen_dbus_init()
{
    dbus_error_init(&dbus_error);
    dbus_conn = dbus_bus_get(DBUS_BUS_SESSION, &dbus_error);

    if (!dbus_conn)
    {
        printf("Failed to connect to the D-BUS daemon: %s\n", dbus_error.message);
        dbus_error_free(&dbus_error);
        return 1;
    }

    dbus_connection_setup_with_g_main(dbus_conn, NULL);
    dbus_bus_add_match(dbus_conn, "type='signal',interface='com.asianux.pdaemon'", NULL);
    dbus_connection_add_filter (dbus_conn, mking_signal_filter, NULL, NULL);
//    dbus_bus_request_name(dbus_conn, "com.asianux.pdaemon", DBUS_NAME_FLAG_DO_NOT_QUEUE, NULL);

    return TRUE;
}

void register_msg(char *sigvalue)
{
    DBusMessage* msg;
    DBusMessageIter args;
    DBusConnection* conn;
    DBusError err;
    int ret;

    dbus_uint32_t serial = 0;


    // initialise the error value
    dbus_error_init(&err);

    // connect to the DBUS system bus, and check for errors
    conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "Connection Error (%s)\n", err.message);
        dbus_error_free(&err);
    }

    if (NULL == conn) {
        exit(1);
    }

    // register our name on the bus, and check for errors
    ret = dbus_bus_request_name(conn, "com.asianux.pdaemon", DBUS_NAME_FLAG_DO_NOT_QUEUE, &err);
    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "Name Error (%s)\n", err.message);
        dbus_error_free(&err);
    }

    if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != ret) {
        exit(1);
    }
}


unsigned long HFPMsgCallBack(HFPEVENT event, void* lpParam, unsigned long param)
{
    printf("HFPMsgCallBack:%d\n",event);

    switch (event)
    {
    case RING_EVENT:
    {
        return 1;
    }
    break;
	case SMS_SUCCESS:
	{
		printf("send sms success\n");
		system("dbus-send / com.asianux.sms.succesd ");
		system("dbus-send / com.asianux.fom.succesd ");
	}
	break;
	case SMS_FAILED:
	{
		system("dbus-send / com.asianux.sms.failed ");
		system("dbus-send / com.asianux.fom.failed ");
		printf("sms sms filed\n");
	}
	break;
    case PHONESTATUS_EVENT:
    {
        PHONESTATUS status = (PHONESTATUS)param;
        switch (status)
        {
        case CALLINCOMING:
        {
			printf("-----call  in  -----\n");

            return 0;
        }
        break;
        case CALLOUTGOING:
		{
			printf("-----call  out -----\n");
			
            usleep(1200);
			
			send_panel_message();
			
//			app_agent_set_play(appAgent);

			is_calling = TRUE;

        }
        break;
        case CALLACTIVE:
        {
			printf("-----call active-----\n");
            DBusMessage *message;
            dbus_uint32_t serial = 0;

            message = dbus_message_new_signal("/","com.asianux.notification","begin");
            if (!dbus_connection_send(dbus_conn,message,&serial))
            {
                exit(1);
            }

            dbus_connection_flush(dbus_conn);
            dbus_message_unref(message);

//			app_agent_set_play(appAgent);

            send_panel_message();

            is_calling = TRUE;

        }
        break;
        case CALLINACTIVE:
        {
			printf("-----call end-----\n");

			if(is_calling == TRUE)
			{
				printf("-----end---\n\n");
	            DBusMessage *msg;
		        dbus_uint32_t serial_1 = 0;
			    msg = dbus_message_new_signal("/","com.asianux.notification","end");

				if (!dbus_connection_send(dbus_conn,msg,&serial_1))
				{
					printf("-----send end notification failed-----\n");
					exit(1);
				}

	            dbus_connection_flush(dbus_conn);
		        dbus_message_unref(msg);
			}
		    else
			{
				printf("----hook----\n\n");
				DBusMessage *msg2;
				dbus_uint32_t serial_2 =0;
				msg2 = dbus_message_new_signal("/","com.asianux.notification","hook");

				if(!dbus_connection_send(dbus_conn,msg2,&serial_2))
				{
					printf("-------send hook notification failed-----\n");
					exit(1);
				}

				dbus_connection_flush(dbus_conn);
				dbus_message_unref(msg2);
			}

//			app_agent_set_stop(appAgent);

            is_calling = FALSE;
            is_new = FALSE;

            //    printf("##############################end\n");

        }
        break;
        }
    }
    break;
    case CALLERNUMBER_EVENT:	//呼叫着电话号码
    {

        char number[512];

        sprintf(number," dbus-send / com.asianux.notification.notify string:dial string:%s string: string: ",param);

        if (!is_new)
        {
            system(number);
            is_new = TRUE;
        }
        return 1;
    }
    break;
    case SMMT_EVENT:
    {
		printf("----param is %s-----\n",param);

		const char *pconstChar =(const char *)param;

	    char test[1024];
		
		sprintf(test,"%s",pconstChar);

		printf("****************%s\n",test);
		
		int length = strlen(pconstChar);

        if (length == 0)
		{
			return 1;
		}
	
        char *sms =(char *)malloc(sizeof(char)*(length+1));

        strcpy(sms,pconstChar);

		printf("------%s--------------%s\n",sms,pconstChar);

        char *pchar[3];

        pchar[0] = pchar[1] = pchar[2]=0;

        int pos=0;

        pchar[0] = sms;
        int itmp = 1;

        pos = strcspn(sms,";");
	
		while (pos<length)
        {
            pchar[itmp-1][pos] ='\0';
			pchar[itmp] = pchar[itmp-1]+pos+1;
            length-=(pos)+1;
        }
	
        pos = strcspn(pchar[itmp],";");

        pchar[itmp][pos]='\0';
        pchar[itmp+1]=pchar[itmp]+pos+1;

        printf("收到短信（rev sm）:%s ##  %s ##  %s  %s\n", pchar[0],pchar[1],pchar[2],sms);

        char *sms_fom =(char *)malloc(512);
        sms_fom = strdup(pchar[2]);

        int index = 0;
        char *sms_str[3];
        sms_str[0] = sms_str[1] = sms_str[2]=0;
        sms_str[0] =sms_fom;

        index = strcspn(sms_fom,":");


        if (index == 3)
        {
			
            sms_str[0][index] = '\0';
            sms_str[1] =sms_str[0]+index+1;

            if (strcmp(sms_str[0],"tas") == 0)
            {
                char *number = (char *)malloc(64);

                strncpy(number,sms_str[1],3);

                char *message = (char *)malloc(1024);
			
                sprintf(message,"dbus-send  / com.asianux.smsterm.sms string:\"%s\"",strdup(test));

				printf("==============%s\n",message);

                system(message);

                free(message);
                message = NULL;

                free(number);
                number = NULL;

                return 1;
            }
            else
            {

                char *errMsg = NULL;
                char sql[500];

                int ret = sqlite3_open("/root/.sms.db",&db);
                if (ret == SQLITE_OK)
                {
                }
                else
                {
                    //                  printf("open database failed\n");
                }

                time(&sms_time);

                char *number =(char *) malloc(50);
                char *time = (char *)malloc(50);
                char *text = (char *)malloc(1024);
                char *content = (char *)malloc(1024);
                char *filter_number = (char *)malloc(50);
                char *filter_sms = (char *)malloc(1024);

                number = strdup(pchar[0]);
                time = strdup(pchar[1]);
                text = strdup(pchar[2]);

                int i=0;
                int j = 0;

                for (; i<strlen(text); i++)
                {
                    if (text[i] =='\n' || text[i] =='\r')
                    {
                        content[j]=' ';
                    }
                    else if (text[i] == '\'')
                    {
                        content[j] =='\'';
                        j++;
                        content[j]='\'';
                    }
                    else
                    {
                        content[j]=text[i];
                    }
                    j++;
                }

                content[j] = '\0';

                int m=0;
                int n=0;

                for (; n<strlen(text); n++)
                {
                    if (text[n] =='\n' || text[n] =='\r')
                    {
                        filter_sms[m]=' ';
                    }
                    else if (text[n] == '\'' || text[n] == '\"')
                    {
                        filter_sms[m]=' ';
                    }
                    else
                    {
                        filter_sms[m]=text[n];
                    }
                    m++;
                }

                filter_sms[m]='\0';

                char *str= NULL;
                str = strstr(number,"+86");
                if (str)
                {
                    int j=3;
                    int m = 0;

                    for (; number[j] != '\0'; j++)
                    {
                        filter_number[m]=number[j];
                        m++;
                    }

                    filter_number[m]='\0';

                }
                else
                {
                    filter_number = strdup(number);
                }

                sprintf(sql,"INSERT INTO \"sms\" VALUES(NULL,1,'%s',NULL,'%s\n','%s',1,%ld);",filter_number,content,time,sms_time);
                ret = sqlite3_exec(db,sql,0,0,&errMsg);

                if (ret == SQLITE_OK)
                {
                    sqlite3_close(db);
                    free(sms);
                    sms = NULL;

                    char *send = (char *)malloc((1024)*sizeof(char));
                    sprintf(send,"dbus-send / com.asianux.notification.notify string:\"sms\" string:\"%s\" string:\"%s\" string:\"%s\"",number,filter_sms,time);

                    system(send);

                    free(send);
                    send = NULL;

                    free(number);
                    free(time);
                    free(text);
                    free(content);
                    free(filter_number);
                    free(filter_sms);
                    number = NULL;
                    time = NULL;
                    text = NULL;
                    content = NULL;
                    filter_number = NULL;
                    filter_sms = NULL;

                    return 1;
                }
                else
                {
                    sqlite3_close(db);
                    free(sms);
                    sms = NULL;
                    free(number);
                    free(time);
                    free(text);
                    free(content);
                    free(filter_number);
                    free(filter_sms);
                    number = NULL;
                    time = NULL;
                    text = NULL;
                    content = NULL;
                    filter_number = NULL;
                    filter_sms = NULL;
                    return 0;
                }

            }

        }
        else
        {

            char *errMsg = NULL;
            char sql[500];

            int ret = sqlite3_open("/root/.sms.db",&db);
            if (ret == SQLITE_OK)
            {
            }
            else
            {
            }

            time(&sms_time);

            char *number =(char *) malloc(50);
            char *time = (char *)malloc(50);
            char *text = (char *)malloc(1024);
            char *content = (char *)malloc(1024);
            char *filter_number = (char *)malloc(50);
            char *filter_sms = (char *)malloc(1024);

            number = strdup(pchar[0]);
            time = strdup(pchar[1]);
            text = strdup(pchar[2]);

            int i=0;
            int j=0;

            for (; i<strlen(text); i++)
            {
                if (text[i] =='\n' || text[i] =='\r')
                {
                    content[j]=' ';
                }
                else if (text[i] == '\'')
                {
                    content[j]='\'';
                    j++;
                    content[j] = '\'';
                }
                else
                {
                    content[j]=text[i];
                }
                j++;
            }

            content[j]='\0';

            int m=0;
            int n=0;

            for (; n<strlen(text); n++)
            {
                if (text[n] =='\n' || text[n] =='\r')
                {
                    filter_sms[m]=' ';
                }
                else if (text[n] == '\'' || text[n] == '\"')
                {
                    filter_sms[m]=' ';
                }
                else
                {
                    filter_sms[m]=text[n];
                }
                m++;
            }

            filter_sms[m]='\0';

            char *str= NULL;
            str = strstr(number,"+86");
            if (str)
            {
                int j=3;
                int m = 0;

                for (; number[j] != '\0'; j++)
                {
                    filter_number[m]=number[j];
                    m++;
                }

                filter_number[m]='\0';


            }
            else
            {
                filter_number = strdup(number);

            }

            sprintf(sql,"INSERT INTO \"sms\" VALUES(NULL,1,'%s',NULL,'%s\n','%s',1,%ld);",filter_number,content,time,sms_time);

            ret = sqlite3_exec(db,sql,0,0,&errMsg);

            if (ret == SQLITE_OK)
            {
                sqlite3_close(db);
                free(sms);
                sms = NULL;

                char *send = (char *)malloc((1024)*sizeof(char));
                sprintf(send,"dbus-send  / com.asianux.notification.notify string:sms string:\"%s\" string:\"%s\" string:\"%s\"",number,filter_sms,time);

                system(send);

                free(send);
                send = NULL;
                free(number);
                free(time);
                free(text);
                free(content);
                free(filter_number);
                free(filter_sms);
                number = NULL;
                time = NULL;
                text = NULL;
                content = NULL;
                filter_number = NULL;
                filter_sms = NULL;
                return 1;
            }
            else
            {
                sqlite3_close(db);
                free(sms);
                sms = NULL;
                free(number);
                free(time);
                free(text);
                free(content);
                free(filter_number);
                free(filter_sms);
                number = NULL;
                time = NULL;
                text = NULL;
                content = NULL;
                filter_number = NULL;
                filter_sms = NULL;
                return 0;
            }


        }
        
		free(sms_fom);
        sms_fom = NULL;

    }
    break;
	}
}

void *thread1(void *arg)
{
    int tmp =0;
    setMsgCallBack(HFPMsgCallBack, &tmp);

    if (HFP_SUCCESS != initBlueHFP(1000))
    {
        printf("无法链接设备\n");
        setMsgCallBack(NULL, NULL);

        unInitBlueHFP();
        printf("press anykey to quit\n");

        exit(0);

        getchar();

        return 0;
    }
}

void thread_create(void)
{
    int temp;
    memset(&thread,0,sizeof(thread));

    if ((temp = pthread_create(&thread[0],NULL,thread1,NULL)) !=0)
    {
        printf("thread1 create false\n");
    }
    else
    {
        printf("thread1 create succesd\n");

    }
}

void hook(int dunno)
{
    cmdResult("拒听电话 reject call",rejectCall());
    cmdResult("挂机 hangup",handupCall());
    printf("挂断电话\n");
    unInitBlueHFP();
}

int main(int argc,char *argv[])
{
	g_type_init();

	GMainLoop *loop;
    loop = g_main_loop_new(NULL,FALSE);

	listen_dbus_init();
    sms_database_init();
    send_panel_callinginactive();

	if (HFP_SUCCESS != initBlueHFP(1000))		
 	{
		printf("无法链接设备\n");

		setMsgCallBack(NULL, NULL);

		printf("press anykey to quit\n");

		exit(0);
		getchar();
		return 0;
	}

	int tmp =0;
	setMsgCallBack(HFPMsgCallBack, &tmp);

	test();

    g_timeout_add(15000,read_sm,NULL);
	
	g_main_run(loop);

    return 0;
}

void sendmessage(const char* cmdstr, int result)
{
    char *strResult = 0;
    switch (result)
    {
    case HFP_SUCCESS:
    {
        printf("send messgae is succesd\n");
        DBusMessage *message;
        dbus_uint32_t serial = 0;

        message = dbus_message_new_signal("/","com.asianux.sms","succesd");

        if (!dbus_connection_send(dbus_conn,message,&serial))
        {
            printf("send signal dailed\n");
            exit(1);
        }

        dbus_connection_flush(dbus_conn);
        dbus_message_unref(message);

    }
    break;
    default:
    {
        printf("send message is failed\n");
        DBusMessage *message;
        dbus_uint32_t serial = 0;

        message = dbus_message_new_signal("/","com.asianux.sms","failed");

        if (!dbus_connection_send(dbus_conn,message,&serial))
        {
            printf("send signal dailed\n");
            exit(1);
        }

        dbus_connection_flush(dbus_conn);
        dbus_message_unref(message);
    }
    }
}

void sendfom(const char* cmdstr, int result)
{
    char *strResult = 0;
    switch (result)
    {
    case HFP_SUCCESS:
    {
        printf("send messgae is succesd\n");
        DBusMessage *message;
        dbus_uint32_t serial = 0;

        message = dbus_message_new_signal("/","com.asianux.fom","succesd");

        if (!dbus_connection_send(dbus_conn,message,&serial))
        {
            printf("send signal dailed\n");
            exit(1);
        }

        dbus_connection_flush(dbus_conn);
        dbus_message_unref(message);

    }
    break;
    default:
    {
        printf("send message is failed\n");
        DBusMessage *message;
        dbus_uint32_t serial = 0;

        message = dbus_message_new_signal("/","com.asianux.fom","failed");

        if (!dbus_connection_send(dbus_conn,message,&serial))
        {
            printf("send signal dailed\n");
            exit(1);
        }

        dbus_connection_flush(dbus_conn);
        dbus_message_unref(message);
    }
    }
}

void SendRegister(const char* cmdstr, int result)
{
    char *strResult = 0;
    switch (result)
    {
    case HFP_SUCCESS:
    {
        printf("send messgae is succesd\n");
        DBusMessage *message;
        dbus_uint32_t serial = 0;

        message = dbus_message_new_signal("/","com.asianux.register","send_successed");

        if (!dbus_connection_send(dbus_conn,message,&serial))
        {
            printf("send signal dailed\n");
            exit(1);
        }

        dbus_connection_flush(dbus_conn);
        dbus_message_unref(message);

    }
    break;
    default:
    {
        printf("send message is failed\n");
        DBusMessage *message;
        dbus_uint32_t serial = 0;

        message = dbus_message_new_signal("/","com.asianux.register","send_failed");

        if (!dbus_connection_send(dbus_conn,message,&serial))
        {
            printf("send signal dailed\n");
            exit(1);
        }

        dbus_connection_flush(dbus_conn);
        dbus_message_unref(message);
    }
    }
}

void cmdResult(const char* cmdstr, int result)
{
    char* strResult = 0;
    switch (result)
    {
    case HFP_SUCCESS:
    {
        strResult = "succesd";
    }
    break;
    }

    printf("---send commands%s:%s\n", cmdstr,strResult);
}

gboolean sms_database_init()
{
    int rc;
    char *zErrMsg;
    char *sql;
    sqlite3 *init_db;

    const char *home = g_get_home_dir();
    char *base_name = g_build_filename(home,".sms.db",NULL);

    printf("%s\n",base_name);

    rc = sqlite3_open(base_name,&init_db);

    if (rc)
    {
        printf("Can't open database:%s\n",sqlite3_errmsg(init_db));
        sqlite3_close(init_db);
    }
    else
    {
        sql = "CREATE TABLE sms(ID INTEGER PRIMARY KEY autoincrement,READ integer,NUMBER text,NAME text,MES text,TIME text,IO integer,date INTEGER);";
        rc = sqlite3_exec(init_db,sql,0,0,&zErrMsg);

        if (rc == SQLITE_OK)
        {
            printf("create table successed\n");
        }
        else
        {
            printf("create table failed\n");
        }

    }
}
