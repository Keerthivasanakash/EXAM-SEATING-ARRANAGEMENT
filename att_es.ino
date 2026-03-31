#include <WiFi.h>
#include <HTTPClient.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>

#define BENCHES_PER_HALL 12
#define SEATS_PER_BENCH 2
#define TOTAL_HALLS 2

int seatsPerHall = BENCHES_PER_HALL * SEATS_PER_BENCH;

const char* ssid = "ESPTEST";
const char* password = "12345678";

#define FIREBASE_HOST "https://smart-att-sys-3e1e4-default-rtdb.asia-southeast1.firebasedatabase.app"

WiFiServer server(80);
LiquidCrystal_I2C lcd(0x27,16,2);

String today = "2026-02-28";


// ---------------- SEAT CALCULATION ----------------
void calculateSeat(String roll,int &hall,int &bench,int &seat){

  int studentNo = roll.substring(9,12).toInt();
  int seatIndex = studentNo - 1;

  hall = (seatIndex / seatsPerHall) + 1;
  int hallSeatIndex = seatIndex % seatsPerHall;

  bench = (hallSeatIndex / SEATS_PER_BENCH) + 1;
  seat = (hallSeatIndex % SEATS_PER_BENCH) + 1;
}


// ---------------- VERIFY FROM FIREBASE ----------------
bool verifyStudent(String roll,String &name,String &year){

  HTTPClient http;
  String url = String(FIREBASE_HOST)+"/students/"+roll+".json";

  http.begin(url);
  int httpCode=http.GET();

  if(httpCode==200){

    String payload=http.getString();

    if(payload=="null"){
      http.end();
      return false;
    }

    StaticJsonDocument<256> doc;
    deserializeJson(doc,payload);

    name=doc["name"].as<String>();
    year=doc["year"].as<String>();

    http.end();
    return true;
  }

  http.end();
  return false;
}


// ---------------- MARK ATTENDANCE ----------------
void markAttendance(String roll,int hall){

  HTTPClient http;
  String url = String(FIREBASE_HOST)+"/Attendance/"+today+"/"+roll+".json";

  String jsonData = "{\"status\":\"Present\",\"hall\":"+String(hall)+"}";

  http.begin(url);
  http.addHeader("Content-Type","application/json");
  http.PUT(jsonData);
  http.end();
}


// ---------------- LCD DISPLAY ----------------
void displayLCD(String name,String year,int hall,int bench,int seat){

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Y:");
  lcd.print(year);
  lcd.print(" H:");
  lcd.print(hall);

  lcd.setCursor(0,1);
  lcd.print("B:");
  lcd.print(bench);
  lcd.print(" S:");
  lcd.print(seat);
}


// ---------------- MAIN PAGE ----------------
void showMainPage(WiFiClient client){

client.println("HTTP/1.1 200 OK");
client.println("Content-Type: text/html");
client.println("Connection: close");
client.println();

client.println("<!DOCTYPE html>");
client.println("<html><body style='font-family:Segoe UI;background:#141e30;color:yellow;text-align:center;'>");
client.println("<h2>Smart Exam System</h2>");
client.println("<form action='/'>");
client.println("<input name='roll' placeholder='ENTER ROLL NO' style='padding:12px;font-size:18px;width:250px;border-radius:8px;border:2px solid #00bcd4;'>");
client.println("<input type='submit' value='FIND SEAT' style='padding:12px 20px;font-size:18px;background:#00bcd4;color:white;border:none;border-radius:8px;cursor:pointer;margin-left:10px;'>");
client.println("</form>");
client.println("<br><a href='/hall1'style='color:yellow;font-size:25px'>HALL 1 GRID</a>");
client.println("<br><a href='/hall2'style='color:yellow;font-size:25px'>HALL 2 GRID</a>");
client.println("<br><a href='/status'style='color:yellow;font-size:25px'>VIEW STATUS</a>");
client.println("</body></html>");

client.stop();
}


// ---------------- HALL GRID ----------------
void showHallGrid(WiFiClient client,int hallNo){

client.println("HTTP/1.1 200 OK");
client.println("Content-Type: text/html");
client.println("Connection: close");
client.println();

client.println("<!DOCTYPE html>");
client.println("<html>");
client.println("<head>");
client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
client.println("<style>");
client.println("body{font-family:Segoe UI;background:#141e30;color:white;text-align:center;}");
client.println(".bench{display:flex;justify-content:center;margin:8px;}");
client.println(".seat{width:60px;height:60px;background:#e74c3c;margin:5px;border-radius:10px;display:flex;align-items:center;justify-content:center;}");
client.println("</style>");
client.println("</head>");
client.println("<body>");

client.println("<h2>Hall "+String(hallNo)+" Layout</h2>");

for(int b=1;b<=BENCHES_PER_HALL;b++){

  client.println("<div class='bench'>");

  for(int s=1;s<=SEATS_PER_BENCH;s++){
    client.print("<div class='seat'>B");
    client.print(b);
    client.print("<br>S");
    client.print(s);
    client.println("</div>");
  }

  client.println("</div>");
}

client.println("<br><a href='/'>Back</a>");
client.println("</body></html>");

client.stop();
}


// ---------------- STATUS PAGE ----------------
void showStatusPage(WiFiClient client){

int h1=0,h2=0,total=0;

HTTPClient http;
String url = String(FIREBASE_HOST)+"/Attendance/"+today+".json";
http.begin(url);
int httpCode=http.GET();

if(httpCode==200){

  String payload=http.getString();

  if(payload!="null"){

    StaticJsonDocument<4096> doc;
    deserializeJson(doc,payload);

    for(JsonPair kv : doc.as<JsonObject>()){
      int hall = kv.value()["hall"];

      if(hall==1) h1++;
      if(hall==2) h2++;
      total++;
    }
  }
}
http.end();

client.println("HTTP/1.1 200 OK");
client.println("Content-Type: text/html");
client.println("Connection: close");
client.println();

client.println("<!DOCTYPE html>");
client.println("<html><body style='background:#141e30;color:white;text-align:center;'>");
client.println("<h2>Hall Status</h2>");
client.println("<h3>Hall 1 Present: "+String(h1)+"</h3>");
client.println("<h3>Hall 2 Present: "+String(h2)+"</h3>");
client.println("<h3>Total Present: "+String(total)+"</h3>");
client.println("<br><a href='/'>Back</a>");
client.println("</body></html>");

client.stop();
}


// ---------------- SETUP ----------------
void setup(){

  Serial.begin(115200);
  lcd.init();
  lcd.backlight();

  WiFi.begin(ssid,password);

  while(WiFi.status()!=WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected");
  Serial.println(WiFi.localIP());

  server.begin();
  lcd.print("System Ready");
}


// ---------------- LOOP ----------------
void loop(){

  WiFiClient client = server.available();

  if(client){

    String request = client.readStringUntil('\r');
    client.flush();

    if(request.indexOf("roll=")!=-1){

      int start=request.indexOf("roll=")+5;
      int end=request.indexOf(" ",start);
      String roll=request.substring(start,end);

      String name,year;

      if(verifyStudent(roll,name,year)){

        int hall,bench,seat;
        calculateSeat(roll,hall,bench,seat);

        displayLCD(name,year,hall,bench,seat);
        markAttendance(roll,hall);

        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/html");
        client.println("Connection: close");
        client.println();

        client.println("<html><body style='background:#141e30;color:white;text-align:center;'>");
        client.println("<h2 style='font-size:40px;'>"+name+"</h2>");
        client.println("<p style='font-size:35px;'>YEAR: "+year+"</p>");
        client.println("<p style='font-size:35px;'>HALL: "+String(hall)+"</p>");
        client.println("<p style='font-size:35px;'>BENCH: "+String(bench)+"</p>");
        client.println("<p style='font-size:35px;'>SEAT: "+String(seat)+"</p>");
        client.println("<br><a href='/'>Back</a>");
        client.println("</body></html>");
        client.stop();
      }
      else{
        lcd.clear();
        lcd.print("Invalid Roll");

        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/html");
        client.println("Connection: close");
        client.println();
        client.println("<html><body style='background:#141e30;color:white;text-align:center;'>");
        client.println("Invalid Register Number<br><a href='/'>Back</a>");
        client.println("</body></html>");
        client.stop();
      }
    }
    else if(request.indexOf("/hall1")!=-1){
      showHallGrid(client,1);
    }
    else if(request.indexOf("/hall2")!=-1){
      showHallGrid(client,2);
    }
    else if(request.indexOf("/status")!=-1){
      showStatusPage(client);
    }
    else{
      showMainPage(client);
    }
  }
}