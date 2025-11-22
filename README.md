# Operating_System_Project_Nhom3
Members: Nguyen Tien Dat (leader), Hoang Tuan Hung, Nguyen Xuan Hieu, Vu Tien Dat, Nguyen Ho Trieu Duong

#MPU6050
cd ~mpu_project/kernel
make
sudo insmod mpu6050_kmod.ko
dmesg
cd cd ~mpu_project/user
make
sudo ./mpu_monitor
q
sudo rmmod mpu6050_kmod

#OLED SSD1306
cd ~oled
make
sudo insmod ssd1306_i2c.ko
dmesg
gcc -O2 -o test_ssd1306_write test_ssd1306_write.c -lm
sudo ./test_ssd1306_write
