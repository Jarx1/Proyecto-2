//***************************************************************************************************************************************
/* 
 *  Librería para el uso de la pantalla ILI9341 en modo 8 bits
 * Basado en el código de martinayotte - https://www.stm32duino.com/viewtopic.php?t=637
 * Adaptación, migración y creación de nuevas funciones: Pablo Mazariegos y José Morales
 * Con ayuda de: José Guerra
 * IE3027: Electrónica Digital 2 - 2019
 */
//***************************************************************************************************************************************
//***************************************************************************************************************************************
#include <stdint.h>
#include <stdbool.h>
#include <TM4C123GH6PM.h>
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/debug.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/rom_map.h"
#include "driverlib/rom.h"
#include "driverlib/sysctl.h"
#include "driverlib/timer.h"

#include "bitmaps.h"
#include "font.h"
#include "lcd_registers.h"

#define LCD_RST PD_0
#define LCD_CS PD_1
#define LCD_RS PD_2
#define LCD_WR PD_3
#define LCD_RD PE_1
int DPINS[] = {PB_0, PB_1, PB_2, PB_3, PB_4, PB_5, PB_6, PB_7};  

//***************************************************************************************************************************************
// Botones utilizados 
//***************************************************************************************************************************************
const int UP_BUTTON = PF_4;
const int DOWN_BUTTON = PD_7;
const int UP_BUTTON2 = PD_6;
const int DOWN_BUTTON2 = PC_7;
//***************************************************************************************************************************************
// Definicion de colores 
//***************************************************************************************************************************************
#define BLACK           0x0000
#define BLUE            0x001F
#define RED             0xF800
#define YELLOW          0xFFE0  
#define WHITE           0xFFFF
//***************************************************************************************************************************************
//Variables para el juego  
//***************************************************************************************************************************************
const unsigned long PADDLE_RATE = 1;
const unsigned long BALL_RATE = 1;
const uint8_t PADDLE_HEIGHT = 23;
int MAX_SCORE = 8;

int CPU_SCORE = 0;
int PLAYER_SCORE = 0;

uint8_t ball_x = 64, ball_y = 32;
uint8_t ball_dir_x = 1, ball_dir_y = 1;

boolean gameIsRunning = true;
boolean resetBall = false;

unsigned long ball_update;
unsigned long paddle_update;

const uint8_t CPU_X = 47;
uint8_t cpu_y = 100;

const uint8_t PLAYER_X = 210;
uint8_t player_y = 100;

//***************************************************************************************************************************************
// Functions Prototypes
//***************************************************************************************************************************************
void LCD_Init(void);
void LCD_CMD(uint8_t cmd);
void LCD_DATA(uint8_t data);
void SetWindows(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2);
void LCD_Clear(unsigned int c);
void H_line(unsigned int x, unsigned int y, unsigned int l, unsigned int c);
void V_line(unsigned int x, unsigned int y, unsigned int l, unsigned int c);
void Rect(unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned int c);
void FillRect(unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned int c);
void LCD_Print(String text, int x, int y, int fontSize, int color, int background);

void LCD_Bitmap(unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned char bitmap[]);
void LCD_Sprite(int x, int y, int width, int height, unsigned char bitmap[],int columns, int index, char flip, char offset);


//***************************************************************************************************************************************
// Inicialización
//***************************************************************************************************************************************
void setup() {
  pinMode(UP_BUTTON, INPUT);     // LOS
  pinMode(DOWN_BUTTON, INPUT);   // BOTONES 
  pinMode(UP_BUTTON2,INPUT);     // PARA 
  pinMode(DOWN_BUTTON2,INPUT);   // 2 JUGADORES 

  pinMode(PA_6, OUTPUT);         // SALIDAS
  pinMode(PE_3, OUTPUT);         // PARA 
  pinMode(PE_2, OUTPUT);         // ACTIVAR 
  pinMode(PF_1, OUTPUT);         // AUDIO
  
  SysCtlClockSet(SYSCTL_SYSDIV_2_5|SYSCTL_USE_PLL|SYSCTL_OSC_MAIN|SYSCTL_XTAL_16MHZ);
  Serial.begin(9600);
  GPIOPadConfigSet(GPIO_PORTB_BASE, 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7, GPIO_STRENGTH_8MA, GPIO_PIN_TYPE_STD_WPU);
  Serial.println("Inicio");
  //Inicializacion Pantalla de carga
  LCD_Init();
  LCD_Clear(0x00);
  FillRect(0, 0, 320, 240, BLACK);  
  for(int x = 320-20; x >0; x--){
    
  digitalWrite(PE_2, HIGH); //Señal para encender la 
  digitalWrite(PE_2, LOW);  //Cancion de entrada 
  int anim2 = (x/11)%2;
  LCD_Sprite(95,110,144,17,titulo,2,anim2,0,0); 
  }
  LCD_Sprite(95,110,144,17,titulo,2,1,0,0);
  String text1 = "Presiona un boton";
  LCD_Print(text1, 20, 140, 2, 0xffff, 0x00);
  //CONDICION PARA EMPEZAR A JUGAR
  while(digitalRead(UP_BUTTON)    == LOW 
     && digitalRead(DOWN_BUTTON)  == LOW
     && digitalRead(UP_BUTTON2)   == LOW 
     && digitalRead(DOWN_BUTTON2) == LOW) 
  {
    delay(100); //MIENTRAS NO SE OPRIMA SOLO ESPERA 
  }  
  //INICIO Y CONTEO DE TIEMPO 
  unsigned long start = millis();
  LCD_Clear(0x00);
  Rect(10,10,240,220,0xffff);
  while(millis() - start < 2000);
  ball_update = millis();
  paddle_update = ball_update;
  //Direccion pelota inicial 
  ball_x = random(120,125); 
  ball_y = random(110,120);  
}
//***************************************************************************************************************************************
// Loop Infinito
//***************************************************************************************************************************************
void loop() {
unsigned long time = millis();    
static bool up_state = false;
static bool down_state = false;
static bool up_state2 = false;
static bool down_state2 = false;
up_state |= (digitalRead(UP_BUTTON) == LOW);
down_state |= (digitalRead(DOWN_BUTTON) == LOW);
up_state2 |= (digitalRead(UP_BUTTON2) == LOW);
down_state2 |= (digitalRead(DOWN_BUTTON2) == LOW); 

//Reseteo de la pelota siempre inicia en medio   
  if(resetBall)
    {
      ball_x = random(120,125); 
      ball_y = random(110,120);
      do
      {
      ball_dir_x = random(-1,2);
      }while(ball_dir_x==0);

       do
      {
      ball_dir_y = random(-1,2);
      }while(ball_dir_y==0);
      resetBall=false;
    }
    //VERIFICACION DE LOS PUNTOS Y GOLPES 
    if(time > ball_update && gameIsRunning) {
        
        uint8_t new_x = ball_x + ball_dir_x;
        uint8_t new_y = ball_y + ball_dir_y;

        // Miramos si la pelota golpeo los horizontales 
        if(new_x == 10) //Jugador 1 recibe 1 punto
        {
            PLAYER_SCORE++;
            if(PLAYER_SCORE==MAX_SCORE)
            {
            //gameOver();
            }else
            {
            //showScore();
            }
        }
        
         // Check if we hit the vertical walls
        if(new_x == 246) //CPU Gets a Point
        {
            CPU_SCORE++;
            if(CPU_SCORE==MAX_SCORE)
            {
              gameOver();
            }else
            {
              showScore();
            }
        }

        // Check if we hit the horizontal walls.
        if(new_y == 10 || new_y == 229) {
            digitalWrite(PA_6,HIGH);
            ball_dir_y = -ball_dir_y;
            new_y += ball_dir_y + ball_dir_y;
            digitalWrite(PA_6,LOW);
            
        }

        // Check if we hit the CPU paddle
       /* if(new_x == CPU_X && new_y >= cpu_y && new_y <= cpu_y + PADDLE_HEIGHT) {
            ball_dir_x = -ball_dir_x;
            new_x += ball_dir_x + ball_dir_x;
        }*/
        if(new_x == CPU_X
           && new_y >= cpu_y
           && new_y <= cpu_y + PADDLE_HEIGHT)
        {
            digitalWrite(PE_3,HIGH);
            ball_dir_x = -ball_dir_x;
            new_x += ball_dir_x + ball_dir_x;
            digitalWrite(PE_3,HIGH);
        }

        // Check if we hit the player paddle
        if(new_x == PLAYER_X
           && new_y >= player_y
           && new_y <= player_y + PADDLE_HEIGHT)
        {
            digitalWrite(PE_3,HIGH);
            ball_dir_x = -ball_dir_x;
            new_x += ball_dir_x + ball_dir_x;
            digitalWrite(PE_3,LOW);
            
        }
        //FillRect(unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned int c);
        FillRect(ball_x, ball_y,2,2,0x00);
        FillRect(new_x, new_y,2,2,YELLOW);
        ball_x = new_x;
        ball_y = new_y;

        ball_update += BALL_RATE;
    }
    //CREACION DE LOS PADDLES Y RANGOS 
    if(time > paddle_update && gameIsRunning) {
        paddle_update += PADDLE_RATE;

        // CPU paddle
        V_line(CPU_X, cpu_y,PADDLE_HEIGHT, BLACK);
        const uint8_t half_paddle = PADDLE_HEIGHT >> 1;
        
        /*if(cpu_y + half_paddle > ball_y) {
            cpu_y -= 1;
        }
        if(cpu_y + half_paddle < ball_y) {
            cpu_y += 1;
        }
        if(cpu_y < 12) cpu_y = 12;
        if(cpu_y + PADDLE_HEIGHT > 218) cpu_y = 218 - PADDLE_HEIGHT;
        V_line(CPU_X, cpu_y, PADDLE_HEIGHT,BLACK);
*/
        if(up_state2) {
          cpu_y -=1;
        }
        if(down_state2) {
          cpu_y +=1;
        }
        up_state2 = down_state2 = false;
        if(cpu_y < 12) cpu_y = 12;
        if(cpu_y + PADDLE_HEIGHT > 218) cpu_y = 218 - PADDLE_HEIGHT;
        V_line(CPU_X, cpu_y, PADDLE_HEIGHT,BLACK);  
              
        // Player paddle
        V_line(PLAYER_X, player_y,PADDLE_HEIGHT, BLACK);
        if(up_state) {
            player_y -= 1;
        }
        if(down_state) {
            player_y += 1;
        }
        up_state = down_state = false;
        if(player_y < 12) player_y = 12;
        if(player_y + PADDLE_HEIGHT > 218) player_y = 218 - PADDLE_HEIGHT;
        V_line(PLAYER_X, player_y, PADDLE_HEIGHT,BLACK);
    }
  //GRAFICOS DE LAS RAQUETAS 
  LCD_Bitmap(15,cpu_y,35,35,raqueta);
  LCD_Sprite(210,player_y,35,35,raqueta2,1,0,1,0);  
}
//***************************************************************************************************************************************
// Función para el juego 
//***************************************************************************************************************************************
//DIBUJO DE RECTANGULO 
void drawCourt() {
  Rect(10,10,240,220,0xffff);  
}
//JUEGO TERMINADO UN GANADOR
void gameOver()
{
  gameIsRunning = false;
  LCD_Clear(0x00);
  delay(100);
  if(PLAYER_SCORE>CPU_SCORE)
  {
      String text1 = "Ganaste!";
    LCD_Print(text1,140,90,1,0xFFFF,0x00);
  }else
  {
    String text2 = "No Ganaste!";
    LCD_Print(text2,140,90,1,0xFFFF,0x00);
  }
  

  LCD_Print(String(CPU_SCORE),150,100,1,0xFFFF,0x00);
  LCD_Print(String(PLAYER_SCORE),170,100,1,0xFFFF,0x00);
  delay(100);
  
  while(digitalRead(UP_BUTTON) == HIGH && digitalRead(DOWN_BUTTON) == HIGH)  
  {
    delay(100);
  }
  gameIsRunning = true;
  
  CPU_SCORE = PLAYER_SCORE = 0;
  
unsigned long start = millis();
LCD_Clear(0x00);
while(millis() - start < 2000);
ball_update = millis();    
paddle_update = ball_update;
gameIsRunning = true;
resetBall=true;

}
//MARCADOR ENTRE CADA PUNTO 
void showScore()
{
  gameIsRunning = false;
  LCD_Clear(0x00);
  delay(100);
  
  unsigned long start = millis();
  drawCourt();
  String text3 = "Marcador";
  LCD_Print(text3,110,100,1,0xFFFF,0x00);
  LCD_Print(String(CPU_SCORE),110,115,1,0xFFFF,0x00);
  LCD_Print(String(PLAYER_SCORE),130,115,1,0xFFFF,0x00);

            

while(millis() - start < 2000);
ball_update = millis();    
paddle_update = ball_update;
gameIsRunning = true;
resetBall=true;
LCD_Clear(0x00);
delay(50);
Rect(10,10,240,220,0xffff);
uint8_t cpu_y = 100;
uint8_t player_y = 100;

}
//***************************************************************************************************************************************
// Función para inicializar LCD
//***************************************************************************************************************************************
void LCD_Init(void) {
  pinMode(LCD_RST, OUTPUT);
  pinMode(LCD_CS, OUTPUT);
  pinMode(LCD_RS, OUTPUT);
  pinMode(LCD_WR, OUTPUT);
  pinMode(LCD_RD, OUTPUT);
  for (uint8_t i = 0; i < 8; i++){
    pinMode(DPINS[i], OUTPUT);
  }
  //****************************************
  // Secuencia de Inicialización
  //****************************************
  digitalWrite(LCD_CS, HIGH);
  digitalWrite(LCD_RS, HIGH);
  digitalWrite(LCD_WR, HIGH);
  digitalWrite(LCD_RD, HIGH);
  digitalWrite(LCD_RST, HIGH);
  delay(5);
  digitalWrite(LCD_RST, LOW);
  delay(20);
  digitalWrite(LCD_RST, HIGH);
  delay(150);
  digitalWrite(LCD_CS, LOW);
  //****************************************
  LCD_CMD(0xE9);  // SETPANELRELATED
  LCD_DATA(0x20);
  //****************************************
  LCD_CMD(0x11); // Exit Sleep SLEEP OUT (SLPOUT)
  delay(100);
  //****************************************
  LCD_CMD(0xD1);    // (SETVCOM)
  LCD_DATA(0x00);
  LCD_DATA(0x71);
  LCD_DATA(0x19);
  //****************************************
  LCD_CMD(0xD0);   // (SETPOWER) 
  LCD_DATA(0x07);
  LCD_DATA(0x01);
  LCD_DATA(0x08);
  //****************************************
  LCD_CMD(0x36);  // (MEMORYACCESS)
  LCD_DATA(0x40|0x80|0x20|0x08); // LCD_DATA(0x19);
  //****************************************
  LCD_CMD(0x3A); // Set_pixel_format (PIXELFORMAT)
  LCD_DATA(0x05); // color setings, 05h - 16bit pixel, 11h - 3bit pixel
  //****************************************
  LCD_CMD(0xC1);    // (POWERCONTROL2)
  LCD_DATA(0x10);
  LCD_DATA(0x10);
  LCD_DATA(0x02);
  LCD_DATA(0x02);
  //****************************************
  LCD_CMD(0xC0); // Set Default Gamma (POWERCONTROL1)
  LCD_DATA(0x00);
  LCD_DATA(0x35);
  LCD_DATA(0x00);
  LCD_DATA(0x00);
  LCD_DATA(0x01);
  LCD_DATA(0x02);
  //****************************************
  LCD_CMD(0xC5); // Set Frame Rate (VCOMCONTROL1)
  LCD_DATA(0x04); // 72Hz
  //****************************************
  LCD_CMD(0xD2); // Power Settings  (SETPWRNORMAL)
  LCD_DATA(0x01);
  LCD_DATA(0x44);
  //****************************************
  LCD_CMD(0xC8); //Set Gamma  (GAMMASET)
  LCD_DATA(0x04);
  LCD_DATA(0x67);
  LCD_DATA(0x35);
  LCD_DATA(0x04);
  LCD_DATA(0x08);
  LCD_DATA(0x06);
  LCD_DATA(0x24);
  LCD_DATA(0x01);
  LCD_DATA(0x37);
  LCD_DATA(0x40);
  LCD_DATA(0x03);
  LCD_DATA(0x10);
  LCD_DATA(0x08);
  LCD_DATA(0x80);
  LCD_DATA(0x00);
  //****************************************
  LCD_CMD(0x2A); // Set_column_address 320px (CASET)
  LCD_DATA(0x00);
  LCD_DATA(0x00);
  LCD_DATA(0x01);
  LCD_DATA(0x3F);
  //****************************************
  LCD_CMD(0x2B); // Set_page_address 480px (PASET)
  LCD_DATA(0x00);
  LCD_DATA(0x00);
  LCD_DATA(0x01);
  LCD_DATA(0xE0);
//  LCD_DATA(0x8F);
  LCD_CMD(0x29); //display on 
  LCD_CMD(0x2C); //display on

  LCD_CMD(ILI9341_INVOFF); //Invert Off
  delay(120);
  LCD_CMD(ILI9341_SLPOUT);    //Exit Sleep
  delay(120);
  LCD_CMD(ILI9341_DISPON);    //Display on
  digitalWrite(LCD_CS, HIGH);
}
//***************************************************************************************************************************************
// Función para enviar comandos a la LCD - parámetro (comando)
//***************************************************************************************************************************************
void LCD_CMD(uint8_t cmd) {
  digitalWrite(LCD_RS, LOW);
  digitalWrite(LCD_WR, LOW);
  GPIO_PORTB_DATA_R = cmd;
  digitalWrite(LCD_WR, HIGH);
}
//***************************************************************************************************************************************
// Función para enviar datos a la LCD - parámetro (dato)
//***************************************************************************************************************************************
void LCD_DATA(uint8_t data) {
  digitalWrite(LCD_RS, HIGH);
  digitalWrite(LCD_WR, LOW);
  GPIO_PORTB_DATA_R = data;
  digitalWrite(LCD_WR, HIGH);
}
//***************************************************************************************************************************************
// Función para definir rango de direcciones de memoria con las cuales se trabajara (se define una ventana)
//***************************************************************************************************************************************
void SetWindows(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2) {
  LCD_CMD(0x2a); // Set_column_address 4 parameters
  LCD_DATA(x1 >> 8);
  LCD_DATA(x1);   
  LCD_DATA(x2 >> 8);
  LCD_DATA(x2);   
  LCD_CMD(0x2b); // Set_page_address 4 parameters
  LCD_DATA(y1 >> 8);
  LCD_DATA(y1);   
  LCD_DATA(y2 >> 8);
  LCD_DATA(y2);   
  LCD_CMD(0x2c); // Write_memory_start
}
//***************************************************************************************************************************************
// Función para borrar la pantalla - parámetros (color)
//***************************************************************************************************************************************
void LCD_Clear(unsigned int c){  
  unsigned int x, y;
  LCD_CMD(0x02c); // write_memory_start
  digitalWrite(LCD_RS, HIGH);
  digitalWrite(LCD_CS, LOW);   
  SetWindows(0, 0, 319, 239); // 479, 319);
  for (x = 0; x < 320; x++)
    for (y = 0; y < 240; y++) {
      LCD_DATA(c >> 8); 
      LCD_DATA(c); 
    }
  digitalWrite(LCD_CS, HIGH);
} 
//***************************************************************************************************************************************
// Función para dibujar una línea horizontal - parámetros ( coordenada x, cordenada y, longitud, color)
//*************************************************************************************************************************************** 
void H_line(unsigned int x, unsigned int y, unsigned int l, unsigned int c) {  
  unsigned int i, j;
  LCD_CMD(0x02c); //write_memory_start
  digitalWrite(LCD_RS, HIGH);
  digitalWrite(LCD_CS, LOW);
  l = l + x;
  SetWindows(x, y, l, y);
  j = l;// * 2;
  for (i = 0; i < l; i++) {
      LCD_DATA(c >> 8); 
      LCD_DATA(c); 
  }
  digitalWrite(LCD_CS, HIGH);
}
//***************************************************************************************************************************************
// Función para dibujar una línea vertical - parámetros ( coordenada x, cordenada y, longitud, color)
//*************************************************************************************************************************************** 
void V_line(unsigned int x, unsigned int y, unsigned int l, unsigned int c) {  
  unsigned int i,j;
  LCD_CMD(0x02c); //write_memory_start
  digitalWrite(LCD_RS, HIGH);
  digitalWrite(LCD_CS, LOW);
  l = l + y;
  SetWindows(x, y, x, l);
  j = l; //* 2;
  for (i = 1; i <= j; i++) {
    LCD_DATA(c >> 8); 
    LCD_DATA(c);
  }
  digitalWrite(LCD_CS, HIGH);  
}
//***************************************************************************************************************************************
// Función para dibujar un rectángulo - parámetros ( coordenada x, cordenada y, ancho, alto, color)
//***************************************************************************************************************************************
void Rect(unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned int c) {
  H_line(x  , y  , w, c);
  H_line(x  , y+h, w, c);
  V_line(x  , y  , h, c);
  V_line(x+w, y  , h, c);
}
//***************************************************************************************************************************************
// Función para dibujar un rectángulo relleno - parámetros ( coordenada x, cordenada y, ancho, alto, color)
//***************************************************************************************************************************************
void FillRect(unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned int c) {
  unsigned int i;
  for (i = 0; i < h; i++) {
    H_line(x  , y  , w, c);
    H_line(x  , y+i, w, c);
  }
}
//***************************************************************************************************************************************
// Función para dibujar texto - parámetros ( texto, coordenada x, cordenada y, color, background) 
//***************************************************************************************************************************************
void LCD_Print(String text, int x, int y, int fontSize, int color, int background) {
  int fontXSize ;
  int fontYSize ;
  
  if(fontSize == 1){
    fontXSize = fontXSizeSmal ;
    fontYSize = fontYSizeSmal ;
  }
  if(fontSize == 2){
    fontXSize = fontXSizeBig ;
    fontYSize = fontYSizeBig ;
  }
  
  char charInput ;
  int cLength = text.length();
  Serial.println(cLength,DEC);
  int charDec ;
  int c ;
  int charHex ;
  char char_array[cLength+1];
  text.toCharArray(char_array, cLength+1) ;
  for (int i = 0; i < cLength ; i++) {
    charInput = char_array[i];
    Serial.println(char_array[i]);
    charDec = int(charInput);
    digitalWrite(LCD_CS, LOW);
    SetWindows(x + (i * fontXSize), y, x + (i * fontXSize) + fontXSize - 1, y + fontYSize );
    long charHex1 ;
    for ( int n = 0 ; n < fontYSize ; n++ ) {
      if (fontSize == 1){
        charHex1 = pgm_read_word_near(smallFont + ((charDec - 32) * fontYSize) + n);
      }
      if (fontSize == 2){
        charHex1 = pgm_read_word_near(bigFont + ((charDec - 32) * fontYSize) + n);
      }
      for (int t = 1; t < fontXSize + 1 ; t++) {
        if (( charHex1 & (1 << (fontXSize - t))) > 0 ) {
          c = color ;
        } else {
          c = background ;
        }
        LCD_DATA(c >> 8);
        LCD_DATA(c);
      }
    }
    digitalWrite(LCD_CS, HIGH);
  }
}
//***************************************************************************************************************************************
// Función para dibujar una imagen a partir de un arreglo de colores (Bitmap) Formato (Color 16bit R 5bits G 6bits B 5bits)
//***************************************************************************************************************************************
void LCD_Bitmap(unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned char bitmap[]){  
  LCD_CMD(0x02c); // write_memory_start
  digitalWrite(LCD_RS, HIGH);
  digitalWrite(LCD_CS, LOW); 
  
  unsigned int x2, y2;
  x2 = x+width;
  y2 = y+height;
  SetWindows(x, y, x2-1, y2-1);
  unsigned int k = 0;
  unsigned int i, j;

  for (int i = 0; i < width; i++) {
    for (int j = 0; j < height; j++) {
      LCD_DATA(bitmap[k]);
      LCD_DATA(bitmap[k+1]);
      //LCD_DATA(bitmap[k]);    
      k = k + 2;
     } 
  }
  digitalWrite(LCD_CS, HIGH);
}
//***************************************************************************************************************************************
// Función para dibujar una imagen sprite - los parámetros columns = número de imagenes en el sprite, index = cual desplegar, flip = darle vuelta
//***************************************************************************************************************************************
void LCD_Sprite(int x, int y, int width, int height, unsigned char bitmap[],int columns, int index, char flip, char offset){
  LCD_CMD(0x02c); // write_memory_start
  digitalWrite(LCD_RS, HIGH);
  digitalWrite(LCD_CS, LOW); 

  unsigned int x2, y2;
  x2 =   x+width;
  y2=    y+height;
  SetWindows(x, y, x2-1, y2-1);
  int k = 0;
  int ancho = ((width*columns));
  if(flip){
  for (int j = 0; j < height; j++){
      k = (j*(ancho) + index*width -1 - offset)*2;
      k = k+width*2;
     for (int i = 0; i < width; i++){
      LCD_DATA(bitmap[k]);
      LCD_DATA(bitmap[k+1]);
      k = k - 2;
     } 
  }
  }else{
     for (int j = 0; j < height; j++){
      k = (j*(ancho) + index*width + 1 + offset)*2;
     for (int i = 0; i < width; i++){
      LCD_DATA(bitmap[k]);
      LCD_DATA(bitmap[k+1]);
      k = k + 2;
     } 
  }
    
    
    }
  digitalWrite(LCD_CS, HIGH);
}
