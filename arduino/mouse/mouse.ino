#include <Mouse.h>
#include <Wire.h>
#include <SPI.h>
#include <usbhub.h>
#include <HIDBoot.h>

USB Usb;
USBHub Hub(&Usb);
HIDBoot<USB_HID_PROTOCOL_MOUSE> HidMouse(&Usb);

int dX, dY;
bool LButton, RButton, MButton;

class MyMouseReportParser : public MouseReportParser
{
  void OnMouseMove(MOUSEINFO* Info)
  {
    dX += Info->dX;
    dY += Info->dY;
  }
  void OnLeftButtonDown(MOUSEINFO* Info)
  {
    LButton = true;
  }
  void OnLeftButtonUp(MOUSEINFO* Info)
  {
    LButton = false;
  }
  void OnRightButtonDown(MOUSEINFO* Info)
  {
    RButton = true;
  }
  void OnRightButtonUp(MOUSEINFO* Info)
  {
    RButton = false;
  }
  void OnMiddleButtonDown(MOUSEINFO* Info)
  {
    MButton = true;
  }
  void OnMiddleButtonUp(MOUSEINFO* Info)
  {
    MButton = false;
  }
};

MyMouseReportParser MouseParser;

struct PCMouseUpdate
{
  int32_t dX;
  int32_t dY;
  uint32_t Click;
};

static_assert(sizeof(PCMouseUpdate) == 12);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Mouse.begin();

  Usb.Init();
  HidMouse.SetReportParser(0, &MouseParser);
}

void loop() {
  // put your main code here, to run repeatedly:
  Usb.Task();

  if (LButton)
  {
    Mouse.press(MOUSE_LEFT);
  }
  else
  {
    Mouse.release(MOUSE_LEFT);
  }

  if (RButton)
  {
    Mouse.press(MOUSE_RIGHT);
  }
  else
  {
    Mouse.release(MOUSE_RIGHT);
  }

  if (MButton)
  {
    Mouse.press(MOUSE_MIDDLE);
  }
  else
  {
    Mouse.release(MOUSE_MIDDLE);
  }

  if (Serial.available())
  {
    PCMouseUpdate MouseUpdate;
    Serial.readBytes((char*) &MouseUpdate, sizeof(MouseUpdate));

    dX += MouseUpdate.dX;
    dY += MouseUpdate.dY;
  }

  Mouse.move(dX, dY);

  dX = 0;
  dY = 0;
}
