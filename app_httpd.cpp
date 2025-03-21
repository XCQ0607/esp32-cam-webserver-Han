// Original Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <esp_http_server.h>
#include <esp_timer.h>
#include <esp_camera.h>
#include <esp_int_wdt.h>
#include <esp_task_wdt.h>
#include <Arduino.h>
#include <WiFi.h>

#include "index_ov2640.h"
#include "index_ov3660.h"
#include "index_other.h"
#include "css.h"
#include "src/favicons.h"
#include "src/logo.h"
#include "storage.h"

// Functions from the main .ino
extern void flashLED(int flashtime);
extern void setLamp(int newVal);
extern void printLocalTime(bool extraData);

// External variables declared in the main .ino
extern char myName[];
extern char myVer[];
extern char baseVersion[];
extern IPAddress ip;
extern IPAddress net;
extern IPAddress gw;
extern bool accesspoint;
extern char apName[];
extern bool captivePortal;
extern int httpPort;
extern int streamPort;
extern char httpURL[];
extern char streamURL[];
extern char default_index[];
extern int8_t streamCount;
extern unsigned long streamsServed;
extern unsigned long imagesServed;
extern int myRotation;
extern int minFrameTime;
extern int lampVal;
extern bool autoLamp;
extern bool filesystem;
extern String critERR;
extern bool debugData;
extern bool haveTime;
extern int sketchSize;
extern int sketchSpace;
extern String sketchMD5;
extern bool otaEnabled;
extern char otaPassword[];
extern unsigned long xclk;
extern int sensorPID;

typedef struct {
        httpd_req_t *req;
        size_t len;
} jpg_chunking_t;

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;

// Flag that can be set to kill all active streams
bool streamKill;

#ifdef __cplusplus
extern "C" {
#endif
uint8_t temprature_sens_read();
#ifdef __cplusplus
}
#endif

void serialDump() {
    Serial.println();
    // 模块信息
    Serial.printf("名称: %s\r\n", myName);
    if (haveTime) {
        Serial.print("时间: ");
        printLocalTime(true);
    }
    Serial.printf("固件版本: %s (基础版本: %s)\r\n", myVer, baseVersion);
    float sketchPct = 100 * sketchSize / sketchSpace;
    Serial.printf("程序大小: %i (总空间: %i, 已使用 %.1f%%)\r\n", sketchSize, sketchSpace, sketchPct);
    Serial.printf("MD5校验: %s\r\n", sketchMD5.c_str());
    Serial.printf("ESP SDK版本: %s\r\n", ESP.getSdkVersion());
    if (otaEnabled) {
         if (strlen(otaPassword) != 0) {
            Serial.printf("OTA: 已启用, 密码: %s\n\r", otaPassword);
         } else {
            Serial.printf("OTA: 已启用, 无密码! (不安全)\n\r");
         }
    } else {
        Serial.printf("OTA: 已禁用\n\r");
    }

    // 网络信息
    if (accesspoint) {
        if (captivePortal) {
            Serial.printf("WiFi模式: 接入点(带验证门户)\r\n");
        } else {
            Serial.printf("WiFi模式: 接入点\r\n");
        }
        Serial.printf("WiFi SSID: %s\r\n", apName);
    } else {
        Serial.printf("WiFi模式: 客户端\r\n");
        String ssidName = WiFi.SSID();
        Serial.printf("WiFi名称: %s\r\n", ssidName.c_str());
        Serial.printf("WiFi信号强度: %i\r\n", WiFi.RSSI());
        String bssid = WiFi.BSSIDstr();
        Serial.printf("WiFi BSSID: %s\r\n", bssid.c_str());
    }
    Serial.printf("WiFi IP地址: %d.%d.%d.%d\r\n", ip[0], ip[1], ip[2], ip[3]);
    if (!accesspoint) {
        Serial.printf("WiFi 子网掩码: %d.%d.%d.%d\r\n", net[0], net[1], net[2], net[3]);
        Serial.printf("WiFi 网关: %d.%d.%d.%d\r\n", gw[0], gw[1], gw[2], gw[3]);
    }
    Serial.printf("WiFi HTTP端口: %i, 流媒体端口: %i\r\n", httpPort, streamPort);
    byte mac[6];
    WiFi.macAddress(mac);
    Serial.printf("WiFi MAC地址: %02X:%02X:%02X:%02X:%02X:%02X\r\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // 系统信息
    int64_t sec = esp_timer_get_time() / 1000000;
    int64_t upDays = int64_t(floor(sec/86400));
    int upHours = int64_t(floor(sec/3600)) % 24;
    int upMin = int64_t(floor(sec/60)) % 60;
    int upSec = sec % 60;
    int McuTc = (temprature_sens_read() - 32) / 1.8; // 摄氏度
    int McuTf = temprature_sens_read(); // 华氏度
    Serial.printf("系统运行时间: %" PRId64 ":%02i:%02i:%02i (天:时:分:秒)\r\n", upDays, upHours, upMin, upSec);
    Serial.printf("活动流数: %i, 历史流数: %lu, 已捕获图像: %lu\r\n", streamCount, streamsServed, imagesServed);
    Serial.printf("CPU频率: %i MHz, Xclk频率: %i MHz\r\n", ESP.getCpuFreqMHz(), xclk);
    Serial.printf("MCU温度: %i °C, %i °F (近似值)\r\n", McuTc, McuTf);
    Serial.printf("堆内存: %i, 空闲: %i, 最小空闲: %i, 最大块: %i\r\n", ESP.getHeapSize(), ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
    if(psramFound()) {
        Serial.printf("PSRAM: %i, 空闲: %i, 最小空闲: %i, 最大块: %i\r\n", ESP.getPsramSize(), ESP.getFreePsram(), ESP.getMinFreePsram(), ESP.getMaxAllocPsram());
    } else {
        Serial.printf("PSRAM: 未找到; 请检查开发板配置\r\n");
        Serial.printf("- 由于内存不足,高分辨率/高质量设置可能会显示不完整的帧\r\n");
    }

    // 文件系统
    if (filesystem && (SPIFFS.totalBytes() > 0)) {
        Serial.printf("SPIFFS: %i, 已使用: %i\r\n", SPIFFS.totalBytes(), SPIFFS.usedBytes());
    } else {
        Serial.printf("SPIFFS: 未找到文件系统,请检查开发板配置\r\n");
        Serial.printf("- 没有文件系统将无法保存和恢复相机设置\r\n");
    }
    Serial.println("首选项文件: ");
    dumpPrefs(SPIFFS);
    if (critERR.length() > 0) {
        Serial.printf("\r\n\r\n相机硬件发生错误,请查看之前的消息\r\n");
        Serial.printf("需要重启才能恢复\r\n错误信息: (html)\r\n %s\r\n\r\n", critERR.c_str());
    }
    Serial.println();
    return;
}

static esp_err_t capture_handler(httpd_req_t *req){
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;

    Serial.println("收到图像捕获请求");
    if (autoLamp && (lampVal != -1)) {
        setLamp(lampVal);
        delay(75); // 与状态LED闪烁配合,给灯约150ms的稳定时间
    }
    flashLED(75); // 状态LED短闪

    int64_t fr_start = esp_timer_get_time();

    fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("捕获: 获取帧失败");
        httpd_resp_send_500(req);
        if (autoLamp && (lampVal != -1)) setLamp(0);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    size_t fb_len = 0;
    if(fb->format == PIXFORMAT_JPEG){
        fb_len = fb->len;
        res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    } else {
        res = ESP_FAIL;
        Serial.println("捕获错误: 相机模块返回非JPEG图像");
    }
    esp_camera_fb_return(fb);
    fb = NULL;

    int64_t fr_end = esp_timer_get_time();
    if (debugData) {
        Serial.printf("JPEG: %uB %ums\r\n", (uint32_t)(fb_len), (uint32_t)((fr_end - fr_start)/1000));
    }
    imagesServed++;
    if (autoLamp && (lampVal != -1)) {
        setLamp(0);
    }
    return res;
}

static esp_err_t stream_handler(httpd_req_t *req){
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t * _jpg_buf = NULL;
    char * part_buf[64];

    streamKill = false;

    Serial.println("收到流媒体请求");
    if (autoLamp && (lampVal != -1)) setLamp(lampVal);
    streamCount = 1;  // 目前只有一个流处理器,所以值为0或1
    flashLED(75);     // 状态LED双闪
    delay(75);
    flashLED(75);

    static int64_t last_frame = 0;
    if(!last_frame) {
        last_frame = esp_timer_get_time();
    }

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if(res != ESP_OK){
        streamCount = 0;
        if (autoLamp && (lampVal != -1)) setLamp(0);
        Serial.println("流媒体: 设置HTTP响应类型失败");
        return res;
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    if(res == ESP_OK){
        res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }

    while(true){
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("流媒体: 获取帧失败");
            res = ESP_FAIL;
        } else {
            if(fb->format != PIXFORMAT_JPEG){
                Serial.println("流媒体: 相机模块返回非JPEG帧");
                res = ESP_FAIL;
            } else {
                _jpg_buf_len = fb->len;
                _jpg_buf = fb->buf;
            }
        }
        if(res == ESP_OK){
            size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }
        if(fb){
            esp_camera_fb_return(fb);
            fb = NULL;
            _jpg_buf = NULL;
        } else if(_jpg_buf){
            free(_jpg_buf);
            _jpg_buf = NULL;
        }
        if(res != ESP_OK){
            // 这是流循环的错误退出点
            // 只有在遇到硬故障或连接中断时才在此处结束流
            Serial.printf("流媒体失败, 代码 = %i : %s\r\n", res, esp_err_to_name(res));
            break;
        }
        if((res != ESP_OK) || streamKill){
            // 收到终止信号时在此处结束流
            Serial.printf("流媒体已终止\r\n");
            break;
        }
        int64_t frame_time = esp_timer_get_time() - last_frame;
        frame_time /= 1000;
        int32_t frame_delay = (minFrameTime > frame_time) ? minFrameTime - frame_time : 0;
        delay(frame_delay);

        if (debugData) {
            Serial.printf("MJPG: %uB %ums, 延迟: %ums, 帧率 (%.1ffps)\r\n",
                (uint32_t)(_jpg_buf_len),
                (uint32_t)frame_time, frame_delay, 1000.0 / (uint32_t)(frame_time + frame_delay));
        }
        last_frame = esp_timer_get_time();
    }

    streamsServed++;
    streamCount = 0;
    if (autoLamp && (lampVal != -1)) setLamp(0);
    Serial.println("流媒体已结束");
    last_frame = 0;
    return res;
}

static esp_err_t cmd_handler(httpd_req_t *req){
    char*  buf;
    size_t buf_len;
    char variable[32] = {0,};
    char value[32] = {0,};

    flashLED(75);

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = (char*)malloc(buf_len);
        if(!buf){
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            if (httpd_query_key_value(buf, "var", variable, sizeof(variable)) == ESP_OK &&
                httpd_query_key_value(buf, "val", value, sizeof(value)) == ESP_OK) {
            } else {
                free(buf);
                httpd_resp_send_404(req);
                return ESP_FAIL;
            }
        } else {
            free(buf);
            httpd_resp_send_404(req);
            return ESP_FAIL;
        }
        free(buf);
    } else {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    if (critERR.length() > 0) return httpd_resp_send_500(req);

    int val = atoi(value);
    sensor_t * s = esp_camera_sensor_get();
    int res = 0;
    if(!strcmp(variable, "framesize")) {
        if(s->pixformat == PIXFORMAT_JPEG) res = s->set_framesize(s, (framesize_t)val);
    }
    else if(!strcmp(variable, "quality")) res = s->set_quality(s, val);
    else if(!strcmp(variable, "xclk")) { xclk = val; res = s->set_xclk(s, LEDC_TIMER_0, val); }
    else if(!strcmp(variable, "contrast")) res = s->set_contrast(s, val);
    else if(!strcmp(variable, "brightness")) res = s->set_brightness(s, val);
    else if(!strcmp(variable, "saturation")) res = s->set_saturation(s, val);
    else if(!strcmp(variable, "gainceiling")) res = s->set_gainceiling(s, (gainceiling_t)val);
    else if(!strcmp(variable, "colorbar")) res = s->set_colorbar(s, val);
    else if(!strcmp(variable, "awb")) res = s->set_whitebal(s, val);
    else if(!strcmp(variable, "agc")) res = s->set_gain_ctrl(s, val);
    else if(!strcmp(variable, "aec")) res = s->set_exposure_ctrl(s, val);
    else if(!strcmp(variable, "hmirror")) res = s->set_hmirror(s, val);
    else if(!strcmp(variable, "vflip")) res = s->set_vflip(s, val);
    else if(!strcmp(variable, "awb_gain")) res = s->set_awb_gain(s, val);
    else if(!strcmp(variable, "agc_gain")) res = s->set_agc_gain(s, val);
    else if(!strcmp(variable, "aec_value")) res = s->set_aec_value(s, val);
    else if(!strcmp(variable, "aec2")) res = s->set_aec2(s, val);
    else if(!strcmp(variable, "dcw")) res = s->set_dcw(s, val);
    else if(!strcmp(variable, "bpc")) res = s->set_bpc(s, val);
    else if(!strcmp(variable, "wpc")) res = s->set_wpc(s, val);
    else if(!strcmp(variable, "raw_gma")) res = s->set_raw_gma(s, val);
    else if(!strcmp(variable, "lenc")) res = s->set_lenc(s, val);
    else if(!strcmp(variable, "special_effect")) res = s->set_special_effect(s, val);
    else if(!strcmp(variable, "wb_mode")) res = s->set_wb_mode(s, val);
    else if(!strcmp(variable, "ae_level")) res = s->set_ae_level(s, val);
    else if(!strcmp(variable, "rotate")) myRotation = val;
    else if(!strcmp(variable, "min_frame_time")) minFrameTime = val;
    else if(!strcmp(variable, "autolamp") && (lampVal != -1)) {
        autoLamp = val;
        if (autoLamp) {
           if (streamCount > 0) setLamp(lampVal);
           else setLamp(0);
        } else {
            setLamp(lampVal);
        }
    }
    else if(!strcmp(variable, "lamp") && (lampVal != -1)) {
        lampVal = constrain(val,0,100);
        if (autoLamp) {
           if (streamCount > 0) setLamp(lampVal);
           else setLamp(0);
        } else {
            setLamp(lampVal);
        }
    }
    else if(!strcmp(variable, "save_prefs")) {
        if (filesystem) savePrefs(SPIFFS);
    }
    else if(!strcmp(variable, "clear_prefs")) {
        if (filesystem) removePrefs(SPIFFS);
    }
    else if(!strcmp(variable, "reboot")) {
        if (lampVal != -1) setLamp(0); // kill the lamp; otherwise it can remain on during the soft-reboot
        esp_task_wdt_init(3,true);  // schedule a a watchdog panic event for 3 seconds in the future
        esp_task_wdt_add(NULL);
        periph_module_disable(PERIPH_I2C0_MODULE); // try to shut I2C down properly
        periph_module_disable(PERIPH_I2C1_MODULE);
        periph_module_reset(PERIPH_I2C0_MODULE);
        periph_module_reset(PERIPH_I2C1_MODULE);
        Serial.print("REBOOT requested");
        while(true) {
          flashLED(50);
          delay(150);
          Serial.print('.');
        }
    }
    else {
        res = -1;
    }
    if(res){
        return httpd_resp_send_500(req);
    }
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t status_handler(httpd_req_t *req){
    static char json_response[1024];
    char * p = json_response;
    *p++ = '{';
    // Do not get attempt to get sensor when in error; causes a panic..
    if (critERR.length() == 0) {
        sensor_t * s = esp_camera_sensor_get();
        p+=sprintf(p, "\"lamp\":%d,", lampVal);
        p+=sprintf(p, "\"autolamp\":%d,", autoLamp);
        p+=sprintf(p, "\"min_frame_time\":%d,", minFrameTime);
        p+=sprintf(p, "\"framesize\":%u,", s->status.framesize);
        p+=sprintf(p, "\"quality\":%u,", s->status.quality);
        p+=sprintf(p, "\"xclk\":%u,", xclk);
        p+=sprintf(p, "\"brightness\":%d,", s->status.brightness);
        p+=sprintf(p, "\"contrast\":%d,", s->status.contrast);
        p+=sprintf(p, "\"saturation\":%d,", s->status.saturation);
        p+=sprintf(p, "\"sharpness\":%d,", s->status.sharpness);
        p+=sprintf(p, "\"special_effect\":%u,", s->status.special_effect);
        p+=sprintf(p, "\"wb_mode\":%u,", s->status.wb_mode);
        p+=sprintf(p, "\"awb\":%u,", s->status.awb);
        p+=sprintf(p, "\"awb_gain\":%u,", s->status.awb_gain);
        p+=sprintf(p, "\"aec\":%u,", s->status.aec);
        p+=sprintf(p, "\"aec2\":%u,", s->status.aec2);
        p+=sprintf(p, "\"ae_level\":%d,", s->status.ae_level);
        p+=sprintf(p, "\"aec_value\":%u,", s->status.aec_value);
        p+=sprintf(p, "\"agc\":%u,", s->status.agc);
        p+=sprintf(p, "\"agc_gain\":%u,", s->status.agc_gain);
        p+=sprintf(p, "\"gainceiling\":%u,", s->status.gainceiling);
        p+=sprintf(p, "\"bpc\":%u,", s->status.bpc);
        p+=sprintf(p, "\"wpc\":%u,", s->status.wpc);
        p+=sprintf(p, "\"raw_gma\":%u,", s->status.raw_gma);
        p+=sprintf(p, "\"lenc\":%u,", s->status.lenc);
        p+=sprintf(p, "\"vflip\":%u,", s->status.vflip);
        p+=sprintf(p, "\"hmirror\":%u,", s->status.hmirror);
        p+=sprintf(p, "\"dcw\":%u,", s->status.dcw);
        p+=sprintf(p, "\"colorbar\":%u,", s->status.colorbar);
        p+=sprintf(p, "\"cam_name\":\"%s\",", myName);
        p+=sprintf(p, "\"code_ver\":\"%s\",", myVer);
        p+=sprintf(p, "\"rotate\":\"%d\",", myRotation);
        p+=sprintf(p, "\"stream_url\":\"%s\"", streamURL);
    }
    *p++ = '}';
    *p++ = 0;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json_response, strlen(json_response));
}

static esp_err_t info_handler(httpd_req_t *req){
    static char json_response[256];
    char * p = json_response;
    *p++ = '{';
    p+=sprintf(p, "\"cam_name\":\"%s\",", myName);
    p+=sprintf(p, "\"rotate\":\"%d\",", myRotation);
    p+=sprintf(p, "\"stream_url\":\"%s\"", streamURL);
    *p++ = '}';
    *p++ = 0;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json_response, strlen(json_response));
}

static esp_err_t favicon_16x16_handler(httpd_req_t *req){
    httpd_resp_set_type(req, "image/png");
    httpd_resp_set_hdr(req, "Content-Encoding", "identity");
    return httpd_resp_send(req, (const char *)favicon_16x16_png, favicon_16x16_png_len);
}

static esp_err_t favicon_32x32_handler(httpd_req_t *req){
    httpd_resp_set_type(req, "image/png");
    httpd_resp_set_hdr(req, "Content-Encoding", "identity");
    return httpd_resp_send(req, (const char *)favicon_32x32_png, favicon_32x32_png_len);
}

static esp_err_t favicon_ico_handler(httpd_req_t *req){
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_set_hdr(req, "Content-Encoding", "identity");
    return httpd_resp_send(req, (const char *)favicon_ico, favicon_ico_len);
}

static esp_err_t logo_svg_handler(httpd_req_t *req){
    httpd_resp_set_type(req, "image/svg+xml");
    httpd_resp_set_hdr(req, "Content-Encoding", "identity");
    return httpd_resp_send(req, (const char *)logo_svg, logo_svg_len);
}

static esp_err_t dump_handler(httpd_req_t *req){
    flashLED(75);
    Serial.println("\r\n通过网页请求系统信息");
    serialDump();
    static char dumpOut[2000] = "";
    char * d = dumpOut;
    // 页面头部
    d+= sprintf(d,"<html><head><meta charset=\"utf-8\">\n");
    d+= sprintf(d,"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n");
    d+= sprintf(d,"<title>%s - 状态信息</title>\n", myName);
    d+= sprintf(d,"<link rel=\"icon\" type=\"image/png\" sizes=\"32x32\" href=\"/favicon-32x32.png\">\n");
    d+= sprintf(d,"<link rel=\"icon\" type=\"image/png\" sizes=\"16x16\" href=\"/favicon-16x16.png\">\n");
    d+= sprintf(d,"<link rel=\"stylesheet\" type=\"text/css\" href=\"/style.css\">\n");
    d+= sprintf(d,"</head>\n");
    d+= sprintf(d,"<body>\n");
    d+= sprintf(d,"<img src=\"/logo.svg\" style=\"position: relative; float: right;\">\n");
    if (critERR.length() > 0) {
        d+= sprintf(d,"%s<hr>\n", critERR.c_str());
    }
    d+= sprintf(d,"<h1>ESP32 摄像头网络服务器</h1>\n");
    // 模块信息
    d+= sprintf(d,"名称: %s<br>\n", myName);
    d+= sprintf(d,"固件版本: %s (基础版本: %s)<br>\n", myVer, baseVersion);
    float sketchPct = 100 * sketchSize / sketchSpace;
    d+= sprintf(d,"程序大小: %i (总空间: %i, 已使用 %.1f%%)<br>\n", sketchSize, sketchSpace, sketchPct);
    d+= sprintf(d,"MD5校验: %s<br>\n", sketchMD5.c_str());
    d+= sprintf(d,"ESP SDK版本: %s<br>\n", ESP.getSdkVersion());
    // 网络信息
    d+= sprintf(d,"<h2>WiFi网络</h2>\n");
    if (accesspoint) {
        if (captivePortal) {
            d+= sprintf(d,"模式: 接入点(含网页验证)<br>\n");
        } else {
            d+= sprintf(d,"模式: 接入点<br>\n");
        }
        d+= sprintf(d,"SSID: %s<br>\n", apName);
    } else {
        d+= sprintf(d,"模式: 客户端<br>\n");
        String ssidName = WiFi.SSID();
        d+= sprintf(d,"SSID: %s<br>\n", ssidName.c_str());
        d+= sprintf(d,"信号强度: %i<br>\n", WiFi.RSSI());
        String bssid = WiFi.BSSIDstr();
        d+= sprintf(d,"BSSID: %s<br>\n", bssid.c_str());
    }
    d+= sprintf(d,"IP地址: %d.%d.%d.%d<br>\n", ip[0], ip[1], ip[2], ip[3]);
    if (!accesspoint) {
        d+= sprintf(d,"子网掩码: %d.%d.%d.%d<br>\n", net[0], net[1], net[2], net[3]);
        d+= sprintf(d,"网关: %d.%d.%d.%d<br>\n", gw[0], gw[1], gw[2], gw[3]);
    }
    d+= sprintf(d,"HTTP端口: %i, 视频流端口: %i<br>\n", httpPort, streamPort);
    byte mac[6];
    WiFi.macAddress(mac);
    d+= sprintf(d,"MAC地址: %02X:%02X:%02X:%02X:%02X:%02X<br>\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // 系统信息
    d+= sprintf(d,"<h2>系统</h2>\n");
    if (haveTime) {
        struct tm timeinfo;
        if(getLocalTime(&timeinfo)){
            char timeStringBuff[50]; //50个字符应该足够
            strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M:%S, %A, %B %d %Y", &timeinfo);
            //以"const char*"形式打印
            d+= sprintf(d,"当前时间: %s<br>\n", timeStringBuff);
        }
    }
    int64_t sec = esp_timer_get_time() / 1000000;
    int64_t upDays = int64_t(floor(sec/86400));
    int upHours = int64_t(floor(sec/3600)) % 24;
    int upMin = int64_t(floor(sec/60)) % 60;
    int upSec = sec % 60;
    int McuTc = (temprature_sens_read() - 32) / 1.8; // 摄氏度
    int McuTf = temprature_sens_read(); // 华氏度

    d+= sprintf(d,"运行时间: %" PRId64 ":%02i:%02i:%02i (天:时:分:秒)<br>\n", upDays, upHours, upMin, upSec);
    d+= sprintf(d,"活跃视频流: %i, 已服务视频流: %lu, 已捕获图像: %lu<br>\n", streamCount, streamsServed, imagesServed);
    d+= sprintf(d,"CPU频率: %i MHz, Xclk频率: %i MHz<br>\n", ESP.getCpuFreqMHz(), xclk);
    d+= sprintf(d,"<span title=\"注意: ESP32-c1芯片组上的内部温度传感器读数可能不准确，且设备之间可能存在显著差异!\">");
    d+= sprintf(d,"MCU温度: %i &deg;C, %i &deg;F</span>\n<br>", McuTc, McuTf);
    d+= sprintf(d,"堆内存: %i, 空闲: %i, 最小空闲: %i, 最大块: %i<br>\n", ESP.getHeapSize(), ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
    if (psramFound()) {
        d+= sprintf(d,"PSRAM: %i, 空闲: %i, 最小空闲: %i, 最大块: %i<br>\n", ESP.getPsramSize(), ESP.getFreePsram(), ESP.getMinFreePsram(), ESP.getMaxAllocPsram());
    } else {
        d+= sprintf(d,"PSRAM: <span style=\"color:red;\">未找到</span>, 请检查您的开发板配置。<br>\n");
        d+= sprintf(d,"- 由于内存不足，高分辨率/高质量图像和视频流可能会显示不完整的帧。<br>\n");
    }
    if (filesystem && (SPIFFS.totalBytes() > 0)) {
        d+= sprintf(d,"SPIFFS文件系统: %i, 已使用: %i<br>\n", SPIFFS.totalBytes(), SPIFFS.usedBytes());
    } else {
        d+= sprintf(d,"SPIFFS: <span style=\"color:red;\">未找到文件系统</span>, 请检查您的开发板配置。<br>\n");
        d+= sprintf(d,"- 没有文件系统将无法保存和恢复相机设置。<br>\n");
    }

    // 页脚
    d+= sprintf(d,"<br><div class=\"input-group\">\n");
    d+= sprintf(d,"<button title=\"立即刷新；页面每分钟自动重新加载\" onclick=\"location.replace(document.URL)\">刷新</button>\n");
    d+= sprintf(d,"<button title=\"强制停止相机模块上的所有活跃视频流\" ");
    d+= sprintf(d,"onclick=\"let throwaway = fetch('stop');setTimeout(function(){\nlocation.replace(document.URL);\n}, 200);\">停止视频流</button>\n");
    d+= sprintf(d,"<button title=\"关闭此页面\" onclick=\"javascript:window.close()\">关闭</button>\n");
    d+= sprintf(d,"</div>\n</body>\n");
    // 每分钟刷新页面的JavaScript计时器
    d+= sprintf(d,"<script>\nsetTimeout(function(){\nlocation.replace(document.URL);\n}, 60000);\n");
    d+= sprintf(d,"</script>\n</html>\n");
    *d++ = 0;
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "identity");
    return httpd_resp_send(req, dumpOut, strlen(dumpOut));
}

static esp_err_t stop_handler(httpd_req_t *req){
    flashLED(75);
    Serial.println("\r\nStream stop requested via Web");
    streamKill = true;
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, NULL, 0);
}


static esp_err_t style_handler(httpd_req_t *req){
    httpd_resp_set_type(req, "text/css");
    httpd_resp_set_hdr(req, "Content-Encoding", "identity");
    return httpd_resp_send(req, (const char *)style_css, style_css_len);
}

static esp_err_t streamviewer_handler(httpd_req_t *req){
    flashLED(75);
    Serial.println("Stream viewer requested");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "identity");
    return httpd_resp_send(req, (const char *)streamviewer_html, streamviewer_html_len);
}

static esp_err_t error_handler(httpd_req_t *req){
    flashLED(75);
    Serial.println("Sending error page");
    std::string s(error_html);
    size_t index;
    while ((index = s.find("<APPURL>")) != std::string::npos)
        s.replace(index, strlen("<APPURL>"), httpURL);
    while ((index = s.find("<CAMNAME>")) != std::string::npos)
        s.replace(index, strlen("<CAMNAME>"), myName);
    while ((index = s.find("<ERRORTEXT>")) != std::string::npos)
        s.replace(index, strlen("<ERRORTEXT>"), critERR.c_str());
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "identity");
    return httpd_resp_send(req, (const char *)s.c_str(), s.length());
}

static esp_err_t index_handler(httpd_req_t *req){
    char*  buf;
    size_t buf_len;
    char view[32] = {0,};

    flashLED(75);
    // See if we have a specific target (full/simple/portal) and serve as appropriate
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = (char*)malloc(buf_len);
        if(!buf){
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            if (httpd_query_key_value(buf, "view", view, sizeof(view)) == ESP_OK) {
            } else {
                free(buf);
                httpd_resp_send_404(req);
                return ESP_FAIL;
            }
        } else {
            free(buf);
            httpd_resp_send_404(req);
            return ESP_FAIL;
        }
        free(buf);
    } else {
        // no target specified; default.
        strcpy(view,default_index);
        // If captive portal is active send that instead
        if (captivePortal) {
            strcpy(view,"portal");
        }
    }

    if  (strncmp(view,"simple", sizeof(view)) == 0) {
        Serial.println("Simple index page requested");
        if (critERR.length() > 0) return error_handler(req);
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Content-Encoding", "identity");
        return httpd_resp_send(req, (const char *)index_simple_html, index_simple_html_len);
    } else if(strncmp(view,"full", sizeof(view)) == 0) {
        Serial.println("Full index page requested");
        if (critERR.length() > 0) return error_handler(req);
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Content-Encoding", "identity");
        if (sensorPID == OV3660_PID) {
            return httpd_resp_send(req, (const char *)index_ov3660_html, index_ov3660_html_len);
        }
        return httpd_resp_send(req, (const char *)index_ov2640_html, index_ov2640_html_len);
    } else if(strncmp(view,"portal", sizeof(view)) == 0) {
        //Prototype captive portal landing page.
        Serial.println("Portal page requested");
        std::string s(portal_html);
        size_t index;
        while ((index = s.find("<APPURL>")) != std::string::npos)
            s.replace(index, strlen("<APPURL>"), httpURL);
        while ((index = s.find("<STREAMURL>")) != std::string::npos)
            s.replace(index, strlen("<STREAMURL>"), streamURL);
        while ((index = s.find("<CAMNAME>")) != std::string::npos)
            s.replace(index, strlen("<CAMNAME>"), myName);
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Content-Encoding", "identity");
        return httpd_resp_send(req, (const char *)s.c_str(), s.length());
    } else  {
        Serial.print("Unknown page requested: ");
        Serial.println(view);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
}

void startCameraServer(int hPort, int sPort){
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16; // we use more than the default 8 (on port 80)

    httpd_uri_t index_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = index_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t status_uri = {
        .uri       = "/status",
        .method    = HTTP_GET,
        .handler   = status_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t cmd_uri = {
        .uri       = "/control",
        .method    = HTTP_GET,
        .handler   = cmd_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t capture_uri = {
        .uri       = "/capture",
        .method    = HTTP_GET,
        .handler   = capture_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t style_uri = {
        .uri       = "/style.css",
        .method    = HTTP_GET,
        .handler   = style_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t favicon_16x16_uri = {
        .uri       = "/favicon-16x16.png",
        .method    = HTTP_GET,
        .handler   = favicon_16x16_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t favicon_32x32_uri = {
        .uri       = "/favicon-32x32.png",
        .method    = HTTP_GET,
        .handler   = favicon_32x32_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t favicon_ico_uri = {
        .uri       = "/favicon.ico",
        .method    = HTTP_GET,
        .handler   = favicon_ico_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t logo_svg_uri = {
        .uri       = "/logo.svg",
        .method    = HTTP_GET,
        .handler   = logo_svg_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t dump_uri = {
        .uri       = "/dump",
        .method    = HTTP_GET,
        .handler   = dump_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t stop_uri = {
        .uri       = "/stop",
        .method    = HTTP_GET,
        .handler   = stop_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t stream_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = stream_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t streamviewer_uri = {
        .uri       = "/view",
        .method    = HTTP_GET,
        .handler   = streamviewer_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t info_uri = {
        .uri       = "/info",
        .method    = HTTP_GET,
        .handler   = info_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t error_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = error_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t viewerror_uri = {
        .uri       = "/view",
        .method    = HTTP_GET,
        .handler   = error_handler,
        .user_ctx  = NULL
    };

    // Request Handlers; config.max_uri_handlers (above) must be >= the number of handlers
    config.server_port = hPort;
    config.ctrl_port = hPort;
    Serial.printf("在端口 '%d' 启动Web服务器\r\n", config.server_port);
    if (httpd_start(&camera_httpd, &config) == ESP_OK) {
        if (critERR.length() > 0) {
            httpd_register_uri_handler(camera_httpd, &error_uri);
        } else {
            httpd_register_uri_handler(camera_httpd, &index_uri);
            httpd_register_uri_handler(camera_httpd, &cmd_uri);
            httpd_register_uri_handler(camera_httpd, &status_uri);
            httpd_register_uri_handler(camera_httpd, &capture_uri);
        }
        httpd_register_uri_handler(camera_httpd, &style_uri);
        httpd_register_uri_handler(camera_httpd, &favicon_16x16_uri);
        httpd_register_uri_handler(camera_httpd, &favicon_32x32_uri);
        httpd_register_uri_handler(camera_httpd, &favicon_ico_uri);
        httpd_register_uri_handler(camera_httpd, &logo_svg_uri);
        httpd_register_uri_handler(camera_httpd, &dump_uri);
        httpd_register_uri_handler(camera_httpd, &stop_uri);
    }

    config.server_port = sPort;
    config.ctrl_port = sPort;
    Serial.printf("在端口 '%d' 启动流媒体服务器\r\n", config.server_port);
    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        if (critERR.length() > 0) {
            httpd_register_uri_handler(camera_httpd, &error_uri);
            httpd_register_uri_handler(camera_httpd, &viewerror_uri);
        } else {
            httpd_register_uri_handler(stream_httpd, &stream_uri);
            httpd_register_uri_handler(stream_httpd, &info_uri);
            httpd_register_uri_handler(stream_httpd, &streamviewer_uri);
        }
        httpd_register_uri_handler(stream_httpd, &favicon_16x16_uri);
        httpd_register_uri_handler(stream_httpd, &favicon_32x32_uri);
        httpd_register_uri_handler(stream_httpd, &favicon_ico_uri);
    }
}
