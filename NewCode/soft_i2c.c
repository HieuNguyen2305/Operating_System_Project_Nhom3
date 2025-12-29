#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <asm/io.h> // Thư viện quan trọng để truy cập thanh ghi vật lý

/* ================= CẤU HÌNH ================= */
#define SIMULATION_MODE 0  // 0: Chạy thật (Direct HW), 1: Giả lập
#define SDA_PIN 2          // GPIO 2
#define SCL_PIN 3          // GPIO 3
#define I2C_DELAY_US 5     // Tốc độ ~100kHz

/* ================= ĐỊA CHỈ PHẦN CỨNG RASPBERRY PI 4 ================= */
/* Lưu ý: RPi 4 dùng base 0xFE200000. RPi 3 dùng 0x3F200000 */
#define BCM2711_GPIO_BASE  0xFE200000
#define BLOCK_SIZE         4096

/* Offset các thanh ghi GPIO */
#define GPFSEL0   0x00     // Chọn Function cho Pin 0-9
#define GPSET0    0x1C     // Set Output High (Pin 0-31)
#define GPCLR0    0x28     // Set Output Low (Pin 0-31)
#define GPLEV0    0x34     // Đọc giá trị Input (Pin 0-31)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("User");

/* Biến toàn cục lưu địa chỉ ảo sau khi map */
static void __iomem *gpio_base;

/* ================= HÀM ĐIỀU KHIỂN GPIO TRỰC TIẾP (Low Level) ================= */

/* 1. Hàm map bộ nhớ vật lý */
static int direct_gpio_init(void) {
#if !SIMULATION_MODE
    gpio_base = ioremap(BCM2711_GPIO_BASE, BLOCK_SIZE);
    if (!gpio_base) {
        printk(KERN_ERR "Soft_I2C: Loi map bo nho GPIO!\n");
        return -ENOMEM;
    }
#endif
    return 0;
}

/* 2. Hàm hủy map */
static void direct_gpio_exit(void) {
#if !SIMULATION_MODE
    if (gpio_base) {
        iounmap(gpio_base);
    }
#endif
}

/* 3. Hàm set chế độ Input/Output
   mode: 0 = Input, 1 = Output
*/
static void direct_set_mode(int pin, int mode) {
#if !SIMULATION_MODE
    u32 val;
    u32 reg_offset = GPFSEL0 + (pin / 10) * 4; // Tìm thanh ghi quản lý pin
    u32 shift = (pin % 10) * 3;                // Tìm vị trí 3 bit của pin đó

    val = readl(gpio_base + reg_offset);
    val &= ~(7 << shift); // Xóa 3 bit cũ (Mặc định về 000 - Input)
    
    if (mode == 1) {
        val |= (1 << shift); // Set bit thành 001 - Output
    }
    
    writel(val, gpio_base + reg_offset);
#endif
}

/* 4. Hàm ghi mức cao (High) */
static void direct_set_high(int pin) {
#if !SIMULATION_MODE
    writel(1 << pin, gpio_base + GPSET0);
#endif
}

/* 5. Hàm ghi mức thấp (Low) */
static void direct_set_low(int pin) {
#if !SIMULATION_MODE
    writel(1 << pin, gpio_base + GPCLR0);
#endif
}

/* 6. Hàm đọc giá trị pin */
static int direct_get_value(int pin) {
#if !SIMULATION_MODE
    u32 val = readl(gpio_base + GPLEV0);
    return (val & (1 << pin)) ? 1 : 0;
#else
    return 1;
#endif
}

/* ================= LOGIC I2C (High Level) ================= */
/* Logic bên dưới giữ nguyên thuật toán I2C, chỉ thay thế lệnh GPIO */

static void i2c_delay(void) {
    udelay(I2C_DELAY_US);
}

static void sda_out(void) {
    direct_set_mode(SDA_PIN, 1); 
}

static void sda_in(void) {
    direct_set_mode(SDA_PIN, 0);
}

static void scl_out(void) {
    direct_set_mode(SCL_PIN, 1);
}

/* 1. START Condition */
void my_i2c_start(void) {
#if SIMULATION_MODE
    return;
#else
    sda_out();
    scl_out();
    
    direct_set_high(SDA_PIN);
    direct_set_high(SCL_PIN);
    i2c_delay();
    
    direct_set_low(SDA_PIN); // START: SDA High -> Low
    i2c_delay();
    
    direct_set_low(SCL_PIN); // Giữ SCL thấp
#endif
}
EXPORT_SYMBOL(my_i2c_start);

/* 2. STOP Condition */
void my_i2c_stop(void) {
#if SIMULATION_MODE
    return;
#else
    sda_out();
    
    direct_set_low(SDA_PIN);
    direct_set_high(SCL_PIN);
    i2c_delay();
    
    direct_set_high(SDA_PIN); // STOP: SDA Low -> High
    i2c_delay();
#endif
}
EXPORT_SYMBOL(my_i2c_stop);

/* 3. WAIT ACK */
int my_i2c_wait_ack(void) {
#if SIMULATION_MODE
    return 1;
#else
    int ack_bit = 0;
    
    sda_in(); // Thả SDA
    i2c_delay();
    
    direct_set_high(SCL_PIN);
    i2c_delay();
    
    if (direct_get_value(SDA_PIN) == 0) {
        ack_bit = 1;
    } else {
        // Có thể in lỗi, nhưng hạn chế in nhiều trong vòng lặp nhanh
    }
    
    direct_set_low(SCL_PIN);
    i2c_delay();
    
    sda_out(); // Lấy lại quyền điều khiển
    return ack_bit;
#endif
}
EXPORT_SYMBOL(my_i2c_wait_ack);

/* 4. WRITE BYTE */
void my_i2c_write_byte(unsigned char byte) {
#if SIMULATION_MODE
    return;
#else
    int i;
    sda_out();
    
    for (i = 0; i < 8; i++) {
        if ((byte << i) & 0x80) {
            direct_set_high(SDA_PIN);
        } else {
            direct_set_low(SDA_PIN);
        }
        
        i2c_delay();
        direct_set_high(SCL_PIN);
        i2c_delay();
        direct_set_low(SCL_PIN);
        i2c_delay();
    }
#endif
}
EXPORT_SYMBOL(my_i2c_write_byte);

/* 5. READ BYTE */
unsigned char my_i2c_read_byte(unsigned char ack) {
#if SIMULATION_MODE
    static int val = 0;
    val++;
    return (val * 10) & 0xFF;
#else
    unsigned char byte = 0;
    int i;
    
    sda_in(); 
    
    for (i = 0; i < 8; i++) {
        direct_set_high(SCL_PIN);
        i2c_delay();
        
        byte <<= 1;
        if (direct_get_value(SDA_PIN)) {
            byte |= 1;
        }
        
        direct_set_low(SCL_PIN);
        i2c_delay();
    }
    
    sda_out(); 
    
    if (ack) {
        direct_set_low(SDA_PIN); // ACK
    } else {
        direct_set_high(SDA_PIN); // NACK
    }
    
    i2c_delay();
    direct_set_high(SCL_PIN);
    i2c_delay();
    direct_set_low(SCL_PIN);
    i2c_delay();
    
    return byte;
#endif
}
EXPORT_SYMBOL(my_i2c_read_byte);

/* ================= MODULE INIT & EXIT ================= */
static int __init my_i2c_init(void) {
    if (direct_gpio_init() < 0) {
        return -ENOMEM;
    }
    
#if !SIMULATION_MODE
    // Thiết lập trạng thái ban đầu: Cả 2 đều Output High (Idle)
    direct_set_mode(SDA_PIN, 1);
    direct_set_mode(SCL_PIN, 1);
    direct_set_high(SDA_PIN);
    direct_set_high(SCL_PIN);
    
    printk(KERN_INFO "Soft I2C: Loaded (Direct Access Mode) on Pin %d & %d.\n", SDA_PIN, SCL_PIN);
#else
    printk(KERN_INFO "Soft I2C: Simulation Mode Loaded.\n");
#endif
    return 0;
}

static void __exit my_i2c_exit(void) {
    direct_gpio_exit();
    printk(KERN_INFO "Soft I2C: Unloaded.\n");
}

module_init(my_i2c_init);
module_exit(my_i2c_exit);
