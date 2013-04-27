#define DEBUG

#include "blueutils.h"
#include "unicode.h"
#include <glib.h>
#include <iostream>
#include <string>
#include <string.h>

char *blue_utils_get_center_number(char *arg)
{

//    printf("==============%s\n",arg);

    char *center = "86";
    char *p = NULL;
    p= strstr(arg,"+CSCA");
    char str[32];
    int j=0;

    if(p != NULL)
    {
        char *s = NULL;
        s = strstr(p,"+86");

        if(s !=NULL)
        {

            if(strlen(s)>13)
            {
                int i=1,j=0;

                if(s !=NULL)
                {
                    while(s[i] !='\0')
                    {
                        if(j<13)
                        {
                            str[j] = s[i];
                            j++;
                        }
                        i++;
                    }
                }

                str[j]='\0';
                center = strdup(str);
            }
        }


    }

    printf("%s\n",center);

    return center;
}

char *blue_utils_get_ring_number(char *arg)
{
    printf("arg=%s\n",arg);

    char *p = NULL;
    char *number = NULL;
    p= strstr(arg,"CLIP:");
    char str[32];
    int j=0;

    if(p != NULL)
    {
        gchar ** str = g_strsplit(p,"\"",3);
        if(str[1])
        {
            number = strdup(str[1]);
        }
    }

    return number;
}

string blue_utils_get_sms_msg(const string& arg)
{
    string message;
    unsigned found = arg.find("0891");
#ifdef DEBUG
//    cout<<arg.substr(found)<<endl;
#endif
    string msg = arg.substr(found);

    if(msg.length()>40)
    {
        string center_number = msg.substr(0,2);
#ifdef DEBUG
        cout<<center_number<<endl;
#endif
        int ret = blue_utils_number_to10(center_number);
        //包含短信号码 短信时间 短信内容
        string msg_ntm = msg.substr(ret*2+2);
//		cout<<"msg_ntm="<<msg_ntm<<endl;

        int msg_nl = blue_utils_number_to10(msg_ntm.substr(2,2));
        string msg_n;
        if(msg_nl%2==0)
        {
            msg_n = msg_ntm.substr(6,msg_nl);
            const char * number = msg_n.c_str();
			string send_number = blue_utils_switch_number(msg_n);
            cout<<send_number<<endl;

            string msg_tm = msg_ntm.substr(22);

        }
        else
        {
            msg_n = msg_ntm.substr(6,msg_nl+1);
            string send_number = blue_utils_switch_number(msg_n);
            message+=send_number.substr(0,msg_nl);
            message+=':';

            string msg_tm = msg_ntm.substr(6+msg_nl+1);

            if(msg_tm.substr(0,4) == "0000")
            {
                string time = blue_utils_get_msg_time(msg_tm.substr(4,14));
                message+=time;
                message+=':';

                string content = blue_utils_get_msg_content(msg_tm.substr(18),2);

                message+=content;

                return message;
//				cout<<message<<endl;

            }
            else
            {
                string time = blue_utils_get_msg_time(msg_tm.substr(4,14));
                message+=time;
                message+=':';

                string content = blue_utils_get_msg_content(msg_tm.substr(18),4);
                message+=content;
                return message;
            }

        }
    }

}

char *blue_utils_number_to16(int size)
{
    char tmp[16];
    sprintf(tmp,"%x",size);

    char * number = (char *)malloc(sizeof(char *)*16);
//    memset(number,'\0',strlen(number));

    if(strlen(tmp) ==1)
    {
        sprintf(number,"0%s",tmp);
    }
    else
    {
        sprintf(number,"%s",tmp);
    }

    char * str =strdup(number);

    free(number);
    number = NULL;

    return str;
}

int blue_utils_number_to10(const string& str)
{
    return stoi(str,0,16);
}

string blue_utils_switch_number(const string& str)
{
    cout<<str<<endl;

    string dest;
    if(str.length()%2 == 0)
    {
        string src = str;
    //    string dest;

        for(int i=0; i<src.length(); i+=2)
        {
            dest+=src[i+1];
            dest+=src[i];
        }

        return dest;
    }
    else
    {
        string src = str;
        src+='F';

        for(int i=0; i<src.length(); i+=2)
        {
            dest+=src[i+1];
            dest+=src[i];
        }

        return dest;
    }

}

char *blue_utils_get_number(const char *number)
{
    char *str1 = (char *)malloc(sizeof(char*)*32);
    memset(str1,'\0',strlen(str1));
    /* 	char * str = (char *)malloc(sizeof(char*)*128);
    	memset(str,'\0',strlen(str));

    	sprintf(str,"%s",number);
    */
    int i=0;
    while(number[i] !='\0')
    {
        str1[i] = number[i+1];
        str1[i+1] = number[i];
        i+=2;
    }

    if(str1[strlen(number)-1] == 'F')
    {
        str1[strlen(number) -1] ='\0';
    }
    else
    {
        str1[strlen(number)] = '\0';
    }

    //printf("%s\n",str1);

    char *tmp = strdup(str1);
    free(str1);
    str1 = NULL;
    return tmp;
}

string blue_utils_get_msg_time(const string& str)
{

    string src = blue_utils_switch_number(str);

    string time;

    if(src.length()>=10)
    {
        int i =0;
        time+=src.substr(i,2);
        time+='-';
        i+=2;
        time+=src.substr(i,2);
        time+='-';
        i+=2;
        time+=src.substr(i,2);
        time+=' ';
        i+=2;
        time+=src.substr(i,2);
        time+=':';
        i+=2;
        time+=src.substr(i,2);
        time+=':';
        i+=2;
        time+=src.substr(i,2);
    }

//	cout<<time<<endl;

    return time;
}

string blue_utils_get_msg_content(const string& str,int sig)
{
    if(sig  == 4)
    {
        if(str.substr(2,4).compare("0500") == 0)
        {
            string content;
            size_t msg_length = blue_utils_number_to10(str.substr(0,2));

            if(str.length()>msg_length+2)
            {
                string msg = str.substr(14,msg_length*2+2);

                unsigned char ch[CHINA_SET_MAX], *p = NULL, *e = NULL;

                for(int i=0; i<msg.length(); i+=4)
                {
                    int x = blue_utils_unicode_int(strdup(msg.substr(i,4).c_str()));
                    memset((char *)ch, 0, CHINA_SET_MAX);

                    e = UnicodetoUTF8(x,ch);
                    content+=(const char *)ch;
                }
                return content;
            }

        }
        else
        {
            string content;

            size_t msg_length = blue_utils_number_to10(str.substr(0,2));

            if(str.length()>msg_length+2)
            {
                string msg = str.substr(2,msg_length*2+2);

                unsigned char ch[CHINA_SET_MAX], *p = NULL, *e = NULL;

                for(int i=0; i<msg.length(); i+=4)
                {
                    int x = blue_utils_unicode_int(strdup(msg.substr(i,4).c_str()));
                    memset((char *)ch, 0, CHINA_SET_MAX);

                    e = UnicodetoUTF8(x,ch);
                    content+=(const char *)ch;
                }

                return content;

            }

        }
    }
    else
    {
        if(str.substr(2,4).compare("0500") == 0)
        {
            string content;
            size_t msg_length = blue_utils_number_to10(str.substr(0,2));

            if(str.length()>msg_length+2)
            {
                string msg = str.substr(14,msg_length*2+2);	

				char out[512];
				char *dest=(char *)PDU_7BIT_Decoding(out,strdup(msg.c_str()));
				
				content+=out;

                return content;
            }
        }
        else
        {
            string content;

            size_t msg_length = blue_utils_number_to10(str.substr(0,2));

            cout<<msg_length<<endl;

            if(str.length()>msg_length+2)
            {
                string msg = str.substr(2,msg_length*2+2);
                char out[512];

                char *dest=(char *)PDU_7BIT_Decoding(out,strdup(msg.c_str()));

                content+=out;

                return content;

            }
        }
    }

    return "test";
}


unsigned long blue_utils_unicode_int(const char* str)
{
    unsigned long var=0;
    unsigned long t;

    int len = strlen(str);

    if (var > 8) //最长8位
        return -1;

    for (; *str; str++)
    {
        if ((*str>='A' && *str <='F'))
            t = *str-55;
        else
            t = *str-48;

        var<<=4;
        var|=t;
    }
    return var;
}

char *blue_utils_notify_sms(const char *str)
{
    printf("%s\n",str);

    char *p = NULL;
    p=(char *)strstr(str,"CMTI");
    char *index = NULL;

    if(p != NULL)
    {
        gchar ** tmp = g_strsplit(p,",",2);
        if(tmp[1])
        {
            index = strdup(tmp[1]);
        }
    }

    return index;
}

char *blue_utils_get_signal(const char *str)
{
    gchar **tmp = g_strsplit(str,":",2);

    if(tmp[1])
    {
        gchar **tmp1 = g_strsplit(tmp[1],",",2);
        if(tmp1[0])
        {
            char *str = strdup(tmp1[0]);

            int size = strlen(str);
            int i=0;


            char str2[5];
            int j= 0 ;

            for(i=0; i<size; i++)
            {
                if(str[i] !=' ')
                {
                    str2[j] = str[i];
                    j++;
                }
            }

            str2[j] = '\0';

            return strdup(str2);

        }
        else
        {
            return NULL;
        }
    }
    else
    {
        return NULL;
    }
}
