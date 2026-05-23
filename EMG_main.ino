#include <Arduino.h>
#include <math.h>
#include "BluetoothSerial.h"
#include <Preferences.h> // Libraria nativa pentru memorie non-volatila

BluetoothSerial SerialBT;
Preferences prefs; // Instanta pentru Flash

// Pini Servomotoare
const int PIN_SERVO_MARE     = 18; 
const int PIN_SERVO_ARATATOR = 23; 
const int PIN_SERVO_MIJLOCIU = 19; 
const int PIN_SERVO_INELAR   = 22; 
const int PIN_SERVO_MIC      = 21; 

const int piniDegete[] = {PIN_SERVO_MARE, PIN_SERVO_ARATATOR, PIN_SERVO_MIJLOCIU, PIN_SERVO_INELAR, PIN_SERVO_MIC};
const int nrDegete = 5;

#define SENSOR1_PIN 34
#define SENSOR2_PIN 35
#define WINDOW_SIZE 100  
#define STEP_SIZE 50     
#define NUM_VOTES 7      

const int PWM_FREQ = 50;
const int PWM_RES = 12;
const int POS_0 = 102;  
const int POS_1 = 512;  

struct Centroid {
  String nume;
  float mav1 = 0, rms1 = 0, mav2 = 0, rms2 = 0;
  bool calibrat = false;
};

const int NUM_GESTURI = 5; 
Centroid centroizi[NUM_GESTURI];
String numeGesturi[] = {"ARATATOR", "MIJLOCIU", "INELAR", "REPAUS", "PUMN"};

bool modCalibrareBruta = false;
bool modRulareNormala = false;

int buffer1[WINDOW_SIZE];
int buffer2[WINDOW_SIZE];
int writeIdx = 0;
int newSamplesCount = 0;
int voti[NUM_VOTES];
int votIdx = 0;
int gestPrecedent = -1;

// Functie care incarca centroizii salvati anterior din Flash
void incarcaCentroiziDinFlash() {
  prefs.begin("neurogrip", true); // Deschide in mod Read-Only
  Serial.println("\n[Flash] Se incarca centroizii salvati...");
  
  bool totiIncarcati = true;
  for(int i = 0; i < NUM_GESTURI; i++) {
    String cheie = "c_" + String(i);
    if(prefs.isKey(cheie.c_str())) {
      // Citim byte cu byte structura binara direct din Flash
      prefs.getBytes(cheie.c_str(), &centroizi[i], sizeof(Centroid));
      centroizi[i].nume = numeGesturi[i]; // Ne asiguram ca string-ul e corect mapat
      Serial.printf("-> Incarcat %s: M1=%.2f, R1=%.2f, M2=%.2f, R2=%.2f\n", 
                    centroizi[i].nume.c_str(), centroizi[i].mav1, centroizi[i].rms1, centroizi[i].mav2, centroizi[i].rms2);
    } else {
      totiIncarcati = false;
    }
  }
  prefs.end();
  
  if(totiIncarcati) {
    Serial.println("[Flash] Toate profilele au fost restaurate cu succes!");
  } else {
    Serial.println("[Flash] Nu s-au gasit profiluri complete. Necesita calibrare noua.");
  }
}

// Functie care salveaza un centroid in Flash imediat dupa ce a fost primit
void salveazaCentroidInFlash(int id) {
  prefs.begin("neurogrip", false); // Open in Write mode
  String cheie = "c_" + String(id);
  prefs.putBytes(cheie.c_str(), &centroizi[id], sizeof(Centroid));
  prefs.end();
  Serial.printf("[Flash] Centroidul %d a fost securizat in Flash.\n", id);
}

void setup() {
  Serial.begin(115200);
  SerialBT.begin("NeuroGrip_ESP32"); 

  for(int i = 0; i < NUM_GESTURI; i++) {
    centroizi[i].nume = numeGesturi[i];
  }

  // Incarcam datele vechi la pornire
  incarcaCentroiziDinFlash();

  analogReadResolution(12);
  pinMode(SENSOR1_PIN, INPUT);
  pinMode(SENSOR2_PIN, INPUT);
  
  for(int i = 0; i < NUM_VOTES; i++) voti[i] = 3; 

  for (int i = 0; i < nrDegete; i++) {
    if(ledcAttach(piniDegete[i], PWM_FREQ, PWM_RES)) {
      ledcWrite(piniDegete[i], (i == 3 || i == 4) ? POS_1 : POS_0);
      delay(50); 
    }
  }
}

void executaGest(int g) {
  if (g == gestPrecedent) return; 
  int pozitii[5];
  
  switch(g) {
    case 0: pozitii[0]=POS_0; pozitii[1]=POS_1; pozitii[2]=POS_0; pozitii[3]=POS_0; pozitii[4]=POS_0; break; 
    case 1: pozitii[0]=POS_0; pozitii[1]=POS_0; pozitii[2]=POS_1; pozitii[3]=POS_0; pozitii[4]=POS_0; break; 
    case 2: pozitii[0]=POS_0; pozitii[1]=POS_0; pozitii[2]=POS_0; pozitii[3]=POS_1; pozitii[4]=POS_0; break; 
    case 3: for(int i=0; i<5; i++) pozitii[i] = POS_0; break;                                                 
    case 4: for(int i=0; i<5; i++) pozitii[i] = POS_1; break;                                                 
    default: return;
  }

  pozitii[3] = (pozitii[3] == POS_0) ? POS_1 : POS_0;
  pozitii[4] = (pozitii[4] == POS_0) ? POS_1 : POS_0;

  for (int i = 0; i < nrDegete; i++) {
    ledcWrite(piniDegete[i], pozitii[i]);
  }
  gestPrecedent = g;
}

void proceseazaComenziBluetooth() {
  if (SerialBT.available()) {
    String msg = SerialBT.readStringUntil('\n');
    msg.trim();

    if (msg == "CMD:START_RAW") {
      modCalibrareBruta = true;
      modRulareNormala = false;
    }
    else if (msg == "CMD:STOP_RAW") {
      modCalibrareBruta = false;
    }
    else if (msg == "CMD:START_RUN") {
      bool totiCalibrati = true;
      for(int i=0; i<NUM_GESTURI; i++) {
        if(!centroizi[i].calibrat) totiCalibrati = false;
      }
      if(totiCalibrati) {
        modRulareNormala = true;
        modCalibrareBruta = false;
        SerialBT.println("STATUS_RUN:OK");
      } else {
        SerialBT.println("ERR:Sistem necalibrat complet in Flash!");
      }
    }
    else if (msg.startsWith("CENTROID:")) {
      String date = msg.substring(9);
      int idxComa1 = date.indexOf(',');
      int idxComa2 = date.indexOf(',', idxComa1 + 1);
      int idxComa3 = date.indexOf(',', idxComa2 + 1);
      int idxComa4 = date.indexOf(',', idxComa3 + 1);

      int id = date.substring(0, idxComa1).toInt();
      if(id >= 0 && id < NUM_GESTURI) {
        centroizi[id].mav1 = date.substring(idxComa1 + 1, idxComa2).toFloat();
        centroizi[id].rms1 = date.substring(idxComa2 + 1, idxComa3).toFloat();
        centroizi[id].mav2 = date.substring(idxComa3 + 1, idxComa4).toFloat();
        centroizi[id].rms2 = date.substring(idxComa4 + 1).toFloat();
        centroizi[id].calibrat = true;
        
        // Salvare fizica imediata in Flash NV
        salveazaCentroidInFlash(id);
        SerialBT.print("ACK_CALIB:"); SerialBT.println(id);
      }
    }
  }
}

void loop() {
  proceseazaComenziBluetooth();

  static unsigned long lastSampleTime = 0;
  if (micros() - lastSampleTime >= 1000) { 
    lastSampleTime = micros();
    int valADC1 = analogRead(SENSOR1_PIN);
    int valADC2 = analogRead(SENSOR2_PIN);

    if (modCalibrareBruta) {
      SerialBT.print("RAW:");
      SerialBT.print(valADC1);
      SerialBT.print(",");
      SerialBT.println(valADC2);
    }

    buffer1[writeIdx] = valADC1;
    buffer2[writeIdx] = valADC2;
    writeIdx = (writeIdx + 1) % WINDOW_SIZE;
    newSamplesCount++;

    if (modRulareNormala && newSamplesCount >= STEP_SIZE) {
      double s1 = 0, s2 = 0, sq1 = 0, sq2 = 0;
      for (int i = 0; i < WINDOW_SIZE; i++) {
        s1 += buffer1[i]; s2 += buffer2[i];
        sq1 += (double)buffer1[i] * buffer1[i];
        sq2 += (double)buffer2[i] * buffer2[i];
      }
      float m1 = s1 / WINDOW_SIZE, m2 = s2 / WINDOW_SIZE;
      float r1 = sqrt(sq1 / WINDOW_SIZE), r2 = sqrt(sq2 / WINDOW_SIZE);

      int gestCurent = 3; 
      float dMin = 1000000.0;
      
      for (int i = 0; i < NUM_GESTURI; i++) {
        if(!centroizi[i].calibrat) continue;
        float d = sqrt(pow(m1 - centroizi[i].mav1, 2) + pow(r1 - centroizi[i].rms1, 2) + 
                       pow(m2 - centroizi[i].mav2, 2) + pow(r2 - centroizi[i].rms2, 2));
        if (d < dMin) { dMin = d; gestCurent = i; }
      }

      voti[votIdx] = gestCurent;
      votIdx = (votIdx + 1) % NUM_VOTES;

      int numaratori[NUM_GESTURI] = {0};
      for (int i = 0; i < NUM_VOTES; i++) numaratori[voti[i]]++;
      
      int castigator = 3;
      int maxVoturi = -1;
      for (int i = 0; i < NUM_GESTURI; i++) {
        if (numaratori[i] > maxVoturi) { maxVoturi = numaratori[i]; castigator = i; }
      }

      if (SerialBT.connected()) {
        SerialBT.print("GEST:"); SerialBT.println(centroizi[castigator].nume);
        SerialBT.print("DATA:");
        SerialBT.print(m1); SerialBT.print(",");
        SerialBT.print(r1); SerialBT.print(",");
        SerialBT.print(m2); SerialBT.print(",");
        SerialBT.println(r2);
      }
      
      executaGest(castigator); 
      newSamplesCount = 0;
    }
  }
}