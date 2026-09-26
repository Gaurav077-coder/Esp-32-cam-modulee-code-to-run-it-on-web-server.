
#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"

// =====================================================
// ENTER YOUR WI-FI NAME AND PASSWORD
// =====================================================

const char* WIFI_NAME = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// AI-Thinker ESP32-CAM flash LED
#define FLASH_LED_PIN 4

// =====================================================
// AI-THINKER ESP32-CAM CAMERA PINS
// =====================================================

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5

#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

httpd_handle_t websiteServer = NULL;
httpd_handle_t videoServer = NULL;

// =====================================================
// CAMERA WEB PAGE
// =====================================================

const char CAMERA_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport"
          content="width=device-width, initial-scale=1">

    <title>WTE ESP32-CAM</title>

    <style>
        body {
            background-color: #101820;
            color: white;
            font-family: Arial, sans-serif;
            text-align: center;
            margin: 20px;
        }

        h2 {
            color: #39d98a;
        }

        img {
            width: 100%;
            max-width: 640px;
            border: 3px solid #39d98a;
            border-radius: 10px;
        }
    </style>
</head>

<body>
    <h2>WTE ESP32-CAM Live Video</h2>

    <img id="cameraStream"
         alt="ESP32-CAM video stream">

    <p>
        Keep this camera and the laptop connected
        to the same Wi-Fi network.
    </p>

    <script>
        document.getElementById("cameraStream").src =
            "http://" + location.hostname + ":81/stream";
    </script>
</body>
</html>
)rawliteral";

// =====================================================
// WEB PAGE HANDLER
// =====================================================

esp_err_t websiteHandler(httpd_req_t* request) {
    httpd_resp_set_type(request, "text/html");

    return httpd_resp_send(
        request,
        CAMERA_PAGE,
        HTTPD_RESP_USE_STRLEN
    );
}

// =====================================================
// LIVE VIDEO STREAM HANDLER
// =====================================================

esp_err_t videoStreamHandler(httpd_req_t* request) {
    const char* STREAM_TYPE =
        "multipart/x-mixed-replace;boundary=frame";

    const char* STREAM_BOUNDARY =
        "\r\n--frame\r\n";

    const char* STREAM_PART =
        "Content-Type: image/jpeg\r\n"
        "Content-Length: %u\r\n\r\n";

    esp_err_t result =
        httpd_resp_set_type(request, STREAM_TYPE);

    if (result != ESP_OK) {
        return result;
    }

    char header[64];

    while (true) {
        camera_fb_t* frame = esp_camera_fb_get();

        if (frame == NULL) {
            Serial.println("Camera capture failed.");
            return ESP_FAIL;
        }

        uint8_t* imageBuffer = frame->buf;
        size_t imageLength = frame->len;

        uint8_t* convertedBuffer = NULL;

        if (frame->format != PIXFORMAT_JPEG) {
            bool converted = frame2jpg(
                frame,
                80,
                &convertedBuffer,
                &imageLength
            );

            esp_camera_fb_return(frame);
            frame = NULL;

            if (!converted) {
                Serial.println(
                    "JPEG conversion failed."
                );

                return ESP_FAIL;
            }

            imageBuffer = convertedBuffer;
        }

        size_t headerLength = snprintf(
            header,
            sizeof(header),
            STREAM_PART,
            (unsigned int)imageLength
        );

        result = httpd_resp_send_chunk(
            request,
            STREAM_BOUNDARY,
            strlen(STREAM_BOUNDARY)
        );

        if (result == ESP_OK) {
            result = httpd_resp_send_chunk(
                request,
                header,
                headerLength
            );
        }

        if (result == ESP_OK) {
            result = httpd_resp_send_chunk(
                request,
                (const char*)imageBuffer,
                imageLength
            );
        }

        if (frame != NULL) {
            esp_camera_fb_return(frame);
        }

        if (convertedBuffer != NULL) {
            free(convertedBuffer);
        }

        if (result != ESP_OK) {
            break;
        }
    }

    return result;
}

// =====================================================
// START CAMERA SERVERS
// =====================================================

void startCameraServers() {
    // Web page server on port 80
    httpd_config_t websiteConfiguration =
        HTTPD_DEFAULT_CONFIG();

    websiteConfiguration.server_port = 80;

    httpd_uri_t websiteAddress = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = websiteHandler,
        .user_ctx = NULL
    };

    if (
        httpd_start(
            &websiteServer,
            &websiteConfiguration
        ) == ESP_OK
    ) {
        httpd_register_uri_handler(
            websiteServer,
            &websiteAddress
        );
    }

    // Camera video server on port 81
    httpd_config_t videoConfiguration =
        HTTPD_DEFAULT_CONFIG();

    videoConfiguration.server_port = 81;

    videoConfiguration.ctrl_port =
        websiteConfiguration.ctrl_port + 1;

    httpd_uri_t videoAddress = {
        .uri = "/stream",
        .method = HTTP_GET,
        .handler = videoStreamHandler,
        .user_ctx = NULL
    };

    if (
        httpd_start(
            &videoServer,
            &videoConfiguration
        ) == ESP_OK
    ) {
        httpd_register_uri_handler(
            videoServer,
            &videoAddress
        );
    }
}

// =====================================================
// SETUP
// =====================================================

void setup() {
    // Keep the bright flash LED switched off
    pinMode(FLASH_LED_PIN, OUTPUT);
    digitalWrite(FLASH_LED_PIN, LOW);

    Serial.begin(115200);
    Serial.setDebugOutput(false);

    delay(1000);

    Serial.println();
    Serial.println("==============================");
    Serial.println("WTE ESP32-CAM starting...");
    Serial.println("==============================");

    // Camera configuration
    camera_config_t config;

    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;

    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;

    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;

    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;

    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;

    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;

    config.grab_mode = CAMERA_GRAB_LATEST;

    // Use PSRAM when available
    if (psramFound()) {
        Serial.println("PSRAM found.");

        config.frame_size = FRAMESIZE_VGA;
        config.jpeg_quality = 10;
        config.fb_count = 2;
        config.fb_location = CAMERA_FB_IN_PSRAM;
    }
    else {
        Serial.println("PSRAM not found.");

        config.frame_size = FRAMESIZE_QVGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_DRAM;
    }

    // Initialize the camera
    Serial.println("Initializing OV2640 camera...");

    esp_err_t cameraResult =
        esp_camera_init(&config);

    if (cameraResult != ESP_OK) {
        Serial.println();
        Serial.println("==============================");
        Serial.println("CAMERA INITIALIZATION FAILED");
        Serial.println("==============================");

        Serial.printf(
            "Error code: 0x%x\n",
            cameraResult
        );

        if (cameraResult == ESP_ERR_NOT_SUPPORTED) {
            Serial.println(
                "Error 0x106: Camera not supported."
            );

            Serial.println(
                "AI-Thinker pins are already selected."
            );

            Serial.println(
                "Disconnect USB power and check:"
            );

            Serial.println(
                "1. Camera ribbon is fully inserted."
            );

            Serial.println(
                "2. Ribbon is inserted in the correct direction."
            );

            Serial.println(
                "3. Camera sensor is OV2640."
            );

            Serial.println(
                "4. Camera ribbon or sensor is not damaged."
            );
        }

        // Keep flash off and stop the program
        while (true) {
            digitalWrite(FLASH_LED_PIN, LOW);
            delay(1000);
        }
    }

    Serial.println("Camera initialized successfully.");

    // Read camera sensor information
    sensor_t* cameraSensor =
        esp_camera_sensor_get();

    if (cameraSensor != NULL) {
        Serial.printf(
            "Camera sensor PID: 0x%02X\n",
            cameraSensor->id.PID
        );

        cameraSensor->set_framesize(
            cameraSensor,
            FRAMESIZE_VGA
        );

        // Correct orientation if the image is upside down
        // Change 0 to 1 if rotation is required
        cameraSensor->set_vflip(
            cameraSensor,
            0
        );

        cameraSensor->set_hmirror(
            cameraSensor,
            0
        );
    }

    // Connect to Wi-Fi
    WiFi.mode(WIFI_STA);
    WiFi.begin(
        WIFI_NAME,
        WIFI_PASSWORD
    );

    Serial.print("Connecting to Wi-Fi");

    unsigned long connectionStart = millis();

    while (
        WiFi.status() != WL_CONNECTED &&
        millis() - connectionStart < 30000
    ) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println();
        Serial.println(
            "Wi-Fi connection failed."
        );

        Serial.println(
            "Check the Wi-Fi name and password."
        );

        while (true) {
            digitalWrite(FLASH_LED_PIN, LOW);
            delay(1000);
        }
    }

    Serial.println();
    Serial.println("Wi-Fi connected.");

    // Start browser and video servers
    startCameraServers();

    String cameraIP =
        WiFi.localIP().toString();

    Serial.println();
    Serial.println("==============================");
    Serial.println("CAMERA IS READY");
    Serial.println("==============================");

    Serial.print("Browser page: http://");
    Serial.println(cameraIP);

    Serial.print("YOLO stream: http://");
    Serial.print(cameraIP);
    Serial.println(":81/stream");

    Serial.println("==============================");
}

// =====================================================
// LOOP
// =====================================================

void loop() {
    // Keep the flash LED switched off
    digitalWrite(FLASH_LED_PIN, LOW);

    delay(10000);
}
