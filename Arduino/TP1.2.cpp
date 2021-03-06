#include <LiquidCrystal.h>
#include <Adafruit_NeoPixel.h>

//Habilitacion de debug para la impresion por el puerto serial
//----------------------------------------------
#define SERIAL_DEBUG_ENABLED 1

#if SERIAL_DEBUG_ENABLED
#define DebugPrint(str)      \
    {                        \
        Serial.println(str); \
    }
#else
#define DebugPrint(str)
#endif

#define DebugPrintEstado(estado, tipo, valor_lectura)                                     \
    {                                                                                     \
        String est = estado;                                                              \
        String tp = tipo;                                                                 \
        int val = valor_lectura;                                                          \
        String str;                                                                       \
        str = "-----------------------------------------------------";                    \
        DebugPrint(str);                                                                  \
        str = "EST-> [" + est + "]: " + "EVT-> [" + tp + "] " + "VALOR-> [" + val + "]."; \
        DebugPrint(str);                                                                  \
        str = "-----------------------------------------------------";                    \
        DebugPrint(str);                                                                  \
    }
//----------------------------------------------

//----------------------------------------------
// Tipos de eventos disponibles .
#define TIPO_EVENTO_TIMEOUT 0
#define TIPO_EVENTO_CONTINUE 1
#define TIPO_EVENTO_PERSONA_DETECTADA 2
#define TIPO_EVENTO_ATRAER 3
#define TIPO_EVENTO_SALA_LLENA 4
#define TIPO_EVENTO_LIBERAR_SALA 5
#define TIPO_EVENTO_DESCONOCIDO 6
//----------------------------------------------

//----------------------------------------------
// Estado del embeded ...
#define ESTADO_INICIO 0
#define ESTADO_DETECTANDO 1
#define ESTADO_ATRAYENDO 2
#define ESTADO_HIPNOTIZANDO 3
#define ESTADO_ERROR 4
//----------------------------------------------

//----------------------------------------------
// Estado de un sensor ...
#define ESTADO_SENSOR_OK 108
#define ESTADO_SENSOR_ERROR 666
//----------------------------------------------

// Otras constantes ....
//----------------------------------------------
//Umbrales
#define UMBRAL_TIMEOUT_ATRAYENDO 1000
#define UMBRAL_TIMEOUT_DETECTANDO 5000
#define UMBRAL_SENSOR_TEMPERATURA 55
#define UMBRAL_TIMEOUT 30
//Sensores
#define MAX_CANT_SENSORES 3
#define SENSOR_PIR 0
#define SENSOR_TEMP 1
#define SENSOR_INTERRUPTOR 2
#define SENSOR_EVENTO_CONTINUE -1
#define LECTURA_EVENTO_CONTINUE -1
//Pines
#define PIN_SENSOR_PIR 3
#define PIN_LEDS 4
#define PIN_MOTOR 5
#define PIN_SENSOR_TEMP A0
#define PIN_INTERRUPTOR 2
#define PIN_LCD_RS 13
#define PIN_LCD_E 12
#define PIN_LCD_DB4 A2
#define PIN_LCD_DB5 A3
#define PIN_LCD_DB6 A4
#define PIN_LCD_DB7 A5
//Constantes
#define EQUIVALENCIA 100
#define VALOR_BASE_TMP 50.0
#define LECTURAMIN_TEMP 50
#define LECTURAMAX_TEMP 1024
#define VOLT 5.0
#define CM 0.01723
#define MOTOR_APAGADO 0
#define MOTOR_MIN 0
#define MOTOR_MAX 255
#define DIVISOR_MOTOR 128
#define SERIAL 9600
#define COL 16
#define FIL 2
#define NUMERO_PIXELES 12
//----------------------------------------------

//----------------------------------------------
struct stSensor
{
    int pin;
    int estado;
    int valorPir;
    long valor_actual;
    long valor_previo;
};
stSensor sensores[MAX_CANT_SENSORES];
//----------------------------------------------

// Variables globales
//----------------------------------------------
int deteccion_pir;
int estado;
unsigned long tiempo_antes, tiempo_ahora;
long lct;
bool timeout;
String mensaje = "Vote por Cerebro";
int i = 0;
int redColor, greenColor, blueColor;
//----------------------------------------------

//----------------------------------------------
struct stEvento
{
    int tipo;
    int valor_lectura;
};
stEvento evento;
//----------------------------------------------

String estados[] = {"ESTADO_INICIO", "ESTADO_DETECTANDO", "ESTADO_ATRAYENDO", "ESTADO_HIPNOTIZANDO", "ERROR"};
String eventos[] = {"EVENTO_TIMEOUT", "EVENTO_CONTINUE", "EVENTO_SENSOR_PERSONA_DETECTADA", "EVENTO_ATRAER", "EVENTO_SALA_LLENA", "EVENTO_LIBERAR_SALA", "EVENTO_DESCONOCIDO"};

//----------------------------------------------

LiquidCrystal lcd(PIN_LCD_RS, PIN_LCD_E, PIN_LCD_DB4, PIN_LCD_DB5, PIN_LCD_DB6, PIN_LCD_DB7);

Adafruit_NeoPixel pixeles = Adafruit_NeoPixel(12, 4, NEO_GRB + NEO_KHZ800); // 2 es el pin

//-----------------------------------------------

void inicializar()
{
    Serial.begin(SERIAL);

    pinMode(PIN_SENSOR_PIR, INPUT);
    pinMode(PIN_INTERRUPTOR, INPUT);
    pinMode(PIN_SENSOR_TEMP, INPUT);

    pinMode(PIN_MOTOR, OUTPUT);
    pinMode(PIN_LEDS, OUTPUT);

    sensores[SENSOR_PIR].pin = PIN_SENSOR_PIR;
    sensores[SENSOR_PIR].estado = ESTADO_SENSOR_OK;
    sensores[SENSOR_TEMP].pin = PIN_SENSOR_TEMP;
    sensores[SENSOR_TEMP].estado = ESTADO_SENSOR_OK;
    sensores[SENSOR_INTERRUPTOR].pin = PIN_INTERRUPTOR;
    sensores[SENSOR_INTERRUPTOR].estado = ESTADO_SENSOR_OK;

    lcd.begin(FIL, COL);
    pixeles.begin();

    estado = ESTADO_INICIO;

    timeout = false;
    lct = millis();
}

// Verificacion de sensores
//-----------------------------------------------
int leerSensorInterruptor()
{
    return digitalRead(PIN_INTERRUPTOR);
}

long leerSensorTemperatura()
{
    int lectura = analogRead(PIN_SENSOR_TEMP);
    float voltaje = (VOLT / LECTURAMAX_TEMP) * lectura;
    float temp = (voltaje * EQUIVALENCIA) - LECTURAMIN_TEMP;

    return temp;
}

bool verificarEstadoSensorPir()
{
    int persona = digitalRead(PIN_SENSOR_PIR);
    return persona == HIGH;
}

bool verificarTimeoutMotor()
{
    tiempo_ahora = millis();
    return (tiempo_ahora - tiempo_antes) >= UMBRAL_TIMEOUT_ATRAYENDO;
}

bool verificarEstadoSensorTemperatura()
{
    sensores[SENSOR_TEMP].valor_actual = leerSensorTemperatura();

    int valor_actual = sensores[SENSOR_TEMP].valor_actual;
    int valor_previo = sensores[SENSOR_TEMP].valor_previo;

    if (valor_actual != valor_previo)
    {
        sensores[SENSOR_TEMP].valor_previo = valor_actual;

        if (valor_actual > UMBRAL_SENSOR_TEMPERATURA)
        {
            evento.valor_lectura = valor_actual;
            return true;
        }
    }

    return false;
}

bool verificarInterruptor()
{
    sensores[SENSOR_INTERRUPTOR].valor_actual = leerSensorInterruptor();

    int valor_actual = sensores[SENSOR_INTERRUPTOR].valor_actual;
    int valor_previo = sensores[SENSOR_INTERRUPTOR].valor_previo;

    if (valor_actual != valor_previo)
    {
        sensores[SENSOR_INTERRUPTOR].valor_previo = valor_actual;

        if (valor_actual == HIGH)
        {
            evento.valor_lectura = valor_actual;
            return true;
        }
    }
    return false;
}

//-----------------------------------------------

// Funciones de actuadores
//-----------------------------------------------
void iniciarTimer()
{
    cli();
    tiempo_antes = millis();
}

void motorFuncionando()
{
    if ((tiempo_ahora - tiempo_antes) < UMBRAL_TIMEOUT_ATRAYENDO)
    {
        analogWrite(PIN_MOTOR, map((tiempo_ahora - tiempo_antes), MOTOR_MIN, UMBRAL_TIMEOUT_ATRAYENDO, MOTOR_MIN, MOTOR_MAX));
    }
}

void apagarMotor()
{
    analogWrite(PIN_MOTOR, MOTOR_APAGADO);
}
void setColor()
{
    redColor = random(0, 255);
    greenColor = random(0, 255);
    blueColor = random(0, 255);
}

void ledsFuncionando()
{
    if ((tiempo_ahora - tiempo_antes) < UMBRAL_TIMEOUT_ATRAYENDO)
    {
 
        setColor();
        pixeles.setPixelColor(i, pixeles.Color(redColor, greenColor, blueColor));
       	
      	setColor();
        pixeles.setPixelColor(NUMERO_PIXELES - i, pixeles.Color(redColor, greenColor, blueColor));
      	
      	i++;
        pixeles.show();
    }
}

void apagarLeds()
{
    i = 0;
    pixeles.clear();
    pixeles.show();
}
void mostrarMensajeLcd()
{

    int x = 0;
    int y = 0;

    lcd.setCursor(x, y);
    lcd.print(mensaje);
}

//-----------------------------------------------

void generarEvento()
{
    long ct = millis();
    int diferencia = (ct - lct);

    if (diferencia > UMBRAL_TIMEOUT)
        timeout = true;
    else
        timeout = false;

    if (estado == ESTADO_ATRAYENDO)
    {
        if (verificarTimeoutMotor())
        {
            evento.tipo = TIPO_EVENTO_TIMEOUT;
            return;
        }
        else
        {
            evento.tipo = TIPO_EVENTO_ATRAER;
            return;
        }
    }
    else
    {
        if (timeout)
        {
            timeout = false;
            lct = ct;

            if(estado == ESTADO_HIPNOTIZANDO)
            {
                if (verificarInterruptor())
                {
                    evento.tipo = TIPO_EVENTO_LIBERAR_SALA;
                    return;
                }
            }else
            {
                if (verificarEstadoSensorPir())
                {
                    evento.tipo = TIPO_EVENTO_PERSONA_DETECTADA;
                    return;
                }
                if (verificarEstadoSensorTemperatura())
                {
                    evento.tipo = TIPO_EVENTO_SALA_LLENA;
                    return;
                }
            } 
        }
    }
    evento.tipo = TIPO_EVENTO_CONTINUE;
}

void maquina_estados()
{
    generarEvento();

    switch (estado)
    {
    case ESTADO_INICIO:
    {
        switch (evento.tipo)
        {
        case TIPO_EVENTO_CONTINUE:
        {
            DebugPrintEstado(estados[estado], eventos[evento.tipo], evento.valor_lectura);

            apagarMotor();
            apagarLeds();
            lcd.clear();

            estado = ESTADO_DETECTANDO;
        }
        break;
        default:
        {
            DebugPrintEstado(estados[estado], eventos[evento.tipo], evento.valor_lectura);
            evento.tipo = TIPO_EVENTO_DESCONOCIDO;
            estado = ESTADO_ERROR;
        }
        break;
        }
    }
    break;

    case ESTADO_DETECTANDO:
    {
        switch (evento.tipo)
        {
        case TIPO_EVENTO_CONTINUE:
        {
            DebugPrintEstado(estados[estado], eventos[evento.tipo], evento.valor_lectura);

            estado = ESTADO_DETECTANDO;
        }
        break;
        case TIPO_EVENTO_PERSONA_DETECTADA:
        {
            DebugPrintEstado(estados[estado], eventos[evento.tipo], evento.valor_lectura);

            iniciarTimer();

            estado = ESTADO_ATRAYENDO;
        }
        break;
        case TIPO_EVENTO_SALA_LLENA:
        {
            DebugPrintEstado(estados[estado], eventos[evento.tipo], evento.valor_lectura);

            mostrarMensajeLcd();
            estado = ESTADO_HIPNOTIZANDO;
        }
        break;
        default:
        {
            DebugPrintEstado(estados[estado], eventos[evento.tipo], evento.valor_lectura);
            evento.tipo = TIPO_EVENTO_DESCONOCIDO;
            estado = ESTADO_ERROR;
        }
        break;
        }
    }
    break;

    case ESTADO_ATRAYENDO:
    {
        switch (evento.tipo)
        {
        case TIPO_EVENTO_ATRAER:

            DebugPrintEstado(estados[estado], eventos[evento.tipo], evento.valor_lectura);
            motorFuncionando();
            ledsFuncionando();

            estado = ESTADO_ATRAYENDO;
            break;
        case TIPO_EVENTO_TIMEOUT:

            DebugPrintEstado(estados[estado], eventos[evento.tipo], evento.valor_lectura);
            apagarMotor();
            apagarLeds();
            estado = ESTADO_DETECTANDO;
            break;
        default:
            DebugPrintEstado(estados[estado], eventos[evento.tipo], evento.valor_lectura);
            evento.tipo = TIPO_EVENTO_DESCONOCIDO;
            estado = ESTADO_ERROR;
            break;
        }
    }
    break;

    case ESTADO_HIPNOTIZANDO:
    {
        switch (evento.tipo)
        {
        case TIPO_EVENTO_LIBERAR_SALA:
            DebugPrintEstado(estados[estado], eventos[evento.tipo], evento.valor_lectura);
            estado = ESTADO_INICIO;
            break;
        case TIPO_EVENTO_CONTINUE:
            DebugPrintEstado(estados[estado], eventos[evento.tipo], evento.valor_lectura);
            estado = ESTADO_HIPNOTIZANDO;
            break;
        default:
            DebugPrintEstado(estados[estado], eventos[evento.tipo], evento.valor_lectura);
            evento.tipo = TIPO_EVENTO_DESCONOCIDO;
            estado = ESTADO_ERROR;
            break;
        }
    }
    break;

    case ESTADO_ERROR:
    {
        switch (evento.tipo)
        {
        case TIPO_EVENTO_DESCONOCIDO:
            DebugPrintEstado(estados[estado], eventos[evento.tipo], evento.valor_lectura);
            estado = ESTADO_ERROR;
            break;
        }
    }
    break;
    }
    evento.tipo = TIPO_EVENTO_CONTINUE;
}

// Arduino
//----------------------------------------------
void setup()
{
    inicializar();
}

void loop()
{
    maquina_estados();
}
