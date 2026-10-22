#include "stm32f10x.h"

#include "led.h"
#include "beep.h"
#include "key.h"
#include "delay.h"
#include "usart1.h"

/*
项目中需要导入 RT_Thread，必须引入 rtthread.h 头文件。
*/
#include "rtthread.h"

/*
字符串展示函数
*/
void show_string(const char *str);

void thread_task1(void *param);
void thread_task2(void *param);

/*
定义 Mutex 互斥量变量
*/
rt_mutex_t mutex = RT_NULL;

void release_resource(void);

int main(void)
{
    /*
    【互斥量操作】
        创建 Mutex 互斥量
    */
    mutex = rt_mutex_create("mutex1", RT_IPC_FLAG_FIFO);
    if (RT_NULL == mutex)
    {
        rt_kprintf("[rt_mutex_create] Create Mutex [mutex1] Failed!\r\n");
    }

    rt_thread_t tid1 = rt_thread_create("task1", thread_task1, "ABCDEFG", 128, 5, 100);
    if (RT_NULL == tid1)
    {
        rt_kprintf("[rt_thread_create] Create Thread [task1] Failed!\r\n");
    }

    rt_thread_t tid2 = rt_thread_create("task2", thread_task2, "1234567", 128, 10, 100);
    if (RT_NULL == tid2)
    {
        rt_kprintf("[rt_thread_create] Create Thread [task2] Failed!\r\n");
    }

    rt_thread_startup(tid2);
    rt_thread_startup(tid1);


    while (1)
    {
        rt_thread_delay(1000);
    }
}

void show_string(const char *str)
{
    while (*str)
    {
        rt_kprintf("%c", *str);
        str += 1;
        rt_thread_delay(200);
    }

    rt_kprintf("\r\n");

}

void thread_task1(void *param)
{
    int n = 5;
    while (n--)
    {
        /*
        【互斥量操作】
            Mutex 落锁
        */
        rt_err_t err_code = rt_mutex_take(mutex, RT_WAITING_FOREVER);
        if (err_code != RT_EOK)
        {
            rt_kprintf("[rt_mutex_take] Take Mutex Failed!\r\n");
        }

        show_string((const char *)param);

        /*
        【互斥量操作】
            Mutex 解锁
        */
        err_code = rt_mutex_release(mutex);
        if (err_code != RT_EOK)
        {
            rt_kprintf("[rt_mutex_release] Release Mutex Failed!\r\n");
        }
    }
}

void thread_task2(void *param)
{
    int n = 5;
    while (n--)
    {
        /*
        【互斥量操作】
            Mutex 落锁
        */
        rt_err_t err_code = rt_mutex_take(mutex, RT_WAITING_FOREVER);
        if (err_code != RT_EOK)
        {
            rt_kprintf("[rt_mutex_take] Take Mutex Failed!\r\n");
        }

        show_string((const char *)param);
        /*
       【互斥量操作】
           Mutex 解锁
       */
        err_code = rt_mutex_release(mutex);
        if (err_code != RT_EOK)
        {
            rt_kprintf("[rt_mutex_release] Release Mutex Failed!\r\n");
        }
    }
}

void release_resource(void)
{
    // 关闭相关资源
    rt_mutex_delete(mutex);
}