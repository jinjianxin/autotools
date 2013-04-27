#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "unicode.h"


int outbin(FILE *fp, char ch)
{
    int i = 7;
    while(i >= 0)
    {
        fprintf(fp, "%d", ((ch >> i) & 1));
        i--;
    }
}

/* convert unicode to UTF-8 */
unsigned char *UnicodetoUTF8(int unicode, unsigned char *p)
{
    unsigned char *e = NULL;

    if((e = p))
    {
        if(unicode < 0x80) *e++ = unicode;
        else if(unicode < 0x800)
        {
            /*<11011111> < 000 0000 0000>*/
            *e++ = ((unicode >> 6) & 0x1f)|0xc0;
            *e++ = (unicode & 0x3f)|0x80; 
        }
        else if(unicode < 0x10000)
        {
            /*<11101111> <0000 0000 0000 0000>*/
            *e++ = ((unicode >> 12) & 0x0f)|0xe0; 
            *e++ = ((unicode >> 6) & 0x3f)|0x80;
            *e++ = (unicode & 0x3f)|0x80; 
        }
        else if(unicode < 0x200000)
        {
            /*<11110111> <0 0000 0000 0000 0000 0000>*/
            *e++ = ((unicode >> 18) & 0x07)|0xf0; 
            *e++ = ((unicode >> 12) & 0x3f)|0x80;
            *e++ = ((unicode >> 6) & 0x3f)|0x80;
            *e++ = (unicode & 0x3f)|0x80; 
        }
        else if(unicode < 0x4000000)
        {
            /*<11111011> <00 0000 0000 0000 0000 0000 0000>*/
            *e++ = ((unicode >> 24) & 0x03)|0xf8 ; 
            *e++ = ((unicode >> 18) & 0x3f)|0x80;
            *e++ = ((unicode >> 12) & 0x3f)|0x80;
            *e++ = ((unicode >> 6) & 0x3f)|0x80;
            *e++ = (unicode & 0x3f)|0x80; 
        }
        else
        {
            /*<11111101> <0000 0000 0000 0000 0000 0000 0000 0000>*/
            *e++ = ((unicode >> 30) & 0x01)|0xfc; 
            *e++ = ((unicode >> 24) & 0x3f)|0x80;
            *e++ = ((unicode >> 18) & 0x3f)|0x80;
            *e++ = ((unicode >> 12) & 0x3f)|0x80;
            *e++ = ((unicode >> 6) & 0x3f)|0x80;
            *e++ = (unicode & 0x3f)|0x80; 
        }
    }
    return e;
}

/* convert UTF-8 to unicode */
int UTF8toUnicode(unsigned char *ch, int *unicode)
{
    unsigned char *p = NULL;
    int e = 0, n = 0;

    if((p = ch) && unicode)
    {
        if(*p >= 0xfc)
        {
            /*6:<11111100>*/
            e = (p[0] & 0x01) << 30;
            e |= (p[1] & 0x3f) << 24;
            e |= (p[2] & 0x3f) << 18;
            e |= (p[3] & 0x3f) << 12;
            e |= (p[4] & 0x3f) << 6;
            e |= (p[5] & 0x3f);
            n = 6;
        }
        else if(*p >= 0xf8) 
        {
            /*5:<11111000>*/
            e = (p[0] & 0x03) << 24;
            e |= (p[1] & 0x3f) << 18;
            e |= (p[2] & 0x3f) << 12;
            e |= (p[3] & 0x3f) << 6;
            e |= (p[4] & 0x3f);
            n = 5;
        }
        else if(*p >= 0xf0)
        {
            /*4:<11110000>*/
            e = (p[0] & 0x07) << 18;
            e |= (p[1] & 0x3f) << 12;
            e |= (p[2] & 0x3f) << 6;
            e |= (p[3] & 0x3f);
            n = 4;
        }
        else if(*p >= 0xe0)
        {
            /*3:<11100000>*/
            e = (p[0] & 0x0f) << 12;
            e |= (p[1] & 0x3f) << 6;
            e |= (p[2] & 0x3f);
            n = 3;
        }
        else if(*p >= 0xc0) 
        {
            /*2:<11000000>*/
            e = (p[0] & 0x1f) << 6;
            e |= (p[1] & 0x3f);
            n = 2;
        }
        else 
        {
            e = p[0];
            n = 1;
        }
        *unicode = e;
    }
    return n;
}

const char HexTbl[]={"0123456789ABCDEF"};

unsigned char HexChar2Number(char hex)
{
	unsigned char value = 0;
	if(hex >= '0' && hex <= '9')
	{
		value = hex - '0';	
	}
	else if(hex >= 'A' && hex <= 'Z')
	{
		value = hex - 'A' + 10;	
	}
	else if(hex >= 'a' && hex <= 'z')
	{
		value = hex - 'a' + 10;	
	}

	return value;
}

unsigned char strHex2Byte(char *pHex)
{
	unsigned char value;
	value = HexChar2Number(pHex[0]) << 4;
	value |= HexChar2Number(pHex[1]);
	
	return value;	
}

int PDU_7BIT_Encoding(unsigned char *pDst,char *pSrc)
{
	unsigned char hexVlaue;	  					
	unsigned char nLeft;					  	
	unsigned char fillValue;				 	
	int Cnt = 0;							 	
	int nSrcLength = strlen(pSrc);
	int i;
	nLeft = 1;								
	for(i = 0; i < nSrcLength; i++)
	{
		printf("%c",*pSrc);
		hexVlaue = *pSrc >> (nLeft - 1);		
		fillValue = *(pSrc+1) << (8-nLeft);	 	
		hexVlaue = hexVlaue | fillValue;

		*pDst++ = HexTbl[hexVlaue >> 4];	 	
		*pDst++ = HexTbl[hexVlaue & 0x0F];
		Cnt += 2;							 	

		nLeft++;						 		
		if(nLeft == 8) 								
		{
			pSrc++;					 		   	
			i++;							 	
			nLeft = 1;							
		}
	   										 	
		pSrc++;									
	}
	*pDst = '\0';


	return Cnt;
}

int PDU_7BIT_Decoding(char *pDst,char *pSrc)
{
	int i;										
	int Cnt = 0;							   	
	unsigned char nLeft = 1;				  	
	unsigned char fillValue = 0;			 	
	unsigned char oldFillValue = 0;			  	

	int srcLength  = strlen(pSrc);			  	
	for(i = 0; i < srcLength; i += 2)
	{
		*pDst = strHex2Byte(pSrc);    		
		
		fillValue = (unsigned char)*pDst;					  	
		fillValue >>= (8 - nLeft);		 		


		*pDst <<= (nLeft -1 );	   				
		*pDst &= 0x7F;						 	
		*pDst |= oldFillValue;					
		
		oldFillValue = fillValue;
		
		pDst++;								  	
		Cnt++;								  	

		nLeft ++;							  	
		if(nLeft == 8)						  	
		{
			*pDst = oldFillValue;			   	
			pDst++;
			Cnt++;
			
			nLeft = 1;						  	
			oldFillValue = 0;
		} 
		pSrc += 2;							   	
	}
	*pDst = '\0';							 	

	return Cnt;		
}

