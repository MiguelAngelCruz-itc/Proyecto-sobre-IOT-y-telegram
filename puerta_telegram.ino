// Bot de Telegram para control de puerta corredera + Control de Luz por Pulsador Físico / Telegram
// Motor lento (1 vuelta en 2s), Final de Carrera FDC1 y Pulsador Físico para Bombillo

template<class T> inline Print &operator <<(Print &obj, T arg) {
  obj.print(arg);
  return obj;
}

#include "CTBot.h"
CTBot miBot;

#include "token.h"   // debe definir: ssid, password, token

// ----------------- CONFIGURACION DE PINES -----------------
const int PIN_LED         = 2;   // Led de estado (conexion wifi/telegram)
const int PIN_IN1         = 5;   // L298N IN1 (motor A)
const int PIN_IN2         = 4;   // L298N IN2 (motor A)
const int PIN_ENA         = 12;  // L298N ENA (habilita motor A / velocidad por PWM)
const int PIN_FDC1        = 25;  // Final de carrera 1 -> se activa cuando la puerta llega a CERRADA
const int PIN_BOMBILLO    = 22;  // Bombillo/LED
const int PIN_BOTON_LUZ   = 19;  // Pulsador físico para encender/apagar la luz

// ----------------- CALIBRACION DE VELOCIDAD LENTA -----------------
const int VELOCIDAD_MOTOR = 40; 
const int IMPULSO_ARRANQUE = 255;
const unsigned long TIEMPO_IMPULSO_MS = 80; 

// ----------------- CONFIGURACION DE TIEMPOS -----------------
const unsigned long TIEMPO_ABRIR_MS = 15000;      
const unsigned long TIEMPO_MAX_CERRAR_MS = 25000; 
const unsigned long DEBOUNCE_MS = 50;

// ----------------- CONTROL DE BOMBILLO Y PULSADOR -----------------
bool estadoBombillo = false;
int ultimoEstadoBotonRaw = HIGH;
int estadoBotonFiltro = HIGH;
unsigned long tiempoUltimoCambioBoton = 0;

// ----------------- SEGURIDAD: usuarios autorizados -----------------
const int64_t USUARIOS_AUTORIZADOS[] = {
  8110877830,
};
const int CANTIDAD_USUARIOS = sizeof(USUARIOS_AUTORIZADOS) / sizeof(USUARIOS_AUTORIZADOS[0]);

// ----------------- ESTADOS DE LA PUERTA -----------------
enum EstadoPuerta {
  CERRADA,
  ABRIENDO,
  ABIERTA,
  CERRANDO,
  DETENIDA
};

String nombreEstado(EstadoPuerta e);

EstadoPuerta estado = CERRADA; 
unsigned long tiempoInicioMovimiento = 0;

int estadoFDC1Filtro = HIGH;
int ultimoEstadoFDC1Raw = HIGH;
unsigned long tiempoUltimoCambioFDC1 = 0;

int64_t chatIdPendiente = 0; 

void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando Bot - Puerta + Pulsador Físico para Luz");

  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_ENA, OUTPUT);
  pinMode(PIN_BOMBILLO, OUTPUT);
  
  pinMode(PIN_FDC1, INPUT_PULLUP);      // Final de carrera a GND
  pinMode(PIN_BOTON_LUZ, INPUT_PULLUP); // Pulsador a GND

  #if defined(ESP32)
    analogWriteFrequency(PIN_ENA, 1000); // Frecuencia PWM a 1kHz
  #endif

  digitalWrite(PIN_BOMBILLO, LOW);

  detenerMotor();
  digitalWrite(PIN_LED, HIGH);

  miBot.wifiConnect(ssid, password);
  miBot.setTelegramToken(token);

  if (miBot.testConnection()) {
    Serial.println("\nConectado a Telegram correctamente");
  } else {
    Serial.println("\nError de conexion con Telegram");
  }
}

void loop() {
  gestionarMensajes();
  gestionarFinCarrera();
  gestionarMovimiento();
  gestionarPulsadorLuz(); // Revisa si alguien presiona el pulsador físico
  delay(10);
}

// ----------------- TELEGRAM -----------------

void gestionarMensajes() {
  TBMessage msg;

  if (CTBotMessageText == miBot.getNewMessage(msg)) {
    Serial << "Mensaje de " << msg.sender.firstName << " (" << (long)msg.sender.id << "): " << msg.text << "\n";

    if (!usuarioAutorizado(msg.sender.id)) {
      miBot.sendMessage(msg.sender.id, "No tienes autorizacion para controlar este sistema.");
      return;
    }

    String texto = msg.text;
    texto.trim();

    // Comandos de la puerta
    if (texto.equalsIgnoreCase("/abrir") || texto.equalsIgnoreCase("abrir")) {
      solicitarAbrir(msg.sender.id);
    }
    else if (texto.equalsIgnoreCase("/cerrar") || texto.equalsIgnoreCase("cerrar")) {
      solicitarCerrar(msg.sender.id);
    }
    else if (texto.equalsIgnoreCase("/parar") || texto.equalsIgnoreCase("parar") || texto.equalsIgnoreCase("stop")) {
      solicitarParar(msg.sender.id);
    }
    else if (texto.equalsIgnoreCase("/estado") || texto.equalsIgnoreCase("estado")) {
      String mensajeEstado = "Estado puerta: " + nombreEstado(estado) + "\n";
      mensajeEstado += "Bombillo: " + String(estadoBombillo ? "ENCENDIDO" : "APAGADO");
      miBot.sendMessage(msg.sender.id, mensajeEstado);
    }
    // Comandos manuales para el bombillo desde Telegram
    else if (texto.equalsIgnoreCase("/luz_on") || texto.equalsIgnoreCase("luz on")) {
      encenderBombillo();
      miBot.sendMessage(msg.sender.id, "Bombillo encendido.");
    }
    else if (texto.equalsIgnoreCase("/luz_off") || texto.equalsIgnoreCase("luz off")) {
      apagarBombillo();
      miBot.sendMessage(msg.sender.id, "Bombillo apagado.");
    }
    else if (texto.equalsIgnoreCase("/luz") || texto.equalsIgnoreCase("luz")) {
      conmutarBombillo();
      miBot.sendMessage(msg.sender.id, "Bombillo " + String(estadoBombillo ? "encendido" : "apagado") + ".");
    }
    else if (texto.equalsIgnoreCase("/start")) {
      miBot.sendMessage(msg.sender.id,
        "Bienvenido " + msg.sender.firstName + " al control de la puerta y luces.\n\n"
        "Comandos de Puerta:\n"
        "/abrir - Abre la puerta\n"
        "/cerrar - Cierra la puerta\n"
        "/parar - Detiene la puerta\n"
        "/estado - Consulta el estado general\n\n"
        "Comandos de Iluminación:\n"
        "/luz - Cambia el estado del bombillo (ON/OFF)\n"
        "/luz_on - Enciende el bombillo\n"
        "/luz_off - Apaga el bombillo");
    }
    else {
      miBot.sendMessage(msg.sender.id, "Comando no reconocido. Usa /start para ver las opciones.");
    }
  }
}

bool usuarioAutorizado(int64_t id) {
  if (CANTIDAD_USUARIOS == 0) return true; 
  for (int i = 0; i < CANTIDAD_USUARIOS; i++) {
    if (USUARIOS_AUTORIZADOS[i] == id) return true;
  }
  return false;
}

// ----------------- CONTROL DE BOMBILLO Y BOTON FISICO -----------------

void gestionarPulsadorLuz() {
  int lecturaRaw = digitalRead(PIN_BOTON_LUZ);

  // Anti-rebote (Debounce)
  if (lecturaRaw != ultimoEstadoBotonRaw) {
    tiempoUltimoCambioBoton = millis();
  }
  ultimoEstadoBotonRaw = lecturaRaw;

  if ((millis() - tiempoUltimoCambioBoton) > DEBOUNCE_MS) {
    if (lecturaRaw != estadoBotonFiltro) {
      estadoBotonFiltro = lecturaRaw;

      // Al presionar el botón (pasa de HIGH a LOW con INPUT_PULLUP)
      if (estadoBotonFiltro == LOW) {
        conmutarBombillo();
        Serial.println("Pulsador físico presionado. Estado luz: " + String(estadoBombillo ? "ON" : "OFF"));
      }
    }
  }
}

void conmutarBombillo() {
  if (estadoBombillo) {
    apagarBombillo();
  } else {
    encenderBombillo();
  }
}

void encenderBombillo() {
  digitalWrite(PIN_BOMBILLO, HIGH);
  estadoBombillo = true;
}

void apagarBombillo() {
  digitalWrite(PIN_BOMBILLO, LOW);
  estadoBombillo = false;
}

// ----------------- LOGICA DE MOVIMIENTO -----------------

void solicitarAbrir(int64_t chatId) {
  if (estado == ABIERTA) {
    miBot.sendMessage(chatId, "La puerta ya esta abierta.");
    return;
  }
  if (estado == ABRIENDO) {
    miBot.sendMessage(chatId, "La puerta ya se esta abriendo.");
    return;
  }
  chatIdPendiente = chatId;
  estado = ABRIENDO;
  tiempoInicioMovimiento = millis();
  moverAbrir();
  miBot.sendMessage(chatId, "Abriendo puerta...");
}

void solicitarCerrar(int64_t chatId) {
  if (estado == CERRADA) {
    miBot.sendMessage(chatId, "La puerta ya esta cerrada.");
    return;
  }
  if (estado == CERRANDO) {
    miBot.sendMessage(chatId, "La puerta ya se esta cerrando.");
    return;
  }
  chatIdPendiente = chatId;
  estado = CERRANDO;
  tiempoInicioMovimiento = millis();
  moverCerrar();
  miBot.sendMessage(chatId, "Cerrando puerta...");
}

void solicitarParar(int64_t chatId) {
  detenerMotor();
  estado = DETENIDA;
  miBot.sendMessage(chatId, "Puerta detenida manualmente.");
}

void gestionarMovimiento() {
  unsigned long transcurrido = millis() - tiempoInicioMovimiento;

  if (estado == ABRIENDO && transcurrido > TIEMPO_ABRIR_MS) {
    detenerMotor();
    estado = ABIERTA;
    encenderBombillo(); // Enciende automáticamente al terminar de abrir
    notificarPendiente("Puerta abierta.");
  }
  else if (estado == CERRANDO && transcurrido > TIEMPO_MAX_CERRAR_MS) {
    detenerMotor();
    estado = DETENIDA;
    notificarPendiente("Tiempo maximo de cierre alcanzado. Puerta detenida por seguridad.");
  }
}

void gestionarFinCarrera() {
  int lecturaRaw = digitalRead(PIN_FDC1);

  if (lecturaRaw != ultimoEstadoFDC1Raw) {
    tiempoUltimoCambioFDC1 = millis();
  }
  ultimoEstadoFDC1Raw = lecturaRaw;

  if ((millis() - tiempoUltimoCambioFDC1) > DEBOUNCE_MS) {
    if (lecturaRaw != estadoFDC1Filtro) {
      estadoFDC1Filtro = lecturaRaw;

      if (estadoFDC1Filtro == LOW && estado == CERRANDO) {
        detenerMotor();
        estado = CERRADA;
        apagarBombillo(); // Apaga el bombillo al terminar de cerrar
        notificarPendiente("Puerta cerrada (FDC1 detectado).");
      }
    }
  }
}

void notificarPendiente(String mensaje) {
  Serial.println(mensaje);
  if (chatIdPendiente != 0) {
    miBot.sendMessage(chatIdPendiente, mensaje);
    chatIdPendiente = 0;
  }
}

// ----------------- CONTROL DEL MOTOR CON ARRANQUE IMPULSIVO -----------------

void moverAbrir() {
  digitalWrite(PIN_IN1, HIGH);
  digitalWrite(PIN_IN2, LOW);
  analogWrite(PIN_ENA, IMPULSO_ARRANQUE);
  delay(TIEMPO_IMPULSO_MS);
  analogWrite(PIN_ENA, VELOCIDAD_MOTOR);
}

void moverCerrar() {
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, HIGH);
  analogWrite(PIN_ENA, IMPULSO_ARRANQUE);
  delay(TIEMPO_IMPULSO_MS);
  analogWrite(PIN_ENA, VELOCIDAD_MOTOR);
}

void detenerMotor() {
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, LOW);
  analogWrite(PIN_ENA, 0);
}

String nombreEstado(EstadoPuerta e) {
  switch (e) {
    case CERRADA: return "Cerrada";
    case ABRIENDO: return "Abriendo";
    case ABIERTA: return "Abierta";
    case CERRANDO: return "Cerrando";
    case DETENIDA: return "Detenida";
  }
  return "Desconocido";
}
