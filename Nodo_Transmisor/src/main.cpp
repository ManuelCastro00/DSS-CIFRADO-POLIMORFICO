#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

// Configuración de Red
const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;

// Tópicos MQTT
const char* topic_tx = "dss101/redes/tx"; 
const char* topic_rx = "dss101/redes/rx";

WiFiClient espClient;
PubSubClient client(espClient);

// --- VARIABLES DEL MODELO CRIPTOGRÁFICO ---
uint16_t nodeID;            // Identificador único del nodo
uint8_t currentPSN = 0;     // Nibble de secuencia (inicia en 0)
bool keysGenerated = false; // Bandera para saber si ya tenemos llaves

// Tipos de mensaje definidos en el algoritmo
const uint8_t TYPE_FCM = 0; // First Contact Message
const uint8_t TYPE_RM  = 1; // Regular Message
const uint8_t TYPE_KUM = 2; // Key Update Message
const uint8_t TYPE_LCM = 3; // Last Contact Message

// Arreglo de números primos para P y Q
const uint16_t primes[] = {11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97};

uint16_t generatePrime() {
  int index = random(0, 21);
  return primes[index];
}

// --- MOTOR DE GENERACIÓN DE LLAVES DE 64 BITS ---
uint64_t tablaLlaves[16]; // Arreglo para almacenar N=16 llaves
int N_keys = 16;

// Funciones polimórficas ligeras (XOR, desplazamientos y sumas)
uint64_t fs(uint64_t x, uint64_t y) { return (x ^ y) * 0x100000001B3ULL; }
uint64_t fg(uint64_t x, uint64_t y) { return (x << 3) ^ (y >> 1) ^ x; }
uint64_t fm(uint64_t x, uint64_t y) { return (x + y) ^ 0xABCD; }

// Algoritmo de generación de llaves 
void generarTablaLlaves(uint64_t P, uint64_t Q, uint64_t S) {
  int n = N_keys;
  int index = 0;
  
  uint64_t P_act = P;
  uint64_t Q_act = Q;
  uint64_t S_act = S;

  Serial.println("\n[+] Generando tabla de llaves de 64 bits...");

  while (n > 0) {
    // ---- Primera parte del ciclo ----
    uint64_t P_next = fs(P_act, S_act);         // P0 = fs(P, S)
    tablaLlaves[index++] = fg(P_next, Q_act);   // K1 = fg(P0, Q)
    uint64_t S_next = fm(S_act, Q_act);         // S0 = fm(S, Q)
    
    n--;
    if (n == 0) break;

    // ---- Segunda parte del ciclo ----
    uint64_t Q_next = fs(Q_act, S_next);        // Q0 = fs(Q, S0)
    tablaLlaves[index++] = fg(Q_next, P_next);  // K2 = fg(Q0, P0)
    S_act = fm(S_next, P_next);                 // S1 = fm(S0, P0)
    
    n--;
    
    // Mutación de sub-índices de los primos después de 2 llaves
    P_act = P_next;
    Q_act = Q_next;
  }
  
Serial.println("[!] Llaves generadas exitosamente.");
    
    for (int i = 0; i < 16; i++) {
      // El formato %016llX obliga a imprimir 16 caracteres hexadecimales
      Serial.printf("-> Llave [%d]: %016llX\r\n", i, tablaLlaves[i]);
    }
}

// --- FUNCIÓN DE CIFRADO ---
// Función de cifrado XOR polimórfico (ligero y reversible)
String cifrarPayload(String plainText, uint8_t psnIndex) {
  String cypherText = "";
  // Seleccionamos la llave polimórfica usando el PSN como índice
  uint64_t key = tablaLlaves[psnIndex % N_keys];
  
  // Ciframos caracter por caracter usando XOR bit a bit
  for (int i = 0; i < plainText.length(); i++) {
    // Tomamos 8 bits de la llave de 64 bits para cada caracter (rotando si es necesario)
    uint8_t keyPart = (uint8_t)(key >> ((i % 8) * 8));
    cypherText += (char)(plainText[i] ^ keyPart); // Operación XOR
  }
  return cypherText;
}

// --- CONFIGURACIONES BASE ---
void setup_wifi() {
  delay(10);
  Serial.print("Conectando a ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi conectado.");
}

void callback(char* topic, byte* payload, unsigned int length) {
  // Aquí podemos agregar lógica si el transmisor necesita procesar respuestas
}

void reconnect() {
  while (!client.connected()) {
    String clientId = "Transmisor-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      client.subscribe(topic_rx);
    } else {
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  randomSeed(esp_random()); // Inicializar generador aleatorio
  
  nodeID = random(1, 65535); // Asignar Node ID aleatorio de 16 bits
  
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

// --- BUCLE PRINCIPAL ---
void loop() {
  if (!client.connected()) { reconnect(); }
  client.loop();

  static unsigned long lastMsg = 0;
  unsigned long now = millis();
  
  // Intervalo de 8 segundos para envío de mensajes
  if (now - lastMsg > 8000) { 
    lastMsg = now;
    
    // --- FASE 1: Intercambio de Semillas (FCM) ---
    if (!keysGenerated) {
      uint16_t P = generatePrime();
      uint16_t Q = generatePrime();
      uint8_t S = random(1, 255); 
      
      // Construimos el Payload del FCM separando por comas
      String payloadFCM = String(P) + "," + String(Q) + "," + String(S);
      
      // Encapsulamos la trama completa: NodeID | Type | PSN | Payload
      String trama = String(nodeID) + "|" + String(TYPE_FCM) + "|" + String(currentPSN) + "|" + payloadFCM;
      
      Serial.println("\n--- Enviando FCM (Fase de Sincronizacion) ---");
      Serial.println("ParametrosCompartidos -> P: " + String(P) + ", Q: " + String(Q) + ", Semilla: " + String(S));
      client.publish(topic_tx, trama.c_str());

      // Generamos las llaves localmente con los parámetros compartidos
      generarTablaLlaves(P, Q, S);
      keysGenerated = true; // Finalizamos la sincronización
    }
    // --- FASE 2: Envío de Mensajes Regulares Cifrados (RM) ---
    else {
      // 1. Mensaje que queremos proteger (Payload original)
      String mensajeOriginal = "INVESTIGACION_DSS_" + String(nodeID);
      
      // 2. Incrementamos el PSN para seleccionar la siguiente llave polimórfica
      currentPSN = (currentPSN + 1) % N_keys; 
      
      // 3. Ciframos el mensaje localmente
      String mensajeCifrado = cifrarPayload(mensajeOriginal, currentPSN);
      
      // 4. Encapsulamos la trama RM: ID | Tipo(1) | PSN_Actual | Payload_Cifrado
      String tramaRM = String(nodeID) + "|" + String(TYPE_RM) + "|" + String(currentPSN) + "|" + mensajeCifrado;
      
      Serial.println("\n--- Enviando Mensaje Regular Cifrado (RM) ---");
      Serial.print("Payload Original: "); Serial.println(mensajeOriginal);
      Serial.print("Payload Cifrado (HEX): ");
      for(int i=0; i<mensajeCifrado.length(); i++) { 
        Serial.printf("%02X ", mensajeCifrado[i]);
        Serial.print(" "); 
      }
      Serial.println();
      Serial.print("PSN utilizado (Indice de Llave): "); Serial.println(currentPSN);
      
      // 5. Enviamos la trama por MQTT
      client.publish(topic_tx, tramaRM.c_str());
    }
  }
}
