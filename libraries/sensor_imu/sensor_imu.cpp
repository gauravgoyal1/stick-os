#include <M5StickCPlus2.h>
#include <stick_os.h>
#include "sensor_imu.h"

namespace SensorIMU {

void drawBubble(float ax, float ay) {
    auto& d = StickCP2.Display;
    const int cx = d.width() / 2;
    const int cy = 90;
    const int radius = 45;

    // Clear bubble area
    d.fillCircle(cx, cy, radius + 2, BLACK);
    d.drawCircle(cx, cy, radius, d.color565(40, 40, 40));
    d.drawCircle(cx, cy, radius / 2, d.color565(30, 30, 30));

    // Crosshair
    d.drawFastHLine(cx - radius, cy, radius * 2, d.color565(25, 25, 25));
    d.drawFastVLine(cx, cy - radius, radius * 2, d.color565(25, 25, 25));

    // Bubble position (inverted: tilt left -> bubble goes right)
    int bx = cx + constrain((int)(ax * radius * 2), -radius + 4, radius - 4);
    int by = cy + constrain((int)(ay * radius * 2), -radius + 4, radius - 4);
    d.fillCircle(bx, by, 6, GREEN);
    d.drawCircle(bx, by, 6, d.color565(0, 180, 0));
}

void drawValues(float ax, float ay, float az, float gx, float gy, float gz) {
    auto& d = StickCP2.Display;
    int y = 150;
    d.setTextSize(1);

    d.fillRect(0, y, d.width(), 80, BLACK);

    d.setTextColor(d.color565(100, 100, 100), BLACK);
    d.setCursor(4, y);     d.print("Accel (g)");
    d.setCursor(76, y);    d.print("Gyro (dps)");
    y += 14;

    d.setTextColor(RED, BLACK);
    d.setCursor(4, y);     d.printf("X %+.2f", ax);
    d.setCursor(76, y);    d.printf("X %+6.1f", gx);
    y += 12;

    d.setTextColor(GREEN, BLACK);
    d.setCursor(4, y);     d.printf("Y %+.2f", ay);
    d.setCursor(76, y);    d.printf("Y %+6.1f", gy);
    y += 12;

    d.setTextColor(CYAN, BLACK);
    d.setCursor(4, y);     d.printf("Z %+.2f", az);
    d.setCursor(76, y);    d.printf("Z %+6.1f", gz);
}

void init() {
    auto& d = StickCP2.Display;
    d.setRotation(0);
    d.fillScreen(BLACK);
    StickCP2.Imu.init();

    // Title
    d.setTextSize(2);
    d.setTextColor(CYAN, BLACK);
    d.setCursor(8, 6);
    d.print("IMU");
    d.drawFastHLine(0, 28, d.width(), d.color565(40, 40, 40));

    // Footer
    d.setTextSize(1);
    d.setTextColor(d.color565(80, 80, 80), BLACK);
    d.setCursor(8, d.height() - 12);
    d.print("PWR: back");

    while (true) {
        StickCP2.update();
        if (stick_os::checkAppExit()) return;

        StickCP2.Imu.update();
        auto imu = StickCP2.Imu.getImuData();
        drawBubble(imu.accel.x, imu.accel.y);
        drawValues(imu.accel.x, imu.accel.y, imu.accel.z,
                   imu.gyro.x, imu.gyro.y, imu.gyro.z);
        delay(30);
    }
}

void tick() {}

void icon(int x, int y, uint16_t color) {
    auto& d = StickCP2.Display;
    // Compass-like circle + crosshair + dot
    d.drawCircle(x + 14, y + 14, 12, color);
    d.drawFastHLine(x + 4, y + 14, 20, color);
    d.drawFastVLine(x + 14, y + 4, 20, color);
    d.fillCircle(x + 18, y + 10, 3, color);
}

}  // namespace SensorIMU

static const stick_os::AppDescriptor kDesc = {
    "imu", "IMU", "1.0.0",
    stick_os::CAT_SENSOR, stick_os::APP_NONE,
    &SensorIMU::icon, stick_os::RUNTIME_NATIVE,
    { &SensorIMU::init, &SensorIMU::tick, nullptr, nullptr },
    { nullptr, nullptr },
};
STICK_REGISTER_APP(kDesc);
