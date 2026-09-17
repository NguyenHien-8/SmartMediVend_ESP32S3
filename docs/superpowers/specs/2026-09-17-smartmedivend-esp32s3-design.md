# Đặc tả thiết kế SmartMediVend ESP32-S3

- Ngày: 2026-09-17
- Trạng thái: thiết kế đã được người dùng duyệt
- Target: ESP32-S3-WROOM-1-N16R8, Arduino-ESP32 stable
- Phạm vi lâm sàng: hỗ trợ sàng lọc triệu chứng thông thường cho người từ đủ 16 tuổi; không chẩn đoán
- Trạng thái triển khai y khoa: `PHARMACIST_REVIEW_REQUIRED`; mặc định khóa vending thực tế

## 1. Mục tiêu

Xây dựng một firmware Arduino IDE tự chứa cho SmartMediVend, tích hợp Wi-Fi portal, TFT ST7789, hội thoại giọng nói qua Xiaozhi Official Cloud, luật an toàn y khoa cục bộ, danh mục/tồn kho và điều khiển 16 relay qua CD74HC4067.

AI chỉ hội thoại và trích xuất dữ liệu có cấu trúc. AI, cloud và MCP không được chọn SKU, channel, số lượng hay kích relay. Mọi quyết định cấp thuốc phải được ESP32 đánh giá tất định, xác nhận lại cục bộ và đi qua `VendGuard`.

## 2. Thứ tự ưu tiên yêu cầu

Các yêu cầu trực tiếp mới nhất của người dùng ghi đè những điểm mâu thuẫn trong prompt ban đầu:

- Module là ESP32-S3-N16R8; GPIO35–37 bị Octal PSRAM chiếm dụng.
- `MUX_SIG` đổi từ GPIO37 thành GPIO17.
- Không dùng HLK-LD2410 hoặc hai cảm biến hồng ngoại; GPIO43/44 không tham gia logic vending.
- Relay tích cực mức LOW, xung đúng 500 ms.
- Không xác nhận thuốc rơi; sau khi kết thúc xung 500 ms thì trừ tồn kho ước tính ngay.
- Nút GPIO18: nhấn ngắn bắt đầu/dừng nghe; giữ ít nhất 2 giây mở Wi-Fi Portal và không phát sinh sự kiện nhấn ngắn.
- Danh mục thuốc là ảnh mới nhất gồm 13 thuốc khác nhau và 3 kênh dự phòng.
- Chỉ phục vụ người từ đủ 16 tuổi; từ chối trẻ em, người mang thai/cho con bú, trường hợp có dấu hiệu nguy hiểm hoặc thiếu dữ liệu.

## 3. Kiến trúc và ranh giới tin cậy

```text
ESP32WiFiPortal
      |
      v
NetworkService -> XiaozhiBootstrapClient -> XiaozhiWebSocketTransport
                                             |
                                             v
                                      XiaozhiProtocol
                                       /     |      \
                              AudioService  MCP  ConversationController
                                                       |
                                                       v
                                             StructuredInputValidator
                                                       |
                                                       v
                                      MedicalRuleEngine + SafetyPolicy
                                                       |
                                                       v
                               MedicineCatalog + InventoryManager + UI
                                                       |
                                           explicit local confirmation
                                                       |
                                                       v
                                          VendGuard -> VendingManager
                                                       |
                                                       v
                                                   RelayDriver
```

Ranh giới bắt buộc:

- `SmartMediVend.ino` chỉ tạo app, gọi `begin()` và `process()`.
- `ESP32WiFiPortal` là Wi-Fi owner duy nhất.
- Transport chỉ chuyển text/binary và báo sự kiện; không chứa nghiệp vụ.
- Audio, UI, network và relay không gọi chéo phần cứng của nhau.
- Chỉ `RelayDriver` được gọi `digitalWrite()` cho chân MUX/relay.
- Chỉ `MedicalRuleEngine` được tạo candidate SKU.
- Chỉ `VendGuard` được cấp quyền bắt đầu transaction.
- Không expose MCP tool `vend`, `relay_on`, `relay_off` hoặc raw GPIO.

## 4. GPIO cuối cùng

| Chức năng | GPIO |
|---|---:|
| TFT CS | 10 |
| TFT RST | 14 |
| TFT DC | 9 |
| TFT MOSI | 11 |
| TFT SCLK | 12 |
| TFT backlight | 13 |
| Nút SET WIFI / nghe | 18 |
| INMP441 SD | 6 |
| INMP441 WS/LRCLK | 4 |
| INMP441 SCK | 5 |
| MAX98357A LRC | 16 |
| MAX98357A BCLK | 15 |
| MAX98357A DIN | 7 |
| CD74HC4067 S0 | 39 |
| CD74HC4067 S1 | 40 |
| CD74HC4067 S2 | 41 |
| CD74HC4067 S3 | 42 |
| CD74HC4067 SIG | 17 |

Tất cả chân phải được kiểm tra trùng ở compile time. SIG cần trạng thái an toàn HIGH sớm nhất khi boot và điện trở kéo lên ngoài khoảng 10 kΩ. Nếu input relay bị kéo lên 5 V thì phải dùng nguồn/chuyển mức phù hợp; không coi logic 3,3 V vào CD74HC4067 chạy 5 V là bảo đảm.

## 5. Danh mục 16 channel

| Channel | SKU | Hoạt chất/hàm lượng | Nhóm triệu chứng | Vai trò | Tồn đầu |
|---:|---|---|---|---|---:|
| 0 | SMV-PARA500 | Paracetamol 500 mg | Sốt, nhức đầu, đau mỏi nhẹ | chính | 5 vỉ |
| 1 | SMV-IBU200 | Ibuprofen 200 mg | Đau/viêm nhẹ | chính | 5 vỉ |
| 2 | SMV-LOR10 | Loratadine 10 mg | Dị ứng, hắt hơi, sổ mũi dị ứng | chính | 5 vỉ |
| 3 | SMV-DXM15 | Dextromethorphan HBr 15 mg | Ho khan | chính | 5 vỉ |
| 4 | SMV-AMB30 | Ambroxol HCl 30 mg | Ho có đờm | chính | 5 vỉ |
| 5 | SMV-DEQ025 | Dequalinium chloride 0,25 mg, viên ngậm | Đau/rát họng nhẹ | chính | 5 vỉ |
| 6 | SMV-SIM80 | Simethicone 80 mg | Đầy hơi, chướng bụng | chính | 5 vỉ |
| 7 | SMV-ANTACID | Al(OH)3 + Mg(OH)2, ví dụ 200 + 200 mg | Ợ nóng, khó tiêu acid | chính | 5 vỉ |
| 8 | SMV-OME10 | Omeprazole 10 mg | Ợ nóng/trào ngược ngắn hạn | chính | 5 vỉ |
| 9 | SMV-LOP2 | Loperamide HCl 2 mg | Tiêu chảy cấp dạng nước có chọn lọc | chính | 5 vỉ |
| 10 | SMV-BIS5 | Bisacodyl 5 mg | Táo bón ngắn hạn | chính | 5 vỉ |
| 11 | SMV-DIM50 | Dimenhydrinate 50 mg | Say xe | chính | 5 vỉ |
| 12 | SMV-SB250 | Saccharomyces boulardii, ví dụ 250 mg | Hỗ trợ rối loạn tiêu hóa/tiêu chảy | chính | 5 vỉ |
| 13 | SMV-PARA500-B | Paracetamol 500 mg | dự phòng SKU kênh 0 | backup 0 | 5 vỉ |
| 14 | SMV-DXM15-B | Dextromethorphan HBr 15 mg | dự phòng SKU kênh 3 | backup 3 | 5 vỉ |
| 15 | SMV-ANTACID-B | cùng SKU kênh 7 | dự phòng SKU kênh 7 | backup 7 | 5 vỉ |

`medicines.json` là catalog bất biến có version. Runtime inventory tách riêng và lưu trong NVS; không sửa catalog khi vending.

## 6. Hợp đồng dữ liệu AI -> ESP32

AI được gửi một object giới hạn gồm:

- `session_id`, `turn_id`;
- `age_years`, `weight_kg`;
- `pregnancy_or_breastfeeding`;
- symptom enum, mức độ, thời gian;
- danger-sign enum;
- bệnh nền enum;
- thuốc/nhóm thuốc đang dùng;
- dị ứng;
- danh sách trường chưa rõ.

Validator phải giới hạn chiều dài, số phần tử, enum, range số và session/turn monotonic. Bất kỳ trường `sku`, `channel`, `relay`, `quantity` hoặc `vend` từ AI đều bị từ chối. Dữ liệu AI là không tin cậy và không được ghi thẳng vào state an toàn.

Confirmation không phải MCP vend call. Trong `AwaitingConfirmation`, ESP32 chỉ phân loại transcript STT cuối bằng một grammar tiếng Việt nhỏ (`đồng ý`, `xác nhận`, `lấy thuốc`, hoặc phủ định/hủy), ràng buộc đúng session/candidate và timeout.

## 7. SafetyPolicy toàn cục

Kết quả duy nhất:

```text
NEED_MORE_INFO
DENY
REFER
OFFER
```

Điều kiện fail-closed:

- tuổi dưới 16 hoặc không rõ tuổi;
- mang thai, có thể mang thai hoặc cho con bú;
- cân nặng thiếu/không hợp lệ;
- thiếu triệu chứng, thời gian, bệnh nền, dị ứng hoặc thuốc đang dùng;
- câu trả lời mâu thuẫn;
- có dấu hiệu nguy hiểm;
- ngoài tập triệu chứng nhẹ được hỗ trợ;
- chống chỉ định/tương tác không thể loại trừ;
- catalog/ruleset chưa được dược sĩ phê duyệt trong production mode.

Dấu hiệu nguy hiểm tối thiểu gồm khó thở, đau ngực, lú lẫn/ngất/co giật, yếu/liệt/nói khó, đau đầu đột ngột dữ dội, cứng cổ, ho ra máu, nôn ra máu, phân đen/có máu, đau bụng dữ dội, mất nước, không giữ được nước, khó nuốt/chảy dãi/thở rít, sụt cân không chủ ý, triệu chứng nặng hoặc tăng nhanh.

## 8. Quy tắc thuốc cục bộ

- Paracetamol: chỉ sốt/đau đầu/đau mỏi nhẹ; loại trừ bệnh gan/thận, uống rượu nhiều, thuốc khác chứa paracetamol; cân nặng dưới 50 kg -> `REFER`.
- Ibuprofen: chỉ đau/viêm nhẹ; loại trừ loét/xuất huyết tiêu hóa, bệnh thận/tim/gan, mất nước, dị ứng/phản ứng hen với NSAID, thuốc chống đông, steroid hoặc NSAID khác.
- Loratadine: chỉ viêm mũi dị ứng; không phối hợp thuốc kháng histamine khác; bệnh gan -> `REFER`.
- Dextromethorphan: chỉ ho khan; loại trừ ho đờm, ho mạn, hen/COPD chưa đánh giá, MAOI trong 14 ngày và thuốc an thần/xung đột.
- Ambroxol: chỉ ho có đờm; không cấp cùng dextromethorphan; phản ứng da/niêm mạc hoặc tiền sử dị ứng -> từ chối.
- Dequalinium: chỉ đau/rát họng nhẹ, ngắn ngày; loại trừ khó thở/nuốt, chảy dãi, thở rít, sốt cao hoặc diễn tiến nhanh.
- Simethicone: chỉ đầy hơi/chướng bụng; levothyroxine/điều trị tuyến giáp -> `REFER`.
- Antacid: chỉ ợ nóng/khó tiêu acid ngắn hạn; bệnh thận/gan/tim, hạn chế natri hoặc có thuốc khác cần giãn cách -> `REFER`; không cấp cùng omeprazole.
- Omeprazole: chỉ từ đủ 18 tuổi và triệu chứng trào ngược ngắn hạn; 16–17 -> `REFER`; loại trừ alarm symptoms và tương tác quan trọng như clopidogrel.
- Loperamide: chỉ tiêu chảy cấp dạng nước, không máu; loại trừ sốt, mất nước, sau kháng sinh, IBD flare, táo bón/bụng trướng hoặc kéo dài quá 48 giờ.
- Bisacodyl: chỉ từ đủ 18 tuổi và táo bón ngắn hạn sau biện pháp lối sống; 16–17 -> `REFER`; loại trừ mất nước, đau bụng/nôn, tắc ruột, viêm ruột; không dùng gần antacid.
- Dimenhydrinate: chỉ say xe; không dùng cho buồn nôn chưa rõ nguyên nhân; loại trừ glaucoma, bệnh hô hấp, khó tiểu, rượu/thuốc an thần hoặc sắp lái xe/vận hành máy.
- Saccharomyces boulardii: hỗ trợ rối loạn tiêu hóa/tiêu chảy không dấu hiệu nguy hiểm; loại trừ suy giảm miễn dịch, bệnh nặng, catheter tĩnh mạch trung tâm và dị ứng nấm men.

Một transaction tối đa ba SKU, mỗi symptom domain tối đa một thuốc và mỗi SKU tối đa một vỉ. Không tạo chỉ dẫn số viên/liều cá nhân vì chưa có số viên/vỉ hoặc nhãn/tờ hướng dẫn cụ thể. Candidate có thể gồm nhiều thuốc cho các symptom domain độc lập, nhưng phải qua ma trận xung đột cục bộ.

## 9. State machine hội thoại và vending

```text
Booting -> WifiConnecting/WifiPortal -> CloudConnecting -> Idle
Idle -> Listening -> Processing -> Speaking
Processing -> NeedMoreInfo -> Listening
Processing -> Rejected/Refer -> Idle
Processing -> CandidateReady -> Speaking -> AwaitingConfirmation
AwaitingConfirmation -> Cancelled/Timeout -> Idle
AwaitingConfirmation -> Confirmed -> Revalidate -> Dispensing
Dispensing -> Success/OutOfStock/VendError -> Idle
```

Candidate bị vô hiệu nếu session thay đổi, timeout, dữ liệu y khoa thay đổi, ruleset/catalog version thay đổi, stock thay đổi hoặc reconnect cloud xảy ra. Mọi vend request có transaction UUID chống thực thi lặp.

## 10. RelayDriver và giao dịch

CD74HC4067 được dùng như selector một kênh tại một thời điểm, không phải expander latch.

Trình tự một vỉ:

1. ép SIG HIGH;
2. đặt S0–S3;
3. chờ settle 10 ms;
4. SIG LOW 500 ms;
5. SIG HIGH;
6. commit trừ tồn kho và audit `COMMAND_SENT_UNVERIFIED`;
7. chờ guard gap 100 ms rồi mới chọn kênh tiếp theo.

Relay operation dùng state machine/timer không chặn; không gọi từ ISR hoặc network callback. Boot, lỗi, cancel và watchdog recovery đều gọi all-off. Nếu kênh chính hết, dùng đúng backup đã khai báo; không cấp cả main và backup cho cùng SKU trong một transaction.

Không có sensor xác nhận. Firmware không dùng hoặc hiển thị `DISPENSE_CONFIRMED`.

## 11. Persistence

- `medicines.json`: catalog/schema/source metadata.
- `medical_rules.json`: rule version, required fields, exclusions, interactions và source URLs.
- `pharmacist_review.json`: mặc định `approved=false`; production vending chỉ mở khi artifact được phê duyệt.
- NVS lưu stable client ID, cloud bootstrap cache phù hợp, inventory và transaction journal.
- Inventory dùng hai slot snapshot có sequence + CRC; boot chọn bản hợp lệ mới nhất.
- Transaction journal phân biệt `PREPARED`, `PULSE_STARTED`, `COMMAND_SENT_UNVERIFIED`, `FAILED`; không retry tự động transaction không rõ trạng thái sau mất điện.

## 12. Nút GPIO18

- Debounce 35 ms.
- Nhấn rồi thả trước 2 giây: toggle bắt đầu/dừng nghe.
- Giữ đủ 2 giây: mở Wi-Fi Portal một lần; khi thả không phát short-press.
- Trong `Dispensing`, nút không thay đổi transaction; long press chỉ được xử lý sau khi relay trở về HIGH.

## 13. Xiaozhi, TLS, MCP và audio

- Port bootstrap/activation tối thiểu từ upstream hiện tại; lưu Client-Id UUID ổn định.
- WSS gửi đúng Authorization, Protocol-Version, Device-Id và Client-Id; TLS kiểm tra chứng thư, production không dùng `setInsecure()`.
- Hỗ trợ hello/listen/abort/STT/TTS/LLM/MCP và binary protocol version 1/2/3 với bounds check.
- PCM uplink 16 kHz mono từ INMP441, xử lý sample 24-bit trong slot 32-bit; Opus frame bounded và buffer tái sử dụng.
- Downlink sample rate theo server hello, decode Opus và phát MAX98357A.
- Queue có kích thước cố định và backpressure/drop counters; network callback không encode/decode hoặc vẽ TFT.
- MCP chỉ expose status, inventory read-only, medicine info, submit symptom data và candidate status.

## 14. UI

ST7789 portrait 240x320 giữ custom UI. `DisplayManager` chỉ render; `UiController` map state sang view model. Render khi state/data thay đổi, không `fillScreen()` mỗi loop.

Các màn hình tối thiểu: Boot, Wi-Fi connecting/offline/portal, cloud connecting, Idle, Listening, Processing, Speaking, Candidate, Awaiting confirmation, Dispensing, Success, Out of stock, Safety rejected, Network/Audio/Vend error.

TFT hiển thị candidate chính thức do ESP32 tạo, không hiển thị danh sách do AI tự gửi.

## 15. Ổn định và recovery

- Audio chạy trên task/queue giới hạn; app/network/UI cooperative và watchdog-friendly.
- Không malloc/free theo từng audio frame; ưu tiên buffer cố định, PSRAM cho buffer lớn có fallback.
- Wi-Fi reconnect do ESP32WiFiPortal quản lý; cloud reconnect chỉ chạy khi Wi-Fi ổn định.
- Disconnect dừng audio, invalidate confirmation/candidate và không chạm relay.
- JSON/MCP malformed, timeout, queue overflow và lỗi UI/audio không được gây vending.
- Health monitor ghi uptime, heap/min heap, PSRAM, RSSI, reconnect, queue drops, parse errors và vend errors theo chu kỳ không spam.

## 16. Cấu trúc source

Giữ các boundary trong prompt: `app`, `network`, `audio`, `mcp`, `ui`, `medical`, `inventory`, `vending`, `storage`, `diagnostics`, `vendor`. Có thể thêm helper nhỏ nhưng không tạo god object. Thư viện project-specific được vendor kèm LICENSE/NOTICE và version/commit; Arduino-ESP32 không được vendor.

## 17. Kiểm thử và tiêu chí chấp nhận

Host/unit tests tối thiểu:

- structured input validator và malformed JSON;
- global safety/red flags/missing data;
- từng rule thuốc và age/weight boundary;
- interaction/exclusion matrix;
- catalog + backup routing + out of stock;
- confirmation session/timeout/invalidation;
- VendGuard duplicate transaction;
- RelayDriver sequence bằng fake clock/GPIO;
- inventory dual-slot/CRC và power-loss cases;
- protocol binary v1/v2/v3 bounds;
- MCP initialize/tools/list/tools/call/error;
- centralized state transitions.

Build phải sạch cho ESP32-S3 N16R8 bằng Arduino CLI/Arduino IDE backend và pin Arduino-ESP32 stable đã kiểm chứng. Nếu không có board/serial port, báo `NOT RUN` cho flash/TFT/audio/relay/cloud thực tế; không ghi PASS giả.

Hardware smoke test cần tải an toàn trước relay thật: boot all-off, nút, portal, WSS, microphone, TTS speaker, pulse 500 ms từng channel, thứ tự multi-SKU và reset giữa transaction. Soak procedure gồm 50+ session, Wi-Fi/cloud reconnect, malformed input, confirmation timeout và theo dõi heap/task/queue.

## 18. Khóa dược sĩ và giới hạn

`PRODUCTION_VENDING_ENABLED` mặc định false. Việc đổi sang true không đủ để bỏ khóa: firmware còn phải xác minh review artifact đúng rule/catalog version và `approved=true`.

Đặc tả và rule source chỉ là cơ sở kỹ thuật. Dược sĩ được cấp phép tại nơi triển khai phải duyệt chỉ định, chống chỉ định, tương tác, câu hỏi sàng lọc, ngôn ngữ tư vấn, đơn vị vỉ, số viên/vỉ và nhãn thực tế trước khi vận hành với người dùng. Hệ thống không thay bác sĩ/dược sĩ và không chẩn đoán.

## 19. Nguồn chính

- Espressif ESP32-S3-WROOM-1/1U datasheet: https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf
- Xiaozhi upstream: https://github.com/78/xiaozhi-esp32
- ESP32WiFiPortal upstream: https://github.com/NguyenHien-8/ESP32WiFiPortal
- NHS paracetamol: https://www.nhs.uk/medicines/paracetamol-for-adults/
- NHS ibuprofen: https://www.nhs.uk/medicines/ibuprofen-for-adults/
- NHS loratadine: https://www.nhs.uk/medicines/loratadine/
- DailyMed dextromethorphan: https://dailymed.nlm.nih.gov/dailymed/getFile.cfm?setid=13ae4c67-a3fe-440c-91b8-c9412626c3cd&type=pdf
- EMA ambroxol: https://www.ema.europa.eu/en/medicines/human/referrals/ambroxol-bromhexine-containing-medicines
- NPRA dequalinium: https://quest3plus.bpfk.gov.my/front-end/attachment/10182/pharma/525322/V_105132_20250325_152748_D4.pdf
- NHS simeticone: https://www.nhs.uk/medicines/simeticone/
- NHS antacids: https://www.nhs.uk/medicines/antacids/
- NHS omeprazole: https://www.nhs.uk/medicines/omeprazole/
- NHS loperamide: https://www.nhs.uk/medicines/loperamide/
- NHS bisacodyl: https://www.nhs.uk/medicines/bisacodyl/
- DailyMed dimenhydrinate: https://dailymed.nlm.nih.gov/dailymed/lookup.cfm?setid=3fd4a96f-8f69-42bf-b777-6e3a2bfea275
- EMA Saccharomyces boulardii: https://www.ema.europa.eu/en/documents/psusa/saccharomyces-boulardii-cmdh-scientific-conclusions-and-grounds-variation-amendments-product-information-and-timetable-implementation-psusa00009284201702_en.pdf
- NHS red flags: https://www.nhs.uk/symptoms/headaches/, https://www.nhs.uk/symptoms/cough/, https://www.nhs.uk/symptoms/diarrhoea-and-vomiting/, https://www.nhs.uk/conditions/indigestion/

