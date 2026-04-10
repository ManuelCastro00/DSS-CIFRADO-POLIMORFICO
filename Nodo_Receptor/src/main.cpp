#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

// --- CONFIGURACIÓN DE RED ---
const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;

// Tópicos invertidos respecto al transmisor
const char* topic_tx = "dss101/redes/rx"; // Por aquí responde el receptor (ACK)
const char* topic_rx = "dss101/redes/tx"; // Por aquí escucha lo que manda el transmisor

WiFiClient espClient;
PubSubClient client(espClient);

// Variables para manejar los mensajes fuera del callback y evitar desconexiones
bool nuevoMensaje = false;
String mensajeGlobal = "";

// --- MOTOR DE GENERACIÓN DE LLAVES DE 64 BITS ---
uint64_t tablaLlaves[16]; // Arreglo para almacenar N=16 llaves
int N_keys = 16;

// Funciones polimórficas ligeras (XOR, desplazamientos y sumas)
uint64_t fs(uint64_t x, uint64_t y) { return (x ^ y) * 0x100000001B3ULL; }
uint64_t fg(uint64_t x, uint64_t y) { return (x << 3) ^ (y >> 1) ^ x; }
uint64_t fm(uint64_t x, uint64_t y) { return (x + y) ^ 0xABCD; }

// Algoritmo fiel al diagrama de flujo (Figura 3) del artículo
void generarTablaLlaves(uint64_t P, uint64_t Q, uint64_t S) {
  int n = N_keys;
  int index = 0;
  
  uint64_t P_act = P;
  uint64_t Q_act = Q;
  uint64_t S_act = S;

  Serial.println("\n[+] Generando tabla de llaves de 64 bits...");

  while (n > 0) {
    // ---- Primera parte del ciclo ----
    uint64_t P_next = fs(P_act, S_act);         
    tablaLlaves[index++] = fg(P_next, Q_act);   
    uint64_t S_next = fm(S_act, Q_act);         
    
    n--;
    if (n == 0) break;

    // ---- Segunda parte del ciclo ----
    uint64_t Q_next = fs(Q_act, S_next);        
    tablaLlaves[index++] = fg(Q_next, P_next);  
    S_act = fm(S_next, P_next);                 
    
    n--;
    
    // Mutación de sub-índices de los primos
    P_act = P_next;
    Q_act = Q_next;
  }
  
 Serial.println("[!] Llaves generadas exitosamente.");
    
    for (int i = 0; i < 16; i++) {
      // El formato %016llX obliga a imprimir 16 caracteres hexadecimales
      Serial.printf("-> Llave [%d]: %016llX\r\n", i, tablaLlaves[i]);
    }
}

// --- FUNCIÓN DE DESCIFRADO ---
String descifrarPayload(String cypherText, uint8_t psnIndex) {
  String plainText = "";
  // Seleccionamos la MISMA llave polimórfica que usó el transmisor
  uint64_t key = tablaLlaves[psnIndex % N_keys];
  
  // Revertimos el cifrado caracter por caracter usando XOR bit a bit
  for (int i = 0; i < cypherText.length(); i++) {
    uint8_t keyPart = (uint8_t)(key >> ((i % 8) * 8));
    plainText += (char)(cypherText[i] ^ keyPart); 
  }
  return plainText;
}

// --- FUNCIÓN PARA PROCESAR LA TRAMA ---
void procesarTrama(String trama) {
  // Buscamos las posiciones de los delimitadores '|'
  int p1 = trama.indexOf('|');
  int p2 = trama.indexOf('|', p1 + 1);
  int p3 = trama.indexOf('|', p2 + 1);

  if (p1 > 0 && p2 > 0 && p3 > 0) {
    String nodeID = trama.substring(0, p1);
    int type = trama.substring(p1 + 1, p2).toInt();
    int psn = trama.substring(p2 + 1, p3).toInt();
    String payload = trama.substring(p3 + 1);

    Serial.println("\n>>> Trama Entrante Procesada");
    Serial.println("Origen: " + nodeID + " | Tipo: " + String(type));

    // Si es un First Contact Message (Tipo 0) -> Sincronización
    if (type == 0) { 
      int c1 = payload.indexOf(',');
      int c2 = payload.indexOf(',', c1 + 1);
      
      uint16_t P = payload.substring(0, c1).toInt();
      uint16_t Q = payload.substring(c1 + 1, c2).toInt();
      uint16_t S = payload.substring(c2 + 1).toInt();
      
      Serial.println("Extraccion exitosa -> P: " + String(P) + " | Q: " + String(Q) + " | Semilla: " + String(S));
      
      // Generamos las llaves con los datos recibidos
      generarTablaLlaves(P, Q, S);
      
      // Acuse de recibo al transmisor
      String respuesta = "Llaves_FCM_Recibidas";
      client.publish(topic_tx, respuesta.c_str());
    }
    // Si es un Regular Message cifrado (Tipo 1) -> Descifrado
    else if (type == 1) {
      Serial.print("PSN Recibido (Indice de Llave): "); Serial.println(psn);
      
      // Llamamos a la función de descifrado
      String mensajeDescifrado = descifrarPayload(payload, psn);
      
      Serial.print("MENSAJE DESCIFRADO EXITOSAMENTE: ");
      Serial.println(mensajeDescifrado);
      Serial.println("--------------------------------------------------");
    }
  }
}

// --- CONFIGURACIONES BASE ---
void setup_wifi() {
  delay(10);
  Serial.print("Conectando a ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado.");
}

void callback(char* topic, byte* payload, unsigned int length) {
  mensajeGlobal = "";
  for (int i = 0; i < length; i++) {
    mensajeGlobal += (char)payload[i];
  }
  nuevoMensaje = true; // Levantamos la bandera de forma segura
}

void reconnect() {
  while (!client.connected()) {
    String clientId = "NodoReceptor-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("Conectado al Broker!");
      client.subscribe(topic_rx); 
    } else {
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) { reconnect(); }
  client.loop();

  // Atendemos los mensajes fuera del callback
  if (nuevoMensaje) {
    nuevoMensaje = false;
    procesarTrama(mensajeGlobal);
  }
}
