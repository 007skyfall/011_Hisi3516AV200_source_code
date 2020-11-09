/******************************************************************************
  Some simple Hisilicon Hi3531 video encode functions.

  Copyright (C), 2010-2011, Hisilicon Tech. Co., Ltd.
 ******************************************************************************
    Modification:  2011-2 Created
******************************************************************************/

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* End of #ifdef __cplusplus */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>

#include "sample_comm.h"
 
const HI_U8 g_SOI[2] = {0xFF, 0xD8};
const HI_U8 g_EOI[2] = {0xFF, 0xD9};
static pthread_t gs_VencPid;
static SAMPLE_VENC_GETSTREAM_PARA_S gs_stPara;
static HI_S32 gs_s32SnapCnt = 0;

HI_CHAR* DstBuf = NULL;
#define TEMP_BUF_LEN 8
#define MAX_THM_SIZE (64*1024)


#define ORTP_ENABLE  1

#if ORTP_ENABLE
#include <ortp/ortp.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#define Y_PLOAD_TYPE 96 //H.264
#define MAX_RTP_PKT_LENGTH 1400
#define DefaultTimestampIncrement 3600 //(90000/25)
uint32_t g_userts=0;
RtpSession *pRtpSession = NULL;

#define LOCAL_HOST_IP  "192.168.1.20"

/**  ��ʼ��   
 *     
 *   ��Ҫ���ڶ�ortp�Լ������������г�ʼ��   
 *   @param:  char * ipStr Ŀ�Ķ�IP��ַ������   
 *   @param:  int port Ŀ�Ķ�RTP�����˿�   
 *   @return:  RtpSession * ����ָ��RtpSession�����ָ��,���ΪNULL�����ʼ��ʧ��   
 *   @note:      
 */   
RtpSession * rtpInit( char  * ipStr, int  port)
{
    RtpSession *session; 
    char  *ssrc;
    printf("********oRTP for H.264 Init********\n");

    ortp_init();
    ortp_scheduler_init();
    ortp_set_log_level_mask(ORTP_MESSAGE|ORTP_WARNING|ORTP_ERROR);
    session=rtp_session_new(RTP_SESSION_SENDONLY);	

    rtp_session_set_scheduling_mode(session,1);
    rtp_session_set_blocking_mode(session,0);
    //rtp_session_set_connected_mode(session,TRUE);
    rtp_session_set_remote_addr(session,ipStr,port);
    rtp_session_set_payload_type(session,Y_PLOAD_TYPE);

    ssrc=getenv("SSRC");
    if (ssrc!=NULL) {
        printf("using SSRC=%i.\n",atoi(ssrc));
        // �����������SSRC�������˲��Ļ�����������ֵ 
        rtp_session_set_ssrc(session,atoi(ssrc));
    }
    return  session;
}
/**  ����ortp�ķ��ͣ��ͷ���Դ   
 *     
 *   @param:  RtpSession *session RTP�Ự�����ָ��   
 *   @return:  0��ʾ�ɹ�   
 *   @note:       
 */    
int  rtpExit(RtpSession *session)   
{ 
    printf("********oRTP for H.264 Exit********\n");  
    g_userts = 0;   
       
    rtp_session_destroy(session);   
    ortp_exit();   
    ortp_global_stats_display();   
  
     return  0;   
}   
/**  ����rtp���ݰ�   
 *     
 *   ��Ҫ���ڷ���rtp���ݰ�   
 *   @param:  RtpSession *session RTP�Ự�����ָ��   
 *   @param:  const char *buffer Ҫ���͵����ݵĻ�������ַ   
  *   @param: int len Ҫ���͵����ݳ���   
 *   @return:  int ʵ�ʷ��͵����ݰ���Ŀ   
 *   @note:     ���Ҫ���͵����ݰ����ȴ���BYTES_PER_COUNT���������ڲ�����зְ�����   
 */   
int  rtpSend(RtpSession *session, char  *buffer,  int  len)
{  
    int  sendBytes = 0; 
    int status;       
    uint32_t valid_len=len-4;
    unsigned char NALU=buffer[4];
     
    //�������С��MAX_RTP_PKT_LENGTH�ֽڣ�ֱ�ӷ��ͣ���һNAL��Ԫģʽ
    if(valid_len <= MAX_RTP_PKT_LENGTH)
    {
        sendBytes = rtp_session_send_with_ts(session,
                                             &buffer[4],
                                             valid_len,
                                             g_userts);
    }
    else if (valid_len > MAX_RTP_PKT_LENGTH)
    {
        //�з�Ϊ�ܶ�������ͣ�ÿ����ǰҪ��ͷ���д������һ����
        valid_len -= 1;
        int k=0,l=0;
        k=valid_len/MAX_RTP_PKT_LENGTH;
        l=valid_len%MAX_RTP_PKT_LENGTH;
        int t=0;
        int pos=5;
        if(l!=0)
        {
            k=k+1;
        }
        while(t<k)//||(t==k&&l>0))
        {
            if(t<(k-1))//(t<k&&l!=0)||(t<(k-1))&&(l==0))//(0==t)||(t<k&&0!=l))
            {
                buffer[pos-2]=(NALU & 0x60)|28;
                buffer[pos-1]=(NALU & 0x1f);
                if(0==t)
                {
                    buffer[pos-1]|=0x80;
                }
                sendBytes = rtp_session_send_with_ts(session,
                                                     &buffer[pos-2],
                                                     MAX_RTP_PKT_LENGTH+2,
                                                     g_userts);
                t++;
                pos+=MAX_RTP_PKT_LENGTH;
            }
            else //if((k==t&&l>0)||((t==k-1)&&l==0))
            {
                int iSendLen;
                if(l>0)
                {
                    iSendLen=valid_len-t*MAX_RTP_PKT_LENGTH;
                }
                else
                    iSendLen=MAX_RTP_PKT_LENGTH;
                buffer[pos-2]=(NALU & 0x60)|28;
                buffer[pos-1]=(NALU & 0x1f);
                buffer[pos-1]|=0x40;
                sendBytes = rtp_session_send_with_ts(session,
                                                     &buffer[pos-2],
                                                     iSendLen+2,
                                                     g_userts);
                t++;
            }
        }
    }

    g_userts += DefaultTimestampIncrement;//timestamp increase
    return  len;
}
#endif
/******************************************************************************
* function : Get Thumbnail form jpg file
******************************************************************************/
#ifdef __READ_ALL_FILE__
static HI_S32 FileTrans_GetThmFromJpg(HI_CHAR* JPGPath, HI_U32* DstSize)
{
    HI_S32 s32RtnVal = 0;
    FILE* fpJpg = NULL;
    HI_CHAR tempbuf[TEMP_BUF_LEN] = {0};
    HI_S32 bufpos = 0;
    HI_CHAR startflag[2] = {0xff, 0xd8};
    HI_S32 startpos = 0;
    HI_CHAR endflag[2] = {0xff, 0xd9};
    HI_S32 endpos = 0;
    fpJpg = fopen(JPGPath, "rb");
    HI_CHAR* pszFile = NULL;
    HI_S32 fd = 0;
    HI_S32 s32I = 0;
    struct stat stStat;
    memset(&stStat, 0, sizeof(struct stat));
    if (NULL == fpJpg)
    {
        printf("file %s not exist!\n", JPGPath);
        return HI_FAILURE;
    }
    else
    {
        fd = fileno(fpJpg);
        fstat(fd, &stStat);
        pszFile = (HI_CHAR*)malloc(stStat.st_size);
        if ((NULL == pszFile) || (stStat.st_size < 6))
        {
            fclose(fpJpg);
            printf("memory malloc fail!\n");
            return HI_FAILURE;
        } 
 
        if (fread(pszFile, stStat.st_size , 1, fpJpg) <= 0)
        {
            fclose(fpJpg);
            free(pszFile);
            printf("fread jpeg src fail!\n");
            return HI_FAILURE;
        }

        fclose(fpJpg);
        HI_U16 u16THMLen = 0;
        u16THMLen = (pszFile[4] << 8) + pszFile[5];
        while (s32I < stStat.st_size)
        {
            tempbuf[bufpos] = pszFile[s32I++];
            if (bufpos > 0)
            {
                if (0 == memcmp(tempbuf + bufpos - 1, startflag, sizeof(startflag)))
                {
                    startpos = s32I - 2;
                    if (startpos < 0)
                    {
                        startpos = 0;
                    }
                }
                if (0 == memcmp(tempbuf + bufpos - 1, endflag, sizeof(endflag)))
                {
                    if (u16THMLen == s32I)
                    {
                        endpos = s32I;
                        break;
                    }
                    else
                    {
                        endpos = s32I;
                        break;
                    }
                }
            }
            bufpos++;
            if (bufpos == (TEMP_BUF_LEN - 1))
            {
                if (tempbuf[bufpos - 1] != 0xFF)
                {
                    bufpos = 0;
                }

            }
            else if (bufpos > (TEMP_BUF_LEN - 1))
            {
                bufpos = 0;
            }

        }

    }
    if (endpos - startpos <= 0)
    {
        free(pszFile);
        printf("get .thm 11 fail!\n");
        return HI_FAILURE;
    }

    if (endpos - startpos >= stStat.st_size)
    {
        free(pszFile);
        printf("NO DCF info, get .thm 22 fail!\n");
        return HI_FAILURE;
    }

    HI_CHAR* temp = pszFile + startpos;
	if(MAX_THM_SIZE < (endpos - startpos))
	{
		printf("Thm is too large than MAX_THM_SIZE, get .thm 33 fail!\n");
        return HI_FAILURE;
	}

	HI_CHAR* cDstBuf = (HI_CHAR*)malloc(endpos - startpos);
	if (NULL == cDstBuf)
	{
		printf("memory malloc fail!\n");
		return HI_FAILURE;
	}

    memcpy(cDstBuf, temp, endpos - startpos);

	DstBuf = cDstBuf;
    *DstSize = endpos - startpos;
    free(pszFile);

    return HI_SUCCESS;
}

#else
static HI_S32 FileTrans_GetThmFromJpg(HI_CHAR* JPGPath, HI_U32* DstSize)
{
    HI_CHAR tempbuf[TEMP_BUF_LEN] = {0};
    HI_S32 bufpos = 0;
    HI_CHAR startflag[2] = {0xff, 0xd8};
    HI_S32 startpos = 0;
    HI_CHAR endflag[2] = {0xff, 0xd9};
    HI_S32 endpos = 0;
	HI_BOOL bStartMatch = HI_FALSE;
	
    HI_S32 fd = 0;
    struct stat stStat;
    memset(&stStat, 0, sizeof(struct stat));

	FILE* fpJpg = NULL;
	fpJpg = fopen(JPGPath, "rb");
	if (NULL == fpJpg)
    {
        printf("file %s not exist!\n", JPGPath);
        return HI_FAILURE;
    }
    else
    {
        fd = fileno(fpJpg);
        fstat(fd, &stStat);
		
        while (!feof(fpJpg))
        {
            tempbuf[bufpos]=getc(fpJpg);
            if (bufpos > 0)
            {
                if (0 == memcmp(tempbuf + bufpos - 1, startflag, sizeof(startflag)))
                {
                    startpos = ftell(fpJpg)-2;
                    if (startpos < 0)
                    {
                        startpos = 0;
                    }
					bStartMatch = HI_TRUE;
                }
                if (0 == memcmp(tempbuf + bufpos - 1, endflag, sizeof(endflag)))
                {
                    endpos = ftell(fpJpg);
					if(HI_TRUE == bStartMatch)
					{
                    	break;
					}	
                }
            }
            bufpos++;
			
            if (bufpos == (TEMP_BUF_LEN - 1))
            {
                if (tempbuf[bufpos - 1] != 0xFF)
                {
                    bufpos = 0;
                }
            }
            else if (bufpos > (TEMP_BUF_LEN - 1))
            {
				if (tempbuf[bufpos -1] == 0xFF)
				{
					tempbuf[0] = 0xFF;
					bufpos = 1;
				}
				else
				{	
                	bufpos = 0;
				}	
            }
        }
    }
	
    if (endpos - startpos <= 0)
    {
        printf("get .thm 11 fail!\n");
		fclose(fpJpg);
        return HI_FAILURE;
    }

	if (endpos - startpos > MAX_THM_SIZE)
	{
		printf("Thm is too large than MAX_THM_SIZE, get .thm 22 fail!\n");
		fclose(fpJpg);
        return HI_FAILURE;
	}

    if (endpos - startpos >= stStat.st_size)
    {
        printf("NO DCF info, get .thm 33 fail!\n");
		fclose(fpJpg);
        return HI_FAILURE;
    }

	HI_CHAR* cDstBuf = (HI_CHAR*)malloc(endpos - startpos);
	if (NULL == cDstBuf)
	{
		printf("memory malloc fail!\n");
		fclose(fpJpg);
		return HI_FAILURE;
	}

	fseek(fpJpg, (long)startpos, SEEK_SET);
	*DstSize = fread(cDstBuf,1,endpos-startpos,fpJpg);
	if(*DstSize != (endpos - startpos))
	{
		printf("fread fail!\n");
		fclose(fpJpg);
		return HI_FAILURE;
	}
	
	DstBuf = cDstBuf;
	fclose(fpJpg);

    return HI_SUCCESS;
}
#endif


HI_S32 SAMPLE_COMM_VENC_Getdcfinfo(char* SrcJpgPath, char* DstThmPath)
{
    HI_S32 s32RtnVal = HI_SUCCESS;
    HI_CHAR JPGSrcPath[FILE_NAME_LEN] = {0};
    HI_CHAR JPGDesPath[FILE_NAME_LEN] = {0};
    HI_U32 DstSize = 0;
    snprintf(JPGSrcPath, sizeof(JPGSrcPath), "%s", SrcJpgPath);
    snprintf(JPGDesPath, sizeof(JPGDesPath), "%s", DstThmPath);

    s32RtnVal = FileTrans_GetThmFromJpg(JPGSrcPath, &DstSize);
    if ((HI_SUCCESS != s32RtnVal) || (0 == DstSize))
    {
        printf("fail to get thm\n");
        return HI_FAILURE;
    }

    FILE* fpTHM = fopen(JPGDesPath, "w");
    if (HI_NULL == fpTHM)
    {
        printf("file to create file %s\n", JPGDesPath);
        return HI_FAILURE;
    }

    HI_U32 u32WritenSize = 0;
    while (u32WritenSize < DstSize)
    {
        s32RtnVal = fwrite(DstBuf + u32WritenSize, 1, DstSize, fpTHM);
        if (s32RtnVal <= 0)
        {
            printf("fail to wirte file, rtn=%d\n", s32RtnVal);
            break;
        }

        u32WritenSize += s32RtnVal;
    }

    if (fpTHM)
    {
        fclose(fpTHM);
        fpTHM = 0;
    }

	if(NULL != DstBuf)
	{
		free(DstBuf);
		DstBuf = NULL;
	}
	
    return 0;
}

/******************************************************************************
* function : Set venc memory location
******************************************************************************/
HI_S32 SAMPLE_COMM_VENC_MemConfig(HI_VOID)
{
    HI_S32 i = 0;
    HI_S32 s32Ret;

    HI_CHAR * pcMmzName;
    MPP_CHN_S stMppChnVENC;

    /* group, venc max chn is 64*/
    for(i=0;i<64;i++)
    {

        stMppChnVENC.enModId = HI_ID_VENC;
        stMppChnVENC.s32DevId = 0;
        stMppChnVENC.s32ChnId = i;

    
        pcMmzName = NULL;  
        


        /*venc*/
        s32Ret = HI_MPI_SYS_SetMemConf(&stMppChnVENC,pcMmzName);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("HI_MPI_SYS_SetMemConf with %#x!\n", s32Ret);
            return HI_FAILURE;
        }
    }
    
    return HI_SUCCESS;
}

/******************************************************************************
* function : venc bind vpss           
******************************************************************************/
HI_S32 SAMPLE_COMM_VENC_BindVpss(VENC_CHN VeChn,VPSS_GRP VpssGrp,VPSS_CHN VpssChn)
{
    HI_S32 s32Ret = HI_SUCCESS;
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = HI_ID_VPSS;
    stSrcChn.s32DevId = VpssGrp;
    stSrcChn.s32ChnId = VpssChn;

    stDestChn.enModId = HI_ID_VENC;
    stDestChn.s32DevId = 0;
    stDestChn.s32ChnId = VeChn;

    s32Ret = HI_MPI_SYS_Bind(&stSrcChn, &stDestChn);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    return s32Ret;
}

/******************************************************************************
* function : venc unbind vpss           
******************************************************************************/
HI_S32 SAMPLE_COMM_VENC_UnBindVpss(VENC_CHN VeChn,VPSS_GRP VpssGrp,VPSS_CHN VpssChn)
{
    HI_S32 s32Ret = HI_SUCCESS;
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = HI_ID_VPSS;
    stSrcChn.s32DevId = VpssGrp;
    stSrcChn.s32ChnId = VpssChn;

    stDestChn.enModId = HI_ID_VENC;
    stDestChn.s32DevId = 0;
    stDestChn.s32ChnId = VeChn;

    s32Ret = HI_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    return s32Ret;
}


/******************************************************************************
* function : venc bind vo           
******************************************************************************/
HI_S32 SAMPLE_COMM_VENC_BindVo(VO_DEV VoDev,VO_CHN VoChn,VENC_CHN VeChn)
{
    HI_S32 s32Ret = HI_SUCCESS;
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = HI_ID_VOU;
    stSrcChn.s32DevId = VoDev;
    stSrcChn.s32ChnId = VoChn;

    stDestChn.enModId = HI_ID_VENC;
    stDestChn.s32DevId = 0;
    stDestChn.s32ChnId = VeChn;

    s32Ret = HI_MPI_SYS_Bind(&stSrcChn, &stDestChn);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    return s32Ret;
}

/******************************************************************************
* function : venc unbind vo           
******************************************************************************/
HI_S32 SAMPLE_COMM_VENC_UnBindVo(VENC_CHN GrpChn,VO_DEV VoDev,VO_CHN VoChn)
{
    HI_S32 s32Ret = HI_SUCCESS;
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = HI_ID_VOU;
    stSrcChn.s32DevId = VoDev;
    stSrcChn.s32ChnId = VoChn;

    stDestChn.enModId = HI_ID_VENC;
    stDestChn.s32DevId = 0;
    stDestChn.s32ChnId = GrpChn;

    s32Ret = HI_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    return s32Ret;
}


/******************************************************************************
* function : vdec bind venc          
******************************************************************************/
HI_S32 SAMPLE_COMM_VDEC_BindVenc(VDEC_CHN VdChn,VENC_CHN VeChn)
{
    HI_S32 s32Ret = HI_SUCCESS;
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = HI_ID_VDEC;
    stSrcChn.s32DevId = 0;
    stSrcChn.s32ChnId = VdChn;

    stDestChn.enModId = HI_ID_VENC;
    stDestChn.s32DevId = 0;
    stDestChn.s32ChnId = VeChn;

    s32Ret = HI_MPI_SYS_Bind(&stSrcChn, &stDestChn);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    return s32Ret;
}

/******************************************************************************
* function : venc unbind vo           
******************************************************************************/
HI_S32 SAMPLE_COMM_VDEC_UnBindVenc(VDEC_CHN VdChn,VENC_CHN VeChn)
{
    HI_S32 s32Ret = HI_SUCCESS;
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = HI_ID_VDEC;
    stSrcChn.s32DevId = 0;
    stSrcChn.s32ChnId = VdChn;

    stDestChn.enModId = HI_ID_VENC;
    stDestChn.s32DevId = 0;
    stDestChn.s32ChnId = VeChn;


    s32Ret = HI_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    return s32Ret;
}


/******************************************************************************
* funciton : get file postfix according palyload_type.
******************************************************************************/
HI_S32 SAMPLE_COMM_VENC_GetFilePostfix(PAYLOAD_TYPE_E enPayload, char *szFilePostfix)
{
    if (PT_H264 == enPayload)
    {
        strcpy(szFilePostfix, ".h264");
    }
    else if (PT_H265 == enPayload)
    {
        strcpy(szFilePostfix, ".h265");
    }
    else if (PT_JPEG == enPayload)
    {
        strcpy(szFilePostfix, ".jpg");
    }
    else if (PT_MJPEG == enPayload)
    {
        strcpy(szFilePostfix, ".mjp");
    }
    else if (PT_MP4VIDEO == enPayload)
    {
        strcpy(szFilePostfix, ".mp4");
    }
    else
    {
        SAMPLE_PRT("payload type err!\n");
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}

/******************************************************************************
* funciton : save mjpeg stream. 
******************************************************************************/
HI_S32 SAMPLE_COMM_VENC_SaveMJpeg(FILE* fpMJpegFile, VENC_STREAM_S *pstStream)
{
    VENC_PACK_S*  pstData;
    HI_U32 i;

    //fwrite(g_SOI, 1, sizeof(g_SOI), fpJpegFile); //in Hi3531, user needn't write SOI!

    for (i = 0; i < pstStream->u32PackCount; i++)
    {
        pstData = &pstStream->pstPack[i];
        fwrite(pstData->pu8Addr+pstData->u32Offset, pstData->u32Len-pstData->u32Offset, 1, fpMJpegFile);
        fflush(fpMJpegFile);
    }

    return HI_SUCCESS;
}

/******************************************************************************
* funciton : save jpeg stream. 
******************************************************************************/
HI_S32 SAMPLE_COMM_VENC_SaveJpeg(FILE* fpJpegFile, VENC_STREAM_S *pstStream)
{
    VENC_PACK_S*  pstData;
    HI_U32 i;

    for (i = 0; i < pstStream->u32PackCount; i++)
    {
        pstData = &pstStream->pstPack[i];
        fwrite(pstData->pu8Addr+pstData->u32Offset, pstData->u32Len-pstData->u32Offset, 1, fpJpegFile);
        fflush(fpJpegFile);
    }

    return HI_SUCCESS;
}

/******************************************************************************
* funciton : save H264 stream
******************************************************************************/
HI_S32 SAMPLE_COMM_VENC_SaveH264(FILE* fpH264File, VENC_STREAM_S *pstStream)
{
    HI_S32 i;

    
    for (i = 0; i < pstStream->u32PackCount; i++)
    {
    	#if ORTP_ENABLE
		rtpSend(pRtpSession,pstStream->pstPack[i].pu8Addr, pstStream->pstPack[i].u32Len);
       #else
        	fwrite(pstStream->pstPack[i].pu8Addr+pstStream->pstPack[i].u32Offset,
               pstStream->pstPack[i].u32Len-pstStream->pstPack[i].u32Offset, 1, fpH264File);

        	fflush(fpH264File);
        #endif
    }
    

    return HI_SUCCESS;
}

/******************************************************************************
* funciton : save H265 stream
******************************************************************************/
HI_S32 SAMPLE_COMM_VENC_SaveH265(FILE* fpH265File, VENC_STREAM_S *pstStream)
{
    HI_S32 i;
    
    for (i = 0; i < pstStream->u32PackCount; i++)
    {
        fwrite(pstStream->pstPack[i].pu8Addr+pstStream->pstPack[i].u32Offset,
               pstStream->pstPack[i].u32Len-pstStream->pstPack[i].u32Offset, 1, fpH265File);

        fflush(fpH265File);
    }

    return HI_SUCCESS;
}

/******************************************************************************
* funciton : save jpeg stream
******************************************************************************/
HI_S32 SAMPLE_COMM_VENC_SaveJPEG(FILE *fpJpegFile, VENC_STREAM_S *pstStream)
{
    VENC_PACK_S*  pstData;
    HI_U32 i;

    for (i = 0; i < pstStream->u32PackCount; i++)
    {
        pstData = &pstStream->pstPack[i];
        fwrite(pstData->pu8Addr+pstData->u32Offset, pstData->u32Len-pstData->u32Offset, 1, fpJpegFile);
        fflush(fpJpegFile);
    }

    return HI_SUCCESS;
}
/******************************************************************************
* funciton : save snap stream
******************************************************************************/
HI_S32 SAMPLE_COMM_VENC_SaveSnap(VENC_STREAM_S *pstStream, HI_BOOL bSaveJpg, HI_BOOL bSaveThm)
{
    char acFile[FILE_NAME_LEN]  = {0};
    char acFile_dcf[FILE_NAME_LEN]  = {0};
    FILE *pFile;
    HI_S32 s32Ret;

    if(!bSaveJpg && !bSaveThm)
    {
        return HI_SUCCESS;    
    }
    
    sprintf(acFile, "snap_%d.jpg", gs_s32SnapCnt);
    pFile = fopen(acFile, "wb");
    if (pFile == NULL)
    {
        SAMPLE_PRT("open file err\n");
        return HI_FAILURE;
    }
    s32Ret = SAMPLE_COMM_VENC_SaveJPEG(pFile, pstStream);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("save snap picture failed!\n");
        return HI_FAILURE;
    }
    fclose(pFile);
    
    if(bSaveThm)
    {
        sprintf(acFile_dcf, "snap_thm_%d.jpg", gs_s32SnapCnt);
        s32Ret = SAMPLE_COMM_VENC_Getdcfinfo(acFile, acFile_dcf);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("save thm picture failed!\n");
            return HI_FAILURE;
        }
    }
    
    gs_s32SnapCnt++;
    return HI_SUCCESS;
}

/******************************************************************************
* funciton : save stream
******************************************************************************/
HI_S32 SAMPLE_COMM_VENC_SaveStream(PAYLOAD_TYPE_E enType,FILE *pFd, VENC_STREAM_S *pstStream)
{
    HI_S32 s32Ret;

    if (PT_H264 == enType)
    {
        s32Ret = SAMPLE_COMM_VENC_SaveH264(pFd, pstStream);
    }
    else if (PT_MJPEG == enType)
    {
        s32Ret = SAMPLE_COMM_VENC_SaveMJpeg(pFd, pstStream);
    }
    else if (PT_H265 == enType)
    {
        s32Ret = SAMPLE_COMM_VENC_SaveH265(pFd, pstStream);        
    }
    else
    {
        return HI_FAILURE;
    }
    return s32Ret;
}

/******************************************************************************
* funciton : Start venc stream mode (h264, mjpeg)
* note      : rate control parameter need adjust, according your case.
******************************************************************************/
HI_S32 SAMPLE_COMM_VENC_Start(VENC_CHN VencChn, PAYLOAD_TYPE_E enType, VIDEO_NORM_E enNorm, PIC_SIZE_E enSize, SAMPLE_RC_E enRcMode,HI_U32  u32Profile)
{
    HI_S32 s32Ret;
    VENC_CHN_ATTR_S stVencChnAttr;
    VENC_ATTR_H264_S stH264Attr;
    VENC_ATTR_H264_CBR_S    stH264Cbr;
    VENC_ATTR_H264_VBR_S    stH264Vbr;
    VENC_ATTR_H264_FIXQP_S  stH264FixQp;
    VENC_ATTR_H265_S        stH265Attr;
    VENC_ATTR_H265_CBR_S    stH265Cbr;
    VENC_ATTR_H265_VBR_S    stH265Vbr;
    VENC_ATTR_H265_FIXQP_S  stH265FixQp;
    VENC_ATTR_MJPEG_S stMjpegAttr;
    VENC_ATTR_MJPEG_FIXQP_S stMjpegeFixQp;
    VENC_ATTR_JPEG_S stJpegAttr;
    SIZE_S stPicSize;

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enNorm, enSize, &stPicSize);
     if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Get picture size failed!\n");
        return HI_FAILURE;
    }

    /******************************************
     step 1:  Create Venc Channel
    ******************************************/
    stVencChnAttr.stVeAttr.enType = enType;
    switch(enType)
    {
        case PT_H264:
        {
            stH264Attr.u32MaxPicWidth = stPicSize.u32Width;
            stH264Attr.u32MaxPicHeight = stPicSize.u32Height;
            stH264Attr.u32PicWidth = stPicSize.u32Width;/*the picture width*/
            stH264Attr.u32PicHeight = stPicSize.u32Height;/*the picture height*/
            stH264Attr.u32BufSize  = stPicSize.u32Width * stPicSize.u32Height;/*stream buffer size*/
            stH264Attr.u32Profile  = u32Profile;/*0: baseline; 1:MP; 2:HP;  3:svc_t */
            stH264Attr.bByFrame = HI_TRUE;/*get stream mode is slice mode or frame mode?*/
			stH264Attr.u32BFrameNum = 0;/* 0: not support B frame; >=1: number of B frames */
			stH264Attr.u32RefNum = 1;/* 0: default; number of refrence frame*/
			memcpy(&stVencChnAttr.stVeAttr.stAttrH264e, &stH264Attr, sizeof(VENC_ATTR_H264_S));

            if(SAMPLE_RC_CBR == enRcMode)
            {
                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
                stH264Cbr.u32Gop            = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stH264Cbr.u32StatTime       = 1; /* stream rate statics time(s) */
                stH264Cbr.u32SrcFrmRate      = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;/* input (vi) frame rate */
                stH264Cbr.fr32DstFrmRate = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;/* target frame rate */
				
				switch (enSize)
                {
                  case PIC_QCIF:
                       stH264Cbr.u32BitRate = 256; /* average bit rate */
                       break;
                  case PIC_QVGA:    /* 320 * 240 */
                  case PIC_CIF: 

                	   stH264Cbr.u32BitRate = 512;
                       break;

                  case PIC_D1:
                  case PIC_VGA:	   /* 640 * 480 */
                	   stH264Cbr.u32BitRate = 1024*2;
                       break;
                  case PIC_HD720:   /* 1280 * 720 */
                	   stH264Cbr.u32BitRate = 1024*2;
                	   break;
                  case PIC_HD1080:  /* 1920 * 1080 */
                  	   stH264Cbr.u32BitRate = 1024*4;
                	   break;
                  case PIC_5M:  /* 2592 * 1944 */
                  	   stH264Cbr.u32BitRate = 1024*8;
                	   break;                       
                  default :
                       stH264Cbr.u32BitRate = 1024*4;
                       break;
                }
                
                stH264Cbr.u32FluctuateLevel = 0; /* average bit rate */
                memcpy(&stVencChnAttr.stRcAttr.stAttrH264Cbr, &stH264Cbr, sizeof(VENC_ATTR_H264_CBR_S));
            }
            else if (SAMPLE_RC_FIXQP == enRcMode) 
            {
                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264FIXQP;
                stH264FixQp.u32Gop = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stH264FixQp.u32SrcFrmRate = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stH264FixQp.fr32DstFrmRate = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stH264FixQp.u32IQp = 20;
                stH264FixQp.u32PQp = 23;
                memcpy(&stVencChnAttr.stRcAttr.stAttrH264FixQp, &stH264FixQp,sizeof(VENC_ATTR_H264_FIXQP_S));
            }
            else if (SAMPLE_RC_VBR == enRcMode) 
            {
                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264VBR;
                stH264Vbr.u32Gop = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stH264Vbr.u32StatTime = 1;
                stH264Vbr.u32SrcFrmRate = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stH264Vbr.fr32DstFrmRate = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stH264Vbr.u32MinQp = 10;
                stH264Vbr.u32MaxQp = 40;
                switch (enSize)
                {
                  case PIC_QCIF:
                	   stH264Vbr.u32MaxBitRate= 256*3; /* average bit rate */
                	   break;
                  case PIC_QVGA:    /* 320 * 240 */
                  case PIC_CIF:
                	   stH264Vbr.u32MaxBitRate = 512*3;
                       break;
                  case PIC_D1:
                  case PIC_VGA:	   /* 640 * 480 */
                	   stH264Vbr.u32MaxBitRate = 1024*2;
                       break;
                  case PIC_HD720:   /* 1280 * 720 */
                	   stH264Vbr.u32MaxBitRate = 1024*3;
                	   break;
                  case PIC_HD1080:  /* 1920 * 1080 */
                  	   stH264Vbr.u32MaxBitRate = 1024*6;
                	   break;
                  case PIC_5M:  /* 2592 * 1944 */
                  	   stH264Vbr.u32MaxBitRate = 1024*8;
                	   break;                        
                  default :
                       stH264Vbr.u32MaxBitRate = 1024*4;
                       break;
                }
                memcpy(&stVencChnAttr.stRcAttr.stAttrH264Vbr, &stH264Vbr, sizeof(VENC_ATTR_H264_VBR_S));
            }
            else
            {
                return HI_FAILURE;
            }
        }
        break;
        
        case PT_MJPEG:
        {
            stMjpegAttr.u32MaxPicWidth = stPicSize.u32Width;
            stMjpegAttr.u32MaxPicHeight = stPicSize.u32Height;
            stMjpegAttr.u32PicWidth = stPicSize.u32Width;
            stMjpegAttr.u32PicHeight = stPicSize.u32Height;
            stMjpegAttr.u32BufSize = (((stPicSize.u32Width+15)>>4)<<4) * (((stPicSize.u32Height+15)>>4)<<4);
            stMjpegAttr.bByFrame = HI_TRUE;  /*get stream mode is field mode  or frame mode*/
            memcpy(&stVencChnAttr.stVeAttr.stAttrMjpeg, &stMjpegAttr, sizeof(VENC_ATTR_MJPEG_S));

            if(SAMPLE_RC_FIXQP == enRcMode)
            {
                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_MJPEGFIXQP;
                stMjpegeFixQp.u32Qfactor        = 90;
                stMjpegeFixQp.u32SrcFrmRate      = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stMjpegeFixQp.fr32DstFrmRate = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                memcpy(&stVencChnAttr.stRcAttr.stAttrMjpegeFixQp, &stMjpegeFixQp,
                       sizeof(VENC_ATTR_MJPEG_FIXQP_S));
            }
            else if (SAMPLE_RC_CBR == enRcMode)
            {
                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_MJPEGCBR;
                stVencChnAttr.stRcAttr.stAttrMjpegeCbr.u32StatTime       = 1;
                stVencChnAttr.stRcAttr.stAttrMjpegeCbr.u32SrcFrmRate      = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stVencChnAttr.stRcAttr.stAttrMjpegeCbr.fr32DstFrmRate = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stVencChnAttr.stRcAttr.stAttrMjpegeCbr.u32FluctuateLevel = 0;
                switch (enSize)
                {
                  case PIC_QCIF:
                	   stVencChnAttr.stRcAttr.stAttrMjpegeCbr.u32BitRate = 384*3; /* average bit rate */
                	   break;
                  case PIC_QVGA:    /* 320 * 240 */
                  case PIC_CIF:
                	   stVencChnAttr.stRcAttr.stAttrMjpegeCbr.u32BitRate = 768*3;
                       break;
                  case PIC_D1:
                  case PIC_VGA:	   /* 640 * 480 */
                	   stVencChnAttr.stRcAttr.stAttrMjpegeCbr.u32BitRate = 1024*3*3;
                       break;
                  case PIC_HD720:   /* 1280 * 720 */
                	   stVencChnAttr.stRcAttr.stAttrMjpegeCbr.u32BitRate = 1024*5*3;
                	   break;
                  case PIC_HD1080:  /* 1920 * 1080 */
                  	   stVencChnAttr.stRcAttr.stAttrMjpegeCbr.u32BitRate = 1024*10*3;
                	   break;
                  case PIC_5M:  /* 2592 * 1944 */
                  	   stVencChnAttr.stRcAttr.stAttrMjpegeCbr.u32BitRate = 1024*10*3;
                	   break;                       
                  default :
                       stVencChnAttr.stRcAttr.stAttrMjpegeCbr.u32BitRate = 1024*10*3;
                       break;
                }
            }
            else if (SAMPLE_RC_VBR == enRcMode) 
            {
                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_MJPEGVBR;
                stVencChnAttr.stRcAttr.stAttrMjpegeVbr.u32StatTime = 1;
                stVencChnAttr.stRcAttr.stAttrMjpegeVbr.u32SrcFrmRate = (VIDEO_ENCODING_MODE_PAL == enNorm)?25:30;
                stVencChnAttr.stRcAttr.stAttrMjpegeVbr.fr32DstFrmRate = 5;
                stVencChnAttr.stRcAttr.stAttrMjpegeVbr.u32MinQfactor = 50;
                stVencChnAttr.stRcAttr.stAttrMjpegeVbr.u32MaxQfactor = 95;
                switch (enSize)
                {
                  case PIC_QCIF:
                	   stVencChnAttr.stRcAttr.stAttrMjpegeVbr.u32MaxBitRate= 256*3; /* average bit rate */
                	   break;
                  case PIC_QVGA:    /* 320 * 240 */
                  case PIC_CIF:
                	   stVencChnAttr.stRcAttr.stAttrMjpegeVbr.u32MaxBitRate = 512*3;
                       break;
                  case PIC_D1:
                  case PIC_VGA:	   /* 640 * 480 */
                	   stVencChnAttr.stRcAttr.stAttrMjpegeVbr.u32MaxBitRate = 1024*2*3;
                       break;
                  case PIC_HD720:   /* 1280 * 720 */
                	   stVencChnAttr.stRcAttr.stAttrMjpegeVbr.u32MaxBitRate = 1024*3*3;
                	   break;
                  case PIC_HD1080:  /* 1920 * 1080 */
                  	   stVencChnAttr.stRcAttr.stAttrMjpegeVbr.u32MaxBitRate = 1024*6*3;
                	   break;
                  case PIC_5M:  /* 2592 * 1944 */
                  	   stVencChnAttr.stRcAttr.stAttrMjpegeVbr.u32MaxBitRate = 1024*12*3;
                	   break;                        
                  default :
                       stVencChnAttr.stRcAttr.stAttrMjpegeVbr.u32MaxBitRate = 1024*4*3;
                       break;
                }
            }
            else 
            {
                SAMPLE_PRT("cann't support other mode in this version!\n");

                return HI_FAILURE;
            }
        }
        break;
            
        case PT_JPEG:
            stJpegAttr.u32PicWidth  = stPicSize.u32Width;
            stJpegAttr.u32PicHeight = stPicSize.u32Height;
			stJpegAttr.u32MaxPicWidth  = stPicSize.u32Width;
            stJpegAttr.u32MaxPicHeight = stPicSize.u32Height;
            stJpegAttr.u32BufSize   = (((stPicSize.u32Width+15)>>4)<<4) * (((stPicSize.u32Height+15)>>4)<<4);
            stJpegAttr.bByFrame     = HI_TRUE;/*get stream mode is field mode  or frame mode*/
            stJpegAttr.bSupportDCF  = HI_FALSE;
            memcpy(&stVencChnAttr.stVeAttr.stAttrJpeg, &stJpegAttr, sizeof(VENC_ATTR_JPEG_S));
            break;

        case PT_H265:
        {
            stH265Attr.u32MaxPicWidth = stPicSize.u32Width;
            stH265Attr.u32MaxPicHeight = stPicSize.u32Height;
            stH265Attr.u32PicWidth = stPicSize.u32Width;/*the picture width*/
            stH265Attr.u32PicHeight = stPicSize.u32Height;/*the picture height*/
            stH265Attr.u32BufSize  = stPicSize.u32Width * stPicSize.u32Height * 2;/*stream buffer size*/
			if(u32Profile >=1)
				stH265Attr.u32Profile = 0;/*0:MP; */
			else
				stH265Attr.u32Profile  = u32Profile;/*0:MP*/
            stH265Attr.bByFrame = HI_TRUE;/*get stream mode is slice mode or frame mode?*/
			stH265Attr.u32BFrameNum = 0;/* 0: not support B frame; >=1: number of B frames */
			stH265Attr.u32RefNum = 1;/* 0: default; number of refrence frame*/
            memcpy(&stVencChnAttr.stVeAttr.stAttrH265e, &stH265Attr, sizeof(VENC_ATTR_H265_S));

            if(SAMPLE_RC_CBR == enRcMode)
            {
                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265CBR;
                stH265Cbr.u32Gop            = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stH265Cbr.u32StatTime       = 1; /* stream rate statics time(s) */
                stH265Cbr.u32SrcFrmRate      = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;/* input (vi) frame rate */
                stH265Cbr.fr32DstFrmRate = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;/* target frame rate */
                switch (enSize)
                {
                  case PIC_QCIF:
                       stH265Cbr.u32BitRate = 256; /* average bit rate */
                       break;
                  case PIC_QVGA:    /* 320 * 240 */
                  case PIC_CIF: 

                	   stH265Cbr.u32BitRate = 512;
                       break;

                  case PIC_D1:
                  case PIC_VGA:	   /* 640 * 480 */
                	   stH265Cbr.u32BitRate = 1024*2;
                       break;
                  case PIC_HD720:   /* 1280 * 720 */
                	   stH265Cbr.u32BitRate = 1024*3;
                	   break;
                  case PIC_HD1080:  /* 1920 * 1080 */
                  	   stH265Cbr.u32BitRate = 1024*4;
                	   break;
                  case PIC_5M:  /* 2592 * 1944 */
                  	   stH265Cbr.u32BitRate = 1024*8;
                	   break;                        
                  default :
                       stH265Cbr.u32BitRate = 1024*4;
                       break;
                }
                
                stH265Cbr.u32FluctuateLevel = 0; /* average bit rate */
                memcpy(&stVencChnAttr.stRcAttr.stAttrH265Cbr, &stH265Cbr, sizeof(VENC_ATTR_H265_CBR_S));
            }
            else if (SAMPLE_RC_FIXQP == enRcMode) 
            {
                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265FIXQP;
                stH265FixQp.u32Gop = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stH265FixQp.u32SrcFrmRate = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stH265FixQp.fr32DstFrmRate = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stH265FixQp.u32IQp = 20;
                stH265FixQp.u32PQp = 23;
                memcpy(&stVencChnAttr.stRcAttr.stAttrH265FixQp, &stH265FixQp,sizeof(VENC_ATTR_H265_FIXQP_S));
            }
            else if (SAMPLE_RC_VBR == enRcMode) 
            {
                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265VBR;
                stH265Vbr.u32Gop = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stH265Vbr.u32StatTime = 1;
                stH265Vbr.u32SrcFrmRate = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stH265Vbr.fr32DstFrmRate = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stH265Vbr.u32MinQp = 10;
                stH265Vbr.u32MaxQp = 40;
                switch (enSize)
                {
                  case PIC_QCIF:
                	   stH265Vbr.u32MaxBitRate= 256*3; /* average bit rate */
                	   break;
                  case PIC_QVGA:    /* 320 * 240 */
                  case PIC_CIF:
                	   stH265Vbr.u32MaxBitRate = 512*3;
                       break;
                  case PIC_D1:
                  case PIC_VGA:	   /* 640 * 480 */
                	   stH265Vbr.u32MaxBitRate = 1024*2;
                       break;
                  case PIC_HD720:   /* 1280 * 720 */
                	   stH265Vbr.u32MaxBitRate = 1024*3;
                	   break;
                  case PIC_HD1080:  /* 1920 * 1080 */
                  	   stH265Vbr.u32MaxBitRate = 1024*6;
                	   break;
                  case PIC_5M:  /* 2592 * 1944 */
                  	   stH265Vbr.u32MaxBitRate = 1024*8;
                	   break;                        
                  default :
                       stH265Vbr.u32MaxBitRate = 1024*4;
                       break;
                }
                memcpy(&stVencChnAttr.stRcAttr.stAttrH265Vbr, &stH265Vbr, sizeof(VENC_ATTR_H265_VBR_S));
            }
            else
            {
                return HI_FAILURE;
            }
        }
        break;
        default:
            return HI_ERR_VENC_NOT_SUPPORT;
    }

    s32Ret = HI_MPI_VENC_CreateChn(VencChn, &stVencChnAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VENC_CreateChn [%d] faild with %#x!\n",\
                VencChn, s32Ret);
        return s32Ret;
    }

    /******************************************
     step 2:  Start Recv Venc Pictures
    ******************************************/
    s32Ret = HI_MPI_VENC_StartRecvPic(VencChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VENC_StartRecvPic faild with%#x!\n", s32Ret);
        return HI_FAILURE;
    }

    return HI_SUCCESS;

}

/******************************************************************************
* funciton : Stop venc ( stream mode -- H264, MJPEG )
******************************************************************************/
HI_S32 SAMPLE_COMM_VENC_Stop(VENC_CHN VencChn)
{
    HI_S32 s32Ret;

    /******************************************
     step 1:  Stop Recv Pictures
    ******************************************/
    s32Ret = HI_MPI_VENC_StopRecvPic(VencChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VENC_StopRecvPic vechn[%d] failed with %#x!\n",\
               VencChn, s32Ret);
        return HI_FAILURE;
    }

    /******************************************
     step 2:  Distroy Venc Channel
    ******************************************/
    s32Ret = HI_MPI_VENC_DestroyChn(VencChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VENC_DestroyChn vechn[%d] failed with %#x!\n",\
               VencChn, s32Ret);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

/******************************************************************************
* funciton : Start snap
******************************************************************************/
HI_S32 SAMPLE_COMM_VENC_SnapStart(VENC_CHN VencChn, SIZE_S *pstSize, HI_BOOL bSupportDCF)
{
    HI_S32 s32Ret;
    VENC_CHN_ATTR_S stVencChnAttr;
    VENC_ATTR_JPEG_S stJpegAttr;

    /******************************************
     step 1:  Create Venc Channel
    ******************************************/
    stVencChnAttr.stVeAttr.enType = PT_JPEG;
    
    stJpegAttr.u32MaxPicWidth  = pstSize->u32Width;
    stJpegAttr.u32MaxPicHeight = pstSize->u32Height;
    stJpegAttr.u32PicWidth  = pstSize->u32Width;
    stJpegAttr.u32PicHeight = pstSize->u32Height;
    stJpegAttr.u32BufSize = (((pstSize->u32Width+15)>>4)<<4) * (((pstSize->u32Height+15)>>4)<<4);
    stJpegAttr.bByFrame = HI_TRUE;/*get stream mode is field mode  or frame mode*/
    stJpegAttr.bSupportDCF = bSupportDCF;
    memcpy(&stVencChnAttr.stVeAttr.stAttrJpeg, &stJpegAttr, sizeof(VENC_ATTR_JPEG_S));

    s32Ret = HI_MPI_VENC_CreateChn(VencChn, &stVencChnAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VENC_CreateChn [%d] faild with %#x!\n",\
                VencChn, s32Ret);
        return s32Ret;
    }
    return HI_SUCCESS;
}

/******************************************************************************
* funciton : Stop snap
******************************************************************************/
HI_S32 SAMPLE_COMM_VENC_SnapStop(VENC_CHN VencChn)
{
    HI_S32 s32Ret;

	s32Ret = HI_MPI_VENC_StopRecvPic(VencChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VENC_StopRecvPic vechn[%d] failed with %#x!\n", VencChn, s32Ret);
        return HI_FAILURE;
    }
    
    s32Ret = HI_MPI_VENC_DestroyChn(VencChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VENC_DestroyChn vechn[%d] failed with %#x!\n", VencChn, s32Ret);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

/******************************************************************************
* funciton : snap process
******************************************************************************/
HI_S32 SAMPLE_COMM_VENC_SnapProcess(VENC_CHN VencChn, HI_BOOL bSaveJpg, HI_BOOL bSaveThm)
{
    struct timeval TimeoutVal;
    fd_set read_fds;
    HI_S32 s32VencFd;
    VENC_CHN_STAT_S stStat;
    VENC_STREAM_S stStream;
    HI_S32 s32Ret;
	VENC_RECV_PIC_PARAM_S stRecvParam;

    printf("press the Enter key to snap one pic\n");  
	getchar();
    
    /******************************************
     step 2:  Start Recv Venc Pictures
    ******************************************/
    stRecvParam.s32RecvPicNum = 1;
    s32Ret = HI_MPI_VENC_StartRecvPicEx(VencChn,&stRecvParam);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VENC_StartRecvPic faild with%#x!\n", s32Ret);
        return HI_FAILURE;
    }
    /******************************************
     step 3:  recv picture
    ******************************************/
    s32VencFd = HI_MPI_VENC_GetFd(VencChn);
    if (s32VencFd < 0)
    {
    	 SAMPLE_PRT("HI_MPI_VENC_GetFd faild with%#x!\n", s32VencFd);
        return HI_FAILURE;
    }

    FD_ZERO(&read_fds);
    FD_SET(s32VencFd, &read_fds);
    
    TimeoutVal.tv_sec  = 2;
    TimeoutVal.tv_usec = 0;
    s32Ret = select(s32VencFd+1, &read_fds, NULL, NULL, &TimeoutVal);
    if (s32Ret < 0) 
    {
        SAMPLE_PRT("snap select failed!\n");
        return HI_FAILURE;
    }
    else if (0 == s32Ret) 
    {
        SAMPLE_PRT("snap time out!\n");
        return HI_FAILURE;
    }
    else
    {
        if (FD_ISSET(s32VencFd, &read_fds))
        {
            s32Ret = HI_MPI_VENC_Query(VencChn, &stStat);
            if (s32Ret != HI_SUCCESS)
            {
                SAMPLE_PRT("HI_MPI_VENC_Query failed with %#x!\n", s32Ret);
                return HI_FAILURE;
            }
			
			/*******************************************************
			 suggest to check both u32CurPacks and u32LeftStreamFrames at the same time,for example:
			 if(0 == stStat.u32CurPacks || 0 == stStat.u32LeftStreamFrames)
			 {
				SAMPLE_PRT("NOTE: Current  frame is NULL!\n");
				return HI_SUCCESS;
			 }
			*******************************************************/
			if(0 == stStat.u32CurPacks)
			{
				  SAMPLE_PRT("NOTE: Current  frame is NULL!\n");
				  return HI_SUCCESS;
			}
            stStream.pstPack = (VENC_PACK_S*)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
            if (NULL == stStream.pstPack)
            {
                SAMPLE_PRT("malloc memory failed!\n");
                return HI_FAILURE;
            }

            stStream.u32PackCount = stStat.u32CurPacks;
            s32Ret = HI_MPI_VENC_GetStream(VencChn, &stStream, -1);
            if (HI_SUCCESS != s32Ret)
            {
                SAMPLE_PRT("HI_MPI_VENC_GetStream failed with %#x!\n", s32Ret);
                free(stStream.pstPack);
                stStream.pstPack = NULL;
                return HI_FAILURE;
            }

            s32Ret = SAMPLE_COMM_VENC_SaveSnap(&stStream, bSaveJpg, bSaveThm);
            if (HI_SUCCESS != s32Ret)
            {
                SAMPLE_PRT("HI_MPI_VENC_GetStream failed with %#x!\n", s32Ret);
                free(stStream.pstPack);
                stStream.pstPack = NULL;
                return HI_FAILURE;
            }

            s32Ret = HI_MPI_VENC_ReleaseStream(VencChn, &stStream);
            if (s32Ret)
            {
                SAMPLE_PRT("HI_MPI_VENC_ReleaseStream failed with %#x!\n", s32Ret);
                free(stStream.pstPack);
                stStream.pstPack = NULL;
                return HI_FAILURE;
            }

            free(stStream.pstPack);
            stStream.pstPack = NULL;
	    }
    }
    /******************************************
     step 4:  stop recv picture
    ******************************************/
    s32Ret = HI_MPI_VENC_StopRecvPic(VencChn);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("HI_MPI_VENC_StopRecvPic failed with %#x!\n",  s32Ret);
        return HI_FAILURE;
    }


    return HI_SUCCESS;
}

/******************************************************************************
* funciton : get stream from each channels and save them
******************************************************************************/
HI_VOID* SAMPLE_COMM_VENC_GetVencStreamProc(HI_VOID *p)
{
    HI_S32 i;
    HI_S32 s32ChnTotal;
    VENC_CHN_ATTR_S stVencChnAttr;
    SAMPLE_VENC_GETSTREAM_PARA_S *pstPara;
    HI_S32 maxfd = 0;
    struct timeval TimeoutVal;
    fd_set read_fds;
    HI_S32 VencFd[VENC_MAX_CHN_NUM];
    HI_CHAR aszFileName[VENC_MAX_CHN_NUM][64];
    FILE *pFile[VENC_MAX_CHN_NUM];
    char szFilePostfix[10];
    VENC_CHN_STAT_S stStat;
    VENC_STREAM_S stStream;
    HI_S32 s32Ret;
    VENC_CHN VencChn;
    PAYLOAD_TYPE_E enPayLoadType[VENC_MAX_CHN_NUM];
    
    pstPara = (SAMPLE_VENC_GETSTREAM_PARA_S*)p;
    s32ChnTotal = pstPara->s32Cnt;

    /******************************************
     step 1:  check & prepare save-file & venc-fd
    ******************************************/
    if (s32ChnTotal >= VENC_MAX_CHN_NUM)
    {
        SAMPLE_PRT("input count invaild\n");
        return NULL;
    }
    for (i = 0; i < s32ChnTotal; i++)
    {
        /* decide the stream file name, and open file to save stream */
        VencChn = i;
        s32Ret = HI_MPI_VENC_GetChnAttr(VencChn, &stVencChnAttr);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("HI_MPI_VENC_GetChnAttr chn[%d] failed with %#x!\n", \
                   VencChn, s32Ret);
            return NULL;
        }
        enPayLoadType[i] = stVencChnAttr.stVeAttr.enType;

        s32Ret = SAMPLE_COMM_VENC_GetFilePostfix(enPayLoadType[i], szFilePostfix);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("SAMPLE_COMM_VENC_GetFilePostfix [%d] failed with %#x!\n", \
                   stVencChnAttr.stVeAttr.enType, s32Ret);
            return NULL;
        }
        sprintf(aszFileName[i], "stream_chn%d%s", i, szFilePostfix);
        pFile[i] = fopen(aszFileName[i], "wb");
        if (!pFile[i])
        {
            SAMPLE_PRT("open file[%s] failed!\n", 
                   aszFileName[i]);
            return NULL;
        }

        /* Set Venc Fd. */
        VencFd[i] = HI_MPI_VENC_GetFd(i);
        if (VencFd[i] < 0)
        {
            SAMPLE_PRT("HI_MPI_VENC_GetFd failed with %#x!\n", 
                   VencFd[i]);
            return NULL;
        }
        if (maxfd <= VencFd[i])
        {
            maxfd = VencFd[i];
        }
    }
   #if ORTP_ENABLE
    /***rtp init****/
    pRtpSession = rtpInit( LOCAL_HOST_IP ,8080);  
    if (pRtpSession==NULL)   
    {   
        printf( "error rtpInit" ); 
        exit(-1);  
        return  0;   
    } 
    #endif
    /******************************************
     step 2:  Start to get streams of each channel.
    ******************************************/
    while (HI_TRUE == pstPara->bThreadStart)
    {
        FD_ZERO(&read_fds);
        for (i = 0; i < s32ChnTotal; i++)
        {
            FD_SET(VencFd[i], &read_fds);
        }

        TimeoutVal.tv_sec  = 2;
        TimeoutVal.tv_usec = 0;
        s32Ret = select(maxfd + 1, &read_fds, NULL, NULL, &TimeoutVal);
        if (s32Ret < 0)
        {
            SAMPLE_PRT("select failed!\n");
            break;
        }
        else if (s32Ret == 0)
        {
            SAMPLE_PRT("get venc stream time out, exit thread\n");
            continue;
        }
        else
        {
            for (i = 0; i < s32ChnTotal; i++)
            {
                if (FD_ISSET(VencFd[i], &read_fds))
                {
                    /*******************************************************
                     step 2.1 : query how many packs in one-frame stream.
                    *******************************************************/
                    memset(&stStream, 0, sizeof(stStream));
                    s32Ret = HI_MPI_VENC_Query(i, &stStat);
                    if (HI_SUCCESS != s32Ret)
                    {
                        SAMPLE_PRT("HI_MPI_VENC_Query chn[%d] failed with %#x!\n", i, s32Ret);
                        break;
                    }
					
					/*******************************************************
					step 2.2 :suggest to check both u32CurPacks and u32LeftStreamFrames at the same time,for example:
					 if(0 == stStat.u32CurPacks || 0 == stStat.u32LeftStreamFrames)
					 {
						SAMPLE_PRT("NOTE: Current  frame is NULL!\n");
						continue;
					 }
					*******************************************************/
					if(0 == stStat.u32CurPacks)
					{
						  SAMPLE_PRT("NOTE: Current  frame is NULL!\n");
						  continue;
					}
                    /*******************************************************
                     step 2.3 : malloc corresponding number of pack nodes.
                    *******************************************************/
                    stStream.pstPack = (VENC_PACK_S*)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
                    if (NULL == stStream.pstPack)
                    {
                        SAMPLE_PRT("malloc stream pack failed!\n");
                        break;
                    }
                    
                    /*******************************************************
                     step 2.4 : call mpi to get one-frame stream
                    *******************************************************/
                    stStream.u32PackCount = stStat.u32CurPacks;
                    s32Ret = HI_MPI_VENC_GetStream(i, &stStream, HI_TRUE);
                    if (HI_SUCCESS != s32Ret)
                    {
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        SAMPLE_PRT("HI_MPI_VENC_GetStream failed with %#x!\n", \
                               s32Ret);
                        break;
                    }

                    /*******************************************************
                     step 2.5 : save frame to file
                    *******************************************************/
                    s32Ret = SAMPLE_COMM_VENC_SaveStream(enPayLoadType[i], pFile[i], &stStream);
                    if (HI_SUCCESS != s32Ret)
                    {
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        SAMPLE_PRT("save stream failed!\n");
                        break;
                    }
                    /*******************************************************
                     step 2.6 : release stream
                    *******************************************************/
                    s32Ret = HI_MPI_VENC_ReleaseStream(i, &stStream);
                    if (HI_SUCCESS != s32Ret)
                    {
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        break;
                    }
                    /*******************************************************
                     step 2.7 : free pack nodes
                    *******************************************************/
                    free(stStream.pstPack);
                    stStream.pstPack = NULL;
                }
            }
        }
    }

    /*******************************************************
    * step 3 : close save-file
    *******************************************************/
    for (i = 0; i < s32ChnTotal; i++)
    {
        fclose(pFile[i]);
    }

    return NULL;
}



/******************************************************************************
* funciton : get svc_t stream from h264 channels and save them
******************************************************************************/
HI_VOID *SAMPLE_COMM_VENC_GetVencStreamProc_Svc_t(void *p)
{
    HI_S32 i=0;
	HI_S32 s32Cnt=0;
    HI_S32 s32ChnTotal;
    VENC_CHN_ATTR_S stVencChnAttr;
    SAMPLE_VENC_GETSTREAM_PARA_S *pstPara;
    HI_S32 maxfd = 0;
    struct timeval TimeoutVal;
    fd_set read_fds;
    HI_S32 VencFd[VENC_MAX_CHN_NUM];
    HI_CHAR aszFileName[VENC_MAX_CHN_NUM][64];
    FILE *pFile[VENC_MAX_CHN_NUM];
    char szFilePostfix[10];
    VENC_CHN_STAT_S stStat;
    VENC_STREAM_S stStream;
    HI_S32 s32Ret;
    VENC_CHN VencChn;
    PAYLOAD_TYPE_E enPayLoadType[VENC_MAX_CHN_NUM];
    
    pstPara = (SAMPLE_VENC_GETSTREAM_PARA_S*)p;
    s32ChnTotal = pstPara->s32Cnt;

    /******************************************
     step 1:  check & prepare save-file & venc-fd
    ******************************************/
    if (s32ChnTotal >= VENC_MAX_CHN_NUM)
    {
        SAMPLE_PRT("input count invaild\n");
        return NULL;
    }
    for (i = 0; i < s32ChnTotal; i++)
    {
        /* decide the stream file name, and open file to save stream */
        VencChn = i;
        s32Ret = HI_MPI_VENC_GetChnAttr(VencChn, &stVencChnAttr);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("HI_MPI_VENC_GetChnAttr chn[%d] failed with %#x!\n", \
                   VencChn, s32Ret);
            return NULL;
        }
        enPayLoadType[i] = stVencChnAttr.stVeAttr.enType;

        s32Ret = SAMPLE_COMM_VENC_GetFilePostfix(enPayLoadType[i], szFilePostfix);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("SAMPLE_COMM_VENC_GetFilePostfix [%d] failed with %#x!\n", \
                   stVencChnAttr.stVeAttr.enType, s32Ret);
            return NULL;
        }
		for(s32Cnt =0; s32Cnt<3; s32Cnt++)
	    {
	    	sprintf(aszFileName[i+s32Cnt], "Tid%d%s", i+s32Cnt, szFilePostfix);
	        pFile[i+s32Cnt] = fopen(aszFileName[i+s32Cnt], "wb");
			
	        if (!pFile[i+s32Cnt])
	        {
	            SAMPLE_PRT("open file[%s] failed!\n", 
	                   aszFileName[i+s32Cnt]);
	            return NULL;
	        }
		}	

        /* Set Venc Fd. */
        VencFd[i] = HI_MPI_VENC_GetFd(i);
        if (VencFd[i] < 0)
        {
            SAMPLE_PRT("HI_MPI_VENC_GetFd failed with %#x!\n", 
                   VencFd[i]);
            return NULL;
        }
        if (maxfd <= VencFd[i])
        {
            maxfd = VencFd[i];
        }
    }

    /******************************************
     step 2:  Start to get streams of each channel.
    ******************************************/
    while (HI_TRUE == pstPara->bThreadStart)
    {
        FD_ZERO(&read_fds);
        for (i = 0; i < s32ChnTotal; i++)
        {
            FD_SET(VencFd[i], &read_fds);
        }

        TimeoutVal.tv_sec  = 2;
        TimeoutVal.tv_usec = 0;
        s32Ret = select(maxfd + 1, &read_fds, NULL, NULL, &TimeoutVal);
        if (s32Ret < 0)
        {
            SAMPLE_PRT("select failed!\n");
            break;
        }
        else if (s32Ret == 0)
        {
            SAMPLE_PRT("get venc stream time out, exit thread\n");
            continue;
        }
        else
        {
            for (i = 0; i < s32ChnTotal; i++)
            {
                if (FD_ISSET(VencFd[i], &read_fds))
                {
                    /*******************************************************
                     step 2.1 : query how many packs in one-frame stream.
                    *******************************************************/
                    memset(&stStream, 0, sizeof(stStream));
                    s32Ret = HI_MPI_VENC_Query(i, &stStat);
                    if (HI_SUCCESS != s32Ret)
                    {
                        SAMPLE_PRT("HI_MPI_VENC_Query chn[%d] failed with %#x!\n", i, s32Ret);
                        break;
                    }
					
					/*******************************************************
					step 2.2 :suggest to check both u32CurPacks and u32LeftStreamFrames at the same time,for example:
					 if(0 == stStat.u32CurPacks || 0 == stStat.u32LeftStreamFrames)
					 {
						SAMPLE_PRT("NOTE: Current  frame is NULL!\n");
						continue;
					 }
					*******************************************************/
					if(0 == stStat.u32CurPacks)
					{
						  SAMPLE_PRT("NOTE: Current  frame is NULL!\n");
						  continue;
					}
                    /*******************************************************
                     step 2.3 : malloc corresponding number of pack nodes.
                    *******************************************************/
                    stStream.pstPack = (VENC_PACK_S*)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
                    if (NULL == stStream.pstPack)
                    {
                        SAMPLE_PRT("malloc stream pack failed!\n");
                        break;
                    }
                    
                    /*******************************************************
                     step 2.4 : call mpi to get one-frame stream
                    *******************************************************/
                    stStream.u32PackCount = stStat.u32CurPacks;
                    s32Ret = HI_MPI_VENC_GetStream(i, &stStream, HI_TRUE);
                    if (HI_SUCCESS != s32Ret)
                    {
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        SAMPLE_PRT("HI_MPI_VENC_GetStream failed with %#x!\n", \
                               s32Ret);
                        break;
                    }

                    /*******************************************************
                     step 2.5 : save frame to file
                    *******************************************************/
#if 1					
					for(s32Cnt=0;s32Cnt<3;s32Cnt++)
					{
						
						switch(s32Cnt)
						{
							case 0:
								if(BASE_IDRSLICE == stStream.stH264Info.enRefType ||
				    				BASE_PSLICE_REFBYBASE == stStream.stH264Info.enRefType)
								{
									s32Ret = SAMPLE_COMM_VENC_SaveStream(enPayLoadType[i], pFile[i+s32Cnt], &stStream);
								}
							break;

							case 1:
						        if(BASE_IDRSLICE == stStream.stH264Info.enRefType      ||
                                BASE_PSLICE_REFBYBASE == stStream.stH264Info.enRefType ||
                                BASE_PSLICE_REFBYENHANCE== stStream.stH264Info.enRefType)
								{
									s32Ret = SAMPLE_COMM_VENC_SaveStream(enPayLoadType[i], pFile[i+s32Cnt], &stStream);
						        }
							break;

							case 2:
								s32Ret = SAMPLE_COMM_VENC_SaveStream(enPayLoadType[i], pFile[i+s32Cnt], &stStream);
							break;
						}
					
						
						if (HI_SUCCESS != s32Ret)
	                    {
	                        free(stStream.pstPack);
	                        stStream.pstPack = NULL;
	                        SAMPLE_PRT("save stream failed!\n");
	                        break;
	                    }
					}
#else

					for(s32Cnt=0;s32Cnt<3;s32Cnt++)
                    {
					   if(s32Cnt == 0)
                       {
                            if (NULL != pFile[i+s32Cnt])
                            {
                                if(BASE_IDRSLICE == stStream.stH264Info.enRefType ||
			    						BASE_PSLICE_REFBYBASE == stStream.stH264Info.enRefType)
                                {
                                        s32Ret = SAMPLE_COMM_VENC_SaveStream(enPayLoadType[i], pFile[i+s32Cnt], &stStream);
                                }
                                 
                             }
                        
                       }
                       else if(s32Cnt == 1)
                       {
                            if (NULL != pFile[i+s32Cnt])
                            {
                                if(BASE_IDRSLICE == stStream.stH264Info.enRefType         ||
                                    BASE_PSLICE_REFBYBASE == stStream.stH264Info.enRefType||
                                    BASE_PSLICE_REFBYENHANCE== stStream.stH264Info.enRefType)
                                {                                 
									s32Ret = SAMPLE_COMM_VENC_SaveStream(enPayLoadType[i], pFile[i+s32Cnt], &stStream);   
                                }
                                 
                             }
                        
                       }
                       else
                       {
                            if (NULL != pFile[i+s32Cnt])
                            {                            
							 	s32Ret = SAMPLE_COMM_VENC_SaveStream(enPayLoadType[i], pFile[i+s32Cnt], &stStream);                                
                            }
                        
                       }
                    }
					if (HI_SUCCESS != s32Ret)
                    {
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        SAMPLE_PRT("save stream failed!\n");
                        break;
                    }
#endif
                    /*******************************************************
                     step 2.6 : release stream
                    *******************************************************/
                    s32Ret = HI_MPI_VENC_ReleaseStream(i, &stStream);
                    if (HI_SUCCESS != s32Ret)
                    {
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        break;
                    }
                    /*******************************************************
                     step 2.7 : free pack nodes
                    *******************************************************/
                    free(stStream.pstPack);
                    stStream.pstPack = NULL;
                }
            }
        }
    }

    /*******************************************************
     step 3 : close save-file
    *******************************************************/
    for (i = 0; i < s32ChnTotal; i++)
    {
        for (s32Cnt = 0; s32Cnt < 3; s32Cnt++)
        {
            if (pFile[i+s32Cnt])
            {
                fclose(pFile[i+s32Cnt]);
            }
        }
    }

    return NULL;
}

/******************************************************************************
* funciton : start get venc stream process thread
******************************************************************************/
HI_S32 SAMPLE_COMM_VENC_StartGetStream(HI_S32 s32Cnt)
{
    gs_stPara.bThreadStart = HI_TRUE;
    gs_stPara.s32Cnt = s32Cnt;

    return pthread_create(&gs_VencPid, 0, SAMPLE_COMM_VENC_GetVencStreamProc, (HI_VOID*)&gs_stPara);
}

/******************************************************************************
* funciton : start get venc svc-t stream process thread
******************************************************************************/
HI_S32 SAMPLE_COMM_VENC_StartGetStream_Svc_t(HI_S32 s32Cnt)
{
    gs_stPara.bThreadStart = HI_TRUE;
    gs_stPara.s32Cnt = s32Cnt;

    return pthread_create(&gs_VencPid, 0, SAMPLE_COMM_VENC_GetVencStreamProc_Svc_t, (HI_VOID*)&gs_stPara);
}



/******************************************************************************
* funciton : stop get venc stream process.
******************************************************************************/
HI_S32 SAMPLE_COMM_VENC_StopGetStream()
{
    if (HI_TRUE == gs_stPara.bThreadStart)
    {
        gs_stPara.bThreadStart = HI_FALSE;
        pthread_join(gs_VencPid, 0);
    }
    return HI_SUCCESS;
}


HI_VOID SAMPLE_COMM_VENC_ReadOneFrame( FILE * fp, HI_U8 * pY, HI_U8 * pU, HI_U8 * pV,
                                              HI_U32 width, HI_U32 height, HI_U32 stride, HI_U32 stride2)
{
    HI_U8 * pDst;

    HI_U32 u32Row;
    

    pDst = pY;
    for ( u32Row = 0; u32Row < height; u32Row++ )
    {
        fread( pDst, width, 1, fp );
        pDst += stride;
    }
    
    pDst = pU;
    for ( u32Row = 0; u32Row < height/2; u32Row++ )
    {
        fread( pDst, width/2, 1, fp );
        pDst += stride2;
    }
    
    pDst = pV;
    for ( u32Row = 0; u32Row < height/2; u32Row++ )
    {
        fread( pDst, width/2, 1, fp );
        pDst += stride2;
    }
   
}

HI_S32 SAMPLE_COMM_VENC_PlanToSemi(HI_U8 *pY, HI_S32 yStride, 
                       HI_U8 *pU, HI_S32 uStride,
					   HI_U8 *pV, HI_S32 vStride, 
					   HI_S32 picWidth, HI_S32 picHeight)
{
    HI_S32 i;
    HI_U8* pTmpU, *ptu;
    HI_U8* pTmpV, *ptv;
    
    HI_S32 s32HafW = uStride >>1 ;
    HI_S32 s32HafH = picHeight >>1 ;
    HI_S32 s32Size = s32HafW*s32HafH;
        
    pTmpU = malloc( s32Size ); ptu = pTmpU;
    pTmpV = malloc( s32Size ); ptv = pTmpV;
    if((pTmpU==HI_NULL)||(pTmpV==HI_NULL))
    {
        printf("malloc buf failed\n");
        return HI_FAILURE;
    }

    memcpy(pTmpU,pU,s32Size);
    memcpy(pTmpV,pV,s32Size);
    
    for(i = 0;i<s32Size>>1;i++)
    {
        *pU++ = *pTmpV++;
        *pU++ = *pTmpU++;
        
    }
    for(i = 0;i<s32Size>>1;i++)
    {
        *pV++ = *pTmpV++;
        *pV++ = *pTmpU++;        
    }

    free( ptu );
    free( ptv );

    return HI_SUCCESS;
}


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */