#ifndef __APP_EEPROM_WEAR_LEVELING_H
#define __APP_EEPROM_WEAR_LEVELING_H

#ifdef __APP_EEPROM_WEAR_LEVELING
extern "C"
{
#endif
/*
 *	EEPROM ĥ��ƽ�� V1.0
 *	��֧��һ��д�����ݳ��ȳ���EEPROM_WEAR_LEVELING_OPERATION_SIZE���ȵ�����.��ǰд�뻹�е����⣬��������ʱ�üӸ�5ms�ļ����������д��
 *	Ŀǰ��Ч�ʵĿ��ǲ��Ǻ���̣����汾ֻ��ʵ���˹��ܣ��������Ч�ʽ����Ż�
 *	��ʼ��������ռ�ĺ���ʱ�ض��ģ�Ҫ���ݹ���ȥ�޸�,��ʵ��mallocʵ���Ի����һ��
 *	���ڴ���д����������ݵĴ���ûд�ã����������
 *	���籣����ҪӲ��֧�֣����汾��֧�ֵ��籣��
 *
 *	��ֲ��ʱ����Ҫȥ�޸�����3��������
 *		_app_eeprom_wear_leveling_i2c_read
 *		_app_eeprom_wear_leveling_i2c_write
 *		_app_eeprom_wear_leveling_i2c_delay
 *	ʹ�õ�ʱ������ִ������2��������
 *		app_eeprom_wear_leveling_init(���ڳ�ʼ����)
 *		app_eeprom_wear_leveling(������ѭ����)
 *		
 *
 *
 *													 By Czl 2023��8��11��
 */
#include "includes.h"

/*
 *  ���������궨����Ҫ����ʵ������޸ĵ�:
 *	EEPROM_WEAR_LEVELING_START_ADDR
 *	EEPROM_WEAR_LEVELING_SPACE
 *	EEPROM_WEAR_LEVELING_OPERATION_SIZE
 */
/*���屣����ʼ��ַ*/
#define EEPROM_WEAR_LEVELING_START_ADDR 0x0000
/*
 *ĥ��ƽ��ʹ�õĿռ䣬����ʹ�õĵ�ַ�� EEPROM_WEAR_LEVELING_START_ADDR ~ (EEPROM_WEAR_LEVELING_SPACE-1)
 *ע��ռ��С����һ����ȡ���ֽ����ı���
 */
#define EEPROM_WEAR_LEVELING_SPACE 256

/*һ�ζ�д�����������ݿռ��С*/
#define EEPROM_WEAR_LEVELING_OPERATION_SIZE 64

/*���屣������֡ͷΪ0x91*/
#define EEPROM_WEAR_LEVELING_SAVE_HEAD 0x91

/*���������ṹ*/
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
		unsigned char exist; // �ϵ��ȡʱ�Ƿ��ȡ�ɹ���Ĭ����0����ȡ�ɹ���д��0xFF������ȡʧ�ܣ���������д��󣬻Ὣ��ֵд��0xFF
	} WEAR_LEVELING_INDEX;
	/*
	 *������eeprom�����ݵı����ʽ
	 *ʵ�ʻ������ݱ����У��ͣ������������ݳ��Ȳ��̶������Բ������ﶨ��
	 */
	typedef struct
	{
		unsigned char head;	 // ֡ͷ
		unsigned char index; // ����
		unsigned short adr;	 // ��ַ
		unsigned char len;	 // ���ݳ���,���ݱ�����+У��͵ĳ��ȣ�У���Ϊ1�ֽڣ�
	} WEAR_LEVELING_DATA_SAVE;
	/*���幦�ܽṹ��*/
	typedef struct
	{
		unsigned char *buf;				   // ���ݱ���ռ䣬��СΪһҳ���ֽ���
		unsigned char step;				   // ��ǰ����
		time_ms_T tout;					   // ��ʱʱ��
		unsigned short cur_read_adr;	   // ��ǰ��ȡ��ַ
		WEAR_LEVELING_DATA_SAVE save_data; // ��ȡ���������
		unsigned char index_num;		   // ��������
		//����ʹ��
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
