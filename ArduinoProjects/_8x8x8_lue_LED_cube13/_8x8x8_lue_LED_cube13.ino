#include <CapacitiveSensor.h>

// 8x8x8x Blue LED Cube
// by Hari Wiguna 2014
//
// v03 - blinks all 512
// v04 - more test patterns.  Uses TimerOne library.
// v06 - Using MsTimer2
// v07 - Don't turn off previous layer until the very last moment for brighter display
// v09 - Pedal to the metal, no timer libraries!
// v10 - Invert shift register output in DrawLayers() because we longer use transistors there.
// v11 - Add Capsense library
// v12 - Add two potentiometers on Analog 4 and 5
// v13 - Working on patterns

//-- Shift Register pins --
int latchPin = 13; // Arduino D13 to IC pin 12 (ST_CP) -- White
int clockPin = 12; // Arduino D12 to IC pin 11 (SH_CP) -- Yelow
int dataPin  = 11; // Arduino D11 to IC pin 14 (DS) -- Blue

//-- Globals --
volatile int8_t cube[8][8]; // byte bits = X, 1st index=Y, 2nd index = Z
volatile int8_t gZ = 0;
int pot0; // Left Potentiometer (A4)
int pot1; // Right Potentiometer (A5)
int animSpeed; // Animation speed controlled by pot0
float pi = 3.14;
float pi2 = 6.28;
const int8_t sineMaxIndex = 32;
int8_t sineArray[sineMaxIndex];

//-- CapSense --
CapacitiveSensor   cs_14_15 = CapacitiveSensor(14,15);
CapacitiveSensor   cs_14_16 = CapacitiveSensor(14,16);
CapacitiveSensor   cs_14_17 = CapacitiveSensor(14,17);

void SetupPins()
{
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);

  for (int i=0; i<8; i++) {
    pinMode(2+i, OUTPUT);
  }
}

void SetupCapSense()
{
  pinMode(14+0,OUTPUT);
  pinMode(14+1,OUTPUT); //15
  pinMode(14+2,OUTPUT); //16
  pinMode(14+3,OUTPUT); //17
  pinMode(A4,INPUT);
  pinMode(A5,INPUT);
  //cs_14_15.set_CS_AutocaL_Millis(0xFFFFFFFF);     // turn off autocalibrate on channel 1 - just as an example
}

void setup(void) {
  SetupPins();
  SetupCapSense();
  PreComputes();
  CubeAllOff();
  DrawRect(7,0,0, 7,7,7);
  //RunTests();
  SetupTimer();
  Serial.begin(9600);
}

void SetupTimer()
{
  cli();
  // Reset any PWM that arduino may have setup automatically
  TCCR2A = 0;
  TCCR2B = 0;

  TCCR2A |= (1 << WGM21); // CTC mode. Reset counter when OCR2 is reached

  TCCR2B |= (1 << CS21) | (1 << CS22); // Prescaler = 256
  //TCCR2B |= (1 << CS20) | (1 << CS22); // Prescaler = 128
  OCR2A = 70; // Fire interrupt when timer2 has looped 80 times

  TCNT2 = 0; // initial counter value = 0;
  TIMSK2 |= (1<<OCIE2A); // Enable CTC interrupt
  sei();
}

ISR (TIMER2_COMPA_vect)
{
  Refresh();
}

void RunTests()
{
  Serial.begin(9600);

  Serial.print("TestCubeAllOff = ");
  Serial.println(TestCubeAllOff());

  Serial.print("TestRefresh = ");
  Serial.println(TestRefresh());    // 8052 micro seconds

  Serial.end();
}

long TestCubeAllOff()
{
  long start = micros();
  CubeAllOff();
  long finish = micros();
  return finish-start;
}

long TestRefresh()
{
  long start = micros();
  Refresh();
  long finish = micros();
  return finish-start;
}

void TestPattern3()
{
  CubeAllOff();
  delay(500);
  CubeAllOn();
  delay(500);
}


void BottomUp()
{
  for (int8_t z=0; z<8; z++) {
    for (int8_t x=0; x<8; x++) {
      for (int8_t y=0; y<8; y++) {
        SetDot(x,y,z); 
      }
    }
    delay(64);
    CubeAllOff();
  }
}


void OneWall()
{
  for (int8_t z=0; z<8; z++) {
    for (int8_t x=0; x<8; x++) {
      for (int8_t y=7; y<8; y++) {
        SetDot(x,y,z); 
      }
    }
  }
}

void LeftRight()
{
  for (int8_t x=0; x<8; x++) {
    for (int8_t z=0; z<8; z++) {
      for (int8_t y=7; y<8; y++) {
        SetDot(x,y,z); 
      }
    }
    delay(64);
    CubeAllOff();
  }
}

void Refresh(void) // WITHOUT the added delayMicroseconds, this routine takes 8052 microseconds
{
  noInterrupts();

  //-- Compute new layer --
  int8_t prevLayer = gZ;
  gZ++; 
  if (gZ>=8) gZ=0;

  // Prepare for data. Shift data to shift registers but do not reflect it on the outputs yet.
  digitalWrite(latchPin, LOW);

  //-- Spit out the bits --
  DrawLayer(gZ);

  //-- Turn off previous layer --
  digitalWrite(2+prevLayer,LOW); // Turn off prev layer

  //-- Turn on this layer --
  digitalWrite(2+gZ,HIGH); // Turn on this layer


  // All data ready. Instantly reflect all 64 bits on all 8 shift registers to the led layer.
  digitalWrite(latchPin, HIGH);

  interrupts();
}

void TurnOnLayer(int8_t z)
{
  int8_t prev = z==0 ? 7 : z-1;

  // Prepare for data. Shift data to shift registers but do not reflect it on the outputs yet.
  digitalWrite(latchPin, LOW);

  //-- Spit out the bits --
  DrawLayer(z);

  //-- Turn off previous layer --
  digitalWrite(2+prev,LOW); // Turn off prev layer
}

void TurnOffLayer(int8_t z)
{
  // All data ready. Instantly reflect all 64 bits on all 8 shift registers to the led layer.
  digitalWrite(latchPin, HIGH);

  //-- Turn on this layer --
  digitalWrite(2+z,HIGH); // Turn on this layer
}

void DrawLayer(int8_t z)
{
  // Spit out all 64 bits for the layer.
  for (int8_t y=0; y<8; y++) {
    shiftOut(dataPin, clockPin, MSBFIRST, ~cube[y][z]); // Push Most significant BYTE first   
  }  
}

void LayerOn(int8_t z)
{
  for (int8_t y=0; y<8; y++) {
    for (int8_t x=0; x<8; x++) {
      SetDot(x,y,z);
    }  
  }  
}

void CubeAllOn()
{
  //noInterrupts();
  for (int8_t z=0; z<8; z++) {
    for (int8_t y=0; y<8; y++) {
      for (int8_t x=0; x<8; x++) {
        SetDot(x,y,z);
      }  
    }  
  }  
  //interrupts();
}

void CubeAllOff()
{
  for (int8_t z=0; z<8; z++) {
    SetLayer(z, 0x00);
  }  
}

void CubeUp()
{
  for (int8_t z=0; z<8; z++) {
    SetLayer(z, 0xFF);
    delay(animSpeed);
    SetLayer(z, 0x00);
  }  
}

void CubeDown()
{
  for (int8_t z=7; z>=0; z--) {
    SetLayer(z, 0xFF);
    delay(animSpeed);
    SetLayer(z, 0x00);
  }  
}

void CubeLeftRight()
{
  for (int8_t x=0; x<8; x++) {
    SetXPlane(x);
    delay(animSpeed);
    CubeAllOff();
  }  
}

void CubeRightLeft()
{
  for (int8_t x=7; x>=0; x--) {
    SetXPlane(x);
    delay(animSpeed);
    CubeAllOff();
  }  
}

void SetXPlane(int8_t x)
{
  x = Wrap(x);
  int8_t xPattern = 1 << x;
  for (int8_t z=0; z<8; z++) {
    for (int8_t y=0; y<8; y++) {
      cube[y][z] = xPattern;
    }
  }
}

void TestPattern1()
{
  //int8_t y = 7;
  for (int8_t x=0; x<8; x++) {
    for (int8_t y=0; y<8; y++) {
      SetDot(x,y,0); 
      SetDot(7-x,y,1);
    }

    delay(20);

    for (int8_t y=0; y<8; y++) {
      ClearDot(x,y,0); 
      ClearDot(7-x,y,1);
    }
  }
}

void TestPattern2_Scan_one_layer(int8_t z)
{
  for (int8_t y=0; y<8; y++) {
    for (int8_t x=0; x<8; x++) {
      SetDot(x,y,z);
      delay(300);
      ClearDot(x,y,z);
    }
  }
}

void One_Pixel_Up_a_wall(int8_t y)
{
  for (int8_t z=0; z<8; z++) {
    for (int8_t x=7; x>=0; x--) {
      SetDot(x,y,z);
      delay(64);
      ClearDot(x,y,z);
    }
  }
}

void Line_Up_a_wall(int8_t y)
{
  for (int8_t z=0; z<8; z++) {
    for (int8_t x=7; x>=0; x--) {
      SetDot(x,y,z);
    }
    delay(64);
    CubeAllOff();
  }
}


void TestPattern_scan_all_layers()
{
  for (int8_t z=0; z<8; z++) {
    for (int8_t y=0; y<8; y++) {
      for (int8_t x=0; x<8; x++) {
        SetDot(x,y,z);
        delay(64);
        ClearDot(x,y,z);
      }
    }
  }
}

void TestPattern4_Scan_one_wall()
{
  for (int8_t z=0; z<8; z++) {
    for (int8_t x=7; x>=0; x--) {
      SetDot(x,7,z);
      delay(64);
      ClearDot(x,7,z);
    }
  }
}

void TestPattern5_swipe_wall_up()
{
  for (int8_t z=0; z<8; z++) {
    SetLayer(z,0xFF);
    //for (int8_t x=0; x<8; x++) SetDot(x,7,z);
    //for (int8_t y=3; y<8; y++) for (int8_t x=0; x<8; x++) SetDot(x,y,z);
    delay(64);
    //SetLayer(z,0x00);
    CubeAllOff();
    //for (int8_t x=0; x<8; x++) ClearDot(x,7,z);
    //for (int8_t y=3; y<8; y++) for (int8_t x=0; x<8; x++) ClearDot(x,y,z);
  }
}


void SetDot(int8_t x,int8_t y,int8_t z)
{
  //noInterrupts();
  bitSet(cube[y][z], x);
  //interrupts();
}

void ClearDot(int8_t x,int8_t y,int8_t z)
{
  bitClear(cube[y][z], x);
}

void SetLayer(int8_t z, int8_t xByte)
{
  //z = Wrap(z);
  for (int8_t y=0; y<8; y++) {
    cube[y][z] = xByte;
    //for (int8_t x=0; x<8; x++) {
    //  if (xByte==0) ClearDot(x,y,z); else SetDot(x,y,z);
    //}
  }
}

int8_t Wrap(int8_t val)
{
  if (val>7)
    return 0;
  else if (val<0)
    return 7;
  else
    return val;
}

void BottomCorner()
{
  for (int8_t x=0; x<1; x++) {
    for (int8_t y=7; y<8; y++) {
      SetDot(x,y,1); 
    }
  }
}

void FillLayerLeftToRight(int8_t z)
{
  for (int8_t x=1; x<8; x++) {
    for (int8_t y=0; y<8; y++) {
      SetDot(x,y,z);
    }
    delay(64);
  }
}

void FillWallDownUp(int8_t x)
{
  for (int8_t z=0; z<8; z++) {
    for (int8_t y=0; y<8; y++) {
      SetDot(x,y,z);
    }
    delay(64);
  }
}


void FillLayerRightLeft(int8_t z)
{
  for (int8_t x=7; x>=0; x--) {
    for (int8_t y=0; y<8; y++) {
      SetDot(x,y,z);
    }
    delay(64);
  }
}

void DropOneCenterLine(int8_t x)
{
  for (int8_t z=7; z>=0; z--) {
    SetDot(x,3,z);
    SetDot(x,4,z);
    delay(64);
  }
}

void FillWallFromCenter(int8_t x)
{
  for (int8_t y=1; y<4; y++) {
    for (int8_t z=0; z<8; z++) {
      SetDot(x,3-y,z);
      SetDot(x,4+y,z);
    }
    delay(64);
  }
}

void FillFrontAndBackRightLeft()
{
  for (int8_t x=0; x<8; x++) {
    for (int8_t z=0; z<8; z++) {
      SetDot(x,0,z);
      SetDot(x,7,z);
    }
    delay(64);
  }
}


void CubeShrink()
{
  int8_t cubeSize = 5;
  for (int8_t n=1; n<4; n++) {
    CubeAllOff();
    DrawRect(n,n,n, n, n+cubeSize, n+cubeSize); // YZ plane
    DrawRect(7-n,n,n, 7-n, n+cubeSize, n+cubeSize); // YZ plane
    DrawRect(n,n,n, n+cubeSize, n+cubeSize, n); // XY plane
    DrawRect(n,n,7-n, n+cubeSize, n+cubeSize, 7-n); // XY plane
    cubeSize -= 2;
    delay(128);
  }
}

//void DrawCube(int2_t x0, int2_t y0, int2_t z0, int2_t cubeSize)
//{
//  for (int8_t c=0; c<cubeSize; c++) {
//    DrawFront(x0+x,y0+c,z0+c, cubeSize);
//    delay(64);
//  }  
//}

void CalcLine(int8_t x1, int8_t y1, int8_t z1, int8_t x2, int8_t y2, int8_t z2, byte mode)
{
  byte parallelAxis = 0; // 0=x, 1=y, 2=z
  int8_t a1, a2, b1, b2;
  if (x1!=x2) {
    parallelAxis = 0; 
    a1=x1; 
    a2=x2; 
  } // parallel to X
  if (y1!=y2) {
    parallelAxis = 1; 
    a1=y1; 
    a2=y2; 
  } // parallel to Y
  if (z1!=z2) {
    parallelAxis = 2; 
    a1=z1; 
    a2=z2; 
  } // parallel to Z
  for (int8_t p=a1; p<=a2; p++) {
    int8_t x,y,z;
    switch (parallelAxis) {
    case 0: 
      x=p; 
      y=y1; 
      z=z1; 
      break; // parallel to X
    case 1: 
      x=x1; 
      y=p; 
      z=z1; 
      break; // parallel to Y
    case 2: 
      x=x1; 
      y=y1; 
      z=p; 
      break; // parallel to Z
    }
    if (mode==1) SetDot(x,y,z); 
    else ClearDot(x,y,z);
  }
}

void DrawLine(int8_t x1, int8_t y1, int8_t z1, int8_t x2, int8_t y2, int8_t z2)
{
  CalcLine(x1,y1,z1, x2,y2,z2, 1);
}

void EraseLine(int8_t x1, int8_t y1, int8_t z1, int8_t x2, int8_t y2, int8_t z2)
{
  CalcLine(x1,y1,z1, x2,y2,z2, 0);
}

void CalcRect(int8_t x1, int8_t y1, int8_t z1, int8_t x2, int8_t y2, int8_t z2, byte mode)
{
  byte tangentAxis = 0; // 0=x, 1=y, 2=z
  int8_t a1, a2, b1, b2;
  if (y1!=y2 && z1!=z2) {
    tangentAxis = 0; 
    a1=y1; 
    a2=y2; 
    b1=z1; 
    b2=z2;
  } // YZ Plane
  if (x1!=x2 && z1!=z2) {
    tangentAxis = 1; 
    a1=x1; 
    a2=x2; 
    b1=z1; 
    b2=z2;
  } // XZ plane
  if (x1!=x2 && y1!=y2) {
    tangentAxis = 2; 
    a1=x1; 
    a2=x2; 
    b1=y1; 
    b2=y2;
  } // XY Plane

//      Serial.print("a1,a2 b1,b2 t = ");
//      Serial.print(a1);  Serial.print(",");
//      Serial.print(a2);  Serial.print(" ");
//      Serial.print(b1);  Serial.print(",");
//      Serial.print(b2);  Serial.print(" ");
//      Serial.println(tangentAxis);

  for (int8_t p=a1; p<=a2; p++) {
    for (int8_t q=b1; q<=b2; q++) {
      int8_t x,y,z;
      switch (tangentAxis) {
      case 0: 
        x=x1; 
        y=p; 
        z=q; 
        break; // YZ Plane
      case 1: 
        x=p; 
        y=y1; 
        z=q; 
        break; // XZ plane
      case 2: 
        x=p; 
        y=q; 
        z=z1; 
        break; // XY Plane
      }

//      Serial.print("x,y,z = ");
//      Serial.print(x);  Serial.print(",");
//      Serial.print(y);  Serial.print(",");
//      Serial.print(z);  Serial.print("  mode=");
//      Serial.println(mode);

      if (mode==1) SetDot(x,y,z); 
      else ClearDot(x,y,z);
    }
  }
}

void DrawRect(int8_t x1, int8_t y1, int8_t z1, int8_t x2, int8_t y2, int8_t z2)
{
  CalcRect(x1,y1,z1, x2,y2,z2, 1);
}

void EraseRect(int8_t x1, int8_t y1, int8_t z1, int8_t x2, int8_t y2, int8_t z2)
{
  CalcRect(x1,y1,z1, x2,y2,z2, 0);
}

void NormalState()
{
  int8_t n = 0;
  int8_t cubeSize=7;
  DrawRect(n,n,n, n, n+cubeSize, n+cubeSize); // YZ plane
  DrawRect(7-n,n,n, 7-n, n+cubeSize, n+cubeSize); // YZ plane
  DrawRect(n,n,n, n+cubeSize, n+cubeSize, n); // XY plane
  DrawRect(n,n,7-n, n+cubeSize, n+cubeSize, 7-n); // XY plane
}

void BumpRight()
{
  for (byte f=0; f<5; f++)
  {
    switch (f) {
    case 0: 
      DrawRect(7,0,0, 7,7,7);  
      break;
    case 1: 
      EraseRect(7,1,1, 7,6,6); 
      DrawRect(6,1,1, 6,6,6); 
      break;
    case 2: 
      EraseRect(6,2,2, 6,5,5); 
      DrawRect(5,2,2, 5,5,5);  
      break;
    case 3: 
      EraseRect(5,2,2, 5,5,5); 
      DrawRect(6,1,1, 6,6,6);  
      break;
    case 4: 
      EraseRect(6,1,1, 6,6,6);  
      DrawRect(7,0,0, 7,7,7);  
      break;
    }  
    delay(64);
  }
}

void ShellThenShrink()
{
  FillLayerLeftToRight(0);
  FillWallDownUp(7);
  FillLayerRightLeft(7);
  DropOneCenterLine(0);
  FillWallFromCenter(0);
  FillFrontAndBackRightLeft();
  delay(250);
  CubeShrink();
  DropFromCenter();
  delay(250);
  SpreadFromCenter(0);
  RaiseCorners();
  ZipTop();
  delay(500);
  CollapseToFloor();
  RaiseSeaLevel();
  delay(500);
  RippleToRight();
}

void DropFromCenter()
{
  for (int8_t z=4; z>=0; z--)
  {
    EraseRect(3,3,z+1, 4,4,z+1);
    DrawRect(3,3,z, 4,4,z);
    delay(64);
  }
}

void SpreadFromCenter(int8_t z)
{
  for (int8_t a=0; a<3; a++)
  {
    DrawRect(2-a, 2-a, z, 5+a, 5+a, z);
    delay(64);
  }
}

void RaiseCorners()
{
  for (int8_t z=0; z<8; z++)
  {
    SetDot(0,0,z);
    SetDot(0,7,z);
    SetDot(7,0,z);
    SetDot(7,7,z);
    delay(64);
  }
}

void ZipTop()
{
  for (int8_t a=0; a<8; a++)
  {
    SetDot(a,0,7);
    SetDot(7-a,7,7);
    SetDot(7,a,7);
    SetDot(0,7-a,7);
    delay(64);
  }
}

void CollapseToFloor()
{
  for (int8_t z=7; z>0; z--)
  {
    EraseRect(0,0,z, 7,7,z);
    delay(64);
  }
}

void RaiseSeaLevel()
{
  for (int8_t z=0; z<7; z++)
  {
    EraseRect(0,0,z, 7,7,z);
    DrawRect(0,0,z+1, 7,7,z+1);
    delay(64);
  }
}


void DrawSine(int8_t offset, int8_t y)
{
  for (int8_t x=0; x<8; x++)
  {
    SetDot(x, y, sineArray[ (offset+x) % sineMaxIndex ]);
  }
}

void RippleToRight()
{
//  for (int8_t index=0; index < sineMaxIndex ; index++)
//  {
//    Serial.print("index="); Serial.print(index);
//    Serial.print(" sine="); Serial.println(sineArray[ index ]);
//  }
  
  for (byte n=0; n<8; n++) // repeat animation n times
  {
    for (byte offset=0; offset<sineMaxIndex; offset++)
    {
      CubeAllOff();
      for (int8_t y=0; y<8; y++)
      {
        DrawSine(offset, y); 
      }
      delay(32);
    }
  }
}

void PreComputes()
{
  
  for (int8_t index=0; index < sineMaxIndex; index++)
  {
    float a = pi*2 * index / sineMaxIndex;
    sineArray[ index ] = 4 + (sin(a) * 3.5);
  }
}

void loop(void) {
  //One_Pixel_Up_a_wall(7);
  //Line_Up_a_wall(1);
  //BottomUp();

  //CubeRightLeft();  //delay(1000);
  //CubeLeftRight();  //delay(1000);
  //CubeUp();  //delay(1000);
  //CubeDown();  //delay(1000);
  //CubeAllOn(); delay(2000);
  //CubeAllOff();

  //TestPattern2_Scan_one_layer(0);  //delay(1000);
  //TestPattern_scan_all_layers();  //delay(1000);
  //TestPattern4_Scan_one_wall();  delay(1000);
  //TestPattern5_swipe_wall_up();  //delay(1000);
  //CubeAllOn(); delay(1000);
  //BottomCorner();
  //LeftRight();
  //OneWall();  delay(1000);
  //LayerOn(0); delay(4000);

  //-- CapSense --
  long total1 =  cs_14_15.capacitiveSensor(30);
  long total2 =  cs_14_16.capacitiveSensor(30);
  long total3 =  cs_14_17.capacitiveSensor(30);
  //Serial.print(total1);                  // print sensor output 1
  //Serial.print("\t");
  //Serial.println(total2);
  if (total1>200) CubeLeftRight(); // Left touchpad
  if (total2>200) ShellThenShrink(); // Front touchpad
  if (total3>200) BumpRight(); //CubeRightLeft(); // Right touchpad

  pot0 = analogRead(A4);
  pot1 = analogRead(A5);

  animSpeed = map(pot0, 0,1023, 16,256);

  CubeAllOff();
  //NormalState();
}

