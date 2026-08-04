// #include "ring_buffer.h"

// RingBufManager gRingBufMgr = {0};

// /************************ 内部成员函数实现 ************************/
// static bool RingBuf_IsFull(void *self)
// {
//     RingBufferObj *obj = (RingBufferObj *)self;
//     return ((obj->writePtr + 1) % obj->bufLen) == obj->readPtr;
// }

// static bool RingBuf_IsEmpty(void *self)
// {
//     RingBufferObj *obj = (RingBufferObj *)self;
//     return obj->writePtr == obj->readPtr;
// }

// static uint16_t RingBuf_GetUsed(void *self)
// {
//     RingBufferObj *obj = (RingBufferObj *)self;
//     if(obj->writePtr >= obj->readPtr)
//         return obj->writePtr - obj->readPtr;
//     else
//         return obj->bufLen - obj->readPtr + obj->writePtr;
// }

// static uint16_t RingBuf_GetRemain(void *self)
// {
//     RingBufferObj *obj = (RingBufferObj *)self;
//     return obj->bufLen - 1 - obj->getUsed(obj);
// }

// static void RingBuf_Clear(void *self)
// {
//     RingBufferObj *obj = (RingBufferObj *)self;
//     obj->readPtr = 0;
//     obj->writePtr = 0;
// }

// static void RingBuf_SetMode(void *self, RingWriteMode mode)
// {
//     RingBufferObj *obj = (RingBufferObj *)self;
//     obj->wrMode = mode;
// }

// /**
//  * @brief 写入数据，区分丢弃/覆盖两种模式
//  */
// static RingBufErrCode RingBuf_Write(void *self, uint8_t data)
// {
//     RingBufferObj *obj = (RingBufferObj *)self;

//     // 模式1：满则丢弃新数据，直接返回错误
//     if(obj->wrMode == RING_MODE_DISCARD)
//     {
//         if(obj->isFull(obj))
//             return RING_BUF_ERR_FULL;

//         obj->buf[obj->writePtr] = data;
//         obj->writePtr = (obj->writePtr + 1) % obj->bufLen;
//     }
//     // 模式2：满则覆盖最旧数据
//     else
//     {
//         if(obj->isFull(obj))
//         {
//             // 写指针前移一格，覆盖最早数据，读指针同步前进
//             obj->buf[obj->writePtr] = data;
//             obj->writePtr = (obj->writePtr + 1) % obj->bufLen;
//             obj->readPtr = (obj->readPtr + 1) % obj->bufLen;
//         }
//         else
//         {
//             obj->buf[obj->writePtr] = data;
//             obj->writePtr = (obj->writePtr + 1) % obj->bufLen;
//         }
//     }
//     return RING_BUF_OK;
// }

// static RingBufErrCode RingBuf_Read(void *self, uint8_t *pData)
// {
//     RingBufferObj *obj = (RingBufferObj *)self;
//     if(obj->isEmpty(obj))
//         return RING_BUF_ERR_EMPTY;

//     *pData = obj->buf[obj->readPtr];
//     obj->readPtr = (obj->readPtr + 1) % obj->bufLen;
//     return RING_BUF_OK;
// }

// /**
//  * @brief 遍历当前有效数据，计算总和（uint32防止溢出）
//  */
// static uint32_t RingBuf_GetSum(void *self)
// {
//     RingBufferObj *obj = (RingBufferObj *)self;
//     uint32_t sum = 0;
//     uint16_t ptr = obj->readPtr;
//     uint16_t usedCnt = obj->getUsed(obj);

//     for(uint16_t i = 0; i < usedCnt; i++)
//     {
//         sum += obj->buf[ptr];
//         ptr = (ptr + 1) % obj->bufLen;
//     }
//     return sum;
// }

// /************************ 全局管理API ************************/
// uint8_t RingBuf_CreateInstance(const char *name, uint8_t *buf, uint16_t bufLen, RingWriteMode initMode)
// {
//     if(gRingBufMgr.instCnt >= RING_BUF_MAX_INSTANCE_NUM)
//         return 0xFF;
//     if(buf == NULL || bufLen < 2 || name == NULL)
//         return 0xFF;

//     // 寻找空闲实例槽
//     uint8_t id = 0;
//     for(; id < RING_BUF_MAX_INSTANCE_NUM; id++)
//     {
//         if(gRingBufMgr.inst[id].isUsed == false)
//             break;
//     }

//     RingBufferObj *obj = &gRingBufMgr.inst[id];
//     obj->buf = buf;
//     obj->bufLen = bufLen;
//     obj->readPtr = 0;
//     obj->writePtr = 0;
//     obj->wrMode = initMode;
//     strncpy(obj->name, name, RING_BUF_NAME_LEN_MAX - 1);
//     obj->name[RING_BUF_NAME_LEN_MAX - 1] = '\0';
//     obj->isUsed = true;

//     // 绑定全部成员函数
//     obj->write     = RingBuf_Write;
//     obj->read      = RingBuf_Read;
//     obj->clear     = RingBuf_Clear;
//     obj->isFull    = RingBuf_IsFull;
//     obj->isEmpty   = RingBuf_IsEmpty;
//     obj->getUsed   = RingBuf_GetUsed;
//     obj->getRemain = RingBuf_GetRemain;
//     obj->getSum    = RingBuf_GetSum;
//     obj->setMode   = RingBuf_SetMode;

//     gRingBufMgr.instCnt++;
//     return id;
// }

// RingBufferObj* RingBuf_GetById(uint8_t id)
// {
//     if(id >= RING_BUF_MAX_INSTANCE_NUM)
//         return NULL;
//     if(gRingBufMgr.inst[id].isUsed == false)
//         return NULL;
//     return &gRingBufMgr.inst[id];
// }

// RingBufferObj* RingBuf_GetByName(const char *name)
// {
//     if(name == NULL)
//         return NULL;
//     for(uint8_t i = 0; i < RING_BUF_MAX_INSTANCE_NUM; i++)
//     {
//         if(gRingBufMgr.inst[i].isUsed && strcmp(gRingBufMgr.inst[i].name, name) == 0)
//             return &gRingBufMgr.inst[i];
//     }
//     return NULL;
// }

// RingBufErrCode RingBuf_DelInstance(uint8_t id)
// {
//     RingBufferObj *obj = RingBuf_GetById(id);
//     if(obj == NULL)
//         return RING_BUF_ERR_NO_INST;

//     memset(obj->name, 0, RING_BUF_NAME_LEN_MAX);
//     obj->buf = NULL;
//     obj->bufLen = 0;
//     obj->readPtr = 0;
//     obj->writePtr = 0;
//     obj->wrMode = RING_MODE_DISCARD;
//     obj->isUsed = false;
//     gRingBufMgr.instCnt--;
//     return RING_BUF_OK;
// }




#include "ring_buffer.h"

RingBufManager gRingBufMgr = {0};

/************************ 内部基础工具函数 ************************/
static bool RingBuf_IsFull(void *self)
{
    RingBufferObj *obj = (RingBufferObj *)self;
    return ((obj->writePtr + 1) % obj->elemCount) == obj->readPtr;
}

static bool RingBuf_IsEmpty(void *self)
{
    RingBufferObj *obj = (RingBufferObj *)self;
    return obj->writePtr == obj->readPtr;
}

static uint16_t RingBuf_GetUsed(void *self)
{
    RingBufferObj *obj = (RingBufferObj *)self;
    if (obj->writePtr >= obj->readPtr)
        return obj->writePtr - obj->readPtr;
    else
        return obj->elemCount - obj->readPtr + obj->writePtr;
}

static uint16_t RingBuf_GetRemain(void *self)
{
    RingBufferObj *obj = (RingBufferObj *)self;
    return obj->elemCount - 1 - obj->getUsed(obj);
}

static void RingBuf_Clear(void *self)
{
    RingBufferObj *obj = (RingBufferObj *)self;
    obj->readPtr = 0;
    obj->writePtr = 0;
}

static void RingBuf_SetMode(void *self, RingWriteMode mode)
{
    RingBufferObj *obj = (RingBufferObj *)self;
    obj->wrMode = mode;
}

/************************ 泛型读写（支持uint8 / float / 任意定长类型） ************************/
static RingBufErrCode RingBuf_Write(void *self, const void *pData)
{
    RingBufferObj *obj = (RingBufferObj *)self;
    if (pData == NULL)
        return RING_BUF_ERR_PARAM;

    // 模式1：满丢弃，直接返回错误
    if (obj->wrMode == RING_MODE_DISCARD)
    {
        if (obj->isFull(obj))
            return RING_BUF_ERR_FULL;
    }
    // 模式2：满覆盖，读指针前移丢弃最旧元素
    else
    {
        if (obj->isFull(obj))
        {
            obj->readPtr = (obj->readPtr + 1) % obj->elemCount;
        }
    }

    // 按单元素字节长度拷贝数据
    uint8_t *dst = (uint8_t *)obj->buf + obj->writePtr * obj->elemSize;
    memcpy(dst, pData, obj->elemSize);
    obj->writePtr = (obj->writePtr + 1) % obj->elemCount;
    return RING_BUF_OK;
}

static RingBufErrCode RingBuf_Read(void *self, void *pData)
{
    RingBufferObj *obj = (RingBufferObj *)self;
    if (obj->isEmpty(obj))
        return RING_BUF_ERR_EMPTY;

    uint8_t *src = (uint8_t *)obj->buf + obj->readPtr * obj->elemSize;
    memcpy(pData, src, obj->elemSize);
    obj->readPtr = (obj->readPtr + 1) % obj->elemCount;
    return RING_BUF_OK;
}

/************************ 两套求和接口 ************************/
// uint8_t 字节缓冲求和（原逻辑保留）
static uint32_t RingBuf_GetSumU8(void *self)
{
    RingBufferObj *obj = (RingBufferObj *)self;
    uint32_t sum = 0;
    uint16_t ptr = obj->readPtr;
    uint16_t usedCnt = obj->getUsed(obj);
    uint8_t *pBuf = (uint8_t *)obj->buf;

    for (uint16_t i = 0; i < usedCnt; i++)
    {
        sum += pBuf[ptr];
        ptr = (ptr + 1) % obj->elemCount;
    }
    return sum;
}

// float 浮点缓冲求和（新增）
static float RingBuf_GetSumFloat(void *self)
{
    RingBufferObj *obj = (RingBufferObj *)self;
    float sum = 0.0f;
    uint16_t ptr = obj->readPtr;
    uint16_t usedCnt = obj->getUsed(obj);
    float *pBuf = (float *)obj->buf;

    for (uint16_t i = 0; i < usedCnt; i++)
    {
        sum += pBuf[ptr];
        ptr = (ptr + 1) % obj->elemCount;
    }
    return sum;
}

/************************ 全局实例管理API ************************/
uint8_t RingBuf_CreateInstance(const char *name, void *buf, uint16_t elemCount, uint8_t elemSize, RingWriteMode initMode)
{
    if (gRingBufMgr.instCnt >= RING_BUF_MAX_INSTANCE_NUM)
        return 0xFF;
    // 元素数量至少2个（牺牲1字节判满）、元素尺寸不能为0
    if (buf == NULL || elemCount < 2 || elemSize == 0 || name == NULL)
        return 0xFF;

    // 查找空闲实例槽位
    uint8_t id = 0;
    for (; id < RING_BUF_MAX_INSTANCE_NUM; id++)
    {
        if (gRingBufMgr.inst[id].isUsed == false)
            break;
    }

    RingBufferObj *obj = &gRingBufMgr.inst[id];
    // 泛型字段初始化
    obj->buf = buf;
    obj->elemCount = elemCount;
    obj->elemSize = elemSize;
    obj->readPtr = 0;
    obj->writePtr = 0;
    obj->wrMode = initMode;
    strncpy(obj->name, name, RING_BUF_NAME_LEN_MAX - 1);
    obj->name[RING_BUF_NAME_LEN_MAX - 1] = '\0';
    obj->isUsed = true;

    // 绑定全部OOP成员函数指针
    obj->write = RingBuf_Write;
    obj->read = RingBuf_Read;
    obj->clear = RingBuf_Clear;
    obj->isFull = RingBuf_IsFull;
    obj->isEmpty = RingBuf_IsEmpty;
    obj->getUsed = RingBuf_GetUsed;
    obj->getRemain = RingBuf_GetRemain;
    obj->getSumU8 = RingBuf_GetSumU8;
    obj->getSumFloat = RingBuf_GetSumFloat;
    obj->setMode = RingBuf_SetMode;
		
		RingBuf_Clear(obj);

    gRingBufMgr.instCnt++;
    return id;
}

RingBufferObj* RingBuf_GetById(uint8_t id)
{
    if (id >= RING_BUF_MAX_INSTANCE_NUM)
        return NULL;
    if (gRingBufMgr.inst[id].isUsed == false)
        return NULL;
    return &gRingBufMgr.inst[id];
}

RingBufferObj* RingBuf_GetByName(const char *name)
{
    if (name == NULL)
        return NULL;
    for (uint8_t i = 0; i < RING_BUF_MAX_INSTANCE_NUM; i++)
    {
        if (gRingBufMgr.inst[i].isUsed && strcmp(gRingBufMgr.inst[i].name, name) == 0)
            return &gRingBufMgr.inst[i];
    }
    return NULL;
}

RingBufErrCode RingBuf_DelInstance(uint8_t id)
{
    RingBufferObj *obj = RingBuf_GetById(id);
    if (obj == NULL)
        return RING_BUF_ERR_NO_INST;

    memset(obj->name, 0, RING_BUF_NAME_LEN_MAX);
    obj->buf = NULL;
    obj->elemCount = 0;
    obj->elemSize = 0;
    obj->readPtr = 0;
    obj->writePtr = 0;
    obj->wrMode = RING_MODE_DISCARD;
    obj->isUsed = false;
    gRingBufMgr.instCnt--;
    return RING_BUF_OK;
}

// //////////////////////////////////////////// 环形缓冲区 应用实例///////////////////////////////////////////////////

// // 静态分配，无动态内存
// uint8_t adcSampleBuf[64];
// float tempFloatBuf[32];

// void RingBuf_MultiTypeDemo(void)
// {
//     // 创建uint8缓冲
//     uint8_t adcId = RingBuf_CreateInstance("ADC", adcSampleBuf, 64, 1, RING_MODE_DISCARD);
//     // 创建float浮点缓冲
//     uint8_t tempId = RingBuf_CreateInstance("TEMP", tempFloatBuf, 32, 4, RING_MODE_OVERWRITE);

//     // 1. 通过名称访问 uint8 字节缓冲
//     RingBufferObj *adcBuf = RingBuf_GetByName("ADC");
//     if (adcBuf != NULL)
//     {
//         uint8_t val = 0x33;
//         adcBuf->write(adcBuf, &val);
//         uint32_t byteSum = adcBuf->getSumU8(adcBuf);

//         uint8_t readVal;
//         adcBuf->read(adcBuf, &readVal);
//     }

//     // 2. 通过名称访问 float 浮点缓冲
//     RingBufferObj *tempBuf = RingBuf_GetByName("TEMP");
//     if (tempBuf != NULL)
//     {
//         float t1 = 25.5f;
//         float t2 = 26.2f;
//         tempBuf->write(tempBuf, &t1);
//         tempBuf->write(tempBuf, &t2);

//         float floatSum = tempBuf->getSumFloat(tempBuf);
//         float readTemp;
//         tempBuf->read(tempBuf, &readTemp);
//     }
// }


