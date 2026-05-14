/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */
#ifndef UBSE_MEM_PREHANDLE_MANAGER_H
#define UBSE_MEM_PREHANDLE_MANAGER_H
#include "ubse_common_def.h"
#include "src/adapter_plugins/mmi/ubse_mem_types.h"
#include "src/controllers/mem/mem_decoder_utils/ubse_mem_decoder_utils.h"

#include <mutex>
namespace ubse::mem::decoder::utils {
using namespace common::def;
struct PreHandlerInfo {
    uint32_t dcna;                        // 作为key的一部分, isPreImport为true的时候, dcna有效
    UbseMamiMemImportResult importResult; // 添加表项的结果
    bool isPreImport;                     // 是否已经执行了obmm的预上线操作
};

using DecoderLocToIsUsehandleValueMap =
    std::unordered_map<decoder::utils::DecoderEntryLoc, std::vector<PreHandlerInfo>,
                       decoder::utils::DecoderEntryLoc::Hash,
                       decoder::utils::DecoderEntryLoc::
                           Equal>; // 第一个bool表示是否执行了obmm的预上线操作，第二个bool表示这块内存是否有导入设备

using DcnaToSize = std::unordered_map<uint32_t, uint64_t>;

class UbseMemPrehandleManager {
public:
    inline static UbseMemPrehandleManager& GetInstance()
    {
        static UbseMemPrehandleManager instance;
        return instance;
    }
    UbseMemPrehandleManager(const UbseMemPrehandleManager& other) = delete;
    UbseMemPrehandleManager(UbseMemPrehandleManager&& other) = delete;
    UbseMemPrehandleManager& operator=(const UbseMemPrehandleManager& other) = delete;
    UbseMemPrehandleManager& operator=(UbseMemPrehandleManager&& other) noexcept = delete;
    UbseResult InitPreHandle(std::vector<mmi::BasicPreImportInfo> preImportInfos);

    UbseResult GetOneUnImportHandle(
        const decoder::utils::DecoderEntryLoc& loc, uint32_t dcna,
        UbseMamiMemImportResult& handleValue); // 获得一个已经预上线，但没有导入的静态表项，指定了哪张表

    void GetOneUnPreImportHandle(const decoder::utils::DecoderEntryLoc& loc, uint32_t dcna,
                                 UbseMamiMemImportResult& value); // 获得一个还未预上线的表项，指定表

    void CreatePreHandle(const decoder::utils::DecoderEntryLoc& loc, const UbseMamiMemImportResult& handleValue,
                         uint32_t dcna, uint32_t importSize);

    uint32_t GetPreHandleByDcna(const decoder::utils::DecoderEntryLoc& loc, uint32_t dcna,
                                UbseMamiMemImportResult& handleValue);

    uint64_t GetPresizeByDcna(uint32_t dcna);

    void RollbackHandle(const decoder::utils::DecoderEntryLoc& loc, const std::vector<uint64_t>& handles);

    void RollbackHandle(const decoder::utils::DecoderEntryLoc& loc, uint64_t handle);

    void RollbackPreImportHandle(const decoder::utils::DecoderEntryLoc& loc);

    void PrintPreHandleMap();

    bool IsNeedPreOnline(const decoder::utils::DecoderEntryLoc& loc, uint32_t dcna, UbseMamiMemImportResult& outValue);

private:
    UbseMemPrehandleManager() = default;
    inline void PreMapClear()
    {
        preHandleMap.clear();
        dcnaToSize.clear();
    }
    DecoderLocToIsUsehandleValueMap preHandleMap;
    DcnaToSize dcnaToSize;
    std::mutex handleLock;
};
} // namespace ubse::mem::decoder::utils

#endif