/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef REQUEST_ID_H
#define REQUEST_ID_H

#include "ubse_mem_resource.h"

/* *
### 组装唯一且正确的RequestID
非Delete：RequestID = name(资源唯一标识) + requestNodeId
Delete：RequestID = name(资源唯一标识)

场景：
APP_BORROW = 0,   应用借用 (指明借入节点)   attachNode =
WATER_BORROW = 1, 水线借用
SHARE_BORROW = 2, 共享借用
APP_PRI_BORROW_REQUEST = 3, 确定性迁移 (指明借入节点)
APP_BORROW_NUMA_REQUEST = 4 numa借用 / 结构同水线借用  (指明借入节点)

对象履行状态:
UBSE_MEM_STATE_SUCCEEDED,
UBSE_MEM_STATE_FAILED,
UBSE_MEM_STATE_DESTROY_FAILED,
UBSE_MEM_STATE_DESTROYED

接口及对象：
1. Create: requestId = name
   添加Export: 同一个name要保序 (场景: 所有)
   添加Import: name  (状态: UBSE_MEM_STATE_FAILED / UBSE_MEM_STATE_SUCCEEDED) (场景: 除了共享借用)
2. Attach/Detach: requestId = name + attachNode (attachNode来自接口参数)
   添加/删除Import: name + attachNode ( 4种状态) (场景: 只有共享借用)  /// 基准，动不了
3. Delete: requestId = name
   删除Export: 同一个name要保序
   删除Import: name (状态: UBSE_MEM_STATE_DESTROY_FAILED / UBSE_MEM_STATE_DESTROYED) (场景: 都有可能)
       先用: name + attachNode 通知，如果无法匹配，则改用name? // 万一请求已经超时结束呢?

UbseMemImportObj处理：
添加：如果是共享场景，则name + attachNode. 其他场景 用name
删除：如果是共享场景，则name + attachNode 或 name(在Delete时清理). 其他场景 用name
   处理：共享场景，如果name + attachNode找不到请求，则用name通知一遍
*/

namespace ubse::mem_controller {
using namespace ubse::resource::mem;

inline std::string GetRequestIdNew(const std::string &name, const std::string &requestNodeId)
{
    return name + "_" + requestNodeId;
}

inline std::string GetRequestId(const std::string name, const std::string &attachOrDetachNode)
{
    return name + "_" + attachOrDetachNode;
}
} // namespace ubse::mem_controller

#endif // REQUEST_ID_H
