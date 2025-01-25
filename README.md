# SmartLock
## Hệ thống gồm 2 phần Device và Server phụ vục các chức năng chính bao gồm
+ Các chức năng mở khóa
  + Mở khóa bằng RFID thông thường
  + Mở khóa bằng OTP (Google Authentication)
  + Mở khóa trực tiếp trên app admin
+ Theo dõi lịch sử ra vào trên app admin
+ Cập nhật thông tin các user
## Device
+ Bao gồm các module phục vụ mở khóa được thiết kế PCB hoàn chỉnh gồm:
  + Vi xử lý ESP32
  + Module SPI MFRC522 để quét RFID
  + Bàn phím 4*3 cơ bản
  + Màn hình LCD I2C 16*2
+ Phục vụ các chức năng
  + Quét và kiểm tra thẻ RFID
  + Lưu các infor cần thiết cho mở khóa vào flask (phòng trường hợp mất kết nối server)
  + Đọc bàn phím để mở khóa OTP (lấy từ app Google Authentication sau khi điền secret)
  + Hiển thị trạng thái khóa và dùng relay mở khóa từ
  + Cập nhật thông tin ra vào sau khi được mở khóa
## Server (Node_RED)
Bao gồm chức năng 
+ Kết hợp với ngrok để deploy 1 webserver nhỏ gọi là admin app để chủ nhà dễ theo dõi
+ Mở khóa từ xa trên admin app
+ Theo dõi lịch sử ra vào của khóa thông minh
+ Cập nhật và lưu trữ thông tin cơ bản để xác thực tạo user mở khóa như tên, 4 so cuoi SĐT, email
+ Gửi request để device thêm thẻ RFID cho user mới và backup dữ liệu vào flask
+ Sau khi hoàn tất tạo user mới, sẽ gửi email thông báo cho user, bao gồm cả secret để tạo otp bằng Google authentication
