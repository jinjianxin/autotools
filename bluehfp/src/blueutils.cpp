#include "blueutils.h"
#include "unicode.h"
#include <glib.h>

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

char *blue_utils_get_sms_msg(const char *arg)
{
    /*char *str = "+CMGL: 0,1,,25\
    	0891683110406505F0240BA15161512770F1000821010221749523066D4B8BD57684";*/

    char *str = strdup(arg);


    char * p =NULL;
    p =strstr(str,"0891");

    if(p)
    {
        if(strlen(p)>40)
        {
            char tmp[3];
            int ret = 0;
            for(ret=0; ret<3; ret++)
            {
                tmp[ret] = p[ret];
            }
            tmp[2]='\0';

            ret = blue_utils_number_to10(strdup(tmp));

            //去除短信中心号码操作
            char str1[1024];
            char str2[1024];
            int i = ret*2+2;
            int j=0;
            while(p[i] !='\0')
            {
                str1[j] = p[i];
                i++;
                j++;
            }

            str1[j] = '\0';

//            printf("%s\n",str1);
            //获得发送手机号码

            tmp[0]=str1[2];
            tmp[1] = str1[3];
            tmp[2]='\0';

            ret = blue_utils_number_to10(tmp);

            printf("tmp = %s\tret = %d\n",tmp,ret);

            int size = 0;
            if(ret %2 ==0)
            {
                size = 6+ret;
            }
            else
            {
                size =6+ret+1;
            }

            i=6;
            j=0;
            for(i=6; i<size; i++)
            {
                str2[j]=str1[i];
                j++;
            }

            str2[j]='\0';
//            printf("%s\n",str2);

            char * number = blue_utils_get_number(strdup(str2));

            //获得短信发送时间
            i =size;
            j=0;
            for(i=size; i<strlen(str1); i++)
            {
                str2[j] = str1[i];
                j++;
            }

            str2[j]='\0';

            printf("%s\n",str2);


            i=4;
            j=0;

            char word[5];
            static int sign;

            word[0] = str2[0];
            word[1] = str2[1];
            word[2] = str2[2];
            word[3] = str2[3];
            word[4] = '\0';

            if(strcmp(strdup(word),"0000") == 0)
            {
                sign = 2;
            }
            else
            {
                sign = 4;
            }


            for(i=4; i<18; i++)
            {
                str1[j] = str2[i];
                j++;
            }

            str1[j]='\0';

            char *time = blue_utils_get_msg_time(strdup(str1));
            printf("time=%s\n",time);

            //获得短信内容

            i =18;
            j =0;

            while(str2[i] !='\0')
            {
                str1[j] = str2[i];
                j++;
                i++;
            }

            str1[j]='\0';

            char *msg = blue_utils_get_msg_content(strdup(str1),sign);

//			printf("%s\t%s\t%s\n",number,time,msg);

            char *buff = (char *)malloc(sizeof(char *)*1024);
            memset(buff,'\0',strlen(buff));

            sprintf(buff,"%s;%s;%s",number,time,msg);

            char *message = strdup(buff);

            printf("%s\n",message);

            free(buff);
            buff = NULL;

            return message;

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

char *blue_utils_number_to16(int size)
{
    char tmp[16];
    sprintf(tmp,"%x",size);

    char * number = (char *)malloc(sizeof(char *)*16);
    memset(number,'\0',strlen(number));

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

int blue_utils_number_to10(const char *str)
{
    int ret = 0;
    int i=0;
    int size = strlen(str);

    while(str[i] !='\0')
    {
//		printf("%c\n",str[i]);
        switch(str[i])
        {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        {
            char tmp[2];
            tmp[0] = str[i];
            tmp[1] = '\0';
            int m = atoi(tmp);
            ret += m*pow(16,size-1);
        }
        break;
        case 'A':
        case 'a':
        {
            ret +=10*pow(16,size-1);
        }
        break;
        case 'B':
        case 'b':
        {
            ret +=11*pow(16,size-1);
        }
        break;
        case 'C':
        case 'c':
        {
            ret +=12*pow(16,size-1);
        }
        break;
        case 'D':
        case 'd':
        {
            ret +=13*pow(16,size-1);
        }
        break;
        case 'E':
        case 'e':
        {
            ret +=14*pow(16,size-1);
        }
        break;
        case 'F':
        case 'f':
        {
            ret +=15*pow(16,size-1);
        }
        break;
        default:
            break;
        }
        i++;
        size--;
    }

    return ret;
}

char *blue_utils_switch_number(const char *number)
{
    if(strlen(number) %2 !=0)
    {
        char *str1 = (char *)malloc(sizeof(char*)*32);
        memset(str1,'\0',strlen(str1));
        char * str = (char *)malloc(sizeof(char*)*128);
        memset(str,'\0',strlen(str));

        sprintf(str,"%sF",number);

        int i=0;
        while(str[i] !='\0')
        {
            str1[i] = str[i+1];
            str1[i+1] = str[i];
            i+=2;
        }

        str1[strlen(str)] = '\0';

        free(str);
        str = NULL;

        char *tmp = strdup(str1);

        free(str1);
        str1 = NULL;

        return tmp;
//        return  str1;
    }
    else
    {

        char *str1 = (char *)malloc(sizeof(char*)*32);
        memset(str1,'\0',strlen(str1));
        char * str = (char *)malloc(sizeof(char*)*128);
        memset(str,'\0',strlen(str));

        sprintf(str,"%s",number);

        int i=0;
        while(str[i] !='\0')
        {
            str1[i] = str[i+1];
            str1[i+1] = str[i];
            i+=2;
        }

        str1[strlen(str)] = '\0';

        free(str);
        str = NULL;

        char *tmp = strdup(str1);

        free(str1);
        str1 = NULL;

        return tmp;

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

char *blue_utils_get_msg_time(const char *str)
{
    char *str1 = blue_utils_get_number(str);

    char *time1 = (char *)malloc(sizeof(char *)*32);
    memset(time1,'\0',strlen(time1));

    char *time2 = (char *)malloc(sizeof(char *)*32);
    memset(time2,'\0',strlen(time2));

    char *time3 = (char *)malloc(sizeof(char *)*64);
    memset(time3,'\0',strlen(time3));

    int i=0;
    int j=0;

    for(i=0; i<strlen(str1)-2-2*3; i+=2)
    {
        char tmp[4];
        tmp[0]=str1[i];
        tmp[1]=str1[i+1];
        tmp[2]= '-';
        tmp[3]='\0';

        strcat(time1,tmp);
    }

    i = 6;

    for(i=6; i<strlen(str1)-2; i+=2)
    {
        char tmp[4];
        tmp[0]=str1[i];
        tmp[1]=str1[i+1];
        tmp[2]= ':';
        tmp[3]='\0';

        strcat(time2,tmp);
    }

    time1[strlen(time1)-1] = ' ';
    time2[strlen(time2)-1] ='\0';

    sprintf(time3,"%s%s",time1,time2);

    char *time = strdup(time3);


    free(time1);
    time1 = NULL;

    free(time2);
    time2 = NULL;

    free(time3);
    time3 = NULL;

    return time;
}

char *blue_utils_get_msg_content(const char *str,int arg)
{
    if(4 == arg )
    {
        unsigned char ch[CHINA_SET_MAX], *p = NULL, *e = NULL;

        char *msg = (char *)malloc(sizeof(char *)*512);
        memset(msg,'\0',strlen(msg));

//        printf("str=%s\n",str);

//        printf("str size=%d\n",strlen(str));

        char mark[5];

        mark[0] = str[2];
        mark[1] = str[3];
        mark[2] = str[4];
        mark[3] = str[5];
        mark[4]='\0';

        if(strcmp(mark,"0500") ==0 )
        {   char tmp[3];

            tmp[0] = str[0];
            tmp[1] = str[1];
            tmp[2] = '\0';

            int size = blue_utils_number_to10(strdup(tmp));

            int length = size*2+2;
            printf("size=%d\n",size*2);

            char *str1 = (char *)malloc(sizeof(char *)*512);
            memset(str1,'\0',strlen(str1));

            int i = 14;
            int j=0;

            for(i=14; i<length; i++)
            {
                str1[j] = str[i];
                j++;
            }

            str1[j] = '\0';

            i=0;
            while(str1[i] !='\0')
            {
                char tmp[5];

                tmp[0] = str1[i];
                tmp[1] = str1[i+1];
                tmp[2] = str1[i+2];
                tmp[3] = str1[i+3];
                tmp[4] = '\0';

                int x = blue_utils_unicode_int(strdup(tmp));

                memset((char *)ch, 0, CHINA_SET_MAX);

               /* if(e = (UnicodetoUTF8(x, ch)) > ch)
                {
                    e = '\0';
                }
				*/

				e = UnicodetoUTF8(x,ch);


                strcat(msg,(const char *)ch);

                i+=4;
            }

            char *message = strdup(msg);

            free(msg);
            msg = NULL;

            free(str1);
            str1 = NULL;

            return message;

        }
        else
        {

            char tmp[3];

            tmp[0] = str[0];
            tmp[1] = str[1];
            tmp[2] = '\0';

            int size = blue_utils_number_to10(strdup(tmp));

            int length = size*2+2;
            printf("size=%d\n",size*2);

            char *str1 = (char *)malloc(sizeof(char *)*512);
            memset(str1,'\0',strlen(str1));

            int i = 2;
            int j=0;

            for(i=2; i<length; i++)
            {
                str1[j] = str[i];
                j++;
            }

            str1[j] = '\0';

            i=0;
            while(str1[i] !='\0')
            {
                char tmp[5];

                tmp[0] = str1[i];
                tmp[1] = str1[i+1];
                tmp[2] = str1[i+2];
                tmp[3] = str1[i+3];
                tmp[4] = '\0';

                int x = blue_utils_unicode_int(strdup(tmp));

                memset((char *)ch, 0, CHINA_SET_MAX);

                /*if(e = (UnicodetoUTF8(x, ch)) > ch)
                {
                    e = '\0';
                }*/
				e = UnicodetoUTF8(x,ch);

                strcat(msg,(const char*)ch);

                i+=4;
            }

            char *message = strdup(msg);

            free(msg);
            msg = NULL;

            free(str1);
            str1 = NULL;

            return message;
        }
    }
    else
    {
        unsigned char ch[CHINA_SET_MAX], *p = NULL, *e = NULL;

        char *msg = (char *)malloc(sizeof(char *)*512);
        memset(msg,'\0',strlen(msg));

        char mark[5];

        mark[0] = str[2];
        mark[1] = str[3];
        mark[2] = str[4];
        mark[3] = str[5];
        mark[4]='\0';

		printf("************str = %s\n",str);

        if(strcmp(mark,"0500") ==0 )
        { 
			char tmp[3];
		   	tmp[0] = str[0];
		   	tmp[1] = str[1];
		   	tmp[2] = '\0';
			
		   	int size = blue_utils_number_to10(strdup(tmp));

		   	int length = size*2+2;
		   	printf("size=%d\n",size*2);
           
			char *str1 = (char *)malloc(sizeof(char *)*512);
			memset(str1,'\0',strlen(str1));

			int i = 16;
			int j=0;

			for(i=16; i<length; i++)
			{
				str1[j] = str[i];
                j++;
            }

            str1[j] = '\0';

			printf("str1 = %s\n",str1);

			char out[512];

			char *msg1=(char*)PDU_7BIT_Decoding(out,strdup(str1));

			printf("msg = %s\n",out);

			free(str1);
			str1 = NULL;

			return strdup(out);
  
        }
        else
        {
			char tmp[3];
		   	tmp[0] = str[0];
		   	tmp[1] = str[1];
		   	tmp[2] = '\0';
			
		   	int size = blue_utils_number_to10(strdup(tmp));

		   	int length = size*2+2;
           
			char *str1 = (char *)malloc(sizeof(char *)*512);
			memset(str1,'\0',strlen(str1));

			int i = 2;
			int j=0;

			for(i=2; i<length; i++)
			{
				str1[j] = str[i];
                j++;
            }

            str1[j] = '\0';

			printf("str1 = %s\n",str1);

			char out[512];

			char *msg1=(char *)PDU_7BIT_Decoding(out,strdup(str1));

			printf("msg = %s\n",out);

			free(str1);
			str1 = NULL;

			return strdup(out);
			
        }

    }
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
