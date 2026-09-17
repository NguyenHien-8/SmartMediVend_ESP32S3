## Ranh giới quyền hạn và luồng dữ liệu

```flowchart TD
    U[Người dùng nói] --> STT[STT/Xiaozhi]
    STT --> AI[AI hội thoại và trích xuất]
    AI --> V[Kiểm tra schema nghiêm ngặt]
    V --> R[Bộ luật y khoa cục bộ trên ESP32]
    R -->|Thiếu dữ liệu| Q[Câu hỏi tiếp theo]
    R -->|Không an toàn| D[Từ chối và hướng dẫn đi khám]
    R -->|Đủ điều kiện| P[ESP32 chọn SKU từ catalog]
    P --> A[Thông báo thuốc dự kiến]
    A --> C[Chờ xác nhận bằng giọng nói]
    C -->|STT xác nhận hợp lệ| L[ESP32 kiểm tra lại luật và tồn kho]
    L --> M[Chọn kênh chính hoặc kênh dự phòng]
    M --> X[Relay LOW 500 ms tuần tự]
    X --> I[Trừ tồn kho ước tính và ghi nhật ký]
```