/*
  suli2.c/suli2.cpp
  Seeed Unified Library Interface v2.0
  2015 Copyright (c) Seeed Technology Inc.  All right reserved.

  Author:Jack Shao, Loovee
  Change Logs:
  2015-4-17: initial version for v2

  suli is designed for the purpose of reusing the high level implementation
  of each libraries for different platforms out of the hardware layer.
  suli2 is the new reversion of original suli. There're lot of improvements upon
  the previous version. Currently, it can be treated as the internal strategy for
  quick library development of seeed. But always welcome the community to
  follow the basis of suli to contribute grove libraries.

  The MIT License (MIT)

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#include "suli2.h"

//---------------------------------------- common ---------------------------------------------
long suli_map(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}



//---------------------------------------- mbed ---------------------------------------------
#if defined(__MBED__)

/**
 * uint32_t suli_pin_pulse_in(IO_T *, what_state, timeout)
 */
uint32_t suli_pin_pulse_in(IO_T *pio, int state, uint32_t timeout)
{
    //TODO: more efficient implementation
    uint32_t t = us_ticker_read();
    while (gpio_read(pio) != state)
    {
        if (timeout > 0 && (us_ticker_read() - t) > timeout) return 0;
    }
    uint32_t t1 = us_ticker_read();
    while (gpio_read(pio) == state)
    {
        if (timeout > 0 && (us_ticker_read() - t) > timeout) return 0;
    }
    return us_ticker_read() - t1 /*- ??? some wasting code consumes some time */;
}

/**
 * write a buff to reg adress started from reg_addr
 * I2C dev_addr: 8bits address
 * TODO: not tested
 */
uint8_t suli_i2c_write_reg(I2C_T *i2c_device, uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, int len)
{
    uint8_t *buf = (uint8_t *)malloc(len + 1);
    *buf = reg_addr;
    memcpy(buf + 1, data, len);
    uint8_t wlen = i2c_write(i2c_device, (int)dev_addr, (const char *)buf, len+1, 1);
    free(buf);
    return wlen;
}

/**
 * read data from I2C's reg_adress
 * dev_addr: 8bits address
 */
uint8_t suli_i2c_read(I2C_T *i2c_device, uint8_t dev_addr, uint8_t reg_addr, uint8_t *buff, int len)
{
    i2c_write(i2c_device, (int)dev_addr, (const char *)&reg_addr, 1, 0); //not send stop bit
    return i2c_read(i2c_device, (int)dev_addr, (char *)buff, len, 1);
}

/**
 * TODO: suli_i2c_read_reg
 * @Jacky
 */

/**
 * send bytes to uart
 * int suli_uart_write_bytes(UART_T *, uint8_t *, int)
 */
int suli_uart_write_bytes(UART_T *uart, uint8_t *data, int len)
{
    for (int i = 0; i < len;i++)
    {
        serial_putc(uart, (int)(*(data + i)) );
    }
    return len;
}

/**
 * write a float
 * num - number to write
 * decimal - x decimal point
 */
void suli_uart_write_float(UART_T *uart, float float_num, int decimal)
{
    char fmt[6];
    char buff[32];
    snprintf(fmt, 6, "%%.%df", decimal);
    int r = snprintf(buff, 32, (const char *)fmt, float_num);
    suli_uart_write_bytes(uart, (uint8_t *)buff, r);
}
/**
 * write an integer
 * num - number to write
 */
void suli_uart_write_int(UART_T *uart, int32_t num)
{
    char buff[32];
    int r = snprintf(buff, 32, "%ld", num);
    suli_uart_write_bytes(uart, (uint8_t *)buff, r);
}

/**
 * read bytes from uart
 */
int suli_uart_read_bytes(UART_T *uart, uint8_t *buff, int len)
{
    uint8_t *ptr = buff;
    uint8_t *end = ptr + len;
    while (ptr != end)
    {
        int c = serial_getc(uart);
        *ptr++ = c;
    }
    return ptr - buff;
}

/**
 * read bytes from uart with timeout ms
 */
int suli_uart_read_bytes_timeout(UART_T *uart, uint8_t *buff, int len, int timeout_ms)
{
    uint8_t *ptr = buff;
    uint8_t *end = ptr + len;
    uint32_t t = suli_millis();

    while (ptr != end)
    {
        if ((suli_millis() - t) > timeout_ms) break;
        int c = serial_getc(uart);
        *ptr++ = c;
    }
    return ptr - buff;
}



//---------------------------------------------arduino---------------------------------------------
#elif defined(ARDUINO)

#ifdef ESP8266_SEEED_NODE

void suli_pin_attach_interrupt_handler(IO_T *pio, interrupt_handler handler, int mode, void *para)
{
    attachInterruptEx(*pio, handler, mode, para);
}

#endif

#if  defined (ARDUINO_USE_I2C) || defined(ESP8266)
/**
 * I2C interface initialize.
 */
void suli_i2c_init(I2C_T *i2c_device, int pin_sda, int pin_clk)
{
#ifdef ESP8266
    *i2c_device = new TwoWire();  //change the pin defined in pin_arduino.h
    (*i2c_device)->begin(pin_sda, pin_clk);
#else
    *i2c_device = &Wire;
    (*i2c_device)->begin();
#endif
}


/**
 * write a buff to I2C
 * - i2c_device: i2c device pointer
 * - dev_addr: device address
 * - data: data buff
 * - len: data lenght
 */
uint8_t suli_i2c_write(I2C_T *i2c_device, uint8_t dev_addr, uint8_t *data, int len)
{
    dev_addr = dev_addr >> 1;

    (*i2c_device)->beginTransmission(dev_addr);          // start
    for (int i = 0; i < len; i++)
    {
        (*i2c_device)->write(data[i]);                   // send a byte
    }
    (*i2c_device)->endTransmission();                    // end

    return len;
}


/**
 * write a buff to I2C
 * - i2c_device: i2c device pointer
 * - dev_addr: device address
 * - reg_addr: register address
 * - data: data buff
 * - len: data lenght
 */
uint8_t suli_i2c_write_reg(I2C_T *i2c_device, uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, int len)
{
    dev_addr = dev_addr >> 1;

    (*i2c_device)->beginTransmission(dev_addr);          // start
    (*i2c_device)->write(reg_addr);
    for (int i = 0; i < len; i++)
    {
        (*i2c_device)->write(data[i]);                   // send a byte
    }
    (*i2c_device)->endTransmission();                    // end

    return len;
}

/**
 * read a buff to I2C
 * - i2c_device: i2c device pointer
 * - dev_addr: device address
 * - data: data buff
 * - len: data lenght
 * return
 */
uint8_t suli_i2c_read(I2C_T *i2c_device, uint8_t dev_addr, uint8_t *buff, int len)
{
    dev_addr = dev_addr >> 1;
    (*i2c_device)->requestFrom(dev_addr, (uint8_t)len);

    int sum_len = 0;
    while ((*i2c_device)->available())
    {
        buff[sum_len++] = (*i2c_device)->read();
    }
    return sum_len;
}

/**
 * read a buff to I2C
 * - i2c_device: i2c device pointer
 * - dev_addr: device address
 * - reg_addr: register address
 * - data: data buff
 * - len: data lenght
 * return
 */
uint8_t suli_i2c_read_reg(I2C_T *i2c_device, uint8_t dev_addr, uint8_t reg_addr, uint8_t *buff, int len)
{
    dev_addr = dev_addr >> 1;

    (*i2c_device)->beginTransmission(dev_addr);          // start
    (*i2c_device)->write(reg_addr);
    (*i2c_device)->endTransmission(false);                    // repeat start

    (*i2c_device)->requestFrom(dev_addr, (uint8_t)len);

    int sum_len = 0;
    while ((*i2c_device)->available())
    {
        buff[sum_len++] = (*i2c_device)->read();
    }
    return sum_len;
}

#endif

/**
 * void suli_uart_init(UART_T *, int pin_tx, int pin_rx, uint32_t baud)
 */
#if defined(ARDUINO_USE_SOFTWARE_SERIAL)
#include "SoftwareSerial.h"
#endif

void suli_uart_init(UART_T *uart, int pin_tx, int pin_rx, uint32_t baud)
{
#if defined(ESP8266)  //a
    if (pin_tx == 1 && pin_rx == 3)
    {
        *uart = (Stream *)&Serial;
        Serial.begin(baud);
    } else if (pin_tx == 2)
    {
        *uart = (Stream *)&Serial1;
        Serial1.begin(baud);
    } else
    {
        *uart = NULL;  //this will cause the program not passing the compiling
    }
#else  //a
    if (pin_tx == 1 && pin_rx == 0)
    {
#if defined(__AVR_ATmega32U4__)  //b
        *uart = (Stream *)&Serial1;
        Serial1.begin(baud);
#else  //b
        *uart = (Stream *)&Serial;
        Serial.begin(baud);
#endif  //b
    }
#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
    else if (pin_tx == 18 && pin_rx == 19)
    {
        *uart = (Stream *)&Serial1;
        Serial1.begin(baud);
    }
    else if (pin_tx == 16 && pin_rx == 17)
    {
        *uart = (Stream *)&Serial2;
        Serial2.begin(baud);
    }
    else if (pin_tx == 14 && pin_rx == 15)
    {
        *uart = (Stream *)&Serial3;
        Serial3.begin(baud);
    }
#endif  //defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)

#endif  //a

#if defined(ARDUINO_SOFTWARE_SERIAL)
    SoftwareSerial *ser = new SoftwareSerial(pin_rx, pin_tx);
    *uart = ser;
    ser->begin(baud);
#endif

}

/**
 * send bytes to uart
 * int suli_uart_write_bytes(UART_T *, uint8_t *, int)
 */
int suli_uart_write_bytes(UART_T *uart, uint8_t *data, int len)
{
    for (int i = 0; i < len; i++)
    {
        (*uart)->write(*(data + i));
    }
    return len;
}

/**
 * Timer related for arduino
 */
#ifdef ESP8266
void __suli_timer_hw_init()
{
    timer0_isr_init();
    __suli_timer_set_timeout_ticks(0xfffffff);
    __suli_timer_enable_interrupt();
}

void __suli_timer_enable_interrupt()
{
    timer0_attachInterrupt(__suli_timer_isr);
}
void __suli_timer_disable_interrupt()
{
    timer0_detachInterrupt();
}

#else
void __suli_timer_hw_init()
{
    
}
void __suli_timer_enable_interrupt()
{
    
}
void __suli_timer_disable_interrupt()
{
    
}
#endif


#endif  //defined(arduino)


/*
 * ========================================================================= 
 * Platform independent functions are implemented here 
 * ========================================================================= 
 */

/***************************************************************************
 * Event related APIs
 ***************************************************************************/
void suli_event_init(EVENT_T *event, EVENT_CALLBACK_T cb, char *name, EVENT_DATA_TYPE_T event_data_type)
{
    event->cb = cb;
    event->event_name = name;
    event->event_data_type = (int)event_data_type;
}
void suli_event_trigger(EVENT_T *event, void *event_data)
{
    if (event->cb)
    {
        (event->cb)(event->event_name, event_data, event->event_data_type);
    }
}



/***************************************************************************
 * Timer related APIs 
 */
static bool __timer_hw_inited = false;
static TIMER_T *__timer_list_head = NULL;

/**
 * Insert a pre-allocated timer entry into timer's ring 
 * This implementation eats the time elapsed before this insertion operation 
 * if there's a timer entry waiting to be fired at the head of timer entry ring. 
 *  
 * load time for head       cur time      fire time for head
 *        |                    |                 |
 * ----------------------------------------------------------> 
 *        |        eaten       |
 *  
 * So this implementation assumes that all the installation of timers are done 
 * at the very beginning and don't care about the alignment of the timer entries. 
 *  
 * @param src - the entry to be inserted
 * @param new_head - output var, indicate if the ring's head is new one
 * 
 * @return void  
 */
void ICACHE_RAM_ATTR __suli_timer_insert_entry(TIMER_T *src, bool *new_head)
{
    if (__timer_list_head)
    {
        TIMER_T *pt = __timer_list_head;
        uint32_t sum = 0;
        uint32_t dt;
        
        for (;;)
        {
            if (pt == src)
            {
                pt->interval_ticks = src->interval_ticks;
                return;
            }
            sum += pt->fire_ticks;
            if (src->interval_ticks < sum)  //insert before pt
            {
                if(pt->prev)
                {
                    dt = pt->fire_ticks - (sum - src->interval_ticks);
                    src->fire_ticks = dt == 0 ? 80 : dt;  //if 2 timers are fired the same time, the last inserted one is delayed 1us
                    src->prev = pt->prev;
                    src->prev->next = src;
                } else
                {
                    src->prev = NULL;
                    src->fire_ticks = src->interval_ticks;
                    __timer_list_head = src;
                    *new_head = true;
                }
                
                pt->fire_ticks -= src->fire_ticks;
                pt->prev = src;
                src->next = pt;

                return;
            }
            if (pt->next) pt = pt->next;
            else break;
        }
        //insert at the end
        dt = src->interval_ticks - sum;
        src->fire_ticks = (dt == 0) ? 80 : dt;  //if 2 timers are fired the same time, the last inserted one is delayed 1us
        src->next = NULL;
        src->prev = pt;
        pt->next = src;
        
    } else
    {
        src->prev = NULL;
        src->next = NULL;
        src->fire_ticks = src->interval_ticks;
        __timer_list_head = src;
        *new_head = true;
    }
}

void ICACHE_RAM_ATTR __suli_timer_isr()
{
    if (__timer_list_head)
    {
        __timer_list_head->cb(__timer_list_head->data);
        
        TIMER_T *p_last_head = NULL;
        bool new_head = false;
        
        if (__timer_list_head->repeat)
        {
            p_last_head = __timer_list_head;
        }
        
        __timer_list_head = __timer_list_head->next;
        
        if (__timer_list_head)
        {
            __timer_list_head->prev = NULL;
        }
        if (p_last_head)
        {
            __suli_timer_insert_entry(p_last_head, &new_head);
        }
        if (__timer_list_head)
        {
            __suli_timer_set_timeout_ticks(__timer_list_head->fire_ticks);
        } else
        {
            __suli_timer_set_timeout_ticks(0xfffffff);  //must update compare0, or the wdt will timeout
        }
    } else
    {
        __suli_timer_set_timeout_ticks(0xfffffff);  //must update compare0, or the wdt will timeout
    }
}

void suli_timer_install(TIMER_T *timer, uint32_t microseconds, timer_callback_t cb, void *data, bool repeat = false)
{    
    if (!__timer_hw_inited)
    {
        __suli_timer_hw_init();
        __timer_hw_inited = true;
    }
        
    bool new_head = false;
    
    timer->cb = cb;
    timer->interval_ticks = __suli_timer_microseconds_to_ticks(microseconds);
    timer->data = data;
    timer->repeat = repeat;

    __suli_timer_disable_interrupt();
    __suli_timer_insert_entry(timer, &new_head);
    __suli_timer_enable_interrupt();
    
    if (new_head)
    {
        __suli_timer_set_timeout_ticks(__timer_list_head->fire_ticks);
    }

}

void suli_timer_remove(TIMER_T *timer)
{
    if (__timer_list_head)
    {
        TIMER_T *pt = __timer_list_head;
        do
        {
            if (pt == timer)
            {
                if (pt->prev)
                {
                    pt->prev->next = pt->next;
                } else
                {
                    __timer_list_head = pt->next;
                }
                if (pt->next)
                {
                    pt->next->prev = pt->prev;
                    pt->next->fire_ticks += pt->fire_ticks;
                }
                break;
            }
        } while (pt = pt->next);
    }
}

void suli_timer_control_interval(TIMER_T *timer, uint32_t microseconds)
{
    timer->interval_ticks = __suli_timer_microseconds_to_ticks(microseconds);
}
