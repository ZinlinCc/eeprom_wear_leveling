#ifndef __APP_EEPROM_WEAR_LEVELING_H
#define __APP_EEPROM_WEAR_LEVELING_H

#ifdef __APP_EEPROM_WEAR_LEVELING
extern "C"
{
#endif
/*
 *	EEPROM 磨损平衡 V1.0
 *	不支持一次写入数据长度超过EEPROM_WEAR_LEVELING_OPERATION_SIZE长度的数据.当前写入还有点问题，连续操作时得加个5ms的间隔才能正常写入
 *	目前对效率的考虑不是很深刻，本版本只是实现了功能，后续会对效率进行优化
 *	初始化里申请空间的函数时特定的，要根据工程去修改,其实用malloc实用性会更好一点
 *	对于错误写入或者脏数据的处理还没写好，后续会加上
 *	掉电保护需要硬件支持，本版本不支持掉电保护
 *
 *	移植的时候需要去修改下面3个函数：
 *		_app_eeprom_wear_leveling_i2c_read
 *		_app_eeprom_wear_leveling_i2c_write
 *		_app_eeprom_wear_leveling_i2c_delay
 *	使用的时候依次执行下面2个函数：
 *		app_eeprom_wear_leveling_init(放在初始化里)
 *		app_eeprom_wear_leveling(放在主循环里)
 *		
 *
 *
 *													 By Czl 2023年8月11日
 */
#include "includes.h"

/*
 *  以下三个宏定义是要根据实际情况修改的:
 *	EEPROM_WEAR_LEVELING_START_ADDR
 *	EEPROM_WEAR_LEVELING_SPACE
 *	EEPROM_WEAR_LEVELING_OPERATION_SIZE
 */
/*定义保存起始地址*/
#define EEPROM_WEAR_LEVELING_START_ADDR 0x0000
/*
 *磨损平衡使用的空间，可以使用的地址是 EEPROM_WEAR_LEVELING_START_ADDR ~ (EEPROM_WEAR_LEVELING_SPACE-1)
 *注意空间大小得是一个读取块字节数的倍数
 */
#define EEPROM_WEAR_LEVELING_SPACE 256

/*一次读写操作最大的数据空间大小*/
#define EEPROM_WEAR_LEVELING_OPERATION_SIZE 64

/*定义保存数据帧头为0x91*/
#define EEPROM_WEAR_LEVELING_SAVE_HEAD 0x91

/*定义索引结构*/
#define CZL_INDEX(A, B, C)                       \
	{                                            \
		A, (unsigned char *)&B, C, 0xffffffff, 0 \
	}
	typedef struct
	{
		unsigned char index;
		unsigned char *buf;
		unsigned char len;
		unsigned int adr;
		unsigned char exist; // 上电读取时是否读取成功，默认是0，读取成功后写入0xFF。若读取失败，则在重新写入后，会将该值写入0xFF
	} WEAR_LEVELING_INDEX;
	/*
	 *定义在eeprom里数据的保存格式
	 *实际还有数据本身和校验和，不过由于数据长度不固定，所以不在这里定义
	 */
	typedef struct
	{
		unsigned char head;	 // 帧头
		unsigned char index; // 索引
		unsigned short adr;	 // 地址
		unsigned char len;	 // 数据长度,数据本身长度+校验和的长度（校验和为1字节）
	} WEAR_LEVELING_DATA_SAVE;
	/*定义功能结构体*/
	typedef struct
	{
		unsigned char *buf;				   // 数据保存空间，大小为一页的字节数
		unsigned char step;				   // 当前步骤
		time_ms_T tout;					   // 超时时间
		unsigned short cur_read_adr;	   // 当前读取地址
		WEAR_LEVELING_DATA_SAVE save_data; // 读取保存的数据
		unsigned char index_num;		   // 索引数量
		//测试使用
		unsigned int index_1_fail;
		unsigned int index_2_fail;
		unsigned int save_area[10];
		unsigned char *read_buf;
	} APP_EEPROM_WAER_LEVELING_T;
	extern APP_EEPROM_WAER_LEVELING_T g_app_eeprom_wear_leveling;

	enum
	{
		APP_EEPROM_WEAR_LEVELING_STEP_IDLE,
		APP_EEPROM_WEAR_LEVELING_STEP_READ,
		APP_EEPROM_WEAR_LEVELING_STEP_CHECK,
		APP_EEPROM_WEAR_LEVELING_STEP_DOWN,
		APP_EEPROM_WEAR_LEVELING_STEP_SAVE,
		APP_EEPROM_WEAR_LEVELING_STEP_SREAD,
		APP_EEPROM_WEAR_LEVELING_STEP_ERASE,
		APP_EEPROM_WEAR_LEVELING_STEP_ERASE_ALl,
		APP_EEPROM_WEAR_LEVELING_STEP_ERASE_MONITOR,
	};

	extern void app_eeprom_wear_leveling_init(APP_EEPROM_WAER_LEVELING_T *p);
	extern void app_eeprom_wear_leveling(APP_EEPROM_WAER_LEVELING_T *p);
	extern void app_eeprom_wear_leveling_read_all(APP_EEPROM_WAER_LEVELING_T *p);
	extern void app_eeprom_wear_leveling_check(APP_EEPROM_WAER_LEVELING_T *p);
	extern unsigned char app_eeprom_wear_leveling_save(unsigned char num, unsigned char *buf);
	extern unsigned char app_eeprom_wear_leveling_read(unsigned char index);
	extern unsigned char app_eeprom_wear_leveling_erase(unsigned char index);
	extern void app_eeprom_wear_leveling_erase_adr(unsigned int adr, unsigned short len);

#ifdef __APP_EEPROM_WEAR_LEVELING
}
#endif

#endif /* __APP_EEPROM_WEAR_LEVELING_H */
