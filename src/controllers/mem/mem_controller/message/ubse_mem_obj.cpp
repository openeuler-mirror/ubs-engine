/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
*/

#include "ubse_mem_obj.h"
#include <cstdint>
#include <string>
#include <vector>
#include "ubse_mem_fd_borrow_exportobj_simpo.h"
#include "ubse_mem_fd_borrow_importobj_simpo.h"
#include "ubse_mem_numa_borrow_exportobj_simpo.h"
#include "ubse_mem_numa_borrow_importobj_simpo.h"

#include "ubse_mem_obmm_def.h"
#include "securec.h"

namespace ubse::mem::obj {
using namespace ubse::mem::controller::message;
using namespace ubse::mem::serial;
static UbseObjByteBuffer CloneByteBuffer(const UbseObjByteBuffer &dataDesc)
{
    if (dataDesc.len <= 0) {
        return {nullptr, 0};
    }
    auto buff = new (std::nothrow) uint8_t[dataDesc.len];
    if (buff == nullptr) {
        return {nullptr, 0};
    }
    const UbseObjByteBuffer byteBuffer{buff, dataDesc.len};
    auto ret = memcpy_s(byteBuffer.data, byteBuffer.len, dataDesc.data, dataDesc.len);
    if (ret != 0) {
        delete[] buff;
        buff = nullptr;
        return {nullptr, 0};
    }
    return byteBuffer;
}
UbseObjByteBuffer UbseMemFdBorrowExportObj::ToBuffer() const
{
    UbseMemFdBorrowExportObj copy = *this;
    UbseMemFdBorrowExportobjSimpo obj;
    obj.SetUbseMemFdBorrowExportobj(copy);
    obj.Serialize();
    return CloneByteBuffer({obj.SerializedData(), obj.SerializedDataSize()});
}
void UbseMemFdBorrowExportObj::FromBuff(const UbseObjByteBuffer &buffer)
{
    UbseDeSerialization in(buffer.data, buffer.len);
    UbseMemFdBorrowExportObjDeserialization(in, *this);
}
UbseObjByteBuffer UbseMemFdBorrowImportObj::ToBuffer() const
{
    UbseMemFdBorrowImportObj copy = *this;
    UbseMemFdBorrowImportobjSimpo obj;
    obj.SetUbseMemFdBorrowImportobj(copy);
    obj.Serialize();
    return CloneByteBuffer({obj.SerializedData(), obj.SerializedDataSize()});
}
void UbseMemFdBorrowImportObj::FromBuff(const UbseObjByteBuffer &buffer)
{
    UbseDeSerialization in(buffer.data, buffer.len);
    UbseMemFdBorrowImportObjDeserialization(in, *this);
}
UbseObjByteBuffer UbseMemNumaBorrowExportObj::ToBuffer() const
{
    UbseMemNumaBorrowExportObj copy = *this;
    UbseMemNumaBorrowExportobjSimpo obj;
    obj.SetUbseMemNumaBorrowExportobj(copy);
    obj.Serialize();
    return CloneByteBuffer({obj.SerializedData(), obj.SerializedDataSize()});
}
void UbseMemNumaBorrowExportObj::FromBuff(const UbseObjByteBuffer &buffer)
{
    UbseDeSerialization in(buffer.data, buffer.len);
    UbseMemNumaBorrowExportObjDeserialization(in, *this);
}
UbseObjByteBuffer UbseMemNumaBorrowImportObj::ToBuffer() const
{
    UbseMemNumaBorrowImportObj copy = *this;
    UbseMemNumaBorrowImportobjSimpo obj;
    obj.SetUbseMemNumaBorrowImportobj(copy);
    obj.Serialize();
    return CloneByteBuffer({obj.SerializedData(), obj.SerializedDataSize()});
}
void UbseMemNumaBorrowImportObj::FromBuff(const UbseObjByteBuffer &buffer)
{
    UbseDeSerialization in(buffer.data, buffer.len);
    UbseMemNumaBorrowImportObjDeserialization(in, *this);
}
} // namespace ubse::mem::obj
