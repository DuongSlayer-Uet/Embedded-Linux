# Mock project
Dự án cuối module Linux
## Mô tả
Project này xây dựng hai Linux kernel driver để điều khiển LED và đọc trạng thái button thông qua GPIO.
Driver sử dụng Device Tree để cấu hình phần cứng, giúp tách biệt giữa phần mềm và phần cứng.

**Các chức năng chính:**
Điều khiển LED (bật/tắt) qua button và qua device file, đọc trạng thái hiện tại của button.

## Driver
* Led driver: khởi tạo device file cho phép người dùng thao tác write "LEDx ON/OFF" để bật tắt led x. Sử dụng export symbol để cho button driver có thể gọi.
* Button driver: Khởi tạo device file cho phép người dùng read để nhận về status hiện tại của button, sử dụng ngắt khi nút nhấn được nhấn và gọi hàm extern từ bên led driver để bật tắt led.

## Device Tree
* Overlay thêm 2 node: một node cho button và một node cho led. Trong đó led là IO46 và IO47, button là IO68 và IO69.

## APP
* Cho phép người dùng tương tác với device trên tầng user space bằng cách nhập "LEDx ON/OFF" sẽ thực hiện write hành vi tương ứng vào /dev/led_dev
* Cho phép người dùng đọc trạng thái hiện tại của button bằng cách nhập "READ BUTTON" sẽ thực hiện read device file /dev/button_dev

