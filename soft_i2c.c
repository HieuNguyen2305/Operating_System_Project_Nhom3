#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/gpio.h> // Thư viện điều khiển GPIO

/* QUAN TRỌNG: Đặt là 0 để chạy trên Hardware thật */
#define SIMULATION_MODE 0 

/* Cấu hình chân GPIO cho Raspberry Pi */
/* SDA = GPIO 2 (Pin 3), SCL = GPIO 3 (Pin 5) */
#define SDA_PIN 2
#define SCL_PIN 3

MODULE_LICENSE("GPL");
MODULE_AUTHOR("User");

/* Hàm tạo độ trễ ngắn để đảm bảo thiết bị kịp nhận tín hiệu */
static void i2c_delay(void) {
    udelay(5); // Đợi 5 micro-seconds
}

/* 1. Hàm tạo điều kiện START */
/* SDA chuyển từ 1 -> 0 khi SCL đang ở mức 1 */
void my_i2c_start(void) {
    gpio_direction_output(SDA_PIN, 1); // SDA = 1
    gpio_direction_output(SCL_PIN, 1); // SCL = 1
    i2c_delay();
    
    gpio_set_value(SDA_PIN, 0); // Kéo SDA xuống 0
    i2c_delay();
    
    gpio_set_value(SCL_PIN, 0); // Kéo SCL xuống 0 để giữ bus, chuẩn bị gửi dữ liệu
}
EXPORT_SYMBOL(my_i2c_start);

/* 2. Hàm tạo điều kiện STOP */
/* SDA chuyển từ 0 -> 1 khi SCL đang ở mức 1 */
void my_i2c_stop(void) {
    gpio_direction_output(SDA_PIN, 0); // Đảm bảo SDA đang thấp
    gpio_set_value(SCL_PIN, 1);        // Kéo SCL lên cao
    i2c_delay();
    
    gpio_set_value(SDA_PIN, 1);        // Kéo SDA lên cao -> STOP
    i2c_delay();
}
EXPORT_SYMBOL(my_i2c_stop);

/* 3. Hàm Gửi 1 Byte (Dùng cho cả MPU6050 và OLED) */
void my_i2c_write_byte(unsigned char byte) {
    int i;
    gpio_direction_output(SDA_PIN, 1); // Đảm bảo SDA là Output
    
    for (i = 0; i < 8; i++) {
        // Gửi từng bit từ MSB (Bit 7) đến LSB (Bit 0)
        if ((byte << i) & 0x80) {
            gpio_set_value(SDA_PIN, 1);
        } else {
            gpio_set_value(SDA_PIN, 0);
        }
        
        // Tạo xung Clock để đẩy bit đi
        gpio_set_value(SCL_PIN, 1);
        i2c_delay();
        gpio_set_value(SCL_PIN, 0);
        i2c_delay();
    }
}
EXPORT_SYMBOL(my_i2c_write_byte);

/* 4. Hàm Đợi phản hồi ACK từ thiết bị */
/* Màn hình OLED và Cảm biến sẽ kéo SDA xuống 0 nếu nhận được lệnh */
int my_i2c_wait_ack(void) {
    int ack = 0;
    
    gpio_direction_input(SDA_PIN); // Chuyển chân SDA thành Input để lắng nghe
    i2c_delay();
    
    gpio_set_value(SCL_PIN, 1); // Kéo SCL lên để đọc
    i2c_delay();
    
    if (gpio_get_value(SDA_PIN)) {
        ack = 1; // NACK (Không phản hồi/Lỗi)
    } else {
        ack = 0; // ACK (OK)
    }
    
    gpio_set_value(SCL_PIN, 0); // Kết thúc xung clock
    gpio_direction_output(SDA_PIN, 1); // Trả lại quyền điều khiển SDA cho Master
    return ack;
}
EXPORT_SYMBOL(my_i2c_wait_ack);

/* 5. Hàm Đọc 1 Byte (Dùng khi đọc dữ liệu cảm biến) */
unsigned char my_i2c_read_byte(unsigned char ack) {
    unsigned char byte = 0;
    int i;
    
    gpio_direction_input(SDA_PIN); // Chuyển SDA thành Input để nhận dữ liệu
    
    for (i = 0; i < 8; i++) {
        gpio_set_value(SCL_PIN, 1); // Clock High
        i2c_delay();
        
        byte <<= 1; // Dịch bit cũ sang trái
        if (gpio_get_value(SDA_PIN)) {
            byte |= 1; // Nếu chân SDA có điện thì bit đó là 1
        }
        
        gpio_set_value(SCL_PIN, 0); // Clock Low
        i2c_delay();
    }
    
    // Gửi tín hiệu ACK/NACK lại cho cảm biến
    gpio_direction_output(SDA_PIN, 1); // Chuyển lại Output
    
    // ack = 1: Muốn đọc tiếp (Gửi ACK = 0)
    // ack = 0: Muốn dừng (Gửi NACK = 1)
    gpio_set_value(SDA_PIN, ack ? 0 : 1);
    
    gpio_set_value(SCL_PIN, 1);
    i2c_delay();
    gpio_set_value(SCL_PIN, 0);
    
    return byte;
}
EXPORT_SYMBOL(my_i2c_read_byte);

/* Khởi tạo Driver: Cấu hình GPIO */
static int __init my_i2c_init(void) {
    int ret = 0;
    
    pr_info("Soft I2C: Initializing on SDA=%d, SCL=%d\n", SDA_PIN, SCL_PIN);

    // Kiểm tra xem GPIO có hợp lệ không
    if (!gpio_is_valid(SDA_PIN) || !gpio_is_valid(SCL_PIN)) {
        pr_err("Soft I2C: Invalid GPIO pins\n");
        return -ENODEV;
    }

    // Yêu cầu quyền điều khiển GPIO
    gpio_request(SDA_PIN, "SOFT_I2C_SDA");
    gpio_request(SCL_PIN, "SOFT_I2C_SCL");

    // Mặc định ban đầu là mức cao (Idle)
    gpio_direction_output(SDA_PIN, 1);
    gpio_direction_output(SCL_PIN, 1);
    
    return 0;
}

static void __exit my_i2c_exit(void) {
    gpio_set_value(SDA_PIN, 0);
    gpio_set_value(SCL_PIN, 0);
    gpio_free(SDA_PIN);
    gpio_free(SCL_PIN);
    pr_info("Soft I2C: Driver Unloaded\n");
}

module_init(my_i2c_init);
module_exit(my_i2c_exit);
