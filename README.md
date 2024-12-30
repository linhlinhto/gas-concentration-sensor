# Dự án Đo Nồng Độ Khí Gas sử dụng ESP32

## Giới thiệu

Dự án này sử dụng ESP32 kết hợp với cảm biến MQ-9 để đo nồng độ khí gas trong không khí và gửi dữ liệu đến một broker MQTT. Hệ thống cũng hỗ trợ cấu hình WiFi qua giao diện web hoặc cập nhật cấu hình từ xa thông qua MQTT. Ngoài ra, nếu nồng độ khí gas vượt quá mức cho phép, thiết bị sẽ kích hoạt còi báo động để cảnh báo.

## Các tính năng chính
1. **Đo nồng độ khí gas**:
   - Sử dụng cảm biến MQ-9 để đo nồng độ khí gas.
   - Chuyển đổi tín hiệu analog sang phần trăm nồng độ gas.

2. **Cảnh báo nguy hiểm**:
   - Kích hoạt còi báo động nếu nồng độ khí gas vượt quá ngưỡng 40%.
   - Liên tục theo dõi cho đến khi nồng độ giảm xuống mức an toàn.

3. **Kết nối WiFi và MQTT**:
   - Hỗ trợ lưu trữ và chuyển đổi giữa 3 mạng WiFi khác nhau.
   - Gửi dữ liệu đo lường và trạng thái WiFi đến broker MQTT.
   - Cho phép cập nhật SSID và mật khẩu WiFi qua MQTT hoặc giao diện web.

4. **Giao diện cấu hình WiFi**:
   - Tạo điểm phát WiFi (AP) để người dùng cấu hình SSID và mật khẩu thông qua giao diện web.

5. **Tiết kiệm năng lượng**:
   - Chế độ ngủ sâu trong 5 phút (deep sleep) sau 10 giây hoạt động.

## Yêu cầu phần cứng
- **ESP32**: Đóng vai trò là bộ vi điều khiển chính.
- **Cảm biến MQ-9**: Cảm biến đo nồng độ khí gas kết nối với chân `ANALOG_PIN` (chân 33).
- **Còi báo động (Buzzer)**: Được kết nối với chân `BUZZER_PIN` (chân 12).
- **Nguồn cung cấp**: 2 viên pin 2500mAh đảm bảo hoạt động lâu dài.

## Yêu cầu phần mềm
- **HiveMQ**: Broker MQTT để lưu trữ và truyền dữ liệu.
  ![Hive_mq_Sever](https://github.com/user-attachments/assets/a9f96fd1-36f6-41e6-8602-62a356963ced)
  
- **Node-RED**: Xây dựng giao diện điều khiển và hiển thị dữ liệu.
- **PlatformIO**: Nền tảng phát triển và nạp chương trình cho ESP32.
- Các thư viện cần thiết:
  - `WiFiClientSecure.h`
  - `PubSubClient.h`
  - `WebServer.h`
  - `DNSServer.h`

## Ý tưởng cho thiết bị

### 1. Cài đặt và chạy chương trình
- Khi khởi động lần đầu, nếu chưa có cấu hình WiFi, ESP32 sẽ tạo một điểm phát WiFi với tên `ESP32_Config` và mật khẩu `12345678`.
- Kết nối vào điểm phát và truy cập địa chỉ IP `192.168.4.1` để cấu hình WiFi.
  ![web_config_wifi](https://github.com/user-attachments/assets/22ac02b9-6b73-41c3-8df8-b60634e412b0)


### 2. Kết nối WiFi
- ESP32 sẽ cố gắng kết nối vào mạng WiFi đã lưu trữ.
- Nếu không thành công, thiết bị sẽ quay lại chế độ cấu hình.

### 3. Gửi dữ liệu đến MQTT
- Nồng độ khí gas được gửi đến topic `gas_con`.
- Trạng thái WiFi hiện tại được gửi đến các topic `wifi1/status`, `wifi2/status`, `wifi3/status`.
- Cấu hình WiFi được gửi đến các topic tương ứng:
  - `wifi1/ssid`, `wifi1/pass`
  - `wifi2/ssid`, `wifi2/pass`
  - `wifi3/ssid`, `wifi3/pass`

### 4. Cập nhật cấu hình WiFi qua MQTT
- Gửi SSID hoặc mật khẩu mới đến các topic:
  - `wifi1/changessid`, `wifi1/changepass`
  - `wifi2/changessid`, `wifi2/changepass`
  - `wifi3/changessid`, `wifi3/changepass`
- Chuyển đổi WiFi bằng cách gửi số thứ tự mạng WiFi (1, 2 hoặc 3) đến topic `wifi/connect`.

### 5. Giao diện Dashboard trên Node-RED
- Giao diện Dashboard của dự án được cấu hình trên Node-RED như sau:
  - **Tab chính**: "Gas warning".
  - **Nhóm hiển thị**: "Gas state".
    ![Node_Red_flow1](https://github.com/user-attachments/assets/ccd155b7-1061-4343-b9f6-9da530066067)

  - **Hiển thị nồng độ khí gas**:
    - Thành phần `Gauge`: Hiển thị nồng độ khí gas (%), màu sắc thay đổi theo mức độ: xanh (<20%), vàng (20-40%), đỏ (>40%).
    - Thành phần `Circle` trong `ui_template`: Hiển thị nồng độ khí gas kèm hiệu ứng nhấp nháy khi vượt ngưỡng.
       ![Dash_board_group1](https://github.com/user-attachments/assets/fe261b4c-b59e-4746-911e-46d9d914fcac)

    - **Âm thanh cảnh báo**:
    - Thành phần `Audio`: Phát cảnh báo khi nồng độ khí gas vượt quá mức an toàn.
    
  - **Trạng thái WiFi**:
    - Thành phần `Text`: Hiển thị SSID và trạng thái của từng WiFi (ON/OFF).
    - Thành phần `Button`: Chuyển đổi giữa các WiFi.
      ![Node_Red_flow2_1](https://github.com/user-attachments/assets/28c9da40-ad0f-4a29-9870-123b90068771)
      
      ![Node_Red_flow2_2](https://github.com/user-attachments/assets/0b14b289-0bc7-4d38-81ba-4524a73ff054)

  - **Cập nhật WiFi**:
    - Thành phần `Text Input`: Nhập SSID và mật khẩu mới.
    - Thành phần `Button`: Gửi thông tin cập nhật qua MQTT.
      
      ![Dash_board_group2](https://github.com/user-attachments/assets/efb00c82-026d-4ced-9de4-cb176d4a4f02)
    


## Hoạt động của thiết bị
1. **Khởi động**:
   - Nếu có cấu hình WiFi, ESP32 sẽ cố gắng kết nối.
   - Nếu không có cấu hình WiFi, ESP32 sẽ kích hoạt chế độ cấu hình qua AP.

2. **Đo và gửi dữ liệu**:
   - Đọc giá trị analog từ cảm biến MQ-9.
   - Gửi dữ liệu nồng độ khí gas đến broker MQTT.

3. **Cảnh báo**:
   - Nếu nồng độ khí gas vượt ngưỡng 40%, còi báo động sẽ được kích hoạt.

4. **Quản lý WiFi**:
   - Tự động chuyển đổi giữa các cấu hình WiFi nếu mất kết nối.
   - Hỗ trợ thay đổi cấu hình qua giao diện web hoặc MQTT.

5. **Ngủ sâu**:
   - Thiết bị chuyển sang chế độ ngủ sâu sau 10 giây không hoạt động.

## Cấu trúc thư mục
- `wifi_connect.h`: Thư viện hỗ trợ kết nối WiFi.
- `ca_cert.h`: Chứa chứng chỉ CA cho kết nối MQTT bảo mật.
- `secrets/mqtt.h`: Lưu thông tin đăng nhập MQTT.

## Ngưỡng cảnh báo
- **Ngưỡng kích hoạt còi báo động**: 40% nồng độ khí gas.
- Có thể thay đổi trong hàm `checkAnalogData()`.

## Tạo khung cho thiết bị
- dùng Fusion360 để tạo khung cho thiết bị xem chi tiết ở:
  [Khung 3D](https://github.com/linhlinhto/gas-concentration-sensor/blob/main/Gaswarning_image/Gas_Warning.stl)
  [nắp 3D](https://github.com/linhlinhto/gas-concentration-sensor/blob/main/Gaswarning_image/nap_gas.stl)
- Một vài hình ảnh thiết bị thực tế:
  ![Real_component1](https://github.com/user-attachments/assets/46a49ea6-4d11-4dfe-a685-4090138279a7)
  ![Real_component2](https://github.com/user-attachments/assets/fadf6807-76fd-463a-bc9a-07ed0688043d)
  ![Real_component3](https://github.com/user-attachments/assets/91d825cf-73ad-4d51-899e-d53992ab823b)

## Video thử nghiệm thiết bị
- Do để thử nghiệm và không phải chờ video quá lâu nên code đã  được đổi thành sleep 10 giây thay vì sleep 5 phút. Trong thực tế không phải lúc nào gas cũng rò rỉ vì thế để esp ở chế độ ngủ 5 phút sau đó kiểm tra 1 lần giúp đảm bảo tuổi thọ thiết bị.

