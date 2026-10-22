/*----------------------------------------------+
 |												|
 |	ivPlatform.h - InterSound 4 Platform Config |
 |												|
 |	    	Platform:       					|
 |												|
 |		Copyright (c) 1999-2007, iFLYTEK Ltd.	|
 |		All rights reserved.					|
 |												|
 +----------------------------------------------*/
#ifndef __IFLY_FIXFRONT_IVPLATFORM_H__
#define __IFLY_FIXFRONT_IVPLATFORM_H__

/*
 *	TODO: 在这里包含目标平台程序需要的公共头文件
 */
#include <assert.h>

/*
 *	TODO: 根据目标平台特性修改下面的配置选项
 */


#define IV_UNIT_BITS			8			/* 内存基本单元位数 */
#define IV_BIG_ENDIAN			0			/* 是否是 Big-Endian 字节序 */
//所有的android 64位编译器都会自定义__LP64__宏, modify by zhepan 2015/11/5
#if defined (_WIN64) || defined (LINUX64) || defined (IOS64) || (defined (__ANDROID__) && defined (__LP64__))
#define IV_PTR_GRID				8			/* 最大指针对齐值 */
#else
#define IV_PTR_GRID				4			/* 最大指针对齐值 */
#endif

#define IV_PTR_PREFIX						/* 指针修饰关键字(典型取值有 near | far, 可以为空) */
#define IV_CONST				const		/* 常量关键字(可以为空) */
#define IV_EXTERN				extern		/* 外部关键字 */
#define IV_STATIC				static		/* 静态函数关键字(可以为空) */
#define IV_INLINE				__inline	/* 内联关键字(典型取值有 inline, 可以为空) */
#define IV_CALL_STANDARD					/* 普通函数修饰关键字(典型取值有 stdcall | fastcall | pascal, 可以为空) */
#define IV_CALL_REENTRANT					/* 递归函数修饰关键字(典型取值有 stdcall | reentrant, 可以为空) */
#define IV_CALL_VAR_ARG						/* 变参函数修饰关键字(典型取值有 cdecl, 可以为空) */

#define IV_TYPE_INT8			char		/* 8位数据类型 */
#define IV_TYPE_INT16			short		/* 16位数据类型 */
#define IV_TYPE_INT24			int			/* 24位数据类型 */
#define IV_TYPE_INT32			int			/* 32位数据类型 */
#define IV_TYPE_INT64			long long			/* 32位数据类型 */
//所有的android 64位编译器都会自定义__LP64__宏, modify by zhepan 2015/11/5
#if defined (_WIN64) || defined (LINUX64) || defined (IOS64) || (defined (__ANDROID__) && defined (__LP64__))
#define IV_TYPE_ADDRESS			unsigned long long		/* 地址数据类型 */
#define IV_TYPE_SIZE			unsigned long long		/* 大小数据类型 */
#else
#define IV_TYPE_ADDRESS			unsigned int		/* 地址数据类型 */
#define IV_TYPE_SIZE			unsigned int		/* 大小数据类型 */
#endif

#define IV_ANSI_MEMORY			0			/* 是否使用 ANSI 内存操作库 */
#define	IV_ANSI_STRING			0			/* 是否使用 ANSI 字符串操作库 */

#define IV_ASSERT(exp)			assert(exp) /* 断言操作 */
#define IV_YIELD				/* 空闲操作(在协作式调度系统中应定义为任务切换调用, 可以为空) */

#if defined(DEBUG) || defined(_DEBUG)
	#define IV_DEBUG			1			/* 是否支持调试 */
#else
	#define IV_DEBUG			0			/* 是否支持调试 */
#endif


#define IV_UNICODE			1			/* 是否以 Unicode 方式构建 */

#if defined (WIN32)

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

static void OutputLogEx(char* lpFmt, ...)
{
    char szMsg[1024] = {0};
    char szLog[1024] = {0};
    va_list args;
    DWORD dwProcID = GetCurrentProcessId();
    DWORD dwThreadID = GetCurrentThreadId();
    DWORD dwTime = GetTickCount();

    va_start(args, lpFmt);
  //  vsprintf(szMsg, lpFmt, args );
	vsprintf_s(szMsg, 1024, lpFmt, args);
   // sprintf(szLog, "[ProcessID:0x%08X | ThreadID:0x%08X][Time:%d] %s\n", dwProcID, dwThreadID, dwTime, szMsg);
	sprintf_s(szLog, 1024, "[ProcessID:0x%08X | ThreadID:0x%08X][Time:%d] %s\n", dwProcID, dwThreadID, dwTime, szMsg);

    OutputDebugStringA(szLog);

    va_end(args);
}

#define LOGD(fmt, ...)  OutputLogEx(fmt, __VA_ARGS__);

#elif defined (__ANDROID__)

#include <android/log.h>
#define LOGD(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, "LOG_FROM_CMODULE", __VA_ARGS__))

#else

#include <stdio.h>
#define LOGD(fmt, ...) printf(fmt, __VA_ARGS__);

#endif //WIN32

static int checkCPU( )
 {
     {
         union w
         {  
             int a;
             char b;
         } c;
         c.a = 1;
         return(c.b ==1);
     }
 }


#endif /* __IFLY_FIXFRONT_IVPLATFORM_H__ */
