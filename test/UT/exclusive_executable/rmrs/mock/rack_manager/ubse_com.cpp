/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_com.h"

namespace ubse::com {

/**
 * @brief 注册跨节点通信消息处理函数，适用于Master与Agent间的通信
 * @param[in] endpoint: 向Server端注册的ServiceHandler的标识（address不用传）
 * @param[in] handler: 消息处理函数
 * @return 成功返回0, 失败返回非0
 */
uint32_t UbseRegRpcService(const UbseComEndpoint &endpoint, const UbseComServiceHandler &handler)
{
    return 0;
}

/*
static inline BResult Translate(const ByteBuffer &msg, BaseMessagePtr request)
{
    uint32_t size = msg.len;
    uint8_t *data = msg.data;
    if (!data) {
        return E_CODE_RPC_RECEIVE_DATA_NULL;
    }
    if (request == nullptr) {
        LOG_ERROR("request is nullptr.");
        return E_CODE_NULLPTR;
    }
    BResult hr = request->SetInputRawData(data, size);
    if (BresultFail(hr)) {
        return hr;
    }
    return request->Deserialize();
}
template <class TReq, class TRsp> static bool Handle(const ByteBuffer &msg, ByteBuffer &ret, BaseNetMsgExecutor &exePtr)
{
    Ref<TRsp> response = new (std::nothrow) TRsp();
    if (response == nullptr) {
        LOG_ERROR("RPC Alloc response InternalError");
        return false;
    }
    Ref<TReq> request = new (std::nothrow) TReq();
    if (request == nullptr) {
        LOG_ERROR("RPC Alloc request InternalError");
        return false;
    }
    BResult hr = Translate(msg, BaseMessage::Convert<TReq>(request));
    if (BresultFail(hr)) {
        LOG_ERROR("RPC request Translate InternalError ret " << hr);
        response->SetErrCode(hr);
        return false;
    }
    BaseNetContext ctx{};
    try {
        exePtr.ExecuteMsg(BaseMessage::Convert<TReq>(request), BaseMessage::Convert<TRsp>(response), ctx);
    } catch (int32_t hrc) {
        hr = hrc;
    } catch (std::exception &ex) {
        // 注释
    } catch (...) {
        // 注释
    }
    if (BresultFail(hr)) {
        LOG_ERROR("Get exception when execute message as ");
        response->SetErrCode(hr);
        return false;
    }
    response->Serialize();
    ByteBuffer buffer;
    buffer.data = (uint8_t *)response->SerializedData();
    buffer.len = response->SerializedDataSize();
    ret = buffer;
    return true;
}
template <class TReq, class TRsp, class THandler>
int SyncCallFuncStub(const MessageParam &messageParam, const ByteBuffer &data, ByteBuffer &retData)
{
    if (!IpcStubTest::GetInstance().rpcStatus.load()) {
        return E_CODE_MANAGER;
    }
    THandler handler;
    ByteBuffer buffer{};
    Handle<TReq, TRsp>(data, buffer, handler);
    retData.data = buffer.data;
    retData.len = buffer.len;
    return 0;
}

static int32_t SyncCallFunc(const ock::memory::MessageParam &messageParam, const ock::memory::ByteBuffer &message,
    ock::memory::ByteBuffer &retData)
{
    if (RmMemOpCodeRpc::RPC_AGENT_RACKMEMSHM_LOOKUP_SHAREREGIONS == messageParam.opcode) {
        LOG_INFO("RPC_AGENT_RACKMEMSHM_LOOKUP_SHAREREGIONS message handle.");
        return SyncCallFuncStub<ShmRpcLookRegionListRequest, ShmRpcLookRegionListResponse,
            ock::memory::manager::ManagerShmLookRegionListRequestHandler>(messageParam, message, retData);
    }
    if (RmMemOpCodeRpc::RPC_AGENT_RACKMEMSHM_LOOKUP_REGIONINFO == messageParam.opcode) {
        LOG_INFO("RPC_AGENT_RACKMEMSHM_LOOKUP_REGIONINFO message handle.");
        SyncCallFuncStub<ShmRpcLookRegionInfoRequest, ShmRpcLookRegionInfoResponse,
            ock::memory::manager::ManagerShmLookRegionInfoRequestHandler>(messageParam, message, retData);
        return true;
    }
    if (RmMemOpCodeRpc::RPC_AGENT_RACKMEMSHM_DELETE == messageParam.opcode) {
        LOG_INFO("RPC_AGENT_RACKMEMSHM_DELETE message handle.");
        SyncCallFuncStub<ShmRpcDeleteRequest, HcomCommonResponse, ock::memory::manager::ManagerShmDeleteRequestHandler>(
            messageParam, message, retData);
        return true;
    }
    if (RmMemOpCodeRpc::RPC_APP_QUERY_LOCAL_MEM_INFO == messageParam.opcode) {
        LOG_INFO("RPC_MANAGER_SMAP_OUT_DATA message handle.");
        return SyncCallFuncStub<RpcOnlyNodeIdRequest, RPCRackMemQueryLocalMemInfoResponse,
            ock::memory::manager::ManagerRpcAppQueryLocalMemInfoHandler>(messageParam, message, retData);
    }

    return 0;
}
*/

/**
 * @brief 同步发消息
 * @param[in] endpoint: 目标端通信标识
 * @param[in] reqData: 要发送的数据,reqData申请的内存由调用方释放，框架不释放reqData的内存
 * @param[in，out] ctx: 上下文指针
 * @param[in] handler: 处理返回结果的处理函数,对端返回消息后，同步调用回调函数
 * @return 成功返回0, 失败返回非0
 */
uint32_t UbseRpcSend(const UbseComEndpoint &endpoint, const UbseByteBuffer &reqData, void *ctx,
    const UbseComRespHandler &handler)
{
    return 0;
}

/**
 * @brief 异步发消息
 * @param[in] endpoint: 目标端通信标识
 * @param[in] reqData: 要发送的数据,reqData申请的内存由调用方释放，框架不释放reqData的内存
 * @param[in，out] ctx: 上下文指针
 * @param[in] handler: 处理返回结果的处理函数,对端返回消息后，异步调用回调函数
 * @return 成功返回0, 失败返回非0
 */
uint32_t UbseRpcAsyncSend(const UbseComEndpoint &endpoint, const UbseByteBuffer &reqData, void *ctx,
    const UbseComRespHandler &handler)
{
    return 0;
}
}