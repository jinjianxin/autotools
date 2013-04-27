/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * lib.h
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

#include <iostream>
#include <string>

using namespace std;

typedef enum HFPEVENT
{
	RING_EVENT,
	CALLERNUMBER_EVENT,
	CALLWAITINGNUMBER_EVENT,
	PHONESTATUS_EVENT,
	SMMT_EVENT,
	SINGAL_EVENT,
	SMS_SUCCESS,
	SMS_FAILED,
}HFPEVENT;


typedef enum PHONESTATUS
{
	FPDISCONNECTED,        
	HFPCONNECTING,          
	HFPCONNECTED,                  
	CALLINCOMING,       
	CALLOUTGOING,       
	CALLACTIVE,             
	CALLINACTIVE,
}PHONESTATUS;

typedef enum HFP_ERROR
{
	HFP_SUCCESS,                
	HFP_SETPARAMFAILED ,
	HFP_WRITEDATAFAILED,
	HFP_FAILED,
}HFP_ERROR;

typedef unsigned long  (*HFPCallback ) ( HFPEVENT event, void* lpUserParam, unsigned long param);

void setMsgCallBack(HFPCallback pFunc,void *data);

int initBlueHFP(unsigned int heartbeatInterval);

int unInitBlueHFP();

int dial(const char* number);

int dialExtNumber(const char* number);

int answerCall();

int rejectCall();

int handupCall();

int sendSM(const char *content,const char *number);
int sendSM(const string& content,const string& number);

int deletesendSM(unsigned short choice);

int readSM(unsigned short choice);

int openDev(const char *dev);

void set_speed(int fd, int speed);

int set_Parity(int fd,int databits,int stopbits,int parity);

void test();
