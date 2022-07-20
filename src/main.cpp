/*注意事项
本程序初步决定使用的是四个电磁阀，三个园子一个后面
如果要加电磁阀的话，有些部分的电磁阀遍历规律是不一样的
/////我是分割线///////
md 还是3个电磁阀好了。。。
让电磁阀一个一个工作！，水不够了
*/
// todo:可以做一个不同地区的不同浇水比例的工具
////我觉得时间变量的销毁有问题!!!
//! really important👆
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <Ticker.h>
#include <String.h>
///以下是湿度传感器所需要的数据
#include <SoftwareSerial.h>
unsigned char item[8] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x02, 0xC4, 0x0B}; // 16进制测温命令
String data_soil = "";                                                    // 接收到的16进制字符串
SoftwareSerial tempSerial(17, 16);                                        //? 定义了RX, TX,最好不要用16和17

///以下是中心浇水控制器需要的参数
Ticker tk;
StaticJsonDocument<200> doc;
WiFiClient client;
struct tm timeinfo;
int Solenoid_Pin[3] = {18, 19, 22}; //三个电磁阀使用的引脚
int Pump_pin = 25;                  //水泵使用的引脚
int auto_watering_flag = 1;
int carwash_flag = 0;
int hand_watering_flag = 0;
int pump_working_flag = 0;
int working_solenoid_valve[3] = {0, 0, 0};
// volatile int solenoid_valve4 = 0;
int reboot_flag = 0;
String time_status = "";
String solenoid_line = "Now:";
// volatile int which2debug_num = 0; //正在debug的电磁阀的序号,可能不需要这个变量了
float soil_moisture = 0;
float soil_moisture_need = 20; //?
int *time_flag = new int(0);   //必须这样做否则没法delete
int work_times = 0;
int wat_begin_hour = 4;
int wat_begin_min = 40;
int soil2wat = 0; //如果是这个状态代表因为土壤干燥正在浇水
// int pinled = 32;
int delaytime = 1200 * 1000; // 500*1000
int carwash_time = 2400 * 1000;
const char *ssid = "family_2.4g";
const char *password = "13505795150";
const char *host = "tcp.tlink.io";
const uint16_t httpPort = 8647;
const char *device_id = "Z858039W96UZ87H3";
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 4 * 3600;     //不知道为何是4*60*60
const int daylightOffset_sec = 4 * 3600; //不知道为何是4*60*60
int i = 0;
// int led_switch = 0;
int breakpoint_flag = 1; // 23333这个是控制断点的,不是time_flag
char data[64];
char time_temp[10];
char set_begin_time[10];
void time_fun()
{ //中断之后跳到这里来，不要搞的太复杂
    breakpoint_flag = 1;
}
String fenge(String str, String fen, int index)
{
    int weizhi;
    String temps[str.length()];
    int i = 0;
    do
    {
        weizhi = str.indexOf(fen);
        if (weizhi != -1)
        {
            temps[i] = str.substring(0, weizhi);
            str = str.substring(weizhi + fen.length(), str.length());
            i++;
        }
        else
        {
            if (str.length() > 0)
                temps[i] = str;
        }
    } while (weizhi >= 0);

    if (index > i)
        return "-1";
    return temps[index];
}

template <class T>
int length(T &arr)
{

    // cout << sizeof(arr[0]) << endl;
    // cout << sizeof(arr) << endl;
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
void check_client_connected() //检测与tlink服务器的连接状态
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
    // Serial.println("connection sucess!");
}
void send2clinet()
{
    sprintf(set_begin_time, "%d:%d", wat_begin_hour, wat_begin_min); //回传+读取
    sprintf(data, "#%s*%d*%d*%d*%f*%d*%s*%d*%f*%lu*%s#", solenoid_line, carwash_flag, auto_watering_flag, hand_watering_flag, soil_moisture, pump_working_flag, time_status, reboot_flag, soil_moisture_need, delaytime / 60000, set_begin_time);
    // unsigned
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
void Solenoid_OffAll(int a = 0) // here are just flags,no electricity;just solenoids,no pump.
{                               //输入的a不是index,而是人为的编号
    solenoid_line = "nothing";
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
bool time_plus_check(int wat_begin_hour, int wat_begin_min, tm timeinfo)
{                     //这个gap不能超过30!不能让客户操作这个数值
    int time_gap = 7; //在五分钟内必须进行反应
    if (wat_begin_min < time_gap)
    { //?对时间状态进行修改
        if ((timeinfo.tm_hour == wat_begin_hour - 1 && timeinfo.tm_min > wat_begin_min - time_gap + 60) || (timeinfo.tm_hour == wat_begin_hour && timeinfo.tm_min < wat_begin_min + time_gap))
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    else if (time_gap + wat_begin_min > 60)
    {
        if ((timeinfo.tm_hour == wat_begin_hour && timeinfo.tm_min > wat_begin_min - time_gap) || (timeinfo.tm_hour == wat_begin_hour + 1 && timeinfo.tm_min < wat_begin_min + time_gap - 60))
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        if ((timeinfo.tm_hour == wat_begin_hour && timeinfo.tm_min > wat_begin_min - time_gap) && (timeinfo.tm_hour == wat_begin_hour && timeinfo.tm_min < wat_begin_min + time_gap))
        {
            return true;
        }
        else
        {
            return false;
        }
    }
}

bool get_localtime()
{
    if (!getLocalTime(&timeinfo))
    {
        time_status = "Failed";
        Serial.println("Failed to obtain time");
        delay(2000);
        return false;
    }
    else
    {
        sprintf(time_temp, "%d:%d", timeinfo.tm_hour, timeinfo.tm_min);
        time_status = String(time_temp);
        return true;
    }
}

bool time2go()
{
    if (auto_watering_flag == 1)
    {
        if (work_times > 0)
        { //如果手头上还有工作先做掉
            return true;
        }
        if (get_localtime())
        {
            if (pump_working_flag == 0)
            {
                Serial.print("in ttg");
                Serial.print(pump_working_flag);
                // if (timeinfo.tm_hour > wat_begin_hour && timeinfo.tm_min < wat_begin_min)//fixme:做多次循环的话这里还回来吗
                if (time_plus_check(wat_begin_hour, wat_begin_min, timeinfo))
                {
                    pump_working_flag = 1;
                    *time_flag = millis();
                    if (soil2wat == 0)
                    {
                        work_times = 3;
                        soil2wat = 1;
                    }
                    return true;
                }
            }
        }
    }
    return false;
}

void flag_execute() //这个函数只负责给电,不负责别的操作
{
    Serial.print("fle");
    Serial.print(pump_working_flag);
    if (pump_working_flag == 0)
    {
        solenoid_line = "nothing";
        pump_work();
        for (i = 0; i < length(working_solenoid_valve); i++) //电磁阀全部关掉
        {
            digitalWrite(Solenoid_Pin[i], LOW);
        }
    }

    else
    {
        // Serial.print(pump_working_flag);
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

                solenoid_line.concat(i + 1);
            }
        }

        pump_work();
    }
}
float getTemp(String temp)
{
    int commaPosition = -1;
    String info[9]; // 用字符串数组存储
    for (int i = 0; i < 9; i++)
    {
        commaPosition = temp.indexOf(',');
        if (commaPosition != -1)
        {
            info[i] = temp.substring(0, commaPosition);
            temp = temp.substring(commaPosition + 1, temp.length());
        }
        else
        {
            if (temp.length() > 0)
            { // 最后一个会执行这个
                info[i] = temp.substring(0, commaPosition);
            }
        }
    }
    return (info[3].toInt() * 256 + info[4].toInt()) / 10.0; ////这里传回的是一个整数,我们需要小数
}

void check_soil() //检测土壤湿度
{
    delay(500); // 放慢输出频率
    for (int i = 0; i < 8; i++)
    {                              // 发送测温命令
        tempSerial.write(item[i]); // write输出
                                   // Serial.println(item[i]);
    }
    delay(100); // 等待测温数据返回
    data_soil = "";
    while (tempSerial.available())
    { //从串口中读取数据
        // 这里读不出数据
        unsigned char in = (unsigned char)tempSerial.read(); // read读取
        // Serial.print(in, HEX);
        // Serial.print(',');
        data_soil += in;
        data_soil += ',';
    }

    if (data_soil.length() > 0)
    { //先输出一下接收到的数据
        // Serial.println();
        // Serial.println(data);
        soil_moisture = getTemp(data_soil);
        Serial.print(soil_moisture);
        Serial.println("%water");
    }
}
////需要再写一个判断土壤湿度是否适宜的程序,也从网上读取参数
bool soil_go()
{
    check_soil();
    if (soil2wat == 1)
    { //表示目前水还在浇水
        return true;
    }
    if (soil_moisture_need > soil_moisture && auto_watering_flag == 1)
    {
        pump_working_flag = 1;
        *time_flag = millis();
        if (soil2wat == 0)
        {
            work_times = 3;
            soil2wat = 1;
        }
        return true;
    }
    else
    {
        // pump_working_flag = 0;这样写有很大的问题!
        return false;
    }
}

void shut_all()
{ // //做一个关闭所有正在进行状态的东西,避免如正在自动浇水的时候打开carwash的情况
    pump_working_flag = 0;
    soil2wat = 0;
    Solenoid_OffAll(0);
    delete time_flag; //&返回地址,*返回所对应的值
    int *time_flag = new int(0);
}

//再写一个把正在工作的电磁阀放到一个数组中收集好的函数
//要写一个多少时间后就自动关闭的功能，以方万一啊忘记关了
//↑ done
void setup()
{
    // put your setup code here, to run once:
    tempSerial.begin(4800);
    Serial.begin(9600);
    delay(10);
    for (i = 0; i < length(Solenoid_Pin); i++)
    {
        pinMode(Solenoid_Pin[i], OUTPUT);
    }
    pinMode(Pump_pin, OUTPUT);
    // todo:以后可以考虑加入一个开关，来保证esp32挂机的时候仍然可以手动启动水泵！！！！！
    //加入时间、wifi校验显示位置
    //  We start by connecting to a WiFi network

    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);
    wifi_reconnect(); //应该用这个更好一点
    // while (WiFi.status() != WL_CONNECTED)
    // {
    //     delay(500);
    //     Serial.print(".");
    // }
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("connecting to ");
    Serial.print(host);
    Serial.print(':');
    Serial.println(httpPort);
    // init and get the time
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
        tk.attach(40, time_fun); // s,中断服务函数，告诉你中断之后要跑到哪里去工作
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
    // Serial.println(type(pump_working_flag));
    if (1 == breakpoint_flag)
    {
        if (client.connected())
        {
            // Serial.println("send device_id");
            // client.print(device_id); /*指定设备id*/
            //                         //这个函数指的不是print，而是发送！
            Serial.println("send heart beat sense");
            client.print("q");
        }
        breakpoint_flag = 0;
    }
    if (client.available()) //别把语句写到if与else这些东西之间
    {                       /*接收数据，client.available()这个函数是指服务器有数据回传*/
        Serial.print("available\n");
        String ch = client.readString();
        Serial.println(ch);
        if (0 == ch.compareTo("A")) //直接相等好像不大可以的
        {
            Serial.println("heart beat check\n");
        }
        else if (ch.lastIndexOf('%') != -1)
        { //读取你所需要的最低湿度
            Serial.println("lowest wet check\n");
            soil_moisture_need = ch.substring(0, ch.length() - 1).toFloat();
        }
        else if (ch.lastIndexOf('x') != -1)
        { //!读取你所需要的最少时间,小心字母重复!
            Serial.println("least time check\n");
            delaytime = ch.substring(0, ch.length() - 1).toInt() * 60000; /////这个时间不是很对的,需要测试的时候得到相关时间的单位然后进行修改!
            Serial.print(delaytime);
            carwash_time = delaytime * 2;
        }
        else if (ch.lastIndexOf(':') != -1 && ch.lastIndexOf('v') != -1)
        { //应当输入5:31v
            Serial.println("began time check\n");
            wat_begin_hour = fenge(ch, ":", 0).toInt();
            wat_begin_min = fenge(ch, ":", 1).toInt();
        }
        else if (ch.toInt() < 5 && ch.toInt() >= 1) //如果是数字的话，就开启的debug模式
        {
                Serial.println("which pump check\n");
                pump_working_flag = 1; //打开泵,记录flag
                Solenoid_OffAll(ch.toInt());
                // Serial.print(working_solenoid_valve[0]);
                // Serial.print(working_solenoid_valve[1]);
                // Serial.println(working_solenoid_valve[2]);
                // Serial.print("blo");
                // Serial.print(pump_working_flag);
        } //
        else
        {
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

                Solenoid_OffAll(0);    //无论如何，都把所有的都给关上
                pump_working_flag = 1; // do not use 'delay'
                if (carwash_flag == 1) ////carwash应该启动的时候禁止使用别的浇水!
                {
                    *time_flag = millis();
                }
                else
                {
                    carwash_flag = 0;
                    shut_all();
                }

                /////////////////////////////////////////
            }
            else if (doc.containsKey("auto"))
            {
                auto_watering_flag = doc["auto"]; //设置好之后等待定时就好了
                if (auto_watering_flag == 0)
                {
                    shut_all();
                }
            }
            else if (doc.containsKey("hand"))
            {
                hand_watering_flag = doc["hand"]; //收到这个信号的时候代表我应该马上就需要浇水了
                // Serial.print("inhand");
                // Serial.print(hand_watering_flag);
                if (hand_watering_flag == 1)
                {
                    //                     Serial.print("cnm");
                    // Serial.print(pump_working_flag);
                    pump_working_flag = 1;
                    *time_flag = millis();
                    work_times = 3;
                }
                else
                {
                    shut_all();
                }
            }
            else if (doc.containsKey("restart"))
            {
                reboot_flag = doc["restart"];
                if (reboot_flag == 1)
                {
                    ESP.restart();
                }
            }
        }
        //处理时间
    }
    //这些好像和下面的代码矛盾了吧
    //// if (auto_watering_flag == 1 && time2go())
    ////     {
    ////         pump_working_flag = 1;
    ////         time_flag = millis();
    ////         work_times = 3;
    ////     }
    Serial.print("out");
    Serial.print(pump_working_flag);
    if (0 == carwash_flag)
    {
        if (((time2go() || soil_go()) && 1 == auto_watering_flag) || 1 == hand_watering_flag) // fixme:time2go可能需要再大一点；1.算好delay和time2go;2.做一个每天几次，或者上下午几次的东西
        //这里如果auto_watering_flag==1在前面,当其为0的时候time2go()和soil_go()不会执行了,所以应该放在后面
        {
            Serial.print("watt");
            Serial.print(pump_working_flag);
            if (pump_working_flag == 1)
            {
                if (work_times > 0)
                {
                    for (i = 0; i < length(working_solenoid_valve); i++)
                    {

                        working_solenoid_valve[i] = 0;
                    }

                    working_solenoid_valve[3 - work_times] = 1;

                    // Serial.println(work_times);
                    // Serial.println('else分界线');
                }

                // mid_time = time_flag - mid_time;
                *time_flag = millis();
                Serial.println(*time_flag);
                Serial.println(work_times);
                if (*time_flag > delaytime * (4 - work_times)) // 1秒 = 1000 毫秒
                {                                              ////这里是不是没有做减法?建议调试之后再操作这里

                    work_times = work_times - 1;
                }
                if (work_times == 0) //不能是-1否则会在最后一个引脚多执行一次
                {
                    shut_all();
                    hand_watering_flag = 0;
                }
            }
        }
    }
    else //1 == carwash_flag
    {
        if (*time_flag > carwash_time)
        {
            shut_all();
            carwash_flag = 0;
        }
    }
    check_soil(); //?也许不写在这里?写在一开始就检测湿度的地方?
    get_localtime();
    flag_execute();
    send2clinet();
    delay(1000); // don't flood remote service
    //不要用mills代替，否则感觉让他休息的目的都达不到了，就是要阻塞在这里起
}
