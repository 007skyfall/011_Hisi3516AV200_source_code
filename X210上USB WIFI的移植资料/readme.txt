Ŀ¼�ļ���
mt7600.rar       //Դ��
mt7601Usta.ko    //����õ�ģ��
�����������£�
��һ����X210ˢ��(qt��)
�ڶ�����������������
2.1 �鿴оƬ��pid��uid
360����Wifi2���õ�оƬ�ǣ�MT7601
������Ƶ�Wifi�õ�оƬҲ��:MT7601
��������ID����pid vid
lsusb:
Bus 001 Device 002: ID 148f:760b Ralink Technology, Corp.  //360
Bus 001 Device 004: ID 148f:7601 Ralink Technology, Corp.  //����
Rallink��оƬ��pid:148f vid:760b ,���Ҫ��Դ����ȥ������ID�Ž���ƥ�䣬��Ȼ�ò��ˣ����360wifi��
������rtusb_dev_id.c��
USB_DEVICE_ID rtusb_dev_id[] = {
#ifdef RT6570
	{USB_DEVICE(0x148f,0x6570)}, /* Ralink 6570 */
#endif /* RT6570 */
	{USB_DEVICE(0x148f, 0x7650)}, /* MT7650 */
#ifdef MT7601U
	{USB_DEVICE(0x148f,0x6370)}, /* Ralink 6370 */
	{USB_DEVICE(0x148f,0x7601)}, /* MT 6370 */  �����Դ�����
        {USB_DEVICE(0x148f,0x760b)}, /* 360wifi */  ���360wifi��
#endif /* MT7601U */
	{ }/* Terminating entry */
};
//��������Ƶ�wifi�Ļ�������Ĵ��벻��Ҫ���
2.2,��ֲ������������
2.2.1���޸�Makefile
ƽ̨���ɣ�����
PLATFORM = SMDK
���湤����
LINUX_SRC =  //linux�ں�Դ����
CROSS_COMPILE =  //������뻷��

2.2.2���޸���������(��ѡ)
�޸�include/rtmp_def.h�ļ�
#define INF_MAIN_DEV_NAME "ra"
#define INF_MBSSID_DEV_NAME "ra"

2.2.3���鿴os/linux/config.mk�ļ�
ȷ��config.mk�ļ���WPA_SUPPLICANT=y

2.2.4������
make clean
make -j2 
�������ɵ�mt7601Usta.ko�ļ�������������Ҫ����������(os/linux/mt7601Usta.ko)

2.2.5����mt7601Usta.ko������������
�ڿ�����ĸ��ļ�ϵͳ�д���Ŀ¼/etc/Wireless/RT2870STA/ (ע�⣬��Ŀ¼Ϊ������ĸ��ļ�ϵͳ)
mkdir /etc/Wireless/RT2870STA/ -p
��Դ��Ŀ¼�е�RT2870STA.dat��Դ���Ŀ¼�£��������ղŴ�����etc/Wireless/RT2870STA/Ŀ¼��

������������ģ��
3.1����ssid������
�ڿ�����/etc/wpa_wpa2.conf������
network={
#  key_mgmt=NONE
        ssid="gigi"
        psk="1234567890"
}
3.2��������     
insmod mt7601Usta.ko            //��װ��������
ifconfig ra0 up                 //������������
wpa_supplicant -B -c /etc/wpa_supplicant.conf -i ra0  //������������
wpa_cli -i ra0 status       //�鿴����״̬
ifconfig ra0 192.168.1.200  //�ֶ�����ip��ͬһ����
route add default gw 192.168.1.1 dev ra0  //��������
ping 192.168.1.1    //ping ����
ping 8.8.8.8        //ping ����
vi /etc/resolv.conf  //����dns
nameserver 192.168.1.1  
ping www.baidu.com   


ע�⣺
1����ֲwap_supplicant (֧��wpa/wpa2����ģʽ)
   �Ŷ���Qt���ļ�ϵͳ������ֲ�����
   iw���ߣ����ֻ������open��WEP����
2, ��ֲdhcp
   �Ŷ���û����ֲ�����Ծ��ֶ�������

3������ʱ���ں˴�ӡ�Ĵ�����Ϣ
   RtmpUSBNullFrameKickOut - Send NULL Frame @24 Mbps...
�����ڡ�common�������ҵ���cmm_data_usb.c ���ļ�
�� 1181�����ҵ���DBGPRINT(RT_DEBUG_TRACE, ("%s - Send NULL Frame @%d Mbps...\n", __FUNCTION__, RateIdToMbps[pAd->CommonCfg.TxRate]));
����ע�ͼ��ɡ�

