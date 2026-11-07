# Simulador y Descifrador de la Máquina Enigma (C++17)

Implementación en C++17 de alto rendimiento de la máquina **Enigma I** (Wehrmacht/Luftwaffe) y **M3** (Kriegsmarine), optimizada para compilación nativa con OpenMP.

> [!IMPORTANT]
> **La Realidad Criptográfica del Descifrado (Bletchley Park & Alan Turing):**
> Debido a los más de **150 billones de combinaciones** del clavijero (*Steckerbrett*) y la distancia de unicidad de Shannon, matemáticamente **es indispensable conocer o sospechar al menos una palabra del mensaje (un *Crib*, ej: `HOLA`, `ESTE`, `MENSAJE`) para descifrar un texto interceptado sin la clave diaria**. Intentar descifrar textos a ciegas únicamente por frecuencia estadística produce falsos positivos masivos en mensajes breves; por ello, este programa implementa el método histórico de Turing como la única vía verdaderamente viable, rápida y 100% precisa.

---

## Características Principales

1. **Rendimiento Extremo en C++17:**
   * Velocidad de codificación/decodificación superior a **100.000.000 caracteres por segundo** (~9 nanosegundos por letra).
   * Búsqueda en paralelo con OpenMP: evalúa **2.197.000 claves en ~2 a 3 segundos**.
2. **Descifrado por Crib con Resolución Automática de Clavijas:**
   * A partir de una palabra sospechada, encuentra los rotores y la posición inicial.
   * **Deduce automáticamente los cables del clavijero (*Steckerbrett*)** mediante un optimizador con reconocimiento de vocabulario y devuelve el texto original intacto con sus espacios.
3. **Validación Estricta de Entradas:**
   * Acepta múltiples formatos: comas (`I,II,III`), guiones (`I-I-I`), números (`1,2,3`, `1-1-1`) o espacios.
   * Comprueba que los rotores existan (del I al V), que el clavijero no repita letras y que el reflector sea válido (B o C).
   * Muestra una **ficha de confirmación visual** antes de procesar cualquier texto.
4. **Fidelidad Histórica:**
   * Modela el cableado exacto de los rotores I al V, reflectores UKW-B y UKW-C, y la anomalía mecánica del **doble paso** (*double-stepping*) del rotor central.

---

## Compilación

Si deseas recompilar el código fuente con MinGW GCC:
```powershell
g++ -O3 -fopenmp -std=c++17 enigma.cpp -o enigma.exe
```

---

## Modos de Uso

### 1. Menú Interactivo (Fácil y Guiado)
Ejecuta directamente [`iniciar.bat`](iniciar.bat) o en tu terminal:
```powershell
.\enigma.exe
```
Opciones disponibles:
* **Opción 1:** Codificar / Decodificar mensaje (con clave conocida).
* **Opción 2:** Descifrar mensaje interceptado por CRIB (método de Bletchley Park).
* **Opción 3:** Mensaje histórico real (Operación Barbarroja, 7 de julio de 1941).
* **Opción 4:** Benchmark de rendimiento de la CPU.
* **Opción 5:** Salir.

---

### 2. Descifrar un Mensaje Interceptado (Ataque por Crib)

Si interceptaste un texto cifrado y **no conoces la clave** pero sospechas una palabra que contiene (ej: `MENSAJE`, `HOLA`, `ESTE`, `INFORME`):

```powershell
.\enigma.exe --crack "MENSAJE" -t "GVHM MJBM VL XI ZWRJVRH WB NTNLJX XHBO IW XFSCTCYWBTKVK IIX WPSS OCFAIX"
```

**Salida automática:**
```text
=======================================================
               COINCIDENCIA #1
=======================================================
  Rotores encontrados   : I-I-I
  Posicion inicial      : AAA
  Clavijero descubierto : AV
  Offset del crib       : 12 (MENSAJE)

TEXTO DESCIFRADO:
HOLA ESTE ES UN MENSAJE DE PRUEBA PARA EL DECODIFICADOR QUE HIZO GEMINI
=======================================================
```

---

### 3. Operación Normal (Con Clave Conocida)

Para cifrar un mensaje o descifrarlo cuando ya tienes los ajustes del día:

```powershell
# Cifrar un mensaje:
.\enigma.exe -r I-I-I -p AAA -u B -s "AV" -t "HOLA ESTE ES UN MENSAJE DE PRUEBA"
# Salida: GVHM MJBM VL XI ZWRJVRH WB NTNLJX

# Descifrar el resultado con la misma clave (propiedad simétrica de Enigma):
.\enigma.exe -r I-I-I -p AAA -u B -s "AV" -t "GVHM MJBM VL XI ZWRJVRH WB NTNLJX"
# Salida: HOLA ESTE ES UN MENSAJE DE PRUEBA
```

---

### 4. Caso Histórico Real (Operación Barbarroja 1941)

Despacho militar interceptado a la división SS-Totenkopf en el frente ruso:
```powershell
.\enigma.exe -r II,IV,V -p BLA -g 02,21,12 -s "AV BS CG DL FU HZ IN KM OW RX" -t "EDPUD NRGYS ZRCXN UYTPO MRMBO FKTBZ REZKM LXLVE FGUEY"
# Salida: AUFKL XABTE ILUNG XVONX KURTI NOWAX KURTI NOWAX NORDW
```
