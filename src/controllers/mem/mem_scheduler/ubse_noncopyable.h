/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_NONCOPYABLE_H
#define UBSE_NONCOPYABLE_H

namespace ubse::mem::scheduler {

/**
  * Noncopyable被继承后 派生类对象可正常构造和析构 但派生类对象无法进行拷贝构造和赋值构造
  **/
class Noncopyable {
public:
    Noncopyable(const Noncopyable&) = delete;
    Noncopyable& operator=(const Noncopyable&) = delete;
    // 调度器组件（SchedulerImpl、Filter、Score 等）均通过 unique_ptr 或单例模式管理生命周期，不需要移动语义。
    Noncopyable(Noncopyable&&) = delete;
    Noncopyable& operator=(Noncopyable&&) = delete;

protected:
    Noncopyable() = default;
    virtual ~Noncopyable() = default;
};

} // namespace ubse::mem::scheduler

#endif // UBSE_NONCOPYABLE_H