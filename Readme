# OPEN-WIFI-SWITCH 🔌

![Platform](https://img.shields.io/badge/platform-ESP8266-blue.svg)
![Framework](https://img.shields.io/badge/framework-ESP8266__RTOS__SDK-green.svg)
![License](https://img.shields.io/badge/license-MIT-orange.svg)
![Status](https://img.shields.io/badge/status-maintenance-yellow)

## 📖 Giới thiệu (Introduction)

**OPEN-WIFI-SWITCH** là một dự án mã nguồn mở (Open Source) nhằm xây dựng lại Firmware (Custom Firmware) cho các thiết bị công tắc thông minh sử dụng chip ESP8266 bán phổ biến trên thị trường.

Dự án này được thực hiện với mục đích "vọc vạch" tìm hiểu sâu về ESP8266, tối ưu hóa khả năng điều khiển và loại bỏ sự phụ thuộc vào cloud của bên thứ 3.

> **Fun Fact:** Giao diện Web (HTML/CSS) và Backend được hỗ trợ code bởi "trợ lý ảo" ChatGPT & Gemini. 🤖

## 🛠️ Công nghệ sử dụng (Tech Stack)

| Thành phần | Công nghệ / Công cụ |
| :--- | :--- |
| **Vi điều khiển** | ESP8266 (ESP-12F/E) |
| **SDK** | [ESP8266_RTOS_SDK](https://github.com/espressif/ESP8266_RTOS_SDK) (ESP-IDF Style) |
| **Web Interface** | HTML5, CSS3, JavaScript (Embedded Webserver) |
| **Backend Tools** | Node.js
| **IDE** | VS Code + Extension C/C++ |

## 🚀 Tính năng nổi bật (Features)

* [x] **Điều khiển linh hoạt:** Bật/tắt thiết bị qua mạng WiFi (LAN) và Internet.
* [x] **Giao thức MQTT:** Hỗ trợ kết nối Broker để tích hợp vào Home Assistant / Node-RED.
* [x] **Smart Config:** Chế độ cấu hình WiFi thông minh qua SoftAP (Giao diện web captive portal).
* [x] **Lưu trạng thái:** Tự động khôi phục trạng thái Bật/Tắt sau khi mất điện.
* [x] **Lịch trình (Schedule):** Hỗ trợ hẹn giờ bật tắt tự động.

## ⚙️ Cài đặt môi trường (Setup Guide)

*(Dành cho bạn nào quên cách setup ESP8266_RTOS_SDK giống tác giả 😅)*

### 1. Yêu cầu tiên quyết
* Python 3.8+
* Git
* Toolchain cho ESP8266

### 2. Thiết lập dự án
```bash
# Clone dự án về máy
git clone [https://github.com/Chiendlb8630/OPEN-WIFI-SWITCH.git](https://github.com/Chiendlb8630/OPEN-WIFI-SWITCH.git)
cd OPEN-WIFI-SWITCH

# Export đường dẫn IDF (Nếu chưa thêm vào .bashrc hay Environment Variables)
# Ví dụ đường dẫn SDK của bạn là ~/esp/ESP8266_RTOS_SDK
export IDF_PATH=~/esp/ESP8266_RTOS_SDK

# Cài đặt các thư viện Python cần thiết
python -m pip install --user -r $IDF_PATH/requirements.txt
