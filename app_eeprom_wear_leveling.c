#include "app_eeprom_wear_leveling.h"
#include "stdlib.h"

static int _is_all_ff(const void *buf, int len);
static unsigned char _checksum(unsigned char *buf, unsigned char len);
static void _app_eeprom_wear_leveling_i2c_read(unsigned char *buf, unsigned short adr, unsigned short len);
static void _app_eeprom_wear_leveling_i2c_write(unsigned char *buf, unsigned short adr, unsigned short len);
static void _app_eeprom_wear_leveling_i2c_delay(unsigned long us);

// ����
unsigned char save_n = 0, read_n = 0, erase_n = 0;

/**
 * @brief ���ļ�������ĥ��ƽ����������Ķ��塣
 * 
 * ĥ��ƽ�������������ڶ���Ӧ�ó�����ʹ�õĲ�ͬ������EEPROMĥ��ƽ�����������������ÿ�������������š�������ַ�ʹ�С��
 * ����²���ʱֻ��Ҫ�����������һ����¼���ɣ�����Ҫ�޸��������롣ע�������Ų����ظ���
 */
WEAR_LEVELING_INDEX wear_leveling_index[] = {
	CZL_INDEX(1, g_app_act.pow.epa_sum, sizeof(g_app_act.pow.epa_sum)),
	CZL_INDEX(2, g_app_log.OnOffTime, sizeof(g_app_log.OnOffTime)),
};
// ���幦�ܽṹ��
APP_EEPROM_WAER_LEVELING_T g_app_eeprom_wear_leveling;

void app_eeprom_wear_leveling_init(APP_EEPROM_WAER_LEVELING_T *p)
{
	// ����ռ䣨�ⲿ��Ҫ���ݹ����޸ģ�
	p->buf = (unsigned char *)DataPool_Get(EEPROM_WEAR_LEVELING_OPERATION_SIZE);
	memset(p->buf, 0, EEPROM_WEAR_LEVELING_OPERATION_SIZE);
	// ���õ�ǰ��ȡ��ַ
	p->cur_read_adr = EEPROM_WEAR_LEVELING_START_ADDR;
	// ���������б����м�����������
	p->index_num = sizeof(wear_leveling_index) / sizeof(WEAR_LEVELING_INDEX);
	// ���õȴ�ʱ��
	left_ms_set(&p->tout, 100);
	// ���ò���
	p->step = APP_EEPROM_WEAR_LEVELING_STEP_IDLE;
	// ����ʹ��
	p->index_1_fail = 0;
	p->index_2_fail = 0;
	memset((unsigned int *)&p->save_area, 0, 10);

	p->read_buf = (unsigned char *)DataPool_Get(EEPROM_WEAR_LEVELING_SPACE);
	memset(p->read_buf, 0, EEPROM_WEAR_LEVELING_SPACE);
}

void app_eeprom_wear_leveling(APP_EEPROM_WAER_LEVELING_T *p)
{
	switch (p->step)
	{
	case APP_EEPROM_WEAR_LEVELING_STEP_IDLE:
		// �տ���ʱ���ȴ�100ms�ٵ��ã���ֹӰ��©��
		if (left_ms(&p->tout) == 0)
		{
			p->step = APP_EEPROM_WEAR_LEVELING_STEP_READ;
		}
		break;

	case APP_EEPROM_WEAR_LEVELING_STEP_READ:
		// ��ȡ������eeprom�е�����
		app_eeprom_wear_leveling_read_all(p);
		break;

	case APP_EEPROM_WEAR_LEVELING_STEP_CHECK:
		// ��ѯ�����������鿴�Ƿ����ݶ���ȡ����ˡ�����û��ȡ�ģ������һ�����ʱ������Ĭ��������ӵ�eeprom��
		app_eeprom_wear_leveling_check(p);
		// ������ת����
		p->step = APP_EEPROM_WEAR_LEVELING_STEP_DOWN;
		break;

	case APP_EEPROM_WEAR_LEVELING_STEP_DOWN: // 3
		// �ϵ��ȡ�����ݺ󣬱�����������
		left_ms_set(&p->tout, 100);
		p->step = APP_EEPROM_WEAR_LEVELING_STEP_SAVE;
		break;

	case APP_EEPROM_WEAR_LEVELING_STEP_SAVE: // 4
		// ��������
		if (left_ms(&p->tout) == 0)
		{
			app_eeprom_wear_leveling_save(1, wear_leveling_index[0].buf);
			_app_eeprom_wear_leveling_i2c_read(p->read_buf, 0, EEPROM_WEAR_LEVELING_SPACE);
			app_eeprom_wear_leveling_save(2, wear_leveling_index[1].buf);
			_app_eeprom_wear_leveling_i2c_read(p->read_buf, 0, EEPROM_WEAR_LEVELING_SPACE);
			p->step = APP_EEPROM_WEAR_LEVELING_STEP_ERASE_MONITOR;
		}
		break;

	case APP_EEPROM_WEAR_LEVELING_STEP_ERASE_MONITOR: // 8
		// ��������
		if (app_eeprom_wear_leveling_read(1))
		{
			p->save_area[wear_leveling_index[0].adr / EEPROM_WEAR_LEVELING_OPERATION_SIZE]++;
		}
		if (app_eeprom_wear_leveling_read(2))
		{
			p->save_area[wear_leveling_index[1].adr / EEPROM_WEAR_LEVELING_OPERATION_SIZE]++;
		}
		// ��������
		HR[0] = p->save_area[0] / 65535;
		HR[1] = p->save_area[0] % 65535;
		HR[2] = p->save_area[1] / 65535;
		HR[3] = p->save_area[1] % 65535;
		HR[4] = p->save_area[2] / 65535;
		HR[5] = p->save_area[2] % 65535;
		HR[6] = p->save_area[3] / 65535;
		HR[7] = p->save_area[3] % 65535;
		HR[8] = p->save_area[4] / 65535;
		HR[9] = p->save_area[4] % 65535;
		p->step = APP_EEPROM_WEAR_LEVELING_STEP_DOWN;
		break;

	case APP_EEPROM_WEAR_LEVELING_STEP_SREAD: // 5
		// ��ȡ����
		// app_eeprom_wear_leveling_read(read_n);
		_app_eeprom_wear_leveling_i2c_read(p->buf, 256, EEPROM_WEAR_LEVELING_OPERATION_SIZE);
		while (1)
			;
		break;

	case APP_EEPROM_WEAR_LEVELING_STEP_ERASE: // 6
											  // ��������
		//	 	app_eeprom_wear_leveling_erase(erase_n);
		app_eeprom_wear_leveling_erase_adr(0, 256);
		p->step = APP_EEPROM_WEAR_LEVELING_STEP_DOWN;
		break;

	case APP_EEPROM_WEAR_LEVELING_STEP_ERASE_ALl: // 7
		app_eeprom_wear_leveling_erase_adr(0, EEPROM_WEAR_LEVELING_SPACE);
		break;

	default:
		break;
	}
}

/*
 * �ж��Ƿ�ȫΪ0xFF
 * buf: ����
 * len: ���ݳ���
 * return: 0:��ȫΪ0xFF 1:ȫΪ0xFF
 */
int _is_all_ff(const void *buf, int len)
{
	const unsigned char *p = buf;
	unsigned short i = 0;
	for (i = 0; i < len; i++)
	{
		if (p[i] != 0xFF)
		{
			return 0;
		}
	}
	return 1;
}

/*
 * ��ȡ��������
 * �ϵ��տ�ʼ����һ�ξ����ˡ����ڶ�ȡ������eeprom�е����ݡ�ͬʱ�����ݷŵ��������С�
 */
void app_eeprom_wear_leveling_read_all(APP_EEPROM_WAER_LEVELING_T *p)
{
	unsigned short i = 0, j = 0, n = 0, is_all_ff_sta = 0;
	unsigned int crc = 0;
	// ��ȡ����
	_app_eeprom_wear_leveling_i2c_read(p->buf, p->cur_read_adr, EEPROM_WEAR_LEVELING_OPERATION_SIZE);
	// �ж��Ƿ�������
	if (_is_all_ff(p->buf, EEPROM_WEAR_LEVELING_OPERATION_SIZE) != 1)
	{
		for (i = 0; i < EEPROM_WEAR_LEVELING_OPERATION_SIZE;) // ��������ȡ�ռ���Ѱ��֡ͷ
		{
			if (p->buf[i] == EEPROM_WEAR_LEVELING_SAVE_HEAD) // �ҵ���֡ͷ
			{
				// ���ñ�־�������ҵ���֡ͷ
				is_all_ff_sta = 1;
				// ��ȡ��ǰ�������ݵ�������Ϣ
				p->save_data.head = p->buf[i];
				p->save_data.index = p->buf[i + 1];
				p->save_data.adr = p->buf[i + 2] * 256 + p->buf[i + 3];
				p->save_data.len = p->buf[i + 4];
				// �ο���������֤�Ƿ�����Ч����
				for (n = 0; n < p->index_num;)
				{
					// �ҵ�������
					if (p->save_data.index == wear_leveling_index[n].index)
					{
						// ��֤����У���
						crc = 0;
						for (j = 0; j < (p->save_data.len + 4); j++)
						{
							crc += p->buf[i + j];
						}
						crc %= 256;
						if (crc == p->buf[i + p->save_data.len + 4])
						{
							// У�����ȷ����ȡ����
							for (j = 0; j < p->index_num; j++)
							{
								if (wear_leveling_index[j].index == p->save_data.index)
								{
									// ��ȡ����
									memcpy(wear_leveling_index[j].buf, &p->buf[i + 5], p->save_data.len - 1);
									// ����ǰ�����ַ�ŵ���������
									wear_leveling_index[j].adr = p->save_data.adr;
									// ��Ǹ����ݶ�ȡ�ɹ�
									wear_leveling_index[j].exist = 0xff;
								}
							}
						}
						else
						{
							// У��ʹ��󣬲����ò�������
							app_eeprom_wear_leveling_erase_adr(p->cur_read_adr + i, p->save_data.len + 5);
						}
						// ������ǰforѭ��
						break;
					}
					// �ж��Ƿ�������û�и�����
					if (++n == p->index_num)
					{
						// ��������û�е�ǰ���ݵļ�¼������������¼
						app_eeprom_wear_leveling_erase_adr(p->cur_read_adr + i, p->save_data.len + 5);
					}
				}
				// �޸�i��ֵ������һ�β������ݴӱ�����¼���濪ʼ
				i += (p->save_data.len + 5);
			}
			else
			{
				i++;
			}
		}
		if (is_all_ff_sta == 0)
		{
			// ��������˵����ȡ���������ݵ���û��֡ͷ��˵�������������⣬����������
			app_eeprom_wear_leveling_erase_adr(p->cur_read_adr, EEPROM_WEAR_LEVELING_OPERATION_SIZE);
		}
	}
	// ׼����ȡ��һ������
	p->cur_read_adr += EEPROM_WEAR_LEVELING_OPERATION_SIZE;
	// �жϱ��������Ƿ��ȡ���
	if (p->cur_read_adr >= EEPROM_WEAR_LEVELING_SPACE)
	{
		// ���ö�ȡ��ַ,������汣��ʹ��
		p->cur_read_adr = 0;
		// ��ת����һ��
		p->step = APP_EEPROM_WEAR_LEVELING_STEP_CHECK;
	}
}

/*
 * �������
 * ��ѯ�����������鿴�Ƿ����ݶ���ȡ����ˡ�����û��ȡ�ģ������һ�����ʱ������Ĭ��������ӵ�eeprom��
 */
void app_eeprom_wear_leveling_check(APP_EEPROM_WAER_LEVELING_T *p)
{
	unsigned short i = 0;
	for (i = 0; i < p->index_num; i++)
	{
		// �ж��Ƿ�������û�ж�ȡ
		if (wear_leveling_index[i].exist != 0xff)
		{
			// û�ж�ȡ����������ӵ�eeprom��
			app_eeprom_wear_leveling_save(wear_leveling_index[i].index, wear_leveling_index[i].buf);
			_app_eeprom_wear_leveling_i2c_delay(5000);
		}
	}
}

/*
 * ��������
 * num: �������еı��
 * buf: ����
 * return: 1: ����ɹ� 0: ����ʧ��
 */
unsigned char app_eeprom_wear_leveling_save(unsigned char num, unsigned char *buf)
{
	WEAR_LEVELING_INDEX *sf = NULL;
	unsigned short len = 0, i = 0, j = 0, k = 0, is_ff = 0;
	unsigned short adr_last = 0;

	// �����������ҵ���Ӧ������
	for (i = 0; i < g_app_eeprom_wear_leveling.index_num; i++)
	{
		if (wear_leveling_index[i].index == num)
		{
			sf = &wear_leveling_index[i];
			// ����i������Ƿ��ҵ�������
			i = 0xff;
			break;
		}
	}
	if (i != 0xff)
	{
		// û���ҵ�����
		return 0;
	}
	// ��д�����ݵĳ��Ƚ����жϣ�����һ����д�鳤�ȵ����ݲ��豣��
	if (sf->len > EEPROM_WEAR_LEVELING_OPERATION_SIZE - 6)
	{
		return 0;
	}
	// �ж��Ƿ��ǵ�һ���������
	if (sf->adr == 0xffffffff)
	{
		// ���㱣��ļ�¼�ܳ���
		len = sf->len + 6;
		// �ռ����������
		is_ff = 0;
		// ����ѭ��׼��
		for (j = 0; j < (EEPROM_WEAR_LEVELING_SPACE / EEPROM_WEAR_LEVELING_OPERATION_SIZE); j++)
		{
			// ��ȡ����
			_app_eeprom_wear_leveling_i2c_read(g_app_eeprom_wear_leveling.buf, g_app_eeprom_wear_leveling.cur_read_adr, EEPROM_WEAR_LEVELING_OPERATION_SIZE);
			// �ж��Ƿ����㹻�Ŀռ䱣������
			for (i = 0; i < EEPROM_WEAR_LEVELING_OPERATION_SIZE; i++)
			{
				if (g_app_eeprom_wear_leveling.buf[i] == 0xff)
				{
					is_ff++;
				}
				else
				{
					is_ff = 0;
				}
				if (is_ff == (len + 1))
				{
					// �ҵ����㹻�Ŀռ䣬����ѭ��
					break;
				}
			}
			if (is_ff == (len + 1))
			{
				// �ҵ����㹻�Ŀռ䣬����ѭ��
				break;
			}
			// δ�ҵ��㹻�Ŀռ䣬׼����ȡ��һ������
			is_ff = 0;//���Ҫ���ÿռ��������Ϊ��һ������ʣ�²����ŵĲ������ǾͲ�Ҫ�ˣ���һ������Ҫ��ͷ��ʼ����
			g_app_eeprom_wear_leveling.cur_read_adr = j * EEPROM_WEAR_LEVELING_OPERATION_SIZE + EEPROM_WEAR_LEVELING_START_ADDR;
			if (g_app_eeprom_wear_leveling.cur_read_adr >= EEPROM_WEAR_LEVELING_SPACE)
			{
				// ��������˵��û���㹻�Ŀռ䱣�����ݣ����ö�ȡ��ַ
				g_app_eeprom_wear_leveling.cur_read_adr = 0;
			}
		}
		// �ж��Ƿ��ҵ����㹻�Ŀռ�
		if (is_ff == (len + 1))
		{
			// �ҵ����㹻�Ŀռ䣬�ȼ�¼��ǰ��ַ
			sf->adr = g_app_eeprom_wear_leveling.cur_read_adr + i + 1 - len;
			// ��������
			g_app_eeprom_wear_leveling.buf[0] = 0x91;																  // ֡ͷ
			g_app_eeprom_wear_leveling.buf[1] = sf->index;															  // ����
			g_app_eeprom_wear_leveling.buf[2] = sf->adr / 256;														  // �����ַ�߰�λ
			g_app_eeprom_wear_leveling.buf[3] = sf->adr % 256;														  // �����ַ�Ͱ�λ
			g_app_eeprom_wear_leveling.buf[4] = sf->len + 1;														  // ���ݳ���+һ��У��λ����
			memcpy(&g_app_eeprom_wear_leveling.buf[5], sf->buf, sf->len);											  // ����
			g_app_eeprom_wear_leveling.buf[5 + sf->len] = _checksum(&g_app_eeprom_wear_leveling.buf[0], 5 + sf->len); // У��λ
			// д��
			_app_eeprom_wear_leveling_i2c_write(g_app_eeprom_wear_leveling.buf, sf->adr, sf->len + 6);
			// ����ɹ�
			sf->exist = 0xff;
		}
		else
		{
			// û���ҵ��㹻�Ŀռ䣬����ʧ��
			g_app_eeprom_wear_leveling.cur_read_adr = 0;
			return 0;
		}
	}
	else
	{ // �����Ǹ�������ʱ�����õ�
		// ���㱣��ļ�¼�ܳ���
		len = sf->len + 6;
		// �ռ����������
		is_ff = 0;
		// ��¼��ǰ�洢��ַ
		adr_last = sf->adr;
		// ���ݵ�ǰ�洢��ַ�ж�Ӧ�ö��ĸ��������
		if (((sf->adr / EEPROM_WEAR_LEVELING_OPERATION_SIZE + 1) * EEPROM_WEAR_LEVELING_OPERATION_SIZE - sf->adr) > (len * 2))
		{
			j = sf->adr / EEPROM_WEAR_LEVELING_OPERATION_SIZE;
			// ָ���õ�һ�β��ҿռ����ʼ��ַ
			i = (sf->adr + len) % EEPROM_WEAR_LEVELING_OPERATION_SIZE;
		}
		else
		{
			j = sf->adr / EEPROM_WEAR_LEVELING_OPERATION_SIZE + 1;
			if (j == (EEPROM_WEAR_LEVELING_SPACE / EEPROM_WEAR_LEVELING_OPERATION_SIZE))
			{
				j = 0;
			}
			// ָ���õ�һ�β��ҿռ����ʼ��ַ
			i = 0;
		}
		// ��ȡ���ݣ����ҿռ䣬����ѭ��׼��
		for (k = 0; k < (EEPROM_WEAR_LEVELING_SPACE / EEPROM_WEAR_LEVELING_OPERATION_SIZE); k++)
		{
			// ��ȡ����
			g_app_eeprom_wear_leveling.cur_read_adr = j * EEPROM_WEAR_LEVELING_OPERATION_SIZE + EEPROM_WEAR_LEVELING_START_ADDR;
			_app_eeprom_wear_leveling_i2c_read(g_app_eeprom_wear_leveling.buf, g_app_eeprom_wear_leveling.cur_read_adr, EEPROM_WEAR_LEVELING_OPERATION_SIZE);
			// �ж��Ƿ����㹻�Ŀռ䱣������
			for (; i < EEPROM_WEAR_LEVELING_OPERATION_SIZE; i++)
			{
				if (g_app_eeprom_wear_leveling.buf[i] == 0xff)
				{
					is_ff++;
				}
				else
				{
					is_ff = 0;
				}
				if (is_ff == (len + 1))
				{
					// �ҵ����㹻�Ŀռ䣬����ѭ��
					break;
				}
			}
			if (is_ff == (len + 1))
			{
				// �ҵ����㹻�Ŀռ䣬����ѭ��
				break;
			}
			// ��ǰ�洢��û�ռ��ˣ���һ�����ҵ�ʱ��Ҫ��ͷ��ʼ��
			i = 0;
			is_ff = 0; //���Ҫ���ÿռ��������Ϊ��һ������ʣ�²����ŵĲ������ǾͲ�Ҫ�ˣ���һ������Ҫ��ͷ��ʼ����
			// ȥѰ����һ��ռ�
			if (++j == (EEPROM_WEAR_LEVELING_SPACE / EEPROM_WEAR_LEVELING_OPERATION_SIZE))
			{
				j = 0;
			}
		}
		// �ж��Ƿ��ҵ����㹻�Ŀռ�
		if (is_ff == (len + 1))
		{
			// �ҵ����㹻�Ŀռ䣬�ȼ�¼��ǰ��ַ
			sf->adr = g_app_eeprom_wear_leveling.cur_read_adr + i + 1 - len;
			// ��������
			g_app_eeprom_wear_leveling.buf[0] = 0x91;																  // ֡ͷ
			g_app_eeprom_wear_leveling.buf[1] = sf->index;															  // ����
			g_app_eeprom_wear_leveling.buf[2] = sf->adr / 256;														  // �����ַ�߰�λ
			g_app_eeprom_wear_leveling.buf[3] = sf->adr % 256;														  // �����ַ�Ͱ�λ
			g_app_eeprom_wear_leveling.buf[4] = sf->len + 1;														  // ���ݳ���+һ��У��λ����
			memcpy(&g_app_eeprom_wear_leveling.buf[5], sf->buf, sf->len);											  // ����
			g_app_eeprom_wear_leveling.buf[5 + sf->len] = _checksum(&g_app_eeprom_wear_leveling.buf[0], 5 + sf->len); // У��λ
			// д��
			_app_eeprom_wear_leveling_i2c_write(g_app_eeprom_wear_leveling.buf, sf->adr, sf->len + 6);
			// ����ɹ�
			sf->exist = 0xff;
			// ��һ�ᣬ���ԭ�������򣬲���̫�죬�ᵼ����һ��д����ȥ
			_app_eeprom_wear_leveling_i2c_delay(5000);
			app_eeprom_wear_leveling_erase_adr(adr_last, sf->len + 6);
		}
		else
		{
			// û���ҵ��㹻�Ŀռ䣬����ʧ��
			g_app_eeprom_wear_leveling.cur_read_adr = 0;
			return 0;
		}
	}
	// ����ɹ�
	return 1;
}

/*
 * ��ȡ����
 * index: ����
 * buf: ����
 * ����ֵ��0-ʧ�ܣ�1-�ɹ�
 */
unsigned char app_eeprom_wear_leveling_read(unsigned char index)
{
	WEAR_LEVELING_INDEX *sf = NULL;
	unsigned short i = 0, checksum = 0;
	// ��������������������ж�ȡ����
	for (i = 0; i < g_app_eeprom_wear_leveling.index_num; i++)
	{
		if (wear_leveling_index[i].index == index)
		{
			sf = &wear_leveling_index[i];
			// ����i������Ƿ��ҵ�������
			i = 0xff;
			break;
		}
	}
	if (i != 0xff)
	{
		return 0;
	}
	// �Զ�ȡ���ݵĳ��Ƚ����жϣ�����һ����д�鳤�ȵ����ݲ��豣��
	if (sf->len > EEPROM_WEAR_LEVELING_OPERATION_SIZE - 6)
	{
		return 0;
	}
	// Ҫ��ȡ�����������������Ǵ��ڵģ���ȡ����
	_app_eeprom_wear_leveling_i2c_read(g_app_eeprom_wear_leveling.buf, sf->adr, sf->len + 6);
	g_app_eeprom_wear_leveling.save_data.head = g_app_eeprom_wear_leveling.buf[0];											// ֡ͷ
	g_app_eeprom_wear_leveling.save_data.index = g_app_eeprom_wear_leveling.buf[1];											// ����
	g_app_eeprom_wear_leveling.save_data.adr = g_app_eeprom_wear_leveling.buf[2] * 256 + g_app_eeprom_wear_leveling.buf[3]; // �����ַ
	g_app_eeprom_wear_leveling.save_data.len = g_app_eeprom_wear_leveling.buf[4];
	// ��֤�����Ƿ���ȷ
	if (g_app_eeprom_wear_leveling.save_data.head != 0x91)
		return 0;
	else if (g_app_eeprom_wear_leveling.save_data.index != index)
		return 0;
	else if (g_app_eeprom_wear_leveling.save_data.adr != sf->adr)
		return 0;
	else if (g_app_eeprom_wear_leveling.save_data.len != sf->len + 1)
		return 0;
	else
	{
		// ������ȷ������У���
		checksum = _checksum(&g_app_eeprom_wear_leveling.buf[0], sf->len + 5);
		if (checksum != g_app_eeprom_wear_leveling.buf[sf->len + 5])
			return 0;
		else
		{
			// У�����ȷ����ȡ����
			memcpy(sf->buf, &g_app_eeprom_wear_leveling.buf[5], sf->len);
		}
	}
	return 1;
}

/*
 * ��������
 * index: ����
 * ����ֵ��0-ʧ�ܣ�1-�ɹ�
 */
unsigned char app_eeprom_wear_leveling_erase(unsigned char index)
{
	WEAR_LEVELING_INDEX *sf = NULL;
	unsigned short i = 0;
	// ��������������������ж�ȡ����
	for (i = 0; i < g_app_eeprom_wear_leveling.index_num; i++)
	{
		if (wear_leveling_index[i].index == index)
		{
			sf = &wear_leveling_index[i];
			// ����i������Ƿ��ҵ�������
			i = 0xff;
			break;
		}
	}
	if (i != 0xff)
		return 0;
	else
	{
		// Ҫ��ȡ�����������������Ǵ��ڵ�
		// ��������
		app_eeprom_wear_leveling_erase_adr(sf->adr, sf->len + 6);
		// ����������
		sf->exist = 0;
		sf->adr = 0xffffffff;
	}
	return 1;
}

/*
 * �������ݣ����յ�ַ�ķ�ʽ
 * adr����ַ
 * len������
 */
void app_eeprom_wear_leveling_erase_adr(unsigned int adr, unsigned short len)
{
	unsigned char *buf = NULL;

	buf = (unsigned char *)malloc(len);
	memset(buf, 0xFF, len);
	// д��
	_app_eeprom_wear_leveling_i2c_write(buf, adr, len);

	free(buf);
	_app_eeprom_wear_leveling_i2c_delay(5000);
}

/*
 * ����У���
 * buf: ����
 * len: ����
 * return: У���
 */
unsigned char _checksum(unsigned char *buf, unsigned char len)
{
	unsigned short i = 0, sum = 0;

	for (i = 0; i < len; i++)
	{
		sum += buf[i];
	}

	return (sum % 256);
}

/*
 * i2c����-��ȡeeprom����
 * buf: ����
 * adr: ��ַ
 * len: ����
 */
void _app_eeprom_wear_leveling_i2c_read(unsigned char *buf, unsigned short adr, unsigned short len)
{
	/*����д�Լ���i2c��ȡ*/
	memset(buf, 0, len);
	si2c_ee_read(buf, adr, len);
}

/*
 * i2c����-д��eeprom����
 * buf: ����
 * adr: ��ַ
 * len: ����
 */
void _app_eeprom_wear_leveling_i2c_write(unsigned char *buf, unsigned short adr, unsigned short len)
{
	/*����д�Լ���i2cд��*/
	si2c_ee_wen();
	si2c_ee_write(buf, adr, len, SI2C_EE_NO_POLLING);
	si2c_ee_wdis();
}

/*
 * i2c����-��ʱ
 * us: ��ʱʱ�䣬��λus
 */
void _app_eeprom_wear_leveling_i2c_delay(unsigned long us)
{
	/*����д�Լ�����ʱ*/
	delay_us_set_k(us);
}
