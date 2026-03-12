# 1. UBM TOPO 变化通知接口

> 提供给UBM通知UBSE，系统topo变化的通知接口

## 基础信息
- **服务地址**：`127.0.0.1:8082`，其中端口号8082可通过配置文件修改
- **认证方式**：`双向 TLS/SSL 证书认证` 
- **数据格式**：`application/xml`

---

## 接口列表

### 1. /topolink/change/
> notify topolink changed

#### 请求
- **方法**：`POST`
- **路径**：`/topolink/change/` (例: `/users/{id}`)
- **请求头**：
  ```http
  Authorization: Bearer [token]
  Content-Type: application/xml
  ```

#### 请求参数

无

#### 请求响应

无

#### swagger定义
  ```http
openapi: 3.0.3
info:
  title: Swagger Petstore - OpenAPI 3.0
  version: 1.0.11

paths:
  /topolink/change/:
    post:
      summary: notify topolink changed
      description: notify topolink changed
      responses:
        '200':
          description: Successful operation
          content:
            application/xml:
              schema:
                type: string
                example: success

        '500':
          description: internel error
  ```
