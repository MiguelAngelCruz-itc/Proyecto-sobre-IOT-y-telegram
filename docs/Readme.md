# Puerta inteligente controlada desde Telegram

Cerradura IoT con ESP32 que se abre, se cierra y reporta su estado desde un chat de Telegram.

Proyecto de la Escuela Tecnológica Instituto Técnico Central (ETITC).

![Prototipo](docs/foto-prototipo.jpg)

## Qué hace

- Abre y cierra la puerta con comandos de Telegram desde cualquier lugar con internet.
- Confirma el estado real de la puerta con un sensor magnético, no solo la orden enviada.
- Acepta órdenes únicamente de usuarios autorizados.
- No necesita app propia ni servidor: usa la Bot API de Telegram.

## Cómo funciona

1. El usuario escribe un comando en el chat con el bot.
2. El ESP32 consulta a Telegram cada pocos segundos y recibe el mensaje.
3. Verifica que el ID del chat esté en la lista de usuarios autorizados.
4. Activa la cerradura y la vuelve a bloquear.
5. El sensor confirma el estado y el bot responde en el chat.

## Comandos del bot

| Comando | Acción |
|---|---|
| `/abrir` | Libera la cerradura |
| `/cerrar` | Bloquea la cerradura |
| `/estado` | Informa si la puerta está abierta o cerrada |
| `/ayuda` | Lista los comandos disponibles |

## Componentes

Ver la lista completa en [hardware/lista-componentes.md](hardware/lista-componentes.md).

- ESP32 DevKit
- [Servomotor / relé + cerradura eléctrica]
- Sensor magnético (reed switch)
- Fuente de alimentación [5 V / 12 V]

## Conexiones

![Esquema de conexiones](docs/esquema-conexiones.png)

| Componente | Pin del ESP32 |
|---|---|
| [Servo / relé] | GPIO [X] |
| Sensor magnético | GPIO [X] |

## Instalación

1. Instala el [Arduino IDE](https://www.arduino.cc/en/software) y agrega el soporte para placas ESP32.
2. Instala las bibliotecas **UniversalTelegramBot** y **ArduinoJson** desde el gestor de bibliotecas.
3. Crea tu bot con [@BotFather](https://t.me/BotFather) y guarda el token.
4. Obtén tu ID de chat escribiéndole a [@userinfobot](https://t.me/userinfobot).
5. Abre `puerta_telegram.ino` y reemplaza estos datos por los tuyos:

```cpp
const char* ssid = "TU_WIFI";
const char* password = "TU_CONTRASEÑA";
#define BOT_TOKEN "TOKEN_DE_BOTFATHER"
#define AUTHORIZED_CHAT_ID "TU_ID_DE_CHAT"
```

6. Conecta el ESP32, selecciona la placa y el puerto, y carga el programa.

## Seguridad

- Solo responde a los chats incluidos en la lista de usuarios autorizados.
- Toda la comunicación con Telegram viaja por HTTPS.
- El token y las contraseñas reales **nunca se suben a este repositorio**. Si vas a publicar tu propia versión, revísalo antes de subirla.

## Resultados

| Métrica | Valor |
|---|---|
| Latencia entre el comando y el movimiento | [X] s |
| Aperturas exitosas | [N] de [N] pruebas |
| Costo total de componentes | $[X] |

## Video

[Ver la puerta funcionando]([ENLACE_DEL_VIDEO])

## Trabajo futuro

- Botones en línea dentro del chat.
- Apertura temporal para visitas, con caducidad.
- Respaldo con batería y aviso ante cortes de energía o WiFi.
- Registro histórico de accesos.

## Autores

- Miguel Angel Cruz Sanchez
- Manuel Santiago Ardila Becerra

Escuela Tecnológica Instituto Técnico Central (ETITC) · Mecatronica · 2026

## Licencia

MIT. Ver el archivo [LICENSE](LICENSE).
