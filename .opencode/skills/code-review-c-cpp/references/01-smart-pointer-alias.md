# Rule: Identify Smart Pointer Aliases Before Reporting Memory Leaks

项目中使用自定义引用计数智能指针 `Ref<T>`（定义在 `src/framework/misc/referable/ubse_ref.h`），大量指针类型通过 `using XxxPtr = Ref<Xxx>` 别名化，外观类似裸指针。

**报告 `new (std::nothrow)` 内存泄漏前，必须确认目标类型不是 `Ref<T>` 别名。**

## 验证方法

搜索 `using` 声明，确认类型是否为 `Ref<T>`：
```cpp
using UbseMemOptReqSimpoPtr = Ref<UbseMemOptReqSimpo>;  // 智能指针，非裸指针
```

## 已知别名

`UbseMemOptReqSimpoPtr`, `UbseBaseMessagePtr`, `UbseMemOptResultSimpoPtr`, `UbseMemOperationRespSimpoPtr`, `UbseMemShareBorrowReqSimpoPtr`, `UbseMemShareAttachReqSimpoPtr`, `UbseMemShareDetachReqSimpoPtr`, `UbseMemReturnReqSimpoPtr`, `UbseComBaseMessageHandlerPtr` — 均为 `Ref<T>` 别名。

## 判定

- `XxxPtr = new ...` → `Ref<T>` 析构自动释放，**不是泄漏**
- `Xxx* = new ...` → 裸指针，需手动 `delete`，**是泄漏**
