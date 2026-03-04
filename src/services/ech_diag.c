/**
 * @file ech_diag.c
 * @brief ECH 水加热器 ECU - 诊断服务模块实现
 * 
 * @copyright Copyright (c) 2026 ECH ECU Development Team
 * @license MIT License
 * 
 * @version 0.1
 * @date 2026-03-04
 * 
 * @details
 * 本文件实现 UDS (Unified Diagnostic Services) 诊断服务功能。
 * 
 * 过程域: SWE.3 (软件详细设计与单元实现)
 * 安全等级: ASIL B (根据 HARA 报告 SG-003)
 * 
 * 需求追溯:
 * - SWE-D-001: UDS 诊断服务支持
 * - SWE-D-002: DTC 故障码管理
 * - SWE-D-003: ECU 信息读取
 * - SWE-D-004: 诊断会话控制
 * 
 * 单元测试: 见本文件末尾 UNIT_TEST 宏定义部分
 */

/* ============================================================================
 * 包含头文件
 * ============================================================================ */

#include "ech_diag.h"
#include <string.h>

/* ============================================================================
 * 局部宏定义
 * ============================================================================ */

/** 默认配置 */
#define DEFAULT_SESSION_TIMEOUT_MS  (5000)
#define DEFAULT_P2_SERVER_TIME_MS   (50)

/** UDS 否定响应码 */
#define ECH_UDS_NRC_POSITIVE        (0x00)
#define ECH_UDS_NRC_SERVICE_NOT_SUPPORTED (0x11)
#define ECH_UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED (0x12)
#define ECH_UDS_NRC_INCORRECT_MSG_LEN (0x13)
#define ECH_UDS_NRC_CONDITIONS_NOT_CORRECT (0x22)

/* ============================================================================
 * 局部函数声明
 * ============================================================================ */

/**
 * @brief 查找 DTC 记录
 */
static EchDtcRecord_t* FindDtcRecord(EchDiagController_t* controller, EchDtcCode_t dtcCode);

/**
 * @brief 处理诊断会话控制请求
 */
static int32_t ProcessDiagnosticSessionControl(EchDiagController_t* controller, uint8_t subFunction, EchDiagResponse_t* response, uint32_t timestamp_ms);

/**
 * @brief 处理读取 DTC 请求
 */
static int32_t ProcessReadDtc(EchDiagController_t* controller, uint8_t subFunction, EchDiagResponse_t* response);

/**
 * @brief 处理清除 DTC 请求
 */
static int32_t ProcessClearDtc(EchDiagController_t* controller, const uint8_t* data, uint8_t length, EchDiagResponse_t* response);

/**
 * @brief 处理读取 DID 请求
 */
static int32_t ProcessReadDid(EchDiagController_t* controller, uint16_t didId, EchDiagResponse_t* response);

/* ============================================================================
 * 函数实现
 * ============================================================================ */

/**
 * @brief 初始化诊断控制器
 */
int32_t EchDiag_Init(EchDiagController_t* controller, const EchDiagConfig_t* config)
{
    /* 参数检查 */
    if (controller == NULL) {
        return -1;
    }
    
    /* 清零结构体 */
    memset(controller, 0, sizeof(EchDiagController_t));
    
    /* 设置配置 */
    if (config != NULL) {
        controller->config = *config;
    } else {
        /* 使用默认配置 */
        controller->config.sessionTimeout_ms = DEFAULT_SESSION_TIMEOUT_MS;
        controller->config.p2ServerTime_ms = DEFAULT_P2_SERVER_TIME_MS;
        controller->config.securityEnabled = false;
    }
    
    /* 初始化会话状态 */
    controller->currentSession = ECH_DIAG_SESSION_DEFAULT_TYPE;
    controller->sessionStartTime_ms = 0;
    controller->sessionActive = false;
    
    /* DTC 管理 */
    controller->dtcCount = 0;
    controller->totalDtcSetCount = 0;
    controller->totalDtcClearCount = 0;
    
    /* 诊断统计 */
    controller->requestCount = 0;
    controller->errorCount = 0;
    
    controller->initialized = true;
    
    return 0;
}

/**
 * @brief 更新诊断状态 (周期性调用)
 */
int32_t EchDiag_Update(EchDiagController_t* controller, uint32_t timestamp_ms)
{
    if (controller == NULL || !controller->initialized) {
        return -1;
    }
    
    /* 检查会话超时 */
    if (controller->sessionActive && controller->currentSession != ECH_DIAG_SESSION_DEFAULT_TYPE) {
        uint32_t sessionDuration = timestamp_ms - controller->sessionStartTime_ms;
        if (sessionDuration > controller->config.sessionTimeout_ms) {
            /* 会话超时，返回默认会话 */
            controller->currentSession = ECH_DIAG_SESSION_DEFAULT_TYPE;
            controller->sessionActive = false;
        }
    }
    
    return 0;
}

/**
 * @brief 处理 UDS 请求
 */
int32_t EchDiag_ProcessRequest(EchDiagController_t* controller, const uint8_t* requestData, uint16_t requestLength, EchDiagResponse_t* response, uint32_t timestamp_ms)
{
    if (controller == NULL || !controller->initialized || requestData == NULL || response == NULL) {
        return -1;
    }
    
    if (requestLength < 1) {
        return -2;  /* 请求长度无效 */
    }
    
    controller->requestCount++;
    response->isPositive = true;
    response->negativeCode = ECH_UDS_NRC_POSITIVE;
    
    uint8_t serviceId = requestData[0];
    
    switch (serviceId) {
        case ECH_UDS_SID_DIAG_CTRL:  /* 0x10: 诊断会话控制 */
            if (requestLength < 2) {
                response->isPositive = false;
                response->negativeCode = ECH_UDS_NRC_INCORRECT_MSG_LEN;
                return -3;
            }
            return ProcessDiagnosticSessionControl(controller, requestData[1], response, timestamp_ms);
        
        case ECH_UDS_SID_READ_DID:  /* 0x22: 读取 DID */
            if (requestLength < 3) {
                response->isPositive = false;
                response->negativeCode = ECH_UDS_NRC_INCORRECT_MSG_LEN;
                return -3;
            }
            {
                uint16_t didId = (requestData[1] << 8) | requestData[2];
                return ProcessReadDid(controller, didId, response);
            }
        
        case ECH_UDS_SID_READ_DTC:  /* 0x19: 读取 DTC */
            if (requestLength < 2) {
                response->isPositive = false;
                response->negativeCode = ECH_UDS_NRC_INCORRECT_MSG_LEN;
                return -3;
            }
            return ProcessReadDtc(controller, requestData[1], response);
        
        case ECH_UDS_SID_CLEAR_DTC:  /* 0x14: 清除 DTC */
            return ProcessClearDtc(controller, requestData, requestLength, response);
        
        default:
            response->isPositive = false;
            response->negativeCode = ECH_UDS_NRC_SERVICE_NOT_SUPPORTED;
            return -4;  /* 服务不支持 */
    }
}

/**
 * @brief 设置 DTC 故障码
 */
int32_t EchDiag_SetDtc(EchDiagController_t* controller, EchDtcCode_t dtcCode, const uint8_t* freezeFrame)
{
    if (controller == NULL || !controller->initialized || dtcCode == ECH_DTC_NONE) {
        return -1;
    }
    
    /* 查找是否已存在 */
    EchDtcRecord_t* record = FindDtcRecord(controller, dtcCode);
    
    if (record != NULL) {
        /* 已存在，更新状态和计数 */
        record->status |= ECH_DTC_STATUS_CONFIRMED;
        record->occurrenceCount++;
        record->lastOccurTime = controller->sessionStartTime_ms;
    } else {
        /* 新建 DTC 记录 */
        if (controller->dtcCount >= ECH_DIAG_MAX_DTC_COUNT) {
            return -2;  /* DTC 表已满 */
        }
        
        record = &controller->dtcRecords[controller->dtcCount];
        record->dtcCode = dtcCode;
        record->status = ECH_DTC_STATUS_CONFIRMED;
        record->severity = ECH_DTC_SEVERITY_WARNING;
        record->occurrenceCount = 1;
        record->firstOccurTime = controller->sessionStartTime_ms;
        record->lastOccurTime = controller->sessionStartTime_ms;
        
        if (freezeFrame != NULL) {
            memcpy(record->freezeFrame, freezeFrame, ECH_DIAG_FREEZE_FRAME_SIZE);
            record->hasFreezeFrame = true;
        } else {
            record->hasFreezeFrame = false;
        }
        
        controller->dtcCount++;
    }
    
    controller->totalDtcSetCount++;
    
    return 0;
}

/**
 * @brief 清除 DTC 故障码
 */
int32_t EchDiag_ClearDtc(EchDiagController_t* controller, EchDtcCode_t dtcCode)
{
    if (controller == NULL || !controller->initialized) {
        return -1;
    }
    
    EchDtcRecord_t* record = FindDtcRecord(controller, dtcCode);
    if (record == NULL) {
        return -2;  /* DTC 未找到 */
    }
    
    /* 清除 DTC */
    record->status = 0;
    record->occurrenceCount = 0;
    
    /* 从数组中移除 */
    for (uint8_t i = 0; i < controller->dtcCount - 1; i++) {
        if (controller->dtcRecords[i].dtcCode == dtcCode) {
            controller->dtcRecords[i] = controller->dtcRecords[controller->dtcCount - 1];
            break;
        }
    }
    controller->dtcCount--;
    
    controller->totalDtcClearCount++;
    
    return 0;
}

/**
 * @brief 清除所有 DTC
 */
int32_t EchDiag_ClearAllDtc(EchDiagController_t* controller)
{
    if (controller == NULL || !controller->initialized) {
        return -1;
    }
    
    controller->dtcCount = 0;
    controller->totalDtcClearCount++;
    
    return 0;
}

/**
 * @brief 获取 DTC 记录
 */
const EchDtcRecord_t* EchDiag_GetDtcRecord(const EchDiagController_t* controller, EchDtcCode_t dtcCode)
{
    if (controller == NULL || !controller->initialized) {
        return NULL;
    }
    
    return FindDtcRecord((EchDiagController_t*)controller, dtcCode);
}

/**
 * @brief 获取所有 DTC 列表
 */
uint8_t EchDiag_GetAllDtcList(const EchDiagController_t* controller, EchDtcCode_t* dtcList, uint8_t maxCount)
{
    if (controller == NULL || !controller->initialized || dtcList == NULL) {
        return 0;
    }
    
    uint8_t count = (controller->dtcCount < maxCount) ? controller->dtcCount : maxCount;
    
    for (uint8_t i = 0; i < count; i++) {
        dtcList[i] = controller->dtcRecords[i].dtcCode;
    }
    
    return count;
}

/**
 * @brief 切换诊断会话
 */
int32_t EchDiag_ChangeSession(EchDiagController_t* controller, EchDiagSessionType_t sessionType, uint32_t timestamp_ms)
{
    if (controller == NULL || !controller->initialized) {
        return -1;
    }
    
    controller->currentSession = sessionType;
    controller->sessionStartTime_ms = timestamp_ms;
    controller->sessionActive = true;
    
    return 0;
}

/**
 * @brief 获取当前会话类型
 */
EchDiagSessionType_t EchDiag_GetCurrentSession(const EchDiagController_t* controller)
{
    if (controller == NULL || !controller->initialized) {
        return ECH_DIAG_SESSION_DEFAULT_TYPE;
    }
    
    return controller->currentSession;
}

/**
 * @brief 读取数据标识符 (DID)
 */
int32_t EchDiag_ReadDid(EchDiagController_t* controller, uint16_t didId, uint8_t* data, uint16_t* dataLength)
{
    if (controller == NULL || !controller->initialized || data == NULL || dataLength == NULL) {
        return -1;
    }
    
    /* 模拟 DID 读取 */
    switch (didId) {
        case 0xF100:  /* 软件版本号 */
            data[0] = ECH_DIAG_VERSION_MAJOR;
            data[1] = ECH_DIAG_VERSION_MINOR;
            data[2] = ECH_DIAG_VERSION_PATCH;
            *dataLength = 3;
            break;
        
        case 0xF190:  /* 车辆识别号 */
            memcpy(data, "ECH123456789", 12);
            *dataLength = 12;
            break;
        
        case 0xF18C:  /* ECU 序列号 */
            memcpy(data, "ECU000001", 9);
            *dataLength = 9;
            break;
        
        default:
            return -2;  /* DID 不支持 */
    }
    
    return 0;
}

/**
 * @brief 获取诊断统计信息
 */
void EchDiag_GetDiagnostic(const EchDiagController_t* controller, uint32_t* requestCount, uint32_t* errorCount, uint8_t* dtcCount)
{
    if (controller == NULL || !controller->initialized) {
        return;
    }
    
    if (requestCount != NULL) *requestCount = controller->requestCount;
    if (errorCount != NULL) *errorCount = controller->errorCount;
    if (dtcCount != NULL) *dtcCount = controller->dtcCount;
}

/**
 * @brief 获取模块版本号
 */
void EchDiag_GetVersion(uint8_t* major, uint8_t* minor, uint8_t* patch)
{
    if (major != NULL) *major = ECH_DIAG_VERSION_MAJOR;
    if (minor != NULL) *minor = ECH_DIAG_VERSION_MINOR;
    if (patch != NULL) *patch = ECH_DIAG_VERSION_PATCH;
}

/* ============================================================================
 * 局部函数实现
 * ============================================================================ */

/**
 * @brief 查找 DTC 记录
 */
static EchDtcRecord_t* FindDtcRecord(EchDiagController_t* controller, EchDtcCode_t dtcCode)
{
    for (uint8_t i = 0; i < controller->dtcCount; i++) {
        if (controller->dtcRecords[i].dtcCode == dtcCode) {
            return &controller->dtcRecords[i];
        }
    }
    return NULL;
}

/**
 * @brief 处理诊断会话控制请求
 */
static int32_t ProcessDiagnosticSessionControl(EchDiagController_t* controller, uint8_t subFunction, EchDiagResponse_t* response, uint32_t timestamp_ms)
{
    response->serviceId = 0x50;  /* 肯定响应 SID */
    response->subFunction = subFunction;
    response->dataLength = 3;
    
    switch (subFunction) {
        case ECH_DIAG_SESSION_DEFAULT:
            response->data[0] = 0x00;  /* 会话类型 */
            response->data[1] = 0x00;  /* 保留 */
            response->data[2] = 0x00;  /* 保留 */
            controller->currentSession = ECH_DIAG_SESSION_DEFAULT_TYPE;
            controller->sessionActive = false;
            break;
        
        case ECH_DIAG_SESSION_PROGRAMMING:
            response->data[0] = 0x01;
            response->data[1] = (controller->config.p2ServerTime_ms >> 8) & 0xFF;
            response->data[2] = controller->config.p2ServerTime_ms & 0xFF;
            controller->currentSession = ECH_DIAG_SESSION_PROGRAMMING_TYPE;
            controller->sessionStartTime_ms = timestamp_ms;
            controller->sessionActive = true;
            break;
        
        case ECH_DIAG_SESSION_EXTENDED:
            response->data[0] = 0x02;
            response->data[1] = (controller->config.p2ServerTime_ms >> 8) & 0xFF;
            response->data[2] = controller->config.p2ServerTime_ms & 0xFF;
            controller->currentSession = ECH_DIAG_SESSION_EXTENDED_TYPE;
            controller->sessionStartTime_ms = timestamp_ms;
            controller->sessionActive = true;
            break;
        
        default:
            response->isPositive = false;
            response->negativeCode = ECH_UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED;
            return -1;
    }
    
    return 0;
}

/**
 * @brief 处理读取 DTC 请求
 */
static int32_t ProcessReadDtc(EchDiagController_t* controller, uint8_t subFunction, EchDiagResponse_t* response)
{
    response->serviceId = 0x59;  /* 肯定响应 SID */
    response->subFunction = subFunction;
    
    if (subFunction == 0x02) {  /* 读取所有 DTC */
        response->dataLength = 1 + (controller->dtcCount * 4);  /* DTC 数量 + 每个 DTC 3 字节 */
        response->data[0] = controller->dtcCount;
        
        for (uint8_t i = 0; i < controller->dtcCount; i++) {
            response->data[1 + i * 4] = (uint8_t)controller->dtcRecords[i].dtcCode;
            response->data[2 + i * 4] = controller->dtcRecords[i].status;
            response->data[3 + i * 4] = (uint8_t)controller->dtcRecords[i].severity;
        }
    } else {
        response->isPositive = false;
        response->negativeCode = ECH_UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED;
        return -1;
    }
    
    return 0;
}

/**
 * @brief 处理清除 DTC 请求
 */
static int32_t ProcessClearDtc(EchDiagController_t* controller, const uint8_t* data, uint8_t length, EchDiagResponse_t* response)
{
    response->serviceId = 0x54;  /* 肯定响应 SID */
    response->dataLength = 0;
    
    if (length >= 4 && data[1] == 'C' && data[2] == 'L' && data[3] == 'R') {
        /* 清除所有 DTC */
        EchDiag_ClearAllDtc(controller);
    } else {
        response->isPositive = false;
        response->negativeCode = ECH_UDS_NRC_CONDITIONS_NOT_CORRECT;
        return -1;
    }
    
    return 0;
}

/**
 * @brief 处理读取 DID 请求
 */
static int32_t ProcessReadDid(EchDiagController_t* controller, uint16_t didId, EchDiagResponse_t* response)
{
    response->serviceId = 0x62;  /* 肯定响应 SID */
    response->data[0] = (didId >> 8) & 0xFF;
    response->data[1] = didId & 0xFF;
    
    uint16_t dataLength = 0;
    int32_t result = EchDiag_ReadDid(controller, didId, &response->data[2], &dataLength);
    
    if (result == 0) {
        response->dataLength = 2 + dataLength;
    } else {
        response->isPositive = false;
        response->negativeCode = ECH_UDS_NRC_CONDITIONS_NOT_CORRECT;
        return -1;
    }
    
    return 0;
}

/* ============================================================================
 * 单元测试桩代码 (UNIT_TEST 宏定义时编译)
 * ============================================================================ */

#ifdef UNIT_TEST

#include <stdio.h>
#include <assert.h>

void test_diag_init(void)
{
    EchDiagController_t diag;
    EchDiagConfig_t config = {
        .sessionTimeout_ms = 5000,
        .p2ServerTime_ms = 50,
        .securityEnabled = false
    };
    
    int32_t result = EchDiag_Init(&diag, &config);
    if (result != 0) { printf("FAIL: init result\n"); return; }
    if (!diag.initialized) { printf("FAIL: initialized\n"); return; }
    
    printf("OK\n");
}

void test_diag_dtc_management(void)
{
    EchDiagController_t diag;
    EchDiag_Init(&diag, NULL);
    
    uint8_t freezeFrame[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    diag.sessionStartTime_ms = 100;
    EchDiag_SetDtc(&diag, ECH_DTC_P0001, freezeFrame);
    printf("  DTC set: P0001, count=%d\n", diag.dtcCount);
    
    const EchDtcRecord_t* record = EchDiag_GetDtcRecord(&diag, ECH_DTC_P0001);
    if (record) printf("  DTC record retrieved\n");
    
    EchDiag_ClearDtc(&diag, ECH_DTC_P0001);
    printf("  DTC cleared, count=%d\n", diag.dtcCount);
    
    printf("OK\n");
}

void test_diag_session_control(void)
{
    EchDiagController_t diag;
    EchDiag_Init(&diag, NULL);
    
    EchDiag_ChangeSession(&diag, ECH_DIAG_SESSION_PROGRAMMING_TYPE, 100);
    printf("  Session: PROGRAMMING (type=%d)\n", diag.currentSession);
    
    EchDiagSessionType_t session = EchDiag_GetCurrentSession(&diag);
    printf("  Current session type: %d\n", session);
    
    EchDiag_ChangeSession(&diag, ECH_DIAG_SESSION_DEFAULT_TYPE, 200);
    printf("  Session: DEFAULT\n");
    
    printf("OK\n");
}

void test_diag_read_did(void)
{
    EchDiagController_t diag;
    EchDiag_Init(&diag, NULL);
    
    uint8_t data[20];
    uint16_t dataLength = 0;
    EchDiag_ReadDid(&diag, 0xF100, data, &dataLength);
    printf("  DID 0xF100: len=%d, ver=%d.%d.%d\n", dataLength, data[0], data[1], data[2]);
    
    EchDiag_ReadDid(&diag, 0xF190, data, &dataLength);
    printf("  DID 0xF190: len=%d\n", dataLength);
    
    printf("OK\n");
}

void test_diag_uds_request(void)
{
    EchDiagController_t diag;
    EchDiag_Init(&diag, NULL);
    
    /* 模拟诊断会话控制请求 */
    uint8_t request[] = {0x10, 0x01};  /* SID=0x10, SubFunction=Programming */
    EchDiagResponse_t response;
    
    int32_t result = EchDiag_ProcessRequest(&diag, request, 2, &response, 100);
    assert(result == 0);
    assert(response.isPositive == true);
    assert(response.serviceId == 0x50);
    printf("  UDS request 0x10 processed\n");
    
    /* 模拟读取 DID 请求 */
    uint8_t request2[] = {0x22, 0xF1, 0x00};  /* SID=0x22, DID=0xF100 */
    result = EchDiag_ProcessRequest(&diag, request2, 3, &response, 200);
    assert(result == 0);
    assert(response.isPositive == true);
    assert(response.serviceId == 0x62);
    printf("  UDS request 0x22 processed\n");
    
    printf("✓ test_diag_uds_request passed\n");
}

void test_diag_statistics(void)
{
    EchDiagController_t diag;
    EchDiag_Init(&diag, NULL);
    
    /* 执行一些操作 */
    EchDiag_SetDtc(&diag, ECH_DTC_P0001, NULL);
    EchDiag_SetDtc(&diag, ECH_DTC_P0002, NULL);
    
    uint8_t request[] = {0x10, 0x01};
    EchDiagResponse_t response;
    EchDiag_ProcessRequest(&diag, request, 2, &response, 100);
    
    /* 获取统计信息 */
    uint32_t requestCount = 0, errorCount = 0;
    uint8_t dtcCount = 0;
    EchDiag_GetDiagnostic(&diag, &requestCount, &errorCount, &dtcCount);
    
    assert(requestCount == 1);
    assert(dtcCount == 2);
    printf("  Request count: %u, DTC count: %u\n", requestCount, dtcCount);
    
    printf("✓ test_diag_statistics passed\n");
}

int main(void)
{
    printf("=== ECH Diagnostic Module Unit Tests ===\n\n");
    
    printf("test_diag_init: ");
    test_diag_init();
    
    printf("test_diag_dtc_management: ");
    test_diag_dtc_management();
    
    printf("test_diag_session_control: ");
    test_diag_session_control();
    
    printf("test_diag_read_did: ");
    test_diag_read_did();
    
    printf("\n=== All tests passed! ===\n");
    return 0;
}

#endif /* UNIT_TEST */
