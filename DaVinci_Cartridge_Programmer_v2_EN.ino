// DaVinci Cartridge Programmer v2.0 by NeoRame (Originally known as DaVinci Filament Configurationv1.0 by CdRsKuLL)

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_TFTLCD.h> // Hardware-specific library
#include <TouchScreen.h>
#include <Logo.h>            // XYZPrinting Logo

#ifndef _NANODEUNIO_LIB_H
#define _NANODEUNIO_LIB_H

#if ARDUINO >= 100
#include <Arduino.h> // Arduino 1.0
#else
#include <WProgram.h> // Arduino 0022
#endif

#define NANODE_MAC_DEVICE 0xa0
#define NANODE_MAC_ADDRESS 0xfa

class NanodeUNIO {
private:
  byte addr;
public:
  NanodeUNIO(byte address);

  boolean read(byte *buffer,word address,word length);
  boolean start_write(const byte *buffer,word address,word length);
  boolean enable_write(void);
  boolean disable_write(void);
  boolean read_status(byte *status);
  boolean write_status(byte status);
  boolean await_write_complete(void);
  boolean simple_write(const byte *buffer,word address,word length);
};

#endif /* _NANODEUNIO_LIB_H */

// the following are addresses for the various parameters
// to be programmed
#define MATERIAL 0x01   // 1 Byte
#define COLOR 0x02      // 2 Bytes
#define TOTALLEN 0x08   // 4 Bytes
#define NEWLEN 0x0C     // 4 Bytes
#define HEADTEMP 0x10	  // 2 Bytes
#define BEDTEMP 0x12	  // 2 Bytes
#define MLOC 0x14	      // 2 Bytes
#define DLOC 0x16	      // 2 Bytes
#define SN 0x18		      //12 Bytes
#define CRC 0x24	      // 2 Bytes
#define LEN2 0x34	      // 4 Bytes

// ==========================================================
int bed = 90;           //default value
int extruder = 210;     //default value
int materialtype = 1;   //Material type 0 - PLA , 1 - ABS , 2 - Flex
int roll = 1;           //Roll length 0 - 120 , 1 - 240 , 2 - 400
int intColor = 1;       //Color 1 = Black as default
char mt[1];
char bt[] = {bed,0x00};
char et[] = {extruder,0x00};
char m[] = {0x41};
char selcolor[15] = "Black"; //Max Letters 15
char cr[] = {0x4B,0x00};

#define UNIO_STARTHEADER 0x55
#define UNIO_READ        0x03
#define UNIO_CRRD        0x06
#define UNIO_WRITE       0x6c
#define UNIO_WREN        0x96
#define UNIO_WRDI        0x91
#define UNIO_RDSR        0x05
#define UNIO_WRSR        0x6e
#define UNIO_ERAL        0x6d
#define UNIO_SETAL       0x67

#define UNIO_TSTBY 600
#define UNIO_TSS    10
#define UNIO_THDR    5
#define UNIO_QUARTER_BIT 10
#define UNIO_FUDGE_FACTOR 5
#define UNIO_OUTPUT() do { DDRD |= 0x10; } while (0)
#define UNIO_INPUT() do { DDRD &= 0xef; } while (0)

static void set_bus(boolean state) {
  PORTD=(PORTD&0xef)|(!!state)<<4;
}

static boolean read_bus(void) {
  return !!(PIND&0x10);
}
static void unio_inter_command_gap(void) {
  set_bus(1);
  delayMicroseconds(UNIO_TSS+UNIO_FUDGE_FACTOR);
}

static void unio_standby_pulse(void) {
  set_bus(0);
  UNIO_OUTPUT();
  delayMicroseconds(UNIO_TSS+UNIO_FUDGE_FACTOR);
  set_bus(1);
  delayMicroseconds(UNIO_TSTBY+UNIO_FUDGE_FACTOR);
}

static volatile boolean rwbit(boolean w) {
  boolean a,b;
  set_bus(!w);
  delayMicroseconds(UNIO_QUARTER_BIT);
  a=read_bus();
  delayMicroseconds(UNIO_QUARTER_BIT);
  set_bus(w);
  delayMicroseconds(UNIO_QUARTER_BIT);
  b=read_bus();
  delayMicroseconds(UNIO_QUARTER_BIT);
  return b&&!a;
}

static boolean read_bit(void) {
  boolean b;
  UNIO_INPUT();
  b=rwbit(1);
  UNIO_OUTPUT();
  return b;
}

static boolean send_byte(byte b, boolean mak) {
  for (int i=0; i<8; i++) {
    rwbit(b&0x80);
    b<<=1;
  }
  rwbit(mak);
  return read_bit();
}

static boolean read_byte(byte *b, boolean mak) {
  byte data=0;
  UNIO_INPUT();
  for (int i=0; i<8; i++) {
    data = (data << 1) | rwbit(1);
  }
  UNIO_OUTPUT();
  *b=data;
  rwbit(mak);
  return read_bit();
}

static boolean unio_send(const byte *data,word length,boolean end) {
  for (word i=0; i<length; i++) {
    if (!send_byte(data[i],!(((i+1)==length) && end))) return false;
  }
  return true;
}

static boolean unio_read(byte *data,word length)  {
  for (word i=0; i<length; i++) {
    if (!read_byte(data+i,!((i+1)==length))) return false;
  }
  return true;
}

static void unio_start_header(void) {
  set_bus(0);
  delayMicroseconds(UNIO_THDR+UNIO_FUDGE_FACTOR);
  send_byte(UNIO_STARTHEADER,true);
}

NanodeUNIO::NanodeUNIO(byte address) {
  addr=address;
}

#define fail() do { sei(); return false; } while (0)

boolean NanodeUNIO::read(byte *buffer,word address,word length) {
  byte cmd[4];
  cmd[0]=addr;
  cmd[1]=UNIO_READ;
  cmd[2]=(byte)(address>>8);
  cmd[3]=(byte)(address&0xff);
  unio_standby_pulse();
  cli();
  unio_start_header();
  if (!unio_send(cmd,4,false)) fail();
  if (!unio_read(buffer,length)) fail();
  sei();
  return true;
}

boolean NanodeUNIO::start_write(const byte *buffer,word address,word length) {
  byte cmd[4];
  if (((address&0x0f)+length)>16) return false; // would cross page boundary
  cmd[0]=addr;
  cmd[1]=UNIO_WRITE;
  cmd[2]=(byte)(address>>8);
  cmd[3]=(byte)(address&0xff);
  unio_standby_pulse();
  cli();
  unio_start_header();
  if (!unio_send(cmd,4,false)) fail();
  if (!unio_send(buffer,length,true)) fail();
  sei();
  return true;
}

boolean NanodeUNIO::enable_write(void) {
  byte cmd[2];
  cmd[0]=addr;
  cmd[1]=UNIO_WREN;
  unio_standby_pulse();
  cli();
  unio_start_header();
  if (!unio_send(cmd,2,true)) fail();
  sei();
  return true;
}

boolean NanodeUNIO::disable_write(void) {
  byte cmd[2];
  cmd[0]=addr;
  cmd[1]=UNIO_WRDI;
  unio_standby_pulse();
  cli();
  unio_start_header();
  if (!unio_send(cmd,2,true)) fail();
  sei();
  return true;
}

boolean NanodeUNIO::read_status(byte *status) {
  byte cmd[2];
  cmd[0]=addr;
  cmd[1]=UNIO_RDSR;
  unio_standby_pulse();
  cli();
  unio_start_header();
  if (!unio_send(cmd,2,false)) fail();
  if (!unio_read(status,1)) fail();
  sei();
  return true;
}

boolean NanodeUNIO::write_status(byte status) {
  byte cmd[3];
  cmd[0]=addr;
  cmd[1]=UNIO_WRSR;
  cmd[2]=status;
  unio_standby_pulse();
  cli();
  unio_start_header();
  if (!unio_send(cmd,3,true)) fail();
  sei();
  return true;
}

boolean NanodeUNIO::await_write_complete(void) {
  byte cmd[2];
  byte status;
  cmd[0]=addr;
  cmd[1]=UNIO_RDSR;
  unio_standby_pulse();
  do {
    unio_inter_command_gap();
    cli();
    unio_start_header();
    if (!unio_send(cmd,2,false)) fail();
    if (!unio_read(&status,1)) fail();
    sei();
  } 
  while (status&0x01);
  return true;
}

boolean NanodeUNIO::simple_write(const byte *buffer,word address,word length) {
  word wlen;
  while (length>0) {
    wlen=length;
    if (((address&0x0f)+wlen)>16) {
      wlen=16-(address&0x0f);
    }
    if (!enable_write()) return false;
    if (!start_write(buffer,address,wlen)) return false;
    if (!await_write_complete()) return false;
    buffer+=wlen;
    address+=wlen;
    length-=wlen;
  }
  return true;
}

static void status(boolean r)
{
  if (r) Serial.println("(success)");
  else Serial.println("(failure)");
}

static void dump_eeprom(word address,word length)
{
  byte buf[128];
  char lbuf[80];
  char *x;
  int i,j;

  NanodeUNIO unio(NANODE_MAC_DEVICE);

  memset(buf,0,128);
  status(unio.read(buf,address,length));

  for (i=0; i<128; i+=16) {
    x=lbuf;
    sprintf(x,"%02X: ",i);
    x+=4;
    for (j=0; j<16; j++) {
      sprintf(x,"%02X",buf[i+j]);
      x+=2;
    }
    *x=32;
    x+=1;
    for (j=0; j<16; j++) {
      if (buf[i+j]>=32 && buf[i+j]<127) *x=buf[i+j];
      else *x=46;
      x++;
    }
    *x=0;
    Serial.println(lbuf);
  }
}

static void read_eeprom(word address,word length)
{
  byte buf[128];
  char lbuf[80];
  char *x;
  int i,j;

  NanodeUNIO unio(NANODE_MAC_DEVICE);

  memset(buf,0,128);
  status(unio.read(buf,address,length));

  for (i=0; i<128; i+=16) {
    x=lbuf;
    sprintf(x,"%02X: ",i);
    x+=4;
    for (j=0; j<16; j++) {
      sprintf(x,"%02X",buf[i+j]);
      x+=2;
    }
    *x=32;
    x+=1;
    for (j=0; j<16; j++) {
      if (buf[i+j]>=32 && buf[i+j]<127) *x=buf[i+j];
      else *x=46;
      x++;
    }
    *x=0;
    Serial.println(lbuf);
  }

      Serial.println(buf[1]);
      if (buf[1] == (0x50)){
        materialtype = 0;
      } if (buf[1] == (0x41)){
        materialtype = 1;
      } if (buf[1] == (0x56)) {
        materialtype = 2;
      }
      extruder = buf[0x10];
      et[0] = buf[0x10];
      bed = buf[0x12];
}

int led = 13;

byte sr;
NanodeUNIO unio(NANODE_MAC_DEVICE);

// Value to write to the EEPROM for remaining filament length
// Starter Cartdridge is 120m
//char x[] = {0xc0,0xd4,0x01,0x00}; //120m
char x[] = {0x80,0xa9,0x03,0x00}; //default 240m

const int NUMBER_OF_FIELDS = 3;
int fieldIndex = 0;
int values[NUMBER_OF_FIELDS];

// Assign human-readable names to some common 16-bit color values:
#define	BLACK           0x0000
#define	BLUE            0x001F
#define	RED             0xF800
#define	GREEN           0x03E0
#define CYAN            0x07FF
#define MAGENTA         0xF81F
#define YELLOW          0xFFE0
#define WHITE           0xFFFF
#define GREY            0x7BEF
#define NAVY            0x000F      //   0,   0, 128 
#define DARKGREEN       0x03E0      //   0, 128,   0 
#define DARKCYAN        0x03EF      //   0, 128, 128 
#define MAROON          0x7800      // 126,   0,   0 
#define PURPLE_         0x780F      // 128,   0, 128 
#define OLIVE_          0x7BE0      // 128, 128,   0 
#define LIGHTGREY       0xC618      // 192, 192, 192 
#define ORANGE          0xFD20      // 255, 165,   0 
#define GREENYELLOW     0xAFE5      // 173, 255,  47 

// Set Filament Colors in 16-bit color values:
#define PURPLE          0x5908      //  89,  33,  66 [PURPURE] 
#define PURPURIN        0x98EC      // 151,  31, 100
#define VIOLET          0x08AC      //  15,  22, 101
#define VIRDITY         0x1C52      //  30, 138, 145
#define TANGERINE       0xCB65      // 208, 110,  41
#define OLIVE           0x3A25      //  59,  70,  47
#define GOLD            0xBD68      // 187, 175,  65
#define CYBERYELLOW     0xDE4A      // 217, 200,  86
#define VirginViolet    0xF813      // 255,   0, 156 [NEON_TANGERINE]
#define NEONGREEN       0xAFE0      // 171, 255,   0
#define NEONYELLOW      0xF7A4      // 241, 246,  36
#define GRAPEPURPLE     0xA218      // 165,  64, 193

#define LCD_CS A3
#define LCD_CD A2
#define LCD_WR A1
#define LCD_RD A0
// optional
#define LCD_RESET A4
Adafruit_TFTLCD tft(LCD_CS, LCD_CD, LCD_WR, LCD_RD, LCD_RESET);
//Adafruit_TFTLCD tft;

// Calibrates value

#define SENSIBILITY 300
#define MINPRESSURE 10
#define MAXPRESSURE 1000

//These are the pins for the shield!
#define YP A2 
#define XM A1 
#define YM 6
#define XP 7 

// Calibrate values for the screen

#define TS_MINX 125
#define TS_MINY 85
#define TS_MAXX 965
#define TS_MAXY 905

// Init TouchScreen:

TouchScreen ts = TouchScreen(XP, YP, XM, YM, SENSIBILITY);

//Set default settings

void setup(void) {

  tft.reset();
  tft.begin(0x9341); // SDFP5408
  //tft.invertDisplay(true);
  Serial.begin(115200);
  pinMode(13, OUTPUT);
  read_eeprom(0,128);
  updatevars ();
  splash ();
  drawButtons ();
  drawMenu ();

}

void loop(void) {

  digitalWrite(13, HIGH);
  TSPoint p = ts.getPoint();
  digitalWrite(13, LOW);

  // if sharing pins, you'll need to fix the directions of the touchscreen pins
 // pinMode(XP, OUTPUT);
  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);
 // pinMode(YM, OUTPUT);

  if (p.z > MINPRESSURE && p.z < MAXPRESSURE) {

   Serial.print("X = "); Serial.print(p.x);
   Serial.print("\tY = "); Serial.print(p.y);
   Serial.print("\tPressure = "); Serial.println(p.z);
    
  // Check PLA or ABS Buttons
 if ((p.x > 688) && (p.x < 780)){
    if ((p.y > 500) && (p.y <624)){ //PLA BUTTON
      materialtype = 0;
      Serial.println("PLA");
      m[0] = (0x50);
      mat();
      delay(250);
  }
    if ((p.y >328) && (p.y <456)){ //ABS BUTTON
      materialtype = 1;
      Serial.println("ABS");
      m[0] = (0x41);
      mat();
      delay(250);
    }
    if ((p.y >158) && (p.y <286)){ //Flex BUTTON
      materialtype = 2;
      Serial.println("FLX");
      m[0] = (0x56);
      mat();
      delay(250);
    }
 }
  
// Ext Temp Buttons
  if ((p.x > 584) && (p.x < 669)){
    if ((p.y > 482) && (p.y <588)){ //DOWN BUTTON
      extruder = extruder - 5;
      if (extruder < 170){extruder = 170;}
      char net[] = {extruder,0x00};
      et[0] = net[0];
      et[1] = net[1];
      exttemp();
      delay(150);
    }
    if ((p.y >183) && (p.y <286)){ //UP BUTTON
      extruder = extruder + 5;
      if (extruder > 250){extruder = 250;}
      char net[] = {extruder,0x00};
      et[0] = net[0];
      et[1] = net[1];
      exttemp();
      delay(150);
    }
  }
//    // Bed Temp Buttons
  if ((p.x > 487) && (p.x < 573)){
    if ((p.y > 492) && (p.y <573)){ //DOWN BUTTON
      bed = bed - 5;
      if (bed < 45){bed = 45;}
      char nbt[] = {bed,0x00};
      bt[0] = nbt[0];
      bt[1] = nbt[1];
      bedtemp();
      delay(150);
    }
    if ((p.y >183) && (p.y <286)){ //UP BUTTON
      bed = bed + 5;
      if (bed > 110){bed = 110;}
      char nbt[] = {bed,0x00};
      bt[0] = nbt[0];
      bt[1] = nbt[1];
      bedtemp();
      delay(150);
    }
  }
//    // Roll size
  if ((p.x > 399) && (p.x < 475)){
    if ((p.y > 500) && (p.y <624)){ //120
      roll = 0;
      x[0] = 0xc0;
      x[1] = 0xd4;
      x[2] = 0x01;
      x[3] = 0x00; //120m 0xc0,0xd4,0x01,0x00
      rollsub();
      delay(250);
    }
    if ((p.y >328) && (p.y <456)){ //240
      roll = 1;
      x[0] = 0x80;
      x[1] = 0xa9;
      x[2] = 0x03;
      x[3] = 0x00 ; //default 240m 0x80,0xa9,0x03,0x00 
      rollsub();
      delay(250);
    }
    if ((p.y >158) && (p.y <286)){ //400
      roll = 2;
      x[0] = 0x80;
      x[1] = 0x1a;
      x[2] = 0x06;
      x[3] = 0x00; //400m 0x80,0x1a,0x06,0x00
      rollsub();
      delay(250);
    }
  }
 // Color
  if ((p.x > 270) && (p.x < 350)){
    if ((p.y > 637) && (p.y <916)){ //DOWN BUTTON
      intColor = intColor - 1;
      delay(150);
    }
    if ((p.y >131) && (p.y <334)){ //UP BUTTON
      intColor = intColor + 1;
      delay(150);
    }
  }
 // Reset & Save
  if ((p.x > 146) && (p.x < 250)){
    if ((p.y > 677) && (p.y <916)){ //RESET BUTTON
      factory_setting();
      drawMenu();
      delay(150);
    }
    if ((p.y >131) && (p.y <374)){ //SAVE BUTTON
      write_eeprom();
      eeprom_read();
      drawMenu();
      delay(150);
    }
  }

switch (intColor) {
  case 1:
  tft.setTextColor(GREY);
      strcpy(selcolor,"Black"); //Black
      cr[0] = 0x4B;
      cr[1] = 0x00;
      changecolor();
    break;
  case 2:
  tft.setTextColor(WHITE);
      strcpy(selcolor,"White"); //White
      cr[0] = 0x57;
      cr[1] = 0x00;
      changecolor();
    break;
  case 3:
  tft.setTextColor(WHITE);
      strcpy(selcolor,"Nature"); //Nature
      cr[0] = 0x5A;
      cr[1] = 0x00;
      changecolor();
    break;
  case 4:
  tft.setTextColor(BLUE);
      strcpy(selcolor,"Blue"); //Blue
      cr[0] = 0x42;
      cr[1] = 0x00;
      changecolor();
    break;
  case 5:
  tft.setTextColor(GREEN);
      strcpy(selcolor,"Green"); //Green
      cr[0] = 0x47;
      cr[1] = 0x00;
      changecolor();
    break;
  case 6:
  tft.setTextColor(RED);
      strcpy(selcolor,"Red"); //Red
      cr[0] = 0x52;
      cr[1] = 0x00;
      changecolor();
    break;
  case 7:
  tft.setTextColor(YELLOW);
      strcpy(selcolor,"Yellow"); //Yellow
      cr[0] = 0x59;
      cr[1] = 0x00;
      changecolor();
    break;
  case 8:
  tft.setTextColor(PURPLE);
      strcpy(selcolor,"Purple"); //Purple (Purpure)
      cr[0] = 0x41;
      cr[1] = 0x00;
      changecolor();
    break;
  case 9:
  tft.setTextColor(VIOLET);
      strcpy(selcolor,"Violet"); //Violet
      cr[0] = 0x4C;
      cr[1] = 0x00;
      changecolor();
    break;
  case 10:
  tft.setTextColor(PURPURIN);
      strcpy(selcolor,"Purpurin"); //Purpurin
      cr[0] = 0x4E;
      cr[1] = 0x00;
      changecolor();
    break;
  case 11:
  tft.setTextColor(VIRDITY);
      strcpy(selcolor,"Viridity"); //Virdity
      cr[0] = 0x44;
      cr[1] = 0x00;
      changecolor();
    break;
  case 12:
  tft.setTextColor(TANGERINE);
      strcpy(selcolor,"Tangerine"); //Tangerine
      cr[0] = 0x54;
      cr[1] = 0x00;
      changecolor();
    break;
  case 13:
  tft.setTextColor(OLIVE);
      strcpy(selcolor,"Olive"); //Olive
      cr[0] = 0x45;
      cr[1] = 0x00;
      changecolor();
    break;
  case 14:
  tft.setTextColor(GOLD);
      strcpy(selcolor,"Gold"); //Gold
      cr[0] = 0x46;
      cr[1] = 0x00;
      changecolor();
    break;
  case 15:
  tft.setTextColor(CYBERYELLOW);
      strcpy(selcolor,"Cyber Yellow"); //Cyber Yellow
      cr[0] = 0x53;
      cr[1] = 0x00;
      changecolor();
    break;
  case 16:
  tft.setTextColor(VirginViolet);
      strcpy(selcolor,"Virgin Violet"); //Virgin Violet
      cr[0] = 0x43;
      cr[1] = 0x00;
      changecolor();
    break;
  case 17:
  tft.setTextColor(NEONGREEN);
      strcpy(selcolor,"NEON Green"); //NEON Green
      cr[0] = 0x48;
      cr[1] = 0x00;
      changecolor();
    break;
  case 18:
  tft.setTextColor(NEONYELLOW);
      strcpy(selcolor,"NEON Yellow"); //NEON Yellow
      cr[0] = 0x4A;
      cr[1] = 0x00;
      changecolor();
    break;
  case 19:
  tft.setTextColor(GREY);
      strcpy(selcolor,"Snow White"); //Snow White
      cr[0] = 0x49;
      cr[1] = 0x00;
      changecolor();
    break;
  case 20:
  tft.setTextColor(GRAPEPURPLE);
      strcpy(selcolor,"Grape Purple"); //Grape Purple
      cr[0] = 0x4D;
      cr[1] = 0x00;
      changecolor();
    break;
  case 21:
  tft.setTextColor(YELLOW);
      strcpy(selcolor,"Clear Yellow"); //Clear Yellow
      cr[0] = 0x4F;
      cr[1] = 0x00;
      changecolor();
    break;
  case 22:
  tft.setTextColor(GREEN);
      strcpy(selcolor,"Clear Green"); //Clear Green
      cr[0] = 0x50;
      cr[1] = 0x00;
      changecolor();
    break;
  case 23:
  tft.setTextColor(ORANGE);
      strcpy(selcolor,"Clear Tangerine"); //Clear Tangerine
      cr[0] = 0x51;
      cr[1] = 0x00;
      changecolor();
    break;
  case 24:
  tft.setTextColor(BLUE);
      strcpy(selcolor,"Clear Blue"); //Clear Blue
      cr[0] = 0x55;
      cr[1] = 0x00;
      changecolor();
    break;
  case 25:
  tft.setTextColor(PURPLE);
      strcpy(selcolor,"Clear Purple"); //Clear Purple
      cr[0] = 0x56;
      cr[1] = 0x00;
      changecolor();
    break;
  case 26:
  tft.setTextColor(MAGENTA);
      strcpy(selcolor,"Clear Magenta"); //Clear Magenta
      cr[0] = 0x58;
      cr[1] = 0x00;
      changecolor();
    break;
  default:
      intColor = 1;
    break;
  case 0:
      intColor = 26;
    break;
}

  }
}

void drawMenu () {

  // Draw Menu
  tft.fillScreen(BLACK);
  tft.fillRect(0, 0, 320, 25, GREY);
  tft.setTextSize (2);
  tft.setTextColor(WHITE);
  tft.setCursor (35, 5);
  tft.println("Filament Configurator");
  tft.setTextSize (2);
  tft.setTextColor(WHITE);
  tft.setCursor (10, 45); //45
  tft.println("MATERIAL:");
  tft.setCursor (10, 75); //85
  tft.println("EXTRUDER:");
  tft.setCursor (10, 105); //125
  tft.println("HEATBED:");
  tft.setCursor (10, 135); //165
  tft.println("LENGTH:");

  // EXT TEMP
  tft.setCursor (148, 75);
  tft.println("<<");
  tft.setCursor (265, 75);
  tft.println(">>");

  
   // BED TEMP
  tft.setCursor (148, 105);
  tft.println("<<");
  tft.setCursor (265, 105);
  tft.println(">>");
  
  // COLOR
  tft.setCursor (15, 170);
  tft.println("<<");
  tft.setCursor (280, 170);
  tft.println(">>"); 
  
  // RESET BUTTON
  tft.fillRect(5, 205, 95, 30, RED);
  tft.drawRect(5, 205, 95, 30, WHITE);
  tft.setTextColor(WHITE);
  tft.setCursor (25, 213);
  tft.println("RESET");
  
  // SAVE BUTTON
  tft.fillRect(220, 205, 95, 30, GREEN);
  tft.drawRect(220, 205, 95, 30, WHITE);
  tft.setTextColor(WHITE);
  tft.setCursor (246, 213);
  tft.println("SAVE");

  mat ();
  rollsub();
  exttemp();
  bedtemp();
  changecolor();
}

void drawButtons () {
  tft.fillRect(250, 240, 80, 20, RED); // SAVE
}

void splash () {
  tft.setRotation(3);
  uint16_t width = tft.width() - 1;
  uint16_t height = tft.height() - 1;
  uint8_t border = 10;
  tft.fillScreen(BLACK);
  tft.drawBitmap( 60, 7, imageXYZLogo, 200, 61, GREY); //XYZLogo
  delay(300);
  tft.drawRect(80, 80, 168, 85, GREY);
  delay(150);
  tft.fillRect(91, 90, 40, 65, ORANGE);
  delay(150);
  tft.fillRect(144, 90, 40, 65, GRAPEPURPLE);
  delay(150);
  tft.fillRect(197, 90, 40, 65, DARKCYAN);
  delay(250);
  tft.setTextColor(WHITE);
  tft.setTextSize (5);
  tft.setCursor (99, 105);
  tft.println("D");
  delay(150);
  tft.setTextSize (5);
  tft.setCursor (152, 105);
  tft.println("C");
  delay(150);
  tft.setTextSize (5);
  tft.setCursor (206, 105);
  tft.println("P");
  delay(300);
  tft.setTextSize (2);
  tft.setCursor (65, 175);
  tft.println("DaVinci Cartridge");
  tft.setTextSize (2);
  tft.setCursor (105, 195);
  tft.println("Programmer"); 
  tft.setTextSize (1);
  tft.setCursor (120, 225);
  tft.setTextColor(GREY);
  tft.println("NeoRame - 2018");
  delay(2500);
}



void mat () {
  // MATERIAL TEXT
  tft.setTextSize (2);
  tft.setTextColor(WHITE);
  if (materialtype == 0){
    tft.fillRect(127, 38, 50, 28, GREY);
    tft.fillRect(192, 38, 50, 28, BLACK);
    tft.fillRect(257, 38, 50, 28, BLACK);
  }
  if (materialtype == 1){
    tft.fillRect(127, 38, 50, 28, BLACK);
    tft.fillRect(192, 38, 50, 28, DARKCYAN);
    tft.fillRect(257, 38, 50, 28, BLACK);
  } 
  if (materialtype == 2){
    tft.fillRect(127, 38, 50, 28, BLACK);
    tft.fillRect(192, 38, 50, 28, BLACK);
    tft.fillRect(257, 38, 50, 28, GREY);
  }
  tft.drawRect(127, 38, 50, 28, WHITE); // PLA
  tft.drawRect(192, 38, 50, 28, WHITE); // ABS 
  tft.drawRect(257, 38, 50, 28, WHITE); // Flex 
  tft.setCursor (135, 45);
  tft.println("PLA");
  tft.setCursor (200, 45);
  tft.println("ABS");
  tft.setCursor (265, 45);
  tft.println("FLX");
}  

void rollsub() {
    // ROLL LENGTH TEXT
  tft.setTextColor(WHITE);
  tft.fillRect(127, 128, 50, 28, BLACK);
  tft.fillRect(192, 128, 50, 28, BLACK);
  tft.fillRect(257, 128, 50, 28, BLACK);
  if (roll == 0){ tft.fillRect(127, 128, 50, 28, DARKCYAN);}
  if (roll == 1){ tft.fillRect(192, 128, 50, 28, DARKCYAN);}
  if (roll == 2){ tft.fillRect(257, 128, 50, 28, GREY);}  
  tft.drawRect(127, 128, 50, 28, WHITE); // 120
  tft.drawRect(192, 128, 50, 28, WHITE); // 240 
  tft.drawRect(257, 128, 50, 28, WHITE); // 400 
  tft.setCursor (135, 135);
  tft.println("120");
  tft.setCursor (200, 135);
  tft.println("240");
  tft.setCursor (265, 135);
  tft.println("400");
}
void exttemp() {
  tft.setTextColor(WHITE);
  tft.fillRect(195, 75, 60, 22, BLACK);
  if ((extruder > -1) && (extruder < 10)){
    tft.setCursor (217, 75);
  }
  if ((extruder > 9) && (extruder < 100)){
    tft.setCursor (207, 75);
  } if (extruder > 99) {
    tft.setCursor (195, 75);
  }
  tft.println(extruder);
      tft.setCursor (233, 75);
  tft.println(char(247));
}

void bedtemp() {
  tft.setTextColor(WHITE);
  tft.fillRect(195, 105, 60, 22, BLACK);
  if ((bed > -1) && (bed < 10)){
    tft.setCursor (217, 105);
  }
  if ((bed > 9) && (bed < 100)){
    tft.setCursor (207, 105);
  } if (bed > 99) {
    tft.setCursor (195, 105);
  }
  tft.println(bed);
      tft.setCursor (233, 105);
  tft.println(char(247));
}

void changecolor() {
  tft.fillRect(80, 170, 180, 22, BLACK);
  tft.setCursor (80, 170);
  tft.println(selcolor);
}

void eeprom_read()
{
  do {
    Serial.println("Testing connection to Da Vinci EEPROM CHIP\n");    
    delay(100);
  } 
  while(!unio.read_status(&sr));

  Serial.println("Da Vinci EEPROM found...");
  Serial.println("Reading the Davinci EEPROM Contents...");
  dump_eeprom(0,128);
}

void factory_setting()
{
  do {
    Serial.println("Testing connection to Da Vinci EEPROM CHIP\n");    
    delay(100);
  } 
  while(!unio.read_status(&sr));

  char L1[] = {
    0x5A,0x41,0x52,0x00,0x00,0x34,0x33,0x52,0x80,0xA9,0x03,0x00,0x80,0xA9,0x03,0x00
  };
  char L2[] = {
    0xD2,0x00,0x5A,0x00,0x54,0x48,0x47,0x42,0x30,0x31,0x33,0x31,0x00,0x00,0x00,0x00
  };
  char L3[] = {
    0x00,0x00,0x00,0x00,0x34,0x00,0x00,0x00,0x5A,0x41,0x57,0x00,0xAA,0x55,0xAA,0x55
  };
  char L4[] = {
    0x88,0x33,0x55,0xAA,0x80,0xA9,0x03,0x00,0xAA,0x55,0xAA,0x55,0x07,0x83,0x0A,0x00
  };

  while(!unio.read_status(&sr));

  Serial.println("Da Vinci EEPROM found...");
  Serial.println("Reading the Davinci EEPROM Contents...");
  dump_eeprom(0,128);


  Serial.println("Updating EEPROM...");
  status(unio.simple_write((const byte *)L1,0,16));
  status(unio.simple_write((const byte *)L2,16,16));
  status(unio.simple_write((const byte *)L3,32,16));
  status(unio.simple_write((const byte *)L4,48,16));
  // same block from offset 0 is offset 64 bytes
  status(unio.simple_write((const byte *)L1,0+64,16));
  status(unio.simple_write((const byte *)L2,16+64,16));
  status(unio.simple_write((const byte *)L3,32+64,16));
  status(unio.simple_write((const byte *)L4,48+64,16));
}
//===================== END factory_setting ===============================

//=================== IncrementSerial =====================================
void IncrementSerial(unsigned char * cArray, long lAddress, long lSize)
{
  unsigned char szTempBuffer[20] = {   
    0                                                                                                                                            };
  memcpy(szTempBuffer,&cArray[lAddress],lSize);
  long lSerial = atol((char *)szTempBuffer);
  lSerial++;
  sprintf((char *)szTempBuffer,"%04d",lSerial);
  memcpy(&cArray[lAddress],szTempBuffer,lSize);
}

void write_eeprom()
{
  do {
    Serial.println("Testing connection to Da Vinci EEPROM CHIP\n");    
    delay(100);
  } 
  while(!unio.read_status(&sr));

  //Read the serial number - added by Matt
  byte buf[20];
  memset(buf,0,20);
  status(unio.read(buf,SN,12));
  //Increment the serial number
  IncrementSerial(&buf[0], 0, 12);

  Serial.println("Updating EEPROM...");
  status(unio.simple_write((const byte *)m,MATERIAL,1)); // Material
  status(unio.simple_write((const byte *)x,TOTALLEN,4));
  status(unio.simple_write((const byte *)x,NEWLEN,4));
  status(unio.simple_write((const byte *)et,HEADTEMP,2)); // extruder temp
  status(unio.simple_write((const byte *)bt,BEDTEMP,2)); // bed temp
  status(unio.simple_write((const byte *)cr,COLOR,2)); // Color
  //Write the serial number
  status(unio.simple_write((const byte *)buf,SN,12)); //Serial Number
  status(unio.simple_write((const byte *)x,LEN2,4));
  // same block from offset 0 is offset 64 bytes
  status(unio.simple_write((const byte *)m,64 + MATERIAL,1)); // Material
  status(unio.simple_write((const byte *)x,64 + TOTALLEN,4));
  status(unio.simple_write((const byte *)x,64 + NEWLEN,4));
  status(unio.simple_write((const byte *)et,64 + HEADTEMP,2)); // extruder temp
  status(unio.simple_write((const byte *)bt,64 + BEDTEMP,2)); // bed temp
  status(unio.simple_write((const byte *)cr,64 + COLOR,2)); // Color
  //Write the serial number
  status(unio.simple_write((const byte *)buf,64 + SN,12)); //Serial Number
  status(unio.simple_write((const byte *)x,64 + LEN2,4));
}

int readline(int readch, char *buffer, int len)
{
  static int pos = 0;
  int rpos;

  if (readch > 0) {
    switch (readch) {
    case '\n': // Ignore new-lines
      rpos = pos;
      pos = 0;  // Reset position index ready for next time
      return rpos;
      break;
    case '\r': // Return on CR
      rpos = pos;
      pos = 0;  // Reset position index ready for next time
      return rpos;
    default:
      if (pos < len-1) {
        buffer[pos++] = readch;
        buffer[pos] = 0;
      }
    }
  }
  // No end of line has been found, so return -1.
  return -1;
}

void updatevars () {
  if (materialtype = 0) {
    m[0] = (0x50);
  } if (materialtype = 2) {
    m[0] = (0x56);
  } if (materialtype = 1) {
    m[0] = (0x41);
  }
  
  char net[] = {extruder,0x00};
  et[0] = net[0];
  et[1] = net[1];

  char nbt[] = {bed,0x00};
  bt[0] = nbt[0];
  bt[1] = nbt[1];
   
}

