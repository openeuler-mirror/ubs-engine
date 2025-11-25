
#ifndef UBSE_MEM_DECODER_UTILS_H
#define UBSE_MEM_DECODER_UTILS_H

#include "ubse_common_def.h"
#include "ubse_http_common.h"
#include "ubse_mem_mami_def.h"

namespace ubse::mem::decoder::utils {
using namespace common::def;
using namespace mami;
class MemDecoderUtils {
public:
    static UbseResult GetChipAndDieId(uint32_t socketId, std::pair<std::string, std::string> &chipDiePair);
    static UbseResult GetAllHandles(const UbseMamiMemHandleQueryInfo &queryInfo, 
        std::vector<UbseMamiMemHandleValue> &handleValues);
    static UbseResult GetCurNodeSocketInfo(std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> &outSocketInfo);

private:
    static std::vector<uint32_t> GetAllChipId();
};
}

#endif
