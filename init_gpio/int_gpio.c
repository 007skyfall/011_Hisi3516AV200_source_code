#include <linux/init.h>  
#include <linux/module.h>  
#include <linux/fs.h>  
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqreturn.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/io.h>
#define DEBUG
#ifdef DEBUG
# define PRINTK(fmt, arg...)  printk(fmt, ##arg)
#else
# define PRINTK(fmt, arg...)  do {} while(0)
#endif
#define GPIO_15_BASE   0x20240000         //for GPIO15
#define GPIO_DATA(o)   IO_ADDRESS(GPIO_15_BASE + (1<<(o+2)))
#define GPIO_DIR       IO_ADDRESS(GPIO_15_BASE + 0x400)
#define GPIO_IS           IO_ADDRESS(GPIO_15_BASE + 0x404)
#define GPIO_IBE       IO_ADDRESS(GPIO_15_BASE + 0x408)
#define GPIO_IEV       IO_ADDRESS(GPIO_15_BASE + 0x40C)
#define GPIO_IE        IO_ADDRESS(GPIO_15_BASE + 0x410)      
#define GPIO_RIS       IO_ADDRESS(GPIO_15_BASE + 0x414)     
#define GPIO_MIS       IO_ADDRESS(GPIO_15_BASE + 0x418)      
#define GPIO_IC        IO_ADDRESS(GPIO_15_BASE + 0x41C)     
#define GPIO_AFSEL     IO_ADDRESS(GPIO_15_BASE + 0x420)      
#define GPIO_15_2        122
#define GPIO_15_3        123
#define GPIO_IRQ            116
struct gpc_dev {
    spinlock_t        lock; 
    struct miscdevice *misc_dev;
};
struct miscdevice this_misc =   
{  
    .minor  =   MISC_DYNAMIC_MINOR,  
    .name   =   "higpio",  
    .nodename = "higpio"  
};  
struct gpc_dev dev = 
{
    .misc_dev = &this_misc,
};
static int gpio_offset(int gpio, int *offset)
{
    if(offset)
        *offset = gpio%8;
    return 0;    
}
static int get_value(int gpio)
{
    int offset;    
    if(gpio_offset(gpio, &offset) !=0)
        return -1;
    return readl(GPIO_DATA(offset)) & (1<<offset)? 1 : 0;
}
static int gpio_input(int gpio)
{
    u32 value;
    int offset;
    if(gpio_offset(gpio, &offset) !=0)
        return -1;
    spin_lock(&dev.lock);
    value = readl(GPIO_DIR);
    value &= ~(1<<offset);
    writel(value, GPIO_DIR);
    spin_unlock(&dev.lock);
    return 0;
}
static int regs_set_gpio_irq_type(int gpio, unsigned int trigger)
{
    u32 value, val;
    unsigned long flags;
    int offset;
    if(gpio_offset(gpio, &offset) !=0 )
        return -1;
    spin_lock_irqsave(&dev.lock,flags);
    value = readl(GPIO_IS);
    if (trigger & (IRQ_TYPE_LEVEL_HIGH | IRQ_TYPE_LEVEL_LOW)) {
        value |= (1<<offset);
         PRINTK("trigger is level\n");
    }
    else { 
        value &= ~(1<<offset);
         PRINTK("trigger is edge\n");
    }
    writel(value, GPIO_IS);
    value = readl(GPIO_IBE);
    if ((trigger & IRQF_TRIGGER_RISING) && (trigger & IRQ_TYPE_EDGE_FALLING)) {
        value |= (1<<offset);
        PRINTK("trigger both\n");
    }
    else {
        value &= ~(1<<offset);
        val = readl(GPIO_IEV);
        if( trigger & (IRQ_TYPE_LEVEL_LOW  | IRQ_TYPE_EDGE_FALLING))
            val &= ~(1<<offset);
        else
            val |= (1<<offset);
        writel(val, GPIO_IEV);
    }
    writel(value, GPIO_IBE);
    spin_unlock_irqrestore(&dev.lock, flags);
    return 0;
}
static int regs_enable_gpio_irq(int gpio)
{
    u32 value;
    unsigned long flags;
    int offset;
    if(gpio_offset(gpio, &offset) !=0 )
        return-1;
    spin_lock_irqsave(&dev.lock,flags);
    writel(0xff, GPIO_IC);
    value = readl(GPIO_IE);
    value |= (1<<offset);
    writel(value, GPIO_IE);
    spin_unlock_irqrestore(&dev.lock, flags);
    return 0;
}
static void regs_clean_gpio_irq(int gpio)
{
    int offset;
    u32 value;
    if(gpio_offset(gpio, &offset) !=0 )
        return ;
    value = readl(GPIO_IC);
    value |=(1<<offset);
    writel(value, GPIO_IC);
}
static void regs_disable_gpio_irq(int gpio)
{
    u32 value;
    unsigned long flags;
    int offset;
    if(gpio_offset(gpio, &offset) !=0 )
        return;
    spin_lock_irqsave(&dev.lock,flags);
    value = readl(GPIO_IE);
    value &= ~(1<<offset);
    writel(value, GPIO_IE);
    spin_unlock_irqrestore(&dev.lock, flags);
}
static irqreturn_t this_irq_handler(int irq, void* dev_id)
{
    u32 value;
    int i, gpio=120;
    value = readl(GPIO_RIS);
    PRINTK(KERN_NOTICE "gpio：irq=%d, value=%xn",irq, value);
    for(i=0;  i<8; i++) {
        if(value & 0x1){ 
                gpio += i;
                break;
        }
        else {
                value=value>>1;
       }
    }            
    PRINTK(KERN_NOTICE "gpio：gpio=%d\n",gpio);
    regs_clean_gpio_irq(gpio);
    return IRQ_HANDLED;
}
static int this_enable_irq(void)
{
        int err = 0;
        gpio_input(GPIO_15_2);    //设置GPIO_15_2为输入
        gpio_input(GPIO_15_3);
        regs_set_gpio_irq_type(GPIO_15_2, IRQ_TYPE_EDGE_FALLING|IRQF_TRIGGER_RISING);  //设置为单边沿的下降沿出发
        regs_set_gpio_irq_type(GPIO_15_3, IRQ_TYPE_EDGE_FALLING|IRQF_TRIGGER_RISING); //设置为单边沿的下降沿出发
        regs_enable_gpio_irq(GPIO_15_2);      //使能GPIO中断
        regs_enable_gpio_irq(GPIO_15_3);    //使能GPIO中断
        err = request_irq(GPIO_IRQ, this_irq_handler, 0, "higpio", NULL);
        if (err != 0) {
            PRINTK(KERN_ERR "gpc: failed to request irq, err: %d\n", err);
            err = -EBUSY;
        }    
    return err;
}
static void this_disable_irq(void)
{
        regs_disable_gpio_irq(GPIO_15_2);
        regs_disable_gpio_irq(GPIO_15_3);
        free_irq(GPIO_IRQ, NULL);
}
static int __init gpio_init(void)
{
    int err;
    if(this_enable_irq() !=0)
        return -EBUSY;
    spin_lock_init(&dev.lock);
    err = misc_register(dev.misc_dev); 
    return err;
}
static void __exit gpio_exit(void)
{
    this_disable_irq();
    misc_deregister(dev.misc_dev);  
}
module_init(gpio_init);
module_exit(gpio_exit);
MODULE_AUTHOR("GoodMan<2757364047@qq.com>");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION("V1.00");