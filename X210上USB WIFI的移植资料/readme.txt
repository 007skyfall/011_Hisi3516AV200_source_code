目录文件：
mt7600.rar       //源码
mt7601Usta.ko    //编译好的模块
操作步骤如下：
第一步：X210刷机(qt版)
第二步：编译驱动代码
2.1 查看芯片的pid和uid
360随身Wifi2代用的芯片是：MT7601
这个无牌的Wifi用的芯片也是:MT7601
这两个的ID号是pid vid
lsusb:
Bus 001 Device 002: ID 148f:760b Ralink Technology, Corp.  //360
Bus 001 Device 004: ID 148f:7601 Ralink Technology, Corp.  //无牌
Rallink的芯片，pid:148f vid:760b ,这个要在源码中去添加这个ID号进行匹配，不然用不了（针对360wifi）
在驱动rtusb_dev_id.c中
USB_DEVICE_ID rtusb_dev_id[] = {
#ifdef RT6570
	{USB_DEVICE(0x148f,0x6570)}, /* Ralink 6570 */
#endif /* RT6570 */
	{USB_DEVICE(0x148f, 0x7650)}, /* MT7650 */
#ifdef MT7601U
	{USB_DEVICE(0x148f,0x6370)}, /* Ralink 6370 */
	{USB_DEVICE(0x148f,0x7601)}, /* MT 6370 */  驱动自带有了
        {USB_DEVICE(0x148f,0x760b)}, /* 360wifi */  添加360wifi的
#endif /* MT7601U */
	{ }/* Terminating entry */
};
//如果用无牌的wifi的话，上面的代码不需要添加
2.2,移植驱动到开发板
2.2.1：修改Makefile
平台换成：三星
PLATFORM = SMDK
交叉工具链
LINUX_SRC =  //linux内核源码树
CROSS_COMPILE =  //交叉编译环境

2.2.2：修改网卡名字(可选)
修改include/rtmp_def.h文件
#define INF_MAIN_DEV_NAME "ra"
#define INF_MBSSID_DEV_NAME "ra"

2.2.3：查看os/linux/config.mk文件
确保config.mk文件中WPA_SUPPLICANT=y

2.2.4：编译
make clean
make -j2 
其中生成的mt7601Usta.ko文件即是我们所需要的驱动程序(os/linux/mt7601Usta.ko)

2.2.5：把mt7601Usta.ko拷到开发板中
在开发板的根文件系统中创建目录/etc/Wireless/RT2870STA/ (注意，此目录为开发板的根文件系统)
mkdir /etc/Wireless/RT2870STA/ -p
将源码目录中的RT2870STA.dat（源码根目录下）拷贝到刚才创建的etc/Wireless/RT2870STA/目录中

第三步：测试模块
3.1配置ssid和密码
在开发板/etc/wpa_wpa2.conf中配置
network={
#  key_mgmt=NONE
        ssid="gigi"
        psk="1234567890"
}
3.2操作命令     
insmod mt7601Usta.ko            //安装驱动程序
ifconfig ra0 up                 //开启无线网卡
wpa_supplicant -B -c /etc/wpa_supplicant.conf -i ra0  //连接无线网络
wpa_cli -i ra0 status       //查看连接状态
ifconfig ra0 192.168.1.200  //手动配置ip，同一网段
route add default gw 192.168.1.1 dev ra0  //配置网关
ping 192.168.1.1    //ping 网关
ping 8.8.8.8        //ping 外网
vi /etc/resolv.conf  //配置dns
nameserver 192.168.1.1  
ping www.baidu.com   


注意：
1，移植wap_supplicant (支持wpa/wpa2加密模式)
   九鼎的Qt根文件系统中已移植了这个
   iw工具，这个只能用于open和WEP加密
2, 移植dhcp
   九鼎的没有移植，所以就手动配置了

3，测试时有内核打印的错误信息
   RtmpUSBNullFrameKickOut - Send NULL Frame @24 Mbps...
可以在“common”里面找到“cmm_data_usb.c ”文件
在 1181行中找到：DBGPRINT(RT_DEBUG_TRACE, ("%s - Send NULL Frame @%d Mbps...\n", __FUNCTION__, RateIdToMbps[pAd->CommonCfg.TxRate]));
将它注释即可。

