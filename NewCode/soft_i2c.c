#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/gpio.h> // Thư viện GPIO bắt buộc cho Hardware

/* ================= CẤU HÌNH ================= */
#define SIMULATION_MODE 0  // 0: Chạy thật trên Pi, 1: Giả lập trên PC
#define SDA_PIN 2          // GPIO 2 (Pin 3 vật lý)
#define SCL_PIN 3          // GPIO 3 (Pin 5 vật lý)
#define I2C_DELAY_US 5     // Tốc độ ~100kHz (Chuẩn)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("User");

/* ================= HÀM HỖ TRỢ (PRIVATE) ================= */

/* Hàm tạo độ trễ chuẩn */
static void i2c_delay(void) {
    udelay(I2C_DELAY_US);
}

/* Hàm set SDA là Output hay Input */
static void sda_out(void) {
    gpio_direction_output(SDA_PIN, 1); 
}

static void sda_in(void) {
    gpio_direction_input(SDA_PIN);
}

/* Hàm set SCL là Output */
static void scl_out(void) {
    gpio_direction_output(SCL_PIN, 1);
}

/* ================= CÁC HÀM EXPORT (PUBLIC) ================= */

/* 1. START Condition: SDA High -> Low khi SCL High */
void my_i2c_start(void) {
#if SIMULATION_MODE
    return;
#else
    sda_out();
    scl_out();
    
    gpio_set_value(SDA_PIN, 1);
    gpio_set_value(SCL_PIN, 1);
    i2c_delay();
    
    gpio_set_value(SDA_PIN, 0); // START
    i2c_delay();
    
    gpio_set_value(SCL_PIN, 0); // Giữ SCL thấp để bắt đầu truyền
#endif
}
EXPORT_SYMBOL(my_i2c_start);

/* 2. STOP Condition: SDA Low -> High khi SCL High */
void my_i2c_stop(void) {
#if SIMULATION_MODE
    return;
#else
    sda_out();
    
    gpio_set_value(SDA_PIN, 0);
    gpio_set_value(SCL_PIN, 1);
    i2c_delay();
    
    gpio_set_value(SDA_PIN, 1); // STOP
    i2c_delay();
#endif
}
EXPORT_SYMBOL(my_i2c_stop);

/* 3. WAIT ACK: Trả về 1 nếu thành công (ACK), 0 nếu thất bại (NACK) */
int my_i2c_wait_ack(void) {
#if SIMULATION_MODE
    return 1;
#else
    int ack_bit = 0;
    
    // 1. Master thả SDA ra để Slave kéo
    sda_in(); 
    i2c_delay();
    
    // 2. Kéo SCL lên cao để đọc bit ACK
    gpio_set_value(SCL_PIN, 1);
    i2c_delay();
    
    // 3. Đọc giá trị SDA
    if (gpio_get_value(SDA_PIN) == 0) {
        ack_bit = 1; // Slave kéo xuống thấp -> ACK OK
    } else {
        ack_bit = 0; // Slave để cao -> NACK (Lỗi/Không thấy thiết bị)
        // IN LỖI RA LOG HỆ THỐNG ĐỂ NGƯỜI DÙNG BIẾT
        printk(KERN_ERR "Soft_I2C_Error: NACK detected! Sensor missing or wrong address.\n");
    }
    
    // 4. Kéo SCL xuống thấp kết thúc chu kỳ
    gpio_set_value(SCL_PIN, 0);
    i2c_delay();
    
    // 5. Master lấy lại quyền điều khiển SDA
    sda_out();
    
    return ack_bit;
#endif
}
EXPORT_SYMBOL(my_i2c_wait_ack);

/* 4. WRITE BYTE: Ghi 8 bit ra bus */
void my_i2c_write_byte(unsigned char byte) {
#if SIMULATION_MODE
    return;
#else
    int i;
    sda_out();
    
    for (i = 0; i < 8; i++) {
        // Kiểm tra bit thứ 7 (MSB)
        if ((byte << i) & 0x80) {
            gpio_set_value(SDA_PIN, 1);
        } else {
            gpio_set_value(SDA_PIN, 0);
        }
        
        // Tạo xung clock
        i2c_delay(); // Ổn định dữ liệu
        gpio_set_value(SCL_PIN, 1); // Clock High
        i2c_delay();
        gpio_set_value(SCL_PIN, 0); // Clock Low
        i2c_delay();
    }
#endif
}
EXPORT_SYMBOL(my_i2c_write_byte);

/* 5. READ BYTE: Đọc 8 bit từ bus */
unsigned char my_i2c_read_byte(unsigned char ack) {
#if SIMULATION_MODE
    static int val = 0;
    val++;
    return (val * 10) & 0xFF;
#else
    unsigned char byte = 0;
    int i;
    
    sda_in(); // Chuyển sang Input để nhận dữ liệu
    
    for (i = 0; i < 8; i++) {
        gpio_set_value(SCL_PIN, 1); // Clock High
        i2c_delay();
        
        byte <<= 1;
        if (gpio_get_value(SDA_PIN)) {
            byte |= 1;
        }
        
        gpio_set_value(SCL_PIN, 0); // Clock Low
        i2c_delay();
    }
    
    // Gửi ACK/NACK trả lời Slave
    sda_out(); // Quay lại Output
    
    if (ack) {
        gpio_set_value(SDA_PIN, 0); // ACK (0) -> Muốn đọc tiếp
    } else {
        gpio_set_value(SDA_PIN, 1); // NACK (1) -> Dừng
    }
    
    i2c_delay();
    gpio_set_value(SCL_PIN, 1);
    i2c_delay();
    gpio_set_value(SCL_PIN, 0);
    i2c_delay();
    
    return byte;
#endif
}
EXPORT_SYMBOL(my_i2c_read_byte);

/* ================= INIT & EXIT ================= */
static int __init my_i2c_init(void) {
#if !SIMULATION_MODE
    int ret;
    
    // Đăng ký GPIO 2
    ret = gpio_request(SDA_PIN, "SOFT_I2C_SDA");
    if (ret) {
        printk(KERN_ERR "Soft I2C: Failed to request SDA (%d). Error %d\n", SDA_PIN, ret);
        return ret;
    }
    
    // Đăng ký GPIO 3
    ret = gpio_request(SCL_PIN, "SOFT_I2C_SCL");
    if (ret) {
        printk(KERN_ERR "Soft I2C: Failed to request SCL (%d). Error %d\n", SCL_PIN, ret);
        gpio_free(SDA_PIN);
        return ret;
    }

    // Set trạng thái ban đầu: Cả 2 đều cao (Idle)
    gpio_direction_output(SDA_PIN, 1);
    gpio_direction_output(SCL_PIN, 1);
    
    printk(KERN_INFO "Soft I2C: Driver Loaded on GPIO %d & %d. Ready!\n", SDA_PIN, SCL_PIN);
#else
    printk(KERN_INFO "Soft I2C: Simulation Mode Loaded.\n");
#endif
    return 0;
}

static void __exit my_i2c_exit(void) {
#if !SIMULATION_MODE
    gpio_free(SDA_PIN);
    gpio_free(SCL_PIN);
    printk(KERN_INFO "Soft I2C: Driver Unloaded.\n");
#endif
}

module_init(my_i2c_init);
module_exit(my_i2c_exit);
