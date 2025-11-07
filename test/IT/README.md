# IT框架使用

# 1.IT执行流程

申请资源-创建进程-注册操作函数-fork子进程-执行CMD命令-终止子进程-清理资源

# 2.新建IT流程

2.1 新建CMD命令

创建测试用例中需要用到的CMD以及CMD对应函数，如添加`CMD_TELEMETRY_REPORT`命令

1. 在枚举`TagTestCmdInfo`中新建命令`CMD_TELEMETRY_REPORT`

   ```cpp
   enum TagTestCmdInfo {
       CMD_INIT = 0,
       CMD_SERVER_START,
       CMD_CLI_START,
       CMD_SERVER_STOP,
       CMD_CLI_STOP,
       CMD_TELEMETRY_REPORT,
       CMD_MAX,
   };
   ```

   枚举值必须在`CMD_INIT`和`CMD_MAX`之间

2. 新建命令执行函数`ITestCmdTelemetryReport`

   ```cpp
   int32_t ITestCmdTelemetryReport(ProcessMmap *);
   ```

   函数必须为`int32_t Func(ProcessMmap *)`形式

3. 将函数和命令注册给相应进程

   ```cpp
   ITestCmdAndFuncRegister(CMD_TELEMETRY_REPORT, ITestCmdTelemetryReport, ProcessMode::MANAGER);
   ```

   第一个参数为CMD枚举值

   第二个参数为执行函数名

   第三个参数为命令注入的进程类型，分为`MANAGER`进程和`CLI`进程

2.2 新建IT Case

```cpp
TEST_F(TestITInit, IT_TELEMETRY_REPORT)
{
	// 调用CMD命令
	pMmap->stServerInfo.uiCmd = CMD_TELEMETRY_REPORT;
	
	// 取服务运行结果
	ITestGetServerResult(pMmap, UI_TIME_200, &iStatusM);

	// 断言检测结果
	EXPECT_EQ(UBSE_OK, iStatus);

}
```

```cpp
// 获取manager进程状态
void ITestGetServerResult(ProcessMmap *pMmap, uint32_t uiTime, int32_t *psiStatus)；
// 获取cli进程状态
void ITestGetCliResult(ProcessMmap *pMmap, uint32_t uiTime, int32_t *psiStatus)；
```



