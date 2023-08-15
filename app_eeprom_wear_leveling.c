#include "app_eeprom_wear_leveling.h"
#include "stdlib.h"

static int _is_all_ff(const void *buf, int len);
static unsigned char _checksum(unsigned char *buf, unsigned char len);
static void _app_eeprom_wear_leveling_i2c_read(unsigned char *buf, unsigned short adr, unsigned short len);
static void _app_eeprom_wear_leveling_i2c_write(unsigned char *buf, unsigned short adr, unsigned short len);
static void _app_eeprom_wear_leveling_i2c_delay(unsigned long us);

// 测试
unsigned char save_n = 0, read_n = 0, erase_n = 0;

/**
 * @brief 该文件包含了磨损平衡索引数组的定义。
 * 
 * 磨损平衡索引数组用于定义应用程序中使用的不同变量的EEPROM磨损平衡索引。该数组包含每个变量的索引号、变量地址和大小。
 * 添加新参数时只需要在数组中添加一条记录即可，不需要修改其他代码。注意索引号不可重复。
 */
WEAR_LEVELING_INDEX wear_leveling_index[] = {
	CZL_INDEX(1, g_app_act.pow.epa_sum, sizeof(g_app_act.pow.epa_sum)),
	CZL_INDEX(2, g_app_log.OnOffTime, sizeof(g_app_log.OnOffTime)),
};
// 定义功能结构体
APP_EEPROM_WAER_LEVELING_T g_app_eeprom_wear_leveling;

void app_eeprom_wear_leveling_init(APP_EEPROM_WAER_LEVELING_T *p)
{
	// 申请空间（这部分要根据工程修改）
	p->buf = (unsigned char *)DataPool_Get(EEPROM_WEAR_LEVELING_OPERATION_SIZE);
	memset(p->buf, 0, EEPROM_WEAR_LEVELING_OPERATION_SIZE);
	// 设置当前读取地址
	p->cur_read_adr = EEPROM_WEAR_LEVELING_START_ADDR;
	// 计算索引列表里有几条索引数据
	p->index_num = sizeof(wear_leveling_index) / sizeof(WEAR_LEVELING_INDEX);
	// 设置等待时间
	left_ms_set(&p->tout, 100);
	// 设置步骤
	p->step = APP_EEPROM_WEAR_LEVELING_STEP_IDLE;
	// 测试使用
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
		// 刚开机时，等待100ms再调用，防止影响漏电
		if (left_ms(&p->tout) == 0)
		{
			p->step = APP_EEPROM_WEAR_LEVELING_STEP_READ;
		}
		break;

	case APP_EEPROM_WEAR_LEVELING_STEP_READ:
		// 读取保存在eeprom中的数据
		app_eeprom_wear_leveling_read_all(p);
		break;

	case APP_EEPROM_WEAR_LEVELING_STEP_CHECK:
		// 轮询整个索引表，查看是否数据都读取完毕了。若有没读取的（比如第一次添加时），则将默认数据添加到eeprom中
		app_eeprom_wear_leveling_check(p);
		// 设置跳转步骤
		p->step = APP_EEPROM_WEAR_LEVELING_STEP_DOWN;
		break;

	case APP_EEPROM_WEAR_LEVELING_STEP_DOWN: // 3
		// 上电读取完数据后，本程序不再运行
		left_ms_set(&p->tout, 100);
		p->step = APP_EEPROM_WEAR_LEVELING_STEP_SAVE;
		break;

	case APP_EEPROM_WEAR_LEVELING_STEP_SAVE: // 4
		// 保存数据
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
		// 监视数据
		if (app_eeprom_wear_leveling_read(1))
		{
			p->save_area[wear_leveling_index[0].adr / EEPROM_WEAR_LEVELING_OPERATION_SIZE]++;
		}
		if (app_eeprom_wear_leveling_read(2))
		{
			p->save_area[wear_leveling_index[1].adr / EEPROM_WEAR_LEVELING_OPERATION_SIZE]++;
		}
		// 保存数据
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
		// 读取数据
		// app_eeprom_wear_leveling_read(read_n);
		_app_eeprom_wear_leveling_i2c_read(p->buf, 256, EEPROM_WEAR_LEVELING_OPERATION_SIZE);
		while (1)
			;
		break;

	case APP_EEPROM_WEAR_LEVELING_STEP_ERASE: // 6
											  // 擦除数据
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
 * 判断是否全为0xFF
 * buf: 数据
 * len: 数据长度
 * return: 0:不全为0xFF 1:全为0xFF
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
 * 读取所有数据
 * 上电后刚开始调用一次就行了。用于读取保存在eeprom中的数据。同时将数据放到索引表中。
 */
void app_eeprom_wear_leveling_read_all(APP_EEPROM_WAER_LEVELING_T *p)
{
	unsigned short i = 0, j = 0, n = 0, is_all_ff_sta = 0;
	unsigned int crc = 0;
	// 读取数据
	_app_eeprom_wear_leveling_i2c_read(p->buf, p->cur_read_adr, EEPROM_WEAR_LEVELING_OPERATION_SIZE);
	// 判断是否有数据
	if (_is_all_ff(p->buf, EEPROM_WEAR_LEVELING_OPERATION_SIZE) != 1)
	{
		for (i = 0; i < EEPROM_WEAR_LEVELING_OPERATION_SIZE;) // 在整个读取空间中寻找帧头
		{
			if (p->buf[i] == EEPROM_WEAR_LEVELING_SAVE_HEAD) // 找到了帧头
			{
				// 设置标志，表明找到了帧头
				is_all_ff_sta = 1;
				// 获取当前保存数据的索引信息
				p->save_data.head = p->buf[i];
				p->save_data.index = p->buf[i + 1];
				p->save_data.adr = p->buf[i + 2] * 256 + p->buf[i + 3];
				p->save_data.len = p->buf[i + 4];
				// 参考索引表，验证是否是有效数据
				for (n = 0; n < p->index_num;)
				{
					// 找到了数据
					if (p->save_data.index == wear_leveling_index[n].index)
					{
						// 验证数据校验和
						crc = 0;
						for (j = 0; j < (p->save_data.len + 4); j++)
						{
							crc += p->buf[i + j];
						}
						crc %= 256;
						if (crc == p->buf[i + p->save_data.len + 4])
						{
							// 校验和正确，读取数据
							for (j = 0; j < p->index_num; j++)
							{
								if (wear_leveling_index[j].index == p->save_data.index)
								{
									// 读取数据
									memcpy(wear_leveling_index[j].buf, &p->buf[i + 5], p->save_data.len - 1);
									// 将当前保存地址放到索引表内
									wear_leveling_index[j].adr = p->save_data.adr;
									// 标记该数据读取成功
									wear_leveling_index[j].exist = 0xff;
								}
							}
						}
						else
						{
							// 校验和错误，擦除该部分数据
							app_eeprom_wear_leveling_erase_adr(p->cur_read_adr + i, p->save_data.len + 5);
						}
						// 跳出当前for循环
						break;
					}
					// 判断是否索引里没有该数据
					if (++n == p->index_num)
					{
						// 索引表里没有当前数据的记录，擦除本条记录
						app_eeprom_wear_leveling_erase_adr(p->cur_read_adr + i, p->save_data.len + 5);
					}
				}
				// 修改i的值，让下一次查找数据从本条记录后面开始
				i += (p->save_data.len + 5);
			}
			else
			{
				i++;
			}
		}
		if (is_all_ff_sta == 0)
		{
			// 进到这里说明读取区域有数据但是没有帧头，说明该区域有问题，擦除该区域
			app_eeprom_wear_leveling_erase_adr(p->cur_read_adr, EEPROM_WEAR_LEVELING_OPERATION_SIZE);
		}
	}
	// 准备读取下一块数据
	p->cur_read_adr += EEPROM_WEAR_LEVELING_OPERATION_SIZE;
	// 判断保存区域是否读取完毕
	if (p->cur_read_adr >= EEPROM_WEAR_LEVELING_SPACE)
	{
		// 重置读取地址,方便后面保存使用
		p->cur_read_adr = 0;
		// 跳转到下一步
		p->step = APP_EEPROM_WEAR_LEVELING_STEP_CHECK;
	}
}

/*
 * 检查数据
 * 轮询整个索引表，查看是否数据都读取完毕了。若有没读取的（比如第一次添加时），则将默认数据添加到eeprom中
 */
void app_eeprom_wear_leveling_check(APP_EEPROM_WAER_LEVELING_T *p)
{
	unsigned short i = 0;
	for (i = 0; i < p->index_num; i++)
	{
		// 判断是否有数据没有读取
		if (wear_leveling_index[i].exist != 0xff)
		{
			// 没有读取，将数据添加到eeprom中
			app_eeprom_wear_leveling_save(wear_leveling_index[i].index, wear_leveling_index[i].buf);
			_app_eeprom_wear_leveling_i2c_delay(5000);
		}
	}
}

/*
 * 保存数据
 * num: 索引表中的编号
 * buf: 数据
 * return: 1: 保存成功 0: 保存失败
 */
unsigned char app_eeprom_wear_leveling_save(unsigned char num, unsigned char *buf)
{
	WEAR_LEVELING_INDEX *sf = NULL;
	unsigned short len = 0, i = 0, j = 0, k = 0, is_ff = 0;
	unsigned short adr_last = 0;

	// 查找索引表，找到对应的数据
	for (i = 0; i < g_app_eeprom_wear_leveling.index_num; i++)
	{
		if (wear_leveling_index[i].index == num)
		{
			sf = &wear_leveling_index[i];
			// 借用i来标记是否找到了数据
			i = 0xff;
			break;
		}
	}
	if (i != 0xff)
	{
		// 没有找到数据
		return 0;
	}
	// 对写入数据的长度进行判断，超过一个读写块长度的数据不予保存
	if (sf->len > EEPROM_WEAR_LEVELING_OPERATION_SIZE - 6)
	{
		return 0;
	}
	// 判断是否是第一次添加数据
	if (sf->adr == 0xffffffff)
	{
		// 计算保存的记录总长度
		len = sf->len + 6;
		// 空间计数器清零
		is_ff = 0;
		// 做好循环准备
		for (j = 0; j < (EEPROM_WEAR_LEVELING_SPACE / EEPROM_WEAR_LEVELING_OPERATION_SIZE); j++)
		{
			// 读取数据
			_app_eeprom_wear_leveling_i2c_read(g_app_eeprom_wear_leveling.buf, g_app_eeprom_wear_leveling.cur_read_adr, EEPROM_WEAR_LEVELING_OPERATION_SIZE);
			// 判断是否有足够的空间保存数据
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
					// 找到了足够的空间，跳出循环
					break;
				}
			}
			if (is_ff == (len + 1))
			{
				// 找到了足够的空间，跳出循环
				break;
			}
			// 未找到足够的空间，准备读取下一块数据
			is_ff = 0;//务必要重置空间计数。因为这一块区域剩下不够放的部分我们就不要了，下一块区域要从头开始计数
			g_app_eeprom_wear_leveling.cur_read_adr = j * EEPROM_WEAR_LEVELING_OPERATION_SIZE + EEPROM_WEAR_LEVELING_START_ADDR;
			if (g_app_eeprom_wear_leveling.cur_read_adr >= EEPROM_WEAR_LEVELING_SPACE)
			{
				// 进到这里说明没有足够的空间保存数据，重置读取地址
				g_app_eeprom_wear_leveling.cur_read_adr = 0;
			}
		}
		// 判断是否找到了足够的空间
		if (is_ff == (len + 1))
		{
			// 找到了足够的空间，先记录当前地址
			sf->adr = g_app_eeprom_wear_leveling.cur_read_adr + i + 1 - len;
			// 保存数据
			g_app_eeprom_wear_leveling.buf[0] = 0x91;																  // 帧头
			g_app_eeprom_wear_leveling.buf[1] = sf->index;															  // 索引
			g_app_eeprom_wear_leveling.buf[2] = sf->adr / 256;														  // 保存地址高八位
			g_app_eeprom_wear_leveling.buf[3] = sf->adr % 256;														  // 保存地址低八位
			g_app_eeprom_wear_leveling.buf[4] = sf->len + 1;														  // 数据长度+一个校验位长度
			memcpy(&g_app_eeprom_wear_leveling.buf[5], sf->buf, sf->len);											  // 数据
			g_app_eeprom_wear_leveling.buf[5 + sf->len] = _checksum(&g_app_eeprom_wear_leveling.buf[0], 5 + sf->len); // 校验位
			// 写入
			_app_eeprom_wear_leveling_i2c_write(g_app_eeprom_wear_leveling.buf, sf->adr, sf->len + 6);
			// 保存成功
			sf->exist = 0xff;
		}
		else
		{
			// 没有找到足够的空间，保存失败
			g_app_eeprom_wear_leveling.cur_read_adr = 0;
			return 0;
		}
	}
	else
	{ // 这里是更新数据时保存用的
		// 计算保存的记录总长度
		len = sf->len + 6;
		// 空间计数器清零
		is_ff = 0;
		// 记录当前存储地址
		adr_last = sf->adr;
		// 根据当前存储地址判断应该读哪个块的数据
		if (((sf->adr / EEPROM_WEAR_LEVELING_OPERATION_SIZE + 1) * EEPROM_WEAR_LEVELING_OPERATION_SIZE - sf->adr) > (len * 2))
		{
			j = sf->adr / EEPROM_WEAR_LEVELING_OPERATION_SIZE;
			// 指定好第一次查找空间的起始地址
			i = (sf->adr + len) % EEPROM_WEAR_LEVELING_OPERATION_SIZE;
		}
		else
		{
			j = sf->adr / EEPROM_WEAR_LEVELING_OPERATION_SIZE + 1;
			if (j == (EEPROM_WEAR_LEVELING_SPACE / EEPROM_WEAR_LEVELING_OPERATION_SIZE))
			{
				j = 0;
			}
			// 指定好第一次查找空间的起始地址
			i = 0;
		}
		// 读取数据，查找空间，做好循环准备
		for (k = 0; k < (EEPROM_WEAR_LEVELING_SPACE / EEPROM_WEAR_LEVELING_OPERATION_SIZE); k++)
		{
			// 读取数据
			g_app_eeprom_wear_leveling.cur_read_adr = j * EEPROM_WEAR_LEVELING_OPERATION_SIZE + EEPROM_WEAR_LEVELING_START_ADDR;
			_app_eeprom_wear_leveling_i2c_read(g_app_eeprom_wear_leveling.buf, g_app_eeprom_wear_leveling.cur_read_adr, EEPROM_WEAR_LEVELING_OPERATION_SIZE);
			// 判断是否有足够的空间保存数据
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
					// 找到了足够的空间，跳出循环
					break;
				}
			}
			if (is_ff == (len + 1))
			{
				// 找到了足够的空间，跳出循环
				break;
			}
			// 当前存储块没空间了，下一个块找的时候要从头开始找
			i = 0;
			is_ff = 0; //务必要重置空间计数。因为这一块区域剩下不够放的部分我们就不要了，下一块区域要从头开始计数
			// 去寻找下一块空间
			if (++j == (EEPROM_WEAR_LEVELING_SPACE / EEPROM_WEAR_LEVELING_OPERATION_SIZE))
			{
				j = 0;
			}
		}
		// 判断是否找到了足够的空间
		if (is_ff == (len + 1))
		{
			// 找到了足够的空间，先记录当前地址
			sf->adr = g_app_eeprom_wear_leveling.cur_read_adr + i + 1 - len;
			// 保存数据
			g_app_eeprom_wear_leveling.buf[0] = 0x91;																  // 帧头
			g_app_eeprom_wear_leveling.buf[1] = sf->index;															  // 索引
			g_app_eeprom_wear_leveling.buf[2] = sf->adr / 256;														  // 保存地址高八位
			g_app_eeprom_wear_leveling.buf[3] = sf->adr % 256;														  // 保存地址低八位
			g_app_eeprom_wear_leveling.buf[4] = sf->len + 1;														  // 数据长度+一个校验位长度
			memcpy(&g_app_eeprom_wear_leveling.buf[5], sf->buf, sf->len);											  // 数据
			g_app_eeprom_wear_leveling.buf[5 + sf->len] = _checksum(&g_app_eeprom_wear_leveling.buf[0], 5 + sf->len); // 校验位
			// 写入
			_app_eeprom_wear_leveling_i2c_write(g_app_eeprom_wear_leveling.buf, sf->adr, sf->len + 6);
			// 保存成功
			sf->exist = 0xff;
			// 等一会，清除原保存区域，不能太快，会导致这一块写不进去
			_app_eeprom_wear_leveling_i2c_delay(5000);
			app_eeprom_wear_leveling_erase_adr(adr_last, sf->len + 6);
		}
		else
		{
			// 没有找到足够的空间，保存失败
			g_app_eeprom_wear_leveling.cur_read_adr = 0;
			return 0;
		}
	}
	// 保存成功
	return 1;
}

/*
 * 读取数据
 * index: 索引
 * buf: 数据
 * 返回值：0-失败，1-成功
 */
unsigned char app_eeprom_wear_leveling_read(unsigned char index)
{
	WEAR_LEVELING_INDEX *sf = NULL;
	unsigned short i = 0, checksum = 0;
	// 根据索引编号在索引表中读取数据
	for (i = 0; i < g_app_eeprom_wear_leveling.index_num; i++)
	{
		if (wear_leveling_index[i].index == index)
		{
			sf = &wear_leveling_index[i];
			// 借用i来标记是否找到了数据
			i = 0xff;
			break;
		}
	}
	if (i != 0xff)
	{
		return 0;
	}
	// 对读取数据的长度进行判断，超过一个读写块长度的数据不予保存
	if (sf->len > EEPROM_WEAR_LEVELING_OPERATION_SIZE - 6)
	{
		return 0;
	}
	// 要读取的数据在索引表里是存在的，读取数据
	_app_eeprom_wear_leveling_i2c_read(g_app_eeprom_wear_leveling.buf, sf->adr, sf->len + 6);
	g_app_eeprom_wear_leveling.save_data.head = g_app_eeprom_wear_leveling.buf[0];											// 帧头
	g_app_eeprom_wear_leveling.save_data.index = g_app_eeprom_wear_leveling.buf[1];											// 索引
	g_app_eeprom_wear_leveling.save_data.adr = g_app_eeprom_wear_leveling.buf[2] * 256 + g_app_eeprom_wear_leveling.buf[3]; // 保存地址
	g_app_eeprom_wear_leveling.save_data.len = g_app_eeprom_wear_leveling.buf[4];
	// 验证数据是否正确
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
		// 数据正确，计算校验和
		checksum = _checksum(&g_app_eeprom_wear_leveling.buf[0], sf->len + 5);
		if (checksum != g_app_eeprom_wear_leveling.buf[sf->len + 5])
			return 0;
		else
		{
			// 校验和正确，读取数据
			memcpy(sf->buf, &g_app_eeprom_wear_leveling.buf[5], sf->len);
		}
	}
	return 1;
}

/*
 * 擦除数据
 * index: 索引
 * 返回值：0-失败，1-成功
 */
unsigned char app_eeprom_wear_leveling_erase(unsigned char index)
{
	WEAR_LEVELING_INDEX *sf = NULL;
	unsigned short i = 0;
	// 根据索引编号在索引表中读取数据
	for (i = 0; i < g_app_eeprom_wear_leveling.index_num; i++)
	{
		if (wear_leveling_index[i].index == index)
		{
			sf = &wear_leveling_index[i];
			// 借用i来标记是否找到了数据
			i = 0xff;
			break;
		}
	}
	if (i != 0xff)
		return 0;
	else
	{
		// 要读取的数据在索引表里是存在的
		// 擦除数据
		app_eeprom_wear_leveling_erase_adr(sf->adr, sf->len + 6);
		// 擦除索引表
		sf->exist = 0;
		sf->adr = 0xffffffff;
	}
	return 1;
}

/*
 * 擦除数据，按照地址的方式
 * adr：地址
 * len：长度
 */
void app_eeprom_wear_leveling_erase_adr(unsigned int adr, unsigned short len)
{
	unsigned char *buf = NULL;

	buf = (unsigned char *)malloc(len);
	memset(buf, 0xFF, len);
	// 写入
	_app_eeprom_wear_leveling_i2c_write(buf, adr, len);

	free(buf);
	_app_eeprom_wear_leveling_i2c_delay(5000);
}

/*
 * 计算校验和
 * buf: 数据
 * len: 长度
 * return: 校验和
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
 * i2c驱动-读取eeprom数据
 * buf: 数据
 * adr: 地址
 * len: 长度
 */
void _app_eeprom_wear_leveling_i2c_read(unsigned char *buf, unsigned short adr, unsigned short len)
{
	/*这里写自己的i2c读取*/
	memset(buf, 0, len);
	si2c_ee_read(buf, adr, len);
}

/*
 * i2c驱动-写入eeprom数据
 * buf: 数据
 * adr: 地址
 * len: 长度
 */
void _app_eeprom_wear_leveling_i2c_write(unsigned char *buf, unsigned short adr, unsigned short len)
{
	/*这里写自己的i2c写入*/
	si2c_ee_wen();
	si2c_ee_write(buf, adr, len, SI2C_EE_NO_POLLING);
	si2c_ee_wdis();
}

/*
 * i2c驱动-延时
 * us: 延时时间，单位us
 */
void _app_eeprom_wear_leveling_i2c_delay(unsigned long us)
{
	/*这里写自己的延时*/
	delay_us_set_k(us);
}
