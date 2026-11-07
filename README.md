# Simulador y Descifrador Moderno de la Máquina Enigma (C++17)

Herramienta de alto rendimiento para simular la máquina **Enigma I** (Wehrmacht/Luftwaffe) y **M3** (Kriegsmarine), así como para **romper mensajes cifrados a ciegas (Ciphertext-Only)** o mediante **ataque Crib**.

## Rendimiento
- **Velocidad de procesamiento:** > 100,000,000 caracteres/segundo (~9 nanosegundos por letra).
- **Descifrado Moderno a Ciegas (Ciphertext-Only):** Rompe la clave completa (rotores, posición inicial y clavijero) en **~150 milisegundos** sin conocer ninguna palabra previa.
- **Ataque Crib (Bletchley Park / Bombe):** Prueba las 1,054,560 configuraciones de rotores y posiciones en **< 1 segundo** con OpenMP multinúcleo.

## Compilación
Si deseas recompilar el código fuente:
```powershell
g++ -O3 -fopenmp -std=c++17 enigma.cpp -o enigma.exe
```

## Modos de Uso

### 1. Menú Interactivo (Fácil de usar)
Ejecuta directamente sin parámetros en PowerShell o CMD:
```powershell
.\enigma.exe
```
Opciones disponibles:
1. Decodificar / Codificar mensaje (con clave conocida).
2. Descifrador MODERNO A CIEGAS (sin clave ni palabras sospechadas).
3. Descifrador clásico por CRIB (estilo Bletchley Park / Bombe de Turing).
4. Caso histórico real (Operación Barbarroja, 7 de julio de 1941).
5. Benchmark de rendimiento.

---

### 2. Descifrado Moderno a Ciegas (Ciphertext-Only Attack)
Si tienes un texto cifrado y **no sabes qué dice ni qué clave se usó**:
```powershell
.\enigma.exe --blind --lang es -t "JB ISGISR VRLEJGQW O TNU IDOY IK XM RVBNQQ NTP ZJ LEUVSS FHTXM AQZVN XCH XPUDRM FJKFP UHRORYF IGM VVXYHXYKJ XLHQD UX CGK"
```
Idiomas soportados: `es` (español), `de` (alemán), `en` (inglés).

---

### 3. Descifrado con Clave Conocida (Uso normal)
```powershell
# Vector canónico de prueba:
.\enigma.exe -r I,II,III -p AAA -t "BDZGOWCXLT"
# Salida: AAAAAAAAAA
```

---

### 4. Mensaje Histórico Real (Operación Barbarroja 1941)
```powershell
.\enigma.exe -r II,IV,V -p BLA -g 02,21,12 -s "AV BS CG DL FU HZ IN KM OW RX" -t "EDPUD NRGYS ZRCXN UYTPO MRMBO FKTBZ REZKM LXLVE FGUEY"
# Salida: AUFKL XABTE ILUNG XVONX KURTI NOWAX KURTI NOWAX NORDW
```
# CODIFICADOR-MAQUINA-ENIGMA
# CODIFICADOR-MAQUINA-ENIGMA
# CODIFICADOR-MAQUINA-ENIGMA
# DECODIFICADOR-MAQUINA-ENIGMA
# DECODIFICADOR-MAQUINA-ENIGMA
