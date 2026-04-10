## ⚙️ Instrucciones de Ejecución (PlatformIO + Wokwi)

Nuestro proyecto está configurado para ser compilado y simulado utilizando Visual Studio Code junto con la cadena de herramientas de PlatformIO y Wokwi. Para ejecutarlo en una computadora nueva, sigue estos pasos:

### Requisitos Previos
1. Descargar e instalar [Visual Studio Code](https://code.visualstudio.com/).
2. En la pestaña de extensiones de VS Code, buscar e instalar:
   * **PlatformIO IDE** (Para la compilación y gestión de librerías).
   * **Wokwi Simulator** (Para la simulación del circuito virtual).
   * NOTA: Para la extensión de wokwi hace falta una licencia, en este caso se puede obtener una gratuita de un mes para poder utilizar la extension. Esto se hace desde el siguiente enlace:
   https://wokwi.com/license?v=3.5.0&r=UserRequest&s=v

### Pasos para Ejecutar el Proyecto
1. **Clonar el repositorio:** Descarga este proyecto como un archivo ZIP desde GitHub y extráelo, o clónalo mediante la terminal:
   `git clone [AQUÍ_PONES_EL_LINK_DE_TU_REPO]`

2. **Abrir el entorno:**
   Abre Visual Studio Code, ve a `Archivo > Abrir Carpeta` (`File > Open Folder`) y selecciona la carpeta específica del nodo que deseas correr (por ejemplo, `/Nodo_Transmisor`).

3. **Inicialización Automática:**
   Al abrir la carpeta, PlatformIO leerá automáticamente el archivo `platformio.ini` y comenzará a descargar en segundo plano el framework de ESP32 y las librerías necesarias (`PubSubClient` y `WiFi`). Espera a que termine este proceso (verás actividad en la barra azul inferior).

4. **Para Simular (Wokwi):**
   * Abre el archivo `diagram.json` dentro de VS Code.
   * La extensión de Wokwi abrirá una pestaña con el circuito visual.
   * Haz clic en el botón de "Play" ▶️ en la pestaña del simulador.
   * *Nota:* Asegúrate de que tu PC tenga salida a internet para que el simulador pueda conectarse al Broker MQTT público.
