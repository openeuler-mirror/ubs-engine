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

#ifndef UBSE_HTTP_MODULE_H_UBSE_HTTP_COMMON_H
#define UBSE_HTTP_MODULE_H_UBSE_HTTP_COMMON_H

#include <string>
#include <unordered_map>
#include <functional>
#include "ubse_common_def.h"

const unsigned int HTTP_ENUM_BOTTOM = 0xFFFFFFF;
namespace ubse::http {
using namespace ubse::common::def;
/**
 * http方法
 */
enum class UbseHttpMethod {
    UBSE_HTTP_METHOD_GET = 3, /* *< GET方法名. */
    UBSE_HTTP_METHOD_HEAD,    /* *< HEAD方法名. */
    UBSE_HTTP_METHOD_POST,    /* *< POST方法名. */
    UBSE_HTTP_METHOD_PUT,     /* *< PUT方法名. */
    UBSE_HTTP_METHOD_DELETE,  /* *< DELETE方法名. */
    UBSE_HTTP_METHOD_TRACE,   /* *< TRACE方法名. */
    UBSE_HTTP_METHOD_CONNECT, /* *< CONNECT方法名. */
    UBSE_HTTP_METHOD_PATCH,   /* *< PATCH方法名. */
    UBSE_HTTP_METHOD_INVALID = 0xFF,
};

/**
 * @ingroup http_header
 * 响应状态码定义
 */
enum class UbseHttpStatusCode {
    UBSE_HTTP_STATUS_CODE_INVALID = -1,
    UBSE_HTTP_STATUS_CODE_BEGIN = 0,

    /* Informational:  1xx 类响应码 */
    UBSE_HTTP_STATUS_CODE_CONTINUE = 100,        /* *< Continue  */
    UBSE_HTTP_STATUS_CODE_SWITCH_PROTOCOL = 101, /* *< Switching Protocols  */
    /* Successful   :  2xx 类响应码 */
    UBSE_HTTP_STATUS_CODE_OK = 200,              /* *< OK */
    UBSE_HTTP_STATUS_CODE_CREATED = 201,         /* *< Created  */
    UBSE_HTTP_STATUS_CODE_ACCEPTED = 202,        /* *< Accepted  */
    UBSE_HTTP_STATUS_CODE_NON_AUTH_INFO = 203,   /* *< Non-Authoritative Information */
    UBSE_HTTP_STATUS_CODE_NO_CONTENT = 204,      /* *< No Content */
    UBSE_HTTP_STATUS_CODE_RESET_CONTENT = 205,   /* *< Reset Content  */
    UBSE_HTTP_STATUS_CODE_PARTIAL_CONTENT = 206, /* *< Partial Content  */
    /* Redirection  :  3xx 类响应码 */
    UBSE_HTTP_STATUS_CODE_MULTI_CHOICE = 300, /* *< Multiple Choices */
    UBSE_HTTP_STATUS_CODE_MOVED = 301,        /* *< Moved Permanently */
    UBSE_HTTP_STATUS_CODE_FOUND = 302,        /* *< Found  */
    UBSE_HTTP_STATUS_CODE_SEE_OTHER = 303,    /* *< See Other */
    UBSE_HTTP_STATUS_CODE_NOT_MODIFIED = 304, /* *< Not Modified  */
    UBSE_HTTP_STATUS_CODE_USE_PROXY = 305,    /* *< Use Proxy  */
    UBSE_HTTP_STATUS_CODE_UNUSED = 306,       /* *< Unused */
    UBSE_HTTP_STATUS_CODE_TEMP_REDIR = 307,   /* *< Temporary Redirect  */
    /* Client Error :  4xx 类响应码 */
    UBSE_HTTP_STATUS_CODE_BAD_REQ = 400,               /* *< Bad Request  */
    UBSE_HTTP_STATUS_CODE_UNAUTH = 401,                /* *< Unauthorized  */
    UBSE_HTTP_STATUS_CODE_PAY_REQUIRED = 402,          /* *< Payment Required */
    UBSE_HTTP_STATUS_CODE_FORBIDDEN = 403,             /* *< Forbidden  */
    UBSE_HTTP_STATUS_CODE_NOT_FOUND = 404,             /* *< Not Found */
    UBSE_HTTP_STATUS_CODE_METHOD_NOT_ALLOWED = 405,    /* *< Method Not Allowed */
    UBSE_HTTP_STATUS_CODE_NOT_ACCEPTABLE = 406,        /* *< Not Acceptable */
    UBSE_HTTP_STATUS_CODE_PXY_AUTH_REQUIRED = 407,     /* *< Proxy Authentication Required */
    UBSE_HTTP_STATUS_CODE_REQ_TIMEOUT = 408,           /* *< Request Timeout  */
    UBSE_HTTP_STATUS_CODE_CONFICT = 409,               /* *< Conflict  */
    UBSE_HTTP_STATUS_CODE_GONE = 410,                  /* *< Gone  */
    UBSE_HTTP_STATUS_CODE_LEN_REQUIRED = 411,          /* *< Length Required  */
    UBSE_HTTP_STATUS_CODE_PRECOND_FAILED = 412,        /* *< Precondition Failed */
    UBSE_HTTP_STATUS_CODE_ENTITY_TOO_LARGE = 413,      /* *< Request Entity Too Large  */
    UBSE_HTTP_STATUS_CODE_URI_TOO_LONG = 414,          /* *< Request-URI Too Long */
    UBSE_HTTP_STATUS_CODE_UNSUPPORT_MEDIA = 415,       /* *< Unsupported Media Type */
    UBSE_HTTP_STATUS_CODE_RANGE_NOT_SATISFY = 416,     /* *< Requested Range Not Satisfiable */
    UBSE_HTTP_STATUS_CODE_EXCEPT_FAILED = 417,         /* *< Expectation Failed  */
    UBSE_HTTP_STATUS_CODE_UNPROCESSABLE_REQUEST = 422, /* *< Unprocessable Request */
    UBSE_HTTP_STATUS_CODE_FAILED_DEPENDENCY = 424,     /* *< Failed Dependency */
    UBSE_HTTP_STATUS_CODE_TOO_MANY_REQUESTS = 429,     /* *< Too Many Requests */
    /* Server Error :  5xx 类响应码 */
    UBSE_HTTP_STATUS_CODE_INTERNAL_SVR_ERR = 500,   /* *< Internal Server Error */
    UBSE_HTTP_STATUS_CODE_NOT_IMPLEMENTED = 501,    /* *< Not Implemented */
    UBSE_HTTP_STATUS_CODE_BAD_GATEWAY = 502,        /* *< Bad Gateway */
    UBSE_HTTP_STATUS_CODE_SERVICE_UNVAILABLE = 503, /* *< Service Unavailable  */
    UBSE_HTTP_STATUS_CODE_GATEWAY_TIMEOUT = 504,    /* *< Gateway Timeout */
    UBSE_HTTP_STATUS_CODE_HTTP_VER_NET_SUPP = 505,  /* *< HTTP Version Not Supported  */
    /* 其它 */
    UBSE_HTTP_STATUS_CODE_MAX_STATUS_CODE = 599, /* *< HTTP Max status code  */
    UBSE_HTTP_STATUS_CODE_END,

    UBSE_HTTP_STATUS_CODE_BOTTOM = HTTP_ENUM_BOTTOM
};

struct UbseHttpResponse {
    std::unordered_multimap<std::string, std::string> headers;
    std::string body;
    int status;
};

struct UbseHttpRequest {
    std::string method;
    std::string path;
    std::multimap<std::string, std::string> params;
    std::unordered_multimap<std::string, std::string> headers;
    std::string body;
};

/**
 * http回调函数类型
 */
using UbseHttpHandlerFunc = std::function<uint32_t(const UbseHttpRequest &req, UbseHttpResponse &resp)>;

std::string UbseHttpMethodToString(UbseHttpMethod method);

UbseHttpMethod StringToUbseHttpMethod(std::string method);
}


#endif // UBSE_HTTP_MODULE_H_UBSE_HTTP_COMMON_H
