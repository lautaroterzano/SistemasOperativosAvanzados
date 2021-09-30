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
#define UMBRAL_TIMEOUT 500
//Sensores
#define MAX_CANT_SENSORES 3
#define SENSOR_PIR 1
#define SENSOR_TEMP 2
#define SENSOR_INTERRUPTOR 3
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
#define MAX_COLOR 255
#define MIN_COLOR 0
#define POSICION_INICIAL 0
//----------------------------------------------

#define TIMER_EN_USO 1
#define TIMER_EN_DESUSO 0

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
int contadorLeds = 0;
int redColor, greenColor, blueColor;
int timer_motor;
bool imprimir;
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

Adafruit_NeoPixel pixeles = Adafruit_NeoPixel(NUMERO_PIXELES, PIN_LEDS, NEO_GRB + NEO_KHZ800);

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
    if (persona == HIGH)
    {
        evento.tipo = TIPO_EVENTO_PERSONA_DETECTADA;
        evento.valor_lectura = SENSOR_PIR;
        return true;
    }
    return false;
}

bool verificarTimeoutMotor()
{
    tiempo_ahora = millis();
    if ((tiempo_ahora - tiempo_antes) >= UMBRAL_TIMEOUT_ATRAYENDO)
    {
        evento.tipo = TIPO_EVENTO_TIMEOUT;
        return true;
    }
    else
    {
        evento.tipo = TIPO_EVENTO_ATRAER;
        return false;
    }
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
            evento.tipo = TIPO_EVENTO_SALA_LLENA;
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
            evento.tipo = TIPO_EVENTO_LIBERAR_SALA;
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
    timer_motor = TIMER_EN_USO;
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
    timer_motor = TIMER_EN_DESUSO;
}
void setColor()
{
    redColor = random(MIN_COLOR, MAX_COLOR);
    greenColor = random(MIN_COLOR, MAX_COLOR);
    blueColor = random(MIN_COLOR, MAX_COLOR);
}

void ledsFuncionando()
{
    if ((tiempo_ahora - tiempo_antes) < UMBRAL_TIMEOUT_ATRAYENDO)
    {
        setColor();
        pixeles.setPixelColor(contadorLeds, pixeles.Color(redColor, greenColor, blueColor));
        setColor();
        pixeles.setPixelColor(NUMERO_PIXELES - contadorLeds, pixeles.Color(redColor, greenColor, blueColor));
        contadorLeds++;
        pixeles.show();
    }
}

void apagarLeds()
{
    contadorLeds = 0;
    pixeles.clear();
    pixeles.show();
}
void mostrarMensajeLcd()
{

    int x = POSICION_INICIAL;
    int y = POSICION_INICIAL;

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

    if (timer_motor == TIMER_EN_USO)
    {
        verificarTimeoutMotor();
        return;
    }
    else
    {

        if (timeout)
        {
            timeout = false;
            lct = ct;
            if (verificarEstadoSensorPir() || verificarEstadoSensorTemperatura() || verificarInterruptor())
                return;
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

            timer_motor = TIMER_EN_DESUSO;
            imprimir = true;

            estado = ESTADO_DETECTANDO;
        }
        break;
        case TIPO_EVENTO_LIBERAR_SALA:
        {
            estado = ESTADO_INICIO;
        }break;
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
            if (imprimir)
            {
                DebugPrintEstado(estados[estado], eventos[evento.tipo], evento.valor_lectura);
                imprimir = false;
            }
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

            imprimir = true;

            estado = ESTADO_HIPNOTIZANDO;
        }
        break;
        case TIPO_EVENTO_ATRAER:
        {
            estado = ESTADO_DETECTANDO;
        }
        break;
        case TIPO_EVENTO_LIBERAR_SALA:
        {
            estado = ESTADO_DETECTANDO;
        }
        break;
        case TIPO_EVENTO_TIMEOUT:
        {
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

    case ESTADO_ATRAYENDO:
    {
        switch (evento.tipo)
        {
        case TIPO_EVENTO_CONTINUE:
        {
            estado = ESTADO_ATRAYENDO;
        }
        break;
        case TIPO_EVENTO_LIBERAR_SALA:
        {
            estado = ESTADO_ATRAYENDO;
        }
        break;
        case TIPO_EVENTO_ATRAER:
        {
            DebugPrintEstado(estados[estado], eventos[evento.tipo], evento.valor_lectura);

            motorFuncionando();
            ledsFuncionando();

            estado = ESTADO_ATRAYENDO;
        }
        break;
        case TIPO_EVENTO_TIMEOUT:
        {
            DebugPrintEstado(estados[estado], eventos[evento.tipo], evento.valor_lectura);

            apagarMotor();
            apagarLeds();

            imprimir = true;

            estado = ESTADO_DETECTANDO;
        }
        break;
        case TIPO_EVENTO_PERSONA_DETECTADA:
        {
            estado = ESTADO_ATRAYENDO;
        }
        break;
        case TIPO_EVENTO_SALA_LLENA:
        {
            estado = ESTADO_ATRAYENDO;
        }
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
        {
            DebugPrintEstado(estados[estado], eventos[evento.tipo], evento.valor_lectura);
            estado = ESTADO_INICIO;
            imprimir = true;
        }
        break;
        case TIPO_EVENTO_CONTINUE:
        {
            if (imprimir)
            {
                DebugPrintEstado(estados[estado], eventos[evento.tipo], evento.valor_lectura);
                imprimir = false;
            }
            estado = ESTADO_HIPNOTIZANDO;
        }
        break;
        case TIPO_EVENTO_TIMEOUT:
        {
            estado = ESTADO_HIPNOTIZANDO;
        }
        break;
        case TIPO_EVENTO_PERSONA_DETECTADA:
        {
            estado = ESTADO_HIPNOTIZANDO;
        }
        break;
        case TIPO_EVENTO_ATRAER:
        {
            estado = ESTADO_HIPNOTIZANDO;
        }
        break;
        case TIPO_EVENTO_SALA_LLENA:
        {
            estado = ESTADO_HIPNOTIZANDO;
        }
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
            estado = ESTADO_INICIO;
            break;
        }
    }
    break;
    }
    evento.tipo = TIPO_EVENTO_CONTINUE;
    evento.valor_lectura = LECTURA_EVENTO_CONTINUE;
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