#ifndef UBSE_AUTO_SIMPO_H
#define UBSE_AUTO_SIMPO_H

#include "ubse_auto_serial_util.hpp"
#include "ubse_base_message.h"
#include "ubse_error.h"

namespace ubse::simpo::util {
template <class Arg>
class UbseAutoSimpo : public message::UbseBaseMessage {
public:
    UbseAutoSimpo() = default;
    explicit UbseAutoSimpo(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }

    inline void SetUbseMesgInfo(Arg mesgInfo)
    {
        simpoMesg = std::move(mesgInfo);
    }

    inline Arg GetUbseMesgInfo()
    {
        return simpoMesg;
    }

    message::UbseResult Serialize() override;

    message::UbseResult Deserialize() override;

private:
    Arg simpoMesg;
};

template <class Arg>
message::UbseResult UbseAutoSimpo<Arg>::Serialize()
{
    serial::UbseSerialization out;
    if (!ubse::serial::util::SerializeField(out, mErrCode) ||
        !ubse::serial::util::SerializeField(out, simpoMesg)) {
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

template <class Arg>
message::UbseResult UbseAutoSimpo<Arg>::Deserialize()
{
    if (mInputRawData == nullptr) {
        return UBSE_ERROR;
    }
    serial::UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    if (!ubse::serial::util::DeSerializeField(in, mErrCode) ||
        !ubse::serial::util::DeSerializeField(in, simpoMesg)) {
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

template <class Arg>
using UbseAutoSimpoPtr = message::Ref<UbseAutoSimpo<Arg>>;

// ============================================================
// UbseAutoSimpoTuple — holds multiple values as a std::tuple
// instead of a single struct. Each element is serialized in
// declaration order via the tuple SerializeField/DeSerializeField
// overloads defined in ubse_auto_serial_util.hpp.
// ============================================================
template <class... Args>
class UbseAutoSimpoTuple : public message::UbseBaseMessage {
public:
    UbseAutoSimpoTuple() = default;
    explicit UbseAutoSimpoTuple(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }

    inline void SetUbseMesgInfo(std::tuple<Args...> mesgInfo)
    {
        simpoMesg = std::move(mesgInfo);
    }

    inline std::tuple<Args...> GetUbseMesgInfo()
    {
        return simpoMesg;
    }

    template <size_t I>
    inline decltype(auto) Get()
    {
        return std::get<I>(simpoMesg);
    }

    template <size_t I>
    inline void Set(typename std::tuple_element<I, std::tuple<Args...>>::type value)
    {
        std::get<I>(simpoMesg) = std::move(value);
    }

    message::UbseResult Serialize() override;

    message::UbseResult Deserialize() override;

private:
    std::tuple<Args...> simpoMesg;
};

template <class... Args>
message::UbseResult UbseAutoSimpoTuple<Args...>::Serialize()
{
    serial::UbseSerialization out;
    if (!ubse::serial::util::SerializeField(out, mErrCode) ||
        !ubse::serial::util::SerializeField(out, simpoMesg)) {
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

template <class... Args>
message::UbseResult UbseAutoSimpoTuple<Args...>::Deserialize()
{
    if (mInputRawData == nullptr) {
        return UBSE_ERROR;
    }
    serial::UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    if (!ubse::serial::util::DeSerializeField(in, mErrCode) ||
        !ubse::serial::util::DeSerializeField(in, simpoMesg)) {
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

template <class... Args>
using UbseAutoSimpoTuplePtr = message::Ref<UbseAutoSimpoTuple<Args...>>;
} // namespace ubse::simpo::util

#endif // UBSE_AUTO_SIMPO_H
