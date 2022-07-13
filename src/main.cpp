/*注意事项
本程序初步决定使用的是四个电磁阀，三个园子一个后面
如果要加电磁阀的话，有些部分的电磁阀遍历规律是不一样的
/////我是分割线///////
md 还是五个电磁阀好了。。。
todo:让电磁阀一个一个工作！，水不够了
*/
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <Ticker.h>
Ticker tk;
StaticJsonDocument<200> doc;
WiFiClient client;
struct tm timeinfo;
int Solenoid_Pin[4] = {18, 19, 22, 23}; //四个电磁阀使用的引脚
int Pump_pin = 25;                      //水泵使用的引脚
volatile int auto_watering_flag = 1;
volatile int carwash_flag = 0;
volatile int hand_watering_flag = 0;
volatile int pump_working_flag = 0;
int working_solenoid_valve[3] = {0, 0, 0};
volatile int solenoid_valve4 = 0;
volatile int reboot_flag = 0;
String time_status = "";
String solenoid_line = "Now:";
// volatile int which2debug_num = 0; //正在debug的电磁阀的序号,可能不需要这个变量了
volatile float soil_moisture = 0;
unsigned long time_flag = 0;
int work_times = 0;
// int pinled = 32;
unsigned long delaytime = 5 * 10 * 1000;
unsigned long mid_time = 0;
unsigned long carwash_time = 5 * 20 * 1000;
const char *ssid = "family_2.4g";
const char *password = "13505795150";
const char *host = "tcp.tlink.io";
const uint16_t httpPort = 8647;
const char *device_id = "Z858039W96UZ87H3";
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 4 * 3600;     //不知道为何是4*60*60
const int daylightOffset_sec = 4 * 3600; //不知道为何是4*60*60
int i = 0;
//int led_switch = 0;
int flag_time = 1;
char data[64];
void time_fun()
{ //中断之后跳到这里来，不要搞的太复杂
    flag_time = 1;
}
template <class T>
int length(T &arr)
{

    //cout << sizeof(arr[0]) << endl;
    //cout << sizeof(arr) << endl;
    return sizeof(arr) / sizeof(arr[0]);
}
void wifi_reconnect()
{
    while (WiFi.status() != WL_CONNECTED)
    {
        WiFi.begin(ssid, password);
        delay(500);
    }
}
void check_client_connected()
{
    while (!client.connected() && WiFi.status() == WL_CONNECTED)
    {
        Serial.println("prepare to reconnect...");
        client.connect(host, httpPort);
        if (client.connected())
        {
            Serial.println("send device_id");
            client.print(device_id); /*指定设备id*/
                                     //这个函数指的不是print，而是发送！
        }
    }
    //Serial.println("connection sucess!");
}
void send2clinet()
{
    sprintf(data, "#%s*%d*%d*%d*%f*%d*%s*%d#", solenoid_line, carwash_flag, auto_watering_flag, hand_watering_flag, soil_moisture, pump_working_flag, time_status, reboot_flag);
    // sprintf(data, "#1*%d*%d*%d*%d*%d#", carwash_flag, auto_watering_flag, hand_watering_flag, soil_moisture, pump_working_flag);
    Serial.print("回送的数据为：");
    Serial.println(data);
    client.print(data);
    delay(2000);
}
void pump_work()
{
    if (pump_working_flag == 1)
    {
        digitalWrite(Pump_pin, HIGH);
        // pump_working_flag = 1; //不要写，指挥与执行分离
    }
    else
    {
        digitalWrite(Pump_pin, LOW);
        //    pump_working_flag = 0;
    }
}
void Solenoid_OffAll(int a = 0) //here are just flags,no electricity;just solenoids,no pump.
{                               //输入的a不是index,而是人为的编号
    hand_watering_flag = 0;

    for (i = 0; i < length(Solenoid_Pin); i++)
    {
        if (a != 0 && i == a - 1)
        {
            working_solenoid_valve[a - 1] = 1;

            continue;
        }
        working_solenoid_valve[i] = 0;
    }
}

bool time2go()
{
    if (auto_watering_flag == 1)
    {
        if (!getLocalTime(&timeinfo))
        {
            time_status = "失去连接";
            Serial.println("Failed to obtain time");
            delay(2000);
        }
        else
        {
            time_status = "已校准";//校准时间,在时间达到6:20-40之间的时候启动!
            if (pump_working_flag == 0)
            {
                if (timeinfo.tm_hour == 6)
                {
                    if (timeinfo.tm_min > 20 && timeinfo.tm_min < 40)
                    {
                        pump_working_flag = 1;
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

void flag_execute()#fixme:在这里只能一个水泵一个水泵来
{
    if(pump_working_flag==0){
    solenoid_line = "Now:";                              //处理上报的正在工作的电磁阀
    for (i = 0; i < length(working_solenoid_valve); i++) //四个电磁阀
    {
        if (working_solenoid_valve[i] == 0)
        {
            digitalWrite(Solenoid_Pin[i], LOW);
        }
        else
        {
            digitalWrite(Solenoid_Pin[i], HIGH);
            if (i != 0)
            {
                solenoid_line.concat("+");
                solenoid_line.concat(String(i + 1));
            }
            else
            {
                solenoid_line.concat(i + 1);
            }
        }
    }
    if (solenoid_valve4 == 0)
    {
        digitalWrite(Solenoid_Pin[3], LOW);
    }
    else
    {
        solenoid_line.concat("4");
        digitalWrite(Solenoid_Pin[3], HIGH);
    }

    pump_work();
    }
    else{
        pump_work();
         solenoid_line = "Now:";                              //处理上报的正在工作的电磁阀
    for (i = 0; i < length(working_solenoid_valve); i++) //四个电磁阀
    {
        if (working_solenoid_valve[i] == 0)
        {
            digitalWrite(Solenoid_Pin[i], LOW);
        }
        else
        {
            digitalWrite(Solenoid_Pin[i], HIGH);
            if (i != 0)
            {
                solenoid_line.concat("+");
                solenoid_line.concat(String(i + 1));
            }
            else
            {
                solenoid_line.concat(i + 1);
            }
        }
    }
    if (solenoid_valve4 == 0)
    {
        digitalWrite(Solenoid_Pin[3], LOW);
    }
    else
    {
        solenoid_line.concat("4");
        digitalWrite(Solenoid_Pin[3], HIGH);
    }

    
    }
   
}

//再写一个把正在工作的电磁阀放到一个数组中收集好的函数
//要写一个多少时间后就自动关闭的功能，以方万一啊忘记关了
//↑ done
void setup()
{
    // put your setup code here, to run once:
    Serial.begin(9600);
    delay(10);
    for (i = 0; i < length(Solenoid_Pin); i++)
    {
        pinMode(Solenoid_Pin[i], OUTPUT);
    }
    pinMode(Pump_pin, OUTPUT);
    //todo:以后可以考虑加入一个开关，来保证esp32挂机的时候仍然可以手动启动水泵！！！！！
    //加入时间、wifi校验显示位置
    // We start by connecting to a WiFi network

    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("connecting to ");
    Serial.print(host);
    Serial.print(':');
    Serial.println(httpPort);
    //init and get the time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    // Use WiFiClient class to create TCP connections
    if (!client.connect(host, httpPort))
    {
        Serial.println("connection failed");
        delay(5000);
    }
    else
    {
        Serial.println("connection sucess!");
        tk.attach(40, time_fun); //s,中断服务函数，告诉你中断之后要跑到哪里去工作
        Serial.println("send device_id");
        client.print(device_id);
    } //设置这个中断而不是用delay的好处在于，中断之后改变flag，达到处理的目的
    //同时，对于整个函数，还是处于监听的状态，而不是像delay一样全部都停止了
}
void loop()
{
    wifi_reconnect();
    check_client_connected();
    // Serial.println(pump_working_flag);
    //Serial.println(type(pump_working_flag));
    if (1 == flag_time)
    {
        if (client.connected())
        {
            // Serial.println("send device_id");
            // client.print(device_id); /*指定设备id*/
            //                          //这个函数指的不是print，而是发送！
            Serial.println("send heart beat sense");
            client.print("q");
        }
        flag_time = 0;
    }
    if (client.available())
    { /*接收数据，client.available()这个函数是指服务器有数据回传*/
        Serial.print("available\n");
        String ch = client.readString();
        if (0 == ch.compareTo("A")) //直接相等好像不大可以的
        {
            Serial.println("heart beat check\n");
        }                                          //别把语句写到if与else这些东西之间
        else if (ch.toInt() < 5 && ch.toInt() > 0) //如果是数字的话，就开启的debug模式
        {   //todo:debug流量分享
            Serial.println(ch);
            pump_working_flag = 1; //打开泵,记录flag
            Solenoid_OffAll(ch.toInt());
        } //
        else
        {
            Serial.println(ch);
            DeserializationError error = deserializeJson(doc, ch); //将数据ch解析到doc中，然后在最后返回是解析成功与否
            if (error)
            {
                Serial.println("解析错误！");
            }
            else if (doc.containsKey("carwash"))
            { //得到洗车的信号
                carwash_flag = doc["carwash"];
                Serial.print("carwash_flag=");
                Serial.println(carwash_flag);
                Solenoid_OffAll();     //无论如何，都把所有的都给关上
                pump_working_flag = 1; //do not use 'delay'
                if (carwash_flag == 1)
                {
                    time_flag = millis();
                }

                /////////////////////////////////////////
            }
            else if (doc.containsKey("auto"))
            {
                auto_watering_flag = doc["auto"]; //设置好之后等待定时就好了
                if (auto_watering_flag == 0)
                {
                    Solenoid_OffAll();
                    pump_working_flag = 0;
                }
            }
            else if (doc.containsKey("hand"))
            {
                hand_watering_flag = doc["hand"]; //收到这个信号的时候代表我应该马上就需要浇水了
                if (hand_watering_flag == 1)
                {
                    pump_working_flag = 1;
                    time_flag = millis();
                    work_times = 4;
                }
                else
                {
                    Solenoid_OffAll();
                    pump_working_flag = 0;
                }
            }
            else if (doc.containsKey("restart"))
            {
                reboot_flag = doc["restart"];
                if (reboot_flag == 1)
                {
                    esp.restart();
                }
            }
        }
        //处理时间
    }
    if (auto_watering_flag == 1 && time2go()))
        {
            pump_working_flag = 1;
            time_flag = millis();
            work_times = 4;
        }

    if (hand_watering_flag == 1 || (auto_watering_flag == 1 && time2go())) //fixme:time2go可能需要再大一点；1.算好delay和time2go;2.做一个每天几次，或者上下午几次的东西
    {
        if (pump_working_flag == 1)
        {
            if (work_times > 1)
            {
                for (i = 0; i < length(working_solenoid_valve); i++)
                {
                    if ((4 - work_times) == i) //次数为5，电磁阀2、3工作；为4，1，3工作；以此类推. ps:index需要减一
                    {
                        working_solenoid_valve[i] = 0;
                    }
                    else
                    {
                        working_solenoid_valve[i] = 1;
                    }
                }
                // Serial.println(work_times);
                // Serial.println('else分界线');
            }
            else if (work_times == 1)
            {
                // Serial.println('1');
                for (i = 0; i < length(working_solenoid_valve); i++)
                {
                    // Serial.println("1");

                    working_solenoid_valve[i] = 0;
                }
                solenoid_valve4 = 1;
            }
            // mid_time = time_flag - mid_time; //fixme
            Serial.println(mid_time);
            if (time_flag > delaytime) //1秒 = 1000 毫秒
            {
                work_times = work_times - 1;
            }
            if (work_times == -1)
            {
                for (i = 0; i < length(working_solenoid_valve); i++)
                {
                    working_solenoid_valve[i] = 0;
                }
                solenoid_valve4 = 0;
                pump_working_flag = 0;
                hand_watering_flag = 0;
                // Serial.println('1');
                time_flag = 0;
                // Serial.println('1');
                mid_time = 0;
                // Serial.println('1');
            }
        }
    }
    if (carwash_flag == 1)
    {
        if (time_flag > carwash_time)
        {
            pump_working_flag = 0;
            carwash_flag = 0;
            time_flag = 0;
        }
    }
    flag_execute();
    send2clinet();
    delay(1000); //don't flood remote service
    //不要用mills代替，否则感觉让他休息的目的都达不到了，就是要阻塞在这里起
}
