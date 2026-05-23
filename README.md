# A.X.I.S. — Asistență eXoscheletică pentru intervenții și suport

Proiectul A.X.I.S. urmărește investigarea experimentală a semnalelor EMG și optimizarea unor algoritmi care să decodeze corect intenția de mișcare a utilizatorului. Prin extragerea unor parametri statistici precum RMS și MAV din impulsurile musculare, cercetarea demonstrează fezabilitatea transformării biopotonețialelor în răspuns mecanic fluid, oferind o bază pentru tehnologii asistive.
---

##  Structura Proiectului 
*  `AXIS_main.cpp` - Singurul cod necesar pentru ESP32.
*  `AXIS.zip` - Arhiva cu codul pentru Android Studio al aplicației de mobil.
*  `assembley.step` - Fișier CAD cu un deget mecatronic
---

## Specificații Hardware

### Componente:
*   **5 Micro Servo-uri MG90S** (https://sigmanortec.ro/Servomotor-MG90S-angrenaje-metal-p209610310)
*   **1 Senzor ACS712** (https://sigmanortec.ro/Senzor-curent-ACS712-20A-p126400450)
*   **2 Senzori EMG de suprafață H124SG** (https://sigmanortec.ro/modul-senzor-electromiograma-emg-h124sg) 
*   **1 EPS32** (https://sigmanortec.ro/placa-dezvoltare-esp32-cu-wifi-si-bluetooth)
*   **1 Sursă 6V**
*   **2 Surse 9V** 

> !!! **Notă Electrică:** GND-ul servo-motoarelor trebuie separat de cel al senzorilor pentru a evita zgomotul electric.

---

##  Pașii de Lucru 
  1. Se descarcă `AXIS_main.cpp` pentru EPS32.
---
  2. Se descarcă și se deschide `AXIS.zip` folosind Android Studio.
---
  4. După ce se instalează aplicația pe telefon, se urmează pașii de pe ecran.
---
