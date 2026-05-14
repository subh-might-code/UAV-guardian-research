#include <TensorFlowLite_ESP32.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include <MPU6050_tockn.h>
#include <Wire.h>
#include "uav_model.h"
#include <WiFi.h>
#include <PubSubClient.h>

// --- 1. NETWORK CONFIG ---
const char* ssid = "Airtel_EL_bicho_2.4ghz";
const char* password = "air51991";
const char* mqtt_server = "192.168.1.16"; 

WiFiClient espClient;
PubSubClient client(espClient);

// --- 2. AI GLOBALS (Modified for Dynamic Allocation) ---
const int kTensorArenaSize = 60 * 1024; // Reduced to 60KB to fit WiFi/MQTT
uint8_t* tensor_arena = nullptr;        // Pointer instead of array

const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input = nullptr;
TfLiteTensor* output = nullptr;
MPU6050 mpu6050(Wire);

// --- 3. HELPER FUNCTIONS ---
void setup_wifi() {
    delay(10);
    Serial.println("\nConnecting to WiFi...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        Serial.print(WiFi.status());
    }
    Serial.println("\nWiFi connected! IP: " + WiFi.localIP().toString());
}

void reconnect() {
    while (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
        if (client.connect("UAV_Guardian_AEC")) {
            Serial.println("connected");
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            delay(5000);
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000);

    // DYNAMICALLY ALLOCATE ARENA (Fixes the Linker Error)
    tensor_arena = (uint8_t*)malloc(kTensorArenaSize);
    if (tensor_arena == nullptr) {
        Serial.println("CRITICAL: Could not allocate arena!");
        while(1);
    }

    setup_wifi();
    client.setServer(mqtt_server, 1883);

    Serial.println("--- UAV-GUARDIAN: AEC DEPLOYMENT ---");
    Wire.begin(); 
    mpu6050.begin();
    mpu6050.calcGyroOffsets(true); 

    static tflite::MicroErrorReporter micro_error_reporter;
    model = tflite::GetModel(uav_model_tflite);
    static tflite::AllOpsResolver resolver;

    // Initialize interpreter with the dynamically allocated pointer
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, kTensorArenaSize, &micro_error_reporter);
    interpreter = &static_interpreter;

    if (interpreter->AllocateTensors() != kTfLiteOk) {
        Serial.println("Inference Allocation Failed! Arena might be too small.");
        while (1);
    }

    input = interpreter->input(0);
    output = interpreter->output(0);
    Serial.println("--- AI ENGINE ONLINE ---");
}

void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();

    for (int i = 0; i < 100; i++) {
        mpu6050.update();
        if (input != nullptr) {
            input->data.f[i * 3]     = mpu6050.getAccX();
            input->data.f[i * 3 + 1] = mpu6050.getAccY();
            input->data.f[i * 3 + 2] = mpu6050.getAccZ();
        }
        delay(10); 
    }

    if (interpreter->Invoke() != kTfLiteOk) { return; }

    float healthy = output->data.f[0];
    float faulty  = output->data.f[1];

    Serial.print("H: "); Serial.print(healthy * 100, 1);
    Serial.print("% | F: "); Serial.print(faulty * 100, 1); Serial.println("%");

    // --- [ADD THIS BLOCK] ---
   // Create a simple string message to send to your laptop
    String msg = "H:" + String(healthy*100,1) + " F:" + String(faulty*100,1);
    if (faulty > 0.85) {
        msg += " [ALERT]";
        Serial.println("🚨 ALERT: ANOMALY DETECTED!");
    }

    String payload = "H:" + String(healthy * 100, 1) + " F:" + String(faulty * 100, 1);
    client.publish("uav/guardian/health", payload.c_str());

    delay(500); 
}
