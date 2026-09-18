# SmartMediVend ESP32-S3-N16R8

Firmware máy cấp vỉ thuốc có hội thoại Xiaozhi, bộ luật an toàn chạy cục bộ
trên ESP32 và cơ cấu 16 relay qua CD74HC4067.

> **Khóa an toàn mặc định:** firmware được build với
> `SMV_PRODUCTION_VENDING_ENABLED=0` và `SMV_PHARMACIST_APPROVED=0`.
> Relay không được phép cấp thuốc thực tế cho đến khi danh mục và bộ luật đã
> được dược sĩ duyệt đúng phiên bản.

## Ranh giới quyền hạn

- Xiaozhi/AI chỉ hội thoại và gửi các câu trả lời có cấu trúc.
- ESP32 tự kiểm tra tuổi, cân nặng, thai kỳ/cho con bú, dấu hiệu nguy hiểm,
  bệnh nền, thuốc đang dùng, chống chỉ định và tồn kho.
- AI không nhận SKU/channel/relay và không thể gọi thao tác cấp thuốc.
- Chỉ ESP32 ánh xạ `canonical_id` sang channel, yêu cầu xác nhận cuối, kích
  relay LOW 500 ms tuần tự và trừ tồn kho sau mỗi xung đã gửi.
- Không có cảm biến rơi; tồn kho là `command_sent_unverified`.

## Cấu hình phần cứng

| Chức năng | GPIO |
|---|---:|
| TFT ST7789 CS / RST / DC | 10 / 14 / 9 |
| TFT MOSI / SCLK / BL | 11 / 12 / 13 |
| Nút SET WIFI | 18 |
| INMP441 SD / WS / SCK | 6 / 4 / 5 |
| MAX98357A LRC / BCLK / DIN | 16 / 15 / 7 |
| CD74HC4067 S0 / S1 / S2 / S3 | 39 / 40 / 41 / 42 |
| CD74HC4067 SIG | 17 |

GPIO43/44 không dùng. GPIO35–37 không dùng vì ESP32-S3-N16R8 dành chúng cho
Octal PSRAM. Relay 16 kênh là active-LOW; trạng thái nghỉ là HIGH.

Nút GPIO18 active-HIGH dùng pull-down nội của ESP32-S3; nên giữ thêm điện trở
kéo xuống ngoài 4,7 kΩ trên máy thực:

- nhấn ngắn: bắt đầu/dừng nghe khi cloud đã sẵn sàng;
- giữ 2 giây: mở Wi-Fi Portal.

## Cấu hình Arduino IDE bắt buộc

- Board: **ESP32S3 Dev Module**
- Flash Size: **16 MB**
- Partition Scheme: **3MB APP / 9.9MB FATFS**
- PSRAM: **OPI PSRAM**
- Flash Mode: **QIO 80MHZ**
- CPU Frequency: **240 MHz**
- USB Mode: **Hardware CDC and JTAG**
- USB CDC On Boot: **Disabled** khi dùng cầu CH343/COM nối tiếp trên bo

Nếu bật `USB CDC On Boot`, chương trình vẫn chạy nhưng log `Serial` chuyển sang
cổng USB native; COM của CH343 chỉ còn hiện log boot ROM. Điều này dễ làm nhầm
rằng firmware bị treo.

Lệnh build đã xác minh:

```powershell
arduino-cli compile `
  --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=default,CPUFreq=240,FlashMode=qio" `
  SmartMediVend
```

## Kích hoạt Xiaozhi

Firmware dùng endpoint provisioning chính thức:
`https://api.tenclass.net/xiaozhi/ota/`.

Luồng runtime:

1. ESP32 kết nối Wi-Fi và POST thông tin hệ thống qua HTTPS.
2. Nếu chưa liên kết, TFT hiển thị `ACTIVATION`, mã sáu số và `xiaozhi.me`.
3. Nhập mã tại Xiaozhi Control Panel.
4. Firmware long-poll `POST /activate`: HTTP 202 nghĩa là đang chờ, HTTP 200
   nghĩa là đã liên kết.
5. Firmware gọi lại endpoint OTA để nhận URL/token WSS, bắt tay WebSocket TLS,
   gửi client `hello` và chỉ chuyển sang `READY` sau server `hello` hợp lệ.

TLS luôn xác minh CA. Không dùng `setInsecure()`. Token WSS không được ghi ra
Serial. Các log chẩn đoán an toàn có tiền tố `[SMV][XIAOZHI]`, ví dụ:

```text
[SMV][XIAOZHI] bootstrap result=OK HTTP=200
[SMV][XIAOZHI] activation code=123456 long_poll_timeout_ms=35000
[SMV][XIAOZHI] activation HTTP=202 error=OK
[SMV][XIAOZHI] WSS transport connected
```

Nếu lỗi, ghi lại dòng `bootstrap result=... HTTP=...`, `activation HTTP=...` hoặc
`WSS ...`; không chụp/đăng token hoặc nội dung header Authorization.

## Dữ liệu và kiểm duyệt dược sĩ

- Danh mục 16 channel: `SmartMediVend/data/medicines.json`
- Bộ luật cục bộ: `SmartMediVend/data/medical_rules.json`
- Trạng thái duyệt: `SmartMediVend/data/pharmacist_review.json`

`pharmacist_review.json` hiện là `approved: false`. Để triển khai thực tế cần
dược sĩ duyệt nội dung, ghi đúng `catalog_version` và `rules_version`, sau đó
build với cả cờ production và thông tin phiên bản đã duyệt. Chỉ bật một cờ hoặc
sai phiên bản vẫn bị khóa fail-closed.

## Kiểm thử

Host test nằm trong `tests/host` và bao phủ bộ luật an toàn, tồn kho, xác nhận,
relay 500 ms, parser Xiaozhi, session/turn và MCP. Trước khi nạp cần chạy host
test, build đúng FQBN ở trên, sau đó xác minh Serial từ bootstrap đến WSS.
