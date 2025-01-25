#include <WiFi.h>   // Sử dụng ESP32 WiFi thư viện
#include <PubSubClient.h>      // Thư viện MQTT
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <MFRC522.h>
#include <SPIFFS.h>


///////////////////////////////////////
//    KHAI BÁO CÁC GIÁ TRỊ HẰNG SỐ   //
///////////////////////////////////////

// Thông tin WiFi
const char* ssid = "TvT";       // Tên mạng WiFi
const char* password = "1223334444";    // Mật khẩu WiFi

// Thông tin MQTT
const char* mqtt_server = "192.168.53.199";  // Địa chỉ broker MQTT
const int mqtt_port = 1883;            // Cổng của broker MQTT
// const char* mqtt_username = "khai123"; // Tên người dùng
// const char* mqtt_password = ""; // Mật khẩu người dùng

// Bàn phím
const int rows[4] = {13, 12, 14, 27};  // Hàng nối với D13, D12, D14, D27
const int cols[3] = {26, 25, 33};      // Cột nối với D26, D25, D33
const char keyMap[4][3] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'*', '0', '#'}
};
bool keyPressed[4][3] = {false};

// RFID
#define RST_PIN 5
#define SDA_PIN 2
#define MOSI_PIN 16
#define MISO_PIN 17
#define SCK_PIN 4
MFRC522 mfrc522(SDA_PIN, RST_PIN);

//I2C cho LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

#define MSG_BUFFER_SIZE (50)

///////////////////////////////////////
//    KHAI BÁO CÁC BIẾN TOÀN CỤC     //
///////////////////////////////////////

//Biến để dọc dữ liệu từ bàn phím
String idInput = "";
String otpInput = "";

//Định nghĩa các flag
String check_status = "";
bool backup_status = false;
bool wifi_connect = true;
unsigned long wifi_connected_time = 0;

// Dinh nghia luu cuc bo datalock

//Biến toàn cục sử dụng cho tác vụ backup
StaticJsonDocument<1024> backup_data;
JsonArray Unlocker; // Biến Unlocker là mảng chứa các Json dữ liệu backup từ Server

//Khai báo các đối tượng khác
WiFiClient espClient;           //Đối tượng thực thi việc kết nối wifi
PubSubClient client(espClient);       // Khởi tạo đối tượng MQTT

//Biến thực hiện tác vụ MQTT
unsigned long lastMsg = 0;
char msg[MSG_BUFFER_SIZE];

///////////////////////////////////////////////
//    ĐỊNH NGHĨA CÁC HÀM SETUP, KHỞI TẠO     //
///////////////////////////////////////////////

void Alert_connected() {
  unsigned long time = millis();
  if(time - wifi_connected_time > 5000) {
    wifi_connected_time = time;
    client.publish("StatusSmartLock", "OK");
    Serial.println("Wifi_OK");
  }
}
String read_backup_local() {
  if (!SPIFFS.begin()) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return "";
  }
  File file = SPIFFS.open("/datalock.txt", "r");
  if (!file) {
    Serial.println("Error opening file for reading");
    return "";
  } else {
    String content = "";
    while (file.available()) {
        content += (char)file.read();
    }
    file.close();
    return content;
  }
}

void write_backup_local(char* data) {
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  File file = SPIFFS.open("/datalock.txt", "w");
  if (!file) {
    Serial.println("Error opening file for writing");
  } else {
    file.print(data);
    file.close();
    Serial.println("Data written successfully!");
  }
}

void setup_wifi() {
  unsigned long startTime = millis(); // Lưu thời điểm bắt đầu
  unsigned long duration = 300000;     // Chạy vòng lặp trong 30000 ms (30 giây)
  delay(10);
  WiFi.mode(WIFI_STA);
  Serial.println("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (millis() - startTime < duration) {
  if (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    wifi_connect = false;
  } else {
    wifi_connect = true;
    break;
  }
  }
  if(wifi_connect != true) {
    Serial.println("Connect Wifi Fail");
    Serial.println(ssid);
    Serial.println(password);
    return;
  }
  randomSeed(micros());
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void connect_to_broker() {
  if(wifi_connect == false) {
    return;
  }
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientID = "ESP32Client-";
    clientID += String(random(0xffff), HEX);
    if (client.connect(clientID.c_str())) {
      Serial.println("connected");
      client.subscribe("DirectUnlock");  // Đăng ký topic
      client.subscribe("RESInitDataLock");  // Đăng ký topic
      client.subscribe("REQUpdateID");  // Đăng ký topic
      client.subscribe("OTPCheck");  // Đăng ký topic
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 2 seconds");
      delay(2000);
    }
  }
}

// Khởi tạo LCD
void initLCD() {
    lcd.init();
    lcd.backlight();
    lcd.clear();
    lcd.print("System Ready");
    delay(2000);
    lcd.clear();
}

// Khởi tạo RFID
void initRFID() {
    SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SDA_PIN);  // Sử dụng chân tùy chỉnh
    mfrc522.PCD_Init();
    lcd.print("RFID Ready");
    delay(2000);
    lcd.clear();
}

// Khởi tạo bàn phím
void initKeypad() {
    for (int i = 0; i < 4; i++) {
        pinMode(rows[i], OUTPUT);
        digitalWrite(rows[i], HIGH);
    }
    for (int i = 0; i < 3; i++) {
        pinMode(cols[i], INPUT_PULLUP);
    }
}




//////////////////////////////////////////////
//    ĐỊNH NGHĨA CÁC HÀM XỬ LÝ, THỰC THI    //
//////////////////////////////////////////////


//Gọi đến hàm callback mỗi khi ESP32 nhận được dữ liệu từ MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.println(topic);

  // Phân loại xử lý dựa trên topic nhận về

//Thực hiện việc mở khóa trực tiếp qua APP, trả về ID duy nhất và cố định của chủ sở hữu
  if (strcmp(topic, "DirectUnlock") == 0) {
    // Xử lý cho DirectUnlock
    Serial.println("Handling Unlock Request");
    //Lock_Status = 1;
    unlock();
    lcd.print("Unlocked");
    delay(2000);
    lcd.clear();
    Tracking("8576");
  } 
 
//Thực hiện việc backup dữ liệu từ server, chứa các ID và RFID đã đăng ký được lưu trên server    
  else if (strcmp(topic, "RESInitDataLock") == 0) {

    // Chuyển payload sang chuỗi
    char message[length + 1];
    strncpy(message, (char*)payload, length);
    message[length] = '\0';

    // Luu datalock vao Flask
    write_backup_local(message);
    // Phân tích JSON: Nếu dữ liệu nhận được phải là json, trả về error = 1
    DeserializationError error = deserializeJson(backup_data, message);

    if (error) {
        Serial.print("JSON parse error: ");
        Serial.println(error.c_str());
        return;
    }

    // Duyệt qua mảng JSON
    if (backup_data.is<JsonArray>()) {
      Unlocker = backup_data.as<JsonArray>(); //Thực hiện việc lưu backup_data vào Unlocker dưới dạng một JsonArray
      for (JsonObject obj : Unlocker) { //Foreach duyệt qua tất cả các json chứa trong Unlocker
        const char* id = obj["id"];
        const char* RFID = obj["RFID"];
        // Serial.print("ID: ");
        // Serial.print(id);
        // Serial.print(", RFID: ");
        // Serial.println(RFID);
      }
      backup_status = true;
    } 
    else {
        //Serial.println("Dữ liệu không phải là mảng JSON!");
    }
  } 
  
  //Thực hiện việc cập nhật RFID dựa trên tên được gửi về
  else if(strcmp(topic, "REQUpdateID") == 0){
    StaticJsonDocument<256> doc; // Khai báo một định dạng tài liệu chuyên biệt dành cho json
    DeserializationError error = deserializeJson(doc, payload, length); //Chuyển file định dạng json thành dạng tài liệu chuyên biệt ở trên
      if (error) {
        Serial.print("JSON parse error: ");
        Serial.println(error.c_str());
        return;
  }
      else{
        // Đọc dữ liệu JSON theo từng trường
        //Convert tên đọc từ json từ dạng const char* thành dạng char* để thực hiện việc gửi lại
        const char* Name = doc["name"];
        String temp = Name;
        char* name = &temp[0];
        Serial.println("Name" + String(Name));
        String test = read_rfid();
        Serial.println(test);
        // const char* RFID = read_rfid().c_str();
        const char* RFID = test.c_str();

        //Đọc RFID thành công, chuỗi đọc được từ RFID không phải là chuỗi rỗng 
        if(test != "") {
          RESUpdateID(Name, RFID); //Thực hiện việc phản hổi lại tên kèm theo RFID quét được để thêm vào CSDL
        }
        else{
          send_Data("UpdateFail", name); //Nếu không quét được RFID, trả về tên người đã yêu cầu đăng kí để thực hiện việc xóa tên khỏi CSDL
        }
    }
  }
  //Server gửi về kết quả kiểm tra xem OTP nhập từ bàn phím đã đúng hay chưa, nếu đúng trả về "OK", nếu không trả về "NO"
  else if(strcmp(topic, "OTPCheck") == 0){
    String message;
    for (int i = 0; i < length; i++) {
      message += (char)payload[i];
    }
    Serial.println("Message: " + message);
    Serial.println(message.length());
    check_status = message;
    delay(1000);
    //Nếu tồn tại ID và nhập đúng OTP, thực hiện mở khóa và in ra màn hình báo hiệu mở khóa thành công
    if(check_status == "OK"){
      unlock();
      lcd.clear();
      lcd.print("Unlocked");
      delay(2000);
      lcd.clear();
    }
    //Nếu không, in ra màn hình báo hiệu mở khóa thất bại
    else{
      lcd.clear();
      lcd.print("Unlock Fail");
      delay(2000);
      lcd.clear();
    } //Kết quả này được lưu vào biến check_status dưới dạng string.
  }
  else {
    Serial.println("Unknown Topic");
  }
}

//Hàm gửi dữ liệu thông thường
void send_Data(char* topic, char* message){
  client.publish(topic, message);
  delay(2000);
}

//Hàm yêu cầu backup dữ liệu từ Server
void REQInitDataLock(){
  client.publish("REQInitDataLock", ""); //client.publish(topic, message); Hàm gửi dữ liệu lên Server qua MQTT
  delay(1000);
}

//Phản hồi yêu cầu update của APP
void RESUpdateID(const char* name, const char* rfid){

  //Gán các giá trị vào các trường trong json
  StaticJsonDocument<256> doc;
  doc["name"] = name;
  doc["RFID"] = rfid;
  // Serialize JSON thành chuỗi
  char buffer[256];
  size_t n = serializeJson(doc, buffer);
  client.publish("RESUpdateID", buffer, n);
  Serial.println("Sent JSON: ");
  Serial.println(buffer);
}

//Yêu cầu check OTP nhập từ bàn phím
void CheckOTP(const char* id, const char* otp){
   //Gán các giá trị vào các trường trong json
  StaticJsonDocument<256> doc;
  doc["id"] = id;
  doc["otp"] = otp;
  // Serialize JSON thành chuỗi
  char buffer[256];
  size_t n = serializeJson(doc, buffer);
  client.publish("CheckOTP", buffer, n);
  Serial.println("Sent JSON: ");
  Serial.println(buffer);
  delay(1000);
}

void Tracking(const char* id){
   //Gán các giá trị vào các trường trong json
  StaticJsonDocument<256> doc;
  doc["id"] = id;
  // Serialize JSON thành chuỗi
  char buffer[256];
  size_t n = serializeJson(doc, buffer);
  client.publish("Tracking", buffer, n);
  Serial.println("Sent JSON: ");
  Serial.println(buffer);
}

String read_rfid(){
  unsigned long startTime = millis(); // Lưu thời điểm bắt đầu
  unsigned long duration = 10000;     // Chạy vòng lặp trong 10000 ms (10 giây)
  lcd.print("Scan RFID");
  //Trong khoảng thời gian 10s, cho phép người dùng quét thẻ RFID để đọc thẻ
  while (millis() - startTime < duration) {
      String rfid = readRFID();
      //Đọc được RFID, trả về RFID đọc được dưới dạng string
      if (rfid != "") {
        lcd.clear();
        lcd.print("RFID UID:");
        lcd.setCursor(0, 1);
        lcd.print(rfid);
        delay(1000);
        lcd.clear();
        return rfid;
      }
    }
    //Nếu không đọc được, trả về chuỗi rỗng và in ra màn LED báo hiệu đọc fail          
    lcd.print("Read RFID fail");
    delay(2000);
    lcd.clear();
    return ""; 
} 
//Hàm mở khóa bằng cách thiết lập các chân tín hiệu của ESP32
void unlock(){
    digitalWrite(15,HIGH);
    delay(1000);
    digitalWrite(15,LOW);
}

// Quét bàn phím
char scanKeypad() {
    for (int i = 0; i < 4; i++) {
        digitalWrite(rows[i], LOW);
        for (int j = 0; j < 3; j++) {
            if (digitalRead(cols[j]) == LOW) {
                if (!keyPressed[i][j]) {
                    keyPressed[i][j] = true;
                    delay(300);
                    digitalWrite(rows[i], HIGH);
                    return keyMap[i][j];
                }
            } else {
                keyPressed[i][j] = false;
            }
        }
        digitalWrite(rows[i], HIGH);
    }
    return '\0';
}

// Đọc thẻ RFID và trả về UID
String readRFID() {
    if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
        return ""; // Không có thẻ
    }

    String rfidUID = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        rfidUID += String(mfrc522.uid.uidByte[i], HEX);
    }
    rfidUID.toUpperCase();
    mfrc522.PICC_HaltA(); // Ngắt kết nối thẻ
    return rfidUID;
}

// Chế độ OTP: Nhập ID và OTP từ bàn phím
String enterID() {
    lcd.clear();
    lcd.print("Enter ID:");
    String idInput = "";

    while (true) {
        char key = scanKeypad();
        if (key != '\0') {
            if (key == '#') break; // Hoàn tất nhập ID
            if (key == '*') { // Hủy
                lcd.clear();
                lcd.print("Canceled");
                delay(2000);
                return ""; // Trả về chuỗi rỗng nếu hủy
            }
            idInput += key;
            lcd.setCursor(0, 1);
            lcd.print(idInput);
        }
    }

    lcd.clear();
    lcd.print("ID Accepted");
    delay(2000);
    return idInput;
}

String enterOTP() {
    lcd.clear();
    lcd.print("Enter OTP:");
    String otpInput = "";

    while (true) {
        char key = scanKeypad();
        if (key != '\0') {
            if (key == '#') break; // Hoàn tất nhập OTP
            if (key == '*') { // Hủy
                lcd.clear();
                lcd.print("Canceled");
                delay(2000);
                return ""; // Trả về chuỗi rỗng nếu hủy
            }
            otpInput += key;
            lcd.setCursor(0, 1);
            lcd.print(otpInput);
        }
    }

    lcd.clear();
    lcd.print("OTP Accepted");
    delay(2000);
    lcd.clear();
    return otpInput;
}

String enterOTPMode() {
    String idInput = enterID(); // Gọi hàm nhập ID
    if (idInput == "") return ""; // Hủy nếu ID bị hủy

    String otpInput = enterOTP(); // Gọi hàm nhập OTP
    if (otpInput == "") return ""; // Hủy nếu OTP bị hủy

    lcd.clear();
    lcd.print("ID: " + idInput);
    lcd.setCursor(0, 1);
    lcd.print("OTP: " + otpInput);
    delay(2000);

    return "ID:" + idInput + ",OTP:" + otpInput;
}

//Hàm mở khóa bằng RFID
void unlock_RFID(){
  String rfid = readRFID(); //Đọc thẻ RFID, giá trị nhận về là một string
  const char* rfid_to_cmp = rfid.c_str(); //Chuyển chuỗi đọc được thành định dạng const char* để thực hiện việc so sánh chuỗi

  //Nếu đọc được RFID
  if (rfid != "") {
    //In ra màn hình RFID đọc được
    lcd.clear();
    lcd.print("RFID UID:");
    lcd.setCursor(0, 1);
    lcd.print(rfid);
    delay(2000);
    lcd.clear();
  //Kiểm tra xem mảng chứa các json(Bao gồm ID và RFID đã đăng ký) đã được khởi tạo hay chưa  
  if(Unlocker == nullptr) {
    Serial.println("Unlocker initialized fail");
  }
  else{
    for (JsonObject obj : Unlocker) { //Duyệt qua tất cả các json trong mảng
          if(obj != nullptr){
            //Nếu giá trị duyệt được không phải là null, thực hiện việc trích xuất dữ liệu để so sánh
            const char* id = obj["id"]; 
            const char* RFID = obj["RFID"];
            if(strcmp(rfid_to_cmp, RFID) == 0){ //So sánh RFID
              unlock(); //Mở khóa và in ra màn hình thông báo mở khóa thành công
              lcd.print("Unlocked");
              delay(2000);
              lcd.clear();
              Tracking(id); //Chuyển ID vừa mở khóa về Server để lưu lại trong lịch sử mở khóa
              break; //Ngay sau khi tìm được RFID phù hợp, thoát khỏi vòng lặp duyệt các json 
            }
            //Đoạn này cần config này, về đọc lại tự dưng thấy đoạn này cấn cấn :v
            // else{
            //   lcd.print("Fail");
            //   delay(2000);
            //   lcd.clear();
            // }
          }
        }
  }
  }
}




/////////////////////////////////////////////
//    ĐỊNH NGHĨA CÁC HÀM CHÍNH, VÒNG LẶP   //
/////////////////////////////////////////////

void setup() {
  Serial.begin(115200);
  SPIFFS.begin(true);
  // Serial.println("Connecting");
  Serial.setTimeout(500);
  wifi_connected_time = millis();
  Serial.println("Check");
  String buffer = "";
  buffer = read_backup_local();
  if(buffer != "") {
  DeserializationError error = deserializeJson(backup_data, buffer);
  Unlocker = backup_data.as<JsonArray>(); //Thực hiện việc lưu backup_data vào Unlocker dưới dạng một JsonArray
  for (JsonObject obj : Unlocker) { //Foreach duyệt qua tất cả các json chứa trong Unlocker
        const char* id = obj["id"];
        const char* RFID = obj["RFID"];
        Serial.print("ID: ");
        Serial.print(id);
        Serial.print(", RFID: ");
        Serial.println(RFID);
  }
  backup_status = true;
  Serial.println("Check1");
  }
  setup_wifi();                  // Kết nối WiFi
  client.setServer(mqtt_server, mqtt_port); //Thiết lập việc kết nối ESP32 tới Server
  client.setCallback(callback);   //Thiết lập việc xử lý callback khi nhận tin nhắn từ MQTT
  connect_to_broker();
  Serial.println("Initial Successfully");

//Khởi tạo instance cho LCD
  initLCD();
  initRFID();
  initKeypad();
  pinMode(15,OUTPUT);

//Cuối giai đoạn setup, thực hiện việc yêu cầu backup dữ liệu từ Server
  REQInitDataLock();
}


void loop() {
  client.loop();  // Xử lý các yêu cầu MQTT
    if (!client.connected()) {
    connect_to_broker();  // Nếu mất kết nối MQTT, thử lại
  }
  Alert_connected();
  static String keySequence = ""; // Lưu chuỗi phím đã nhập

    // Quét bàn phím
    char key = scanKeypad();
    if (key != '\0') {
        keySequence += key; // Thêm ký tự vừa nhập
        if (keySequence.length() > 3) {
            keySequence = keySequence.substring(1); // Chỉ giữ lại 3 ký tự cuối
        }

        // Nếu nhập đủ "*0#", tự động chuyển chế độ OTP
        if (keySequence == "*0#") {
          //Đọc dữ liệu nhập từ bàn phím dưới dạng string
            String temp_ID = enterID();
            String temp_OTP = enterOTP();

          // Chuyển đổi kiểu string sang kiểu const char* để thực hiện việc gửi lên Server kiểm tra
            const char* id = temp_ID.c_str();
            const char* otp = temp_OTP.c_str();
            if (!client.connected()) {
            connect_to_broker();  // Nếu mất kết nối MQTT, thử lại
            }
            CheckOTP(id, otp);
            keySequence = ""; // Reset chuỗi sau khi chuyển chế độ
        }
    }

    //Nếu đã backup dữ liệu, cho phép thực hiện đọc RFID để mở khóa bất kì lúc nào
    if(backup_status == true){
      unlock_RFID();
    }
}