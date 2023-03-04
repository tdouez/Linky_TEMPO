//--------------------------------------------------------------------
//                 +/
//                 `hh-
//        ::        /mm:
//         hy`      -mmd
//         omo      +mmm.  -+`
//         hmy     .dmmm`   od-
//        smmo    .hmmmy    /mh
//      `smmd`   .dmmmd.    ymm
//     `ymmd-   -dmmmm/    omms
//     ymmd.   :mmmmm/    ommd.
//    +mmd.   -mmmmm/    ymmd-
//    hmm:   `dmmmm/    smmd-
//    dmh    +mmmm+    :mmd-
//    omh    hmmms     smm+
//     sm.   dmmm.     smm`
//      /+   ymmd      :mm
//           -mmm       +m:
//            +mm:       -o
//             :dy
//              `+:
//--------------------------------------------------------------------
//   __|              _/           _ )  |
//   _| |  |   ` \    -_)   -_)    _ \  |   -_)  |  |   -_)
//  _| \_,_| _|_|_| \___| \___|   ___/ _| \___| \_,_| \___|
//--------------------------------------------------------------------
// Code mise à disposition selon les termes de la Licence Creative Commons Attribution
// Pas d’Utilisation Commerciale.
// Partage dans les Mêmes Conditions 4.0 International.
//--------------------------------------------------------------------
// 2023/02/03 - FB V1.0.0 alpha
// 2023/02/16 - FB V1.0.0
// 2023/03/04 - FB V1.0.1 - Ajout sequence test led, correction détection TEMPO histo & standard, utilisation libTeleinfoLite
//--------------------------------------------------------------------

#include <Arduino.h>
#include "LibTeleinfoLite.h"
#include <jled.h>

#define VERSION   "v1.0.1"

//#define DEBUG_TIC

#define LED_ROUGE 8
#define LED_BLANC 9
#define LED_BLEU  10
#define LED_EAU   2
#define LED_CHAUF 3 
#define LED_TIC   7

#define RELAIS_EAU   12
#define RELAIS_CHAUF 13 

#define SW_0   A0
#define SW_1   A1
#define SW_2   A2
#define SW_3   A3
#define SW_4   A4

#define TARIF_AUTRE 0
#define TARIF_TEMPO 1

#define HEURE_TOUTE    0
#define HEURE_PLEINE   1
#define HEURE_CREUSE   2

#define JOUR_SANS  0
#define JOUR_BLEU  1
#define JOUR_BLANC 2
#define JOUR_ROUGE 3

#define CHAUF_0  B000
#define CHAUF_1  B001
#define CHAUF_2  B010
#define CHAUF_3  B011
#define CHAUF_4  B100
#define CHAUF_5  B101
#define CHAUF_6  B110
#define CHAUF_C  B111

#define EAU_0   B000
#define EAU_1   B001
#define EAU_2   B010
#define EAU_3   B011

const unsigned long TEMPS_MAJ  =  15000; // 15s,  Minimum time between send (in milliseconds). 
unsigned long lastTime_maj = 0;
uint8_t tarif = TARIF_AUTRE;
uint8_t jour = JOUR_SANS;
uint8_t heure = HEURE_TOUTE;

auto led_chauf = JLed(LED_CHAUF); 
auto led_eau = JLed(LED_EAU);

byte val_switch=0;
_Mode_e mode_tic;
TInfo tinfo;




// ---------------------------------------------------------------- 
// sequence_test_led
// ----------------------------------------------------------------
void sequence_test_led()
{

  Serial.println(F("-- Sequence test --"));
  Serial.println(F("LED rouge"));
  digitalWrite(LED_ROUGE, HIGH);
  delay(400);
  Serial.println(F("LED blacnhe"));
  digitalWrite(LED_BLANC, HIGH);
  delay(400);
  Serial.println(F("LED bleue"));
  digitalWrite(LED_BLEU, HIGH);
  delay(400);
  Serial.println(F("LED TIC"));
  digitalWrite(LED_TIC, HIGH);
  delay(400);
  Serial.println(F("LED EAU"));
  digitalWrite(LED_EAU, HIGH);
  delay(400);
  Serial.println(F("LED CHAUF"));
  digitalWrite(LED_CHAUF, HIGH);
  delay(400);
  digitalWrite(LED_ROUGE, LOW);
  digitalWrite(LED_BLANC, LOW);
  digitalWrite(LED_BLEU, LOW);
  digitalWrite(LED_TIC, LOW);
  digitalWrite(LED_EAU, LOW);
  digitalWrite(LED_CHAUF, LOW);
  Serial.println(F("-------------------"));
}

// ---------------------------------------------------------------- 
// lecture_val_switch
// ----------------------------------------------------------------
void lecture_val_switch(byte *val_chauf, byte *val_eau)
{

  *val_chauf = 0;
  *val_eau = 0;
  
  for (int i=0; i<5; i++) {
    if (i<3) bitWrite(*val_chauf, i, !digitalRead(i+SW_0));
      else bitWrite(*val_eau, i-3, !digitalRead(i+SW_0));
  }
  Serial.print("Sw chauf:");
  Serial.print(*val_chauf, BIN);
  Serial.print(", Sw eau:");
  Serial.println(*val_eau, BIN);

}

// ---------------------------------------------------------------- 
// recup_val_relais_chauf
// ----------------------------------------------------------------
byte recup_val_relais_chauf(byte val_CHAUF)
{
byte rc=0;

  switch (val_CHAUF) {
    case CHAUF_0:
      Serial.println("CHAUF_0");
      rc=1;
      break;
      
    case CHAUF_1:
      if (jour == JOUR_BLEU || jour == JOUR_BLANC || (jour == JOUR_ROUGE && heure == HEURE_CREUSE)) {
        Serial.println("CHAUF_1");
        rc=1;
      }
      break;

    case CHAUF_2:
      if (jour != JOUR_ROUGE) {
        Serial.println("CHAUF_2");
        rc=1;
      }
      break;

    case CHAUF_3:
      if (jour == JOUR_BLEU || (jour = JOUR_BLANC && heure == HEURE_CREUSE)) {
        Serial.println("CHAUF_3");
        rc=1;
      }
      break;

    case CHAUF_4:
      if (jour == JOUR_BLEU) {
        Serial.println("CHAUF_4");
        rc=1;
      }
      break;

    case CHAUF_5:
      if (jour == JOUR_BLEU && heure == HEURE_CREUSE) {
        Serial.println("CHAUF_5");
        rc=1;
      }
      break;

    case CHAUF_6:
      Serial.println("CHAUF_6");
      rc=0;
      break;

    case CHAUF_C:
      if (heure == HEURE_CREUSE) {
        Serial.println("CHAUF_C");
        rc=1;
      }
      break;
  }

  return rc;
}

// ---------------------------------------------------------------- 
// recup_val_relais_eau
// ----------------------------------------------------------------
byte recup_val_relais_eau(byte val_eau)
{
byte rc=0;

  switch (val_eau) {

    case EAU_0:
      Serial.println("EAU_0");
      rc=1;
      break;

    case EAU_1:
      if (heure == HEURE_CREUSE) {
        Serial.println("EAU_1");
        rc=1;
      }
      break;
      
    case EAU_2:
      if (jour == JOUR_BLEU || (jour == JOUR_BLANC && heure == HEURE_CREUSE) || (jour == JOUR_ROUGE && heure == HEURE_CREUSE)) {
        Serial.println("EAU_2");
        rc=1;
      }
      break;

    case EAU_3:
      if (jour == JOUR_BLEU || jour == JOUR_BLANC || (jour == JOUR_ROUGE && heure == HEURE_CREUSE)) {
        Serial.println("EAU_3");
        rc=1;
      }
      break;
  }

  return rc;
}

// ---------------------------------------------------------------- 
// change_etat_led_teleinfo
// ---------------------------------------------------------------- 
void change_etat_led_teleinfo()
{
  static int led_state;

  led_state = !led_state;
  digitalWrite(LED_TIC, led_state);
}

// ---------------------------------------------------------------- 
// clignote_led
// ---------------------------------------------------------------- 
void clignote_led(uint8_t led, uint8_t nbr, int16_t delais)
{
int led_state;

  for (int i=0; i<nbr*2; i++) {
    led_state = !led_state;
    digitalWrite(led, led_state);
    delay(delais);
  }
  digitalWrite(led, HIGH);
}

// ---------------------------------------------------------------- 
// init_speed_TIC
// ---------------------------------------------------------------- 
_Mode_e init_speed_TIC()
{
boolean flag_timeout = false;
boolean flag_found_speed = false;
uint32_t currentTime = millis();
unsigned step = 0;
_Mode_e mode;

  digitalWrite(LED_TIC, HIGH);
  
  // Test en mode historique
  // Recherche des éléments de début, milieu et fin de trame (0x0A, 0x20, 0x20, 0x0D)
  Serial.begin(1200); // mode historique
  while (!flag_timeout && !flag_found_speed) {
    if (Serial.available()>0) {
      char in = (char)Serial.read() & 127;  // seulement sur 7 bits
      // début trame
        if (in == 0x0A) {
        step = 1;
      }
      // premier milieu de trame
        if (step == 1 && in == 0x20) {
        step = 2;
      }
      // deuxième milieu de trame
        if (step == 2 && in == 0x20) {
        step = 3;
      }
      // fin trame
        if (step == 3 && in == 0x0D) {
        flag_found_speed = true;
        step = 0;
      }
    }
    if (currentTime + 10000 <  millis()) flag_timeout = true; // 10s de timeout
  }

  if (flag_timeout) { // trame avec vistesse histo non trouvée donc passage en mode standard
     mode = TINFO_MODE_STANDARD;
     Serial.end();
     Serial.begin(9600); // mode standard
     Serial.println(F(">> TIC mode standard <<"));
     clignote_led(LED_TIC, 3, 500);
  }
  else {
    mode = TINFO_MODE_HISTORIQUE;
    Serial.println(F(">> TIC mode historique <<"));
    clignote_led(LED_TIC, 5, 500);
  }
  
  digitalWrite(LED_TIC, LOW);

  return mode;
}

// ---------------------------------------------------------------- 
// send_teleinfo
// ---------------------------------------------------------------- 
void send_teleinfo(char *etiq, char *val)
{
   
  change_etat_led_teleinfo();

#ifdef DEBUG_TIC
  Serial.print(etiq);
  Serial.println(val);
#endif

  if (mode_tic == TINFO_MODE_HISTORIQUE) {
    // TIC historique --------------------------------------------------------
    // Recup contrat TEMPO
    // L'option tarifaire choisie (donnée du groupe d’étiquette « OPTARIF ») est codée sur 4 caractères ASCII alphanumériques selon la syntaxe suivante :
    //  BASE => Option Base,
    //  HC.. => Option Heures Creuses,
    //  EJP. => Option EJP,
    //  BBRx => Option Tempo.
    if (strcmp(etiq, "OPTARIF") == 0) {
      Serial.print(F("OPTARIF:"));
      Serial.println(val);
      if (strncmp(val, "BBR", 3) == 0) {
        tarif = TARIF_TEMPO;
        Serial.println(F("H TARIF_TEMPO"));
      }
      else {
        tarif = TARIF_AUTRE;
        Serial.println(F("H TARIF_AUTRE"));
      }
    }

    // Recup HC/HP et couleur jour
    // « TH.. » => Toutes les Heures,
    // « HC.. » => Heures Creuses,
    // « HP.. » => Heures Pleines,
    // « HN.. » => Heures Normales,
    // « PM.. » => Heures de Pointe Mobile,
    // « HCJB » => Heures Creuses Jours Bleus,
    // « HCJW » => Heures Creuses Jours Blancs (White),
    // « HCJR » => Heures Creuses Jours Rouges,
    // « HPJB » => Heures Pleines Jours Bleus,
    // « HPJW » => Heures Pleines Jours Blancs (White),
    // « HPJR » => Heures Pleines Jours Rouges.
    if (strcmp(etiq, "PTEC") == 0) {
      Serial.print(F("PTEC:"));
      Serial.println(val);
      if (tarif == TARIF_TEMPO) {
        jour=JOUR_SANS;
        heure=HEURE_TOUTE;
        
        if (strcmp(val, "HCJB") == 0) {
          jour=JOUR_BLEU;
          heure=HEURE_CREUSE;
          Serial.println(F("j bleu, h creuse"));
        }
        if (strcmp(val, "HCJW") == 0) {
          jour=JOUR_BLANC;
          heure=HEURE_CREUSE;
          Serial.println(F("j blanc, h creuse"));
        }
        if (strcmp(val, "HCJR") == 0) {
          jour=JOUR_ROUGE;
          heure=HEURE_CREUSE;
          Serial.println(F("j rouge, h creuse"));
        }
        if (strcmp(val, "HPJB") == 0) {
          jour=JOUR_BLEU;
          heure=HEURE_PLEINE;
          Serial.println(F("j bleu, h pleine"));
        }
        if (strcmp(val, "HPJW") == 0) {
          jour=JOUR_BLANC;
          heure=HEURE_PLEINE;
          Serial.println(F("j blanc, h pleine"));
        }
        if (strcmp(val, "HPJR") == 0) {
          jour=JOUR_ROUGE;
          heure=HEURE_PLEINE;
          Serial.println(F("j rouge, h pleine"));
        }
      }
    }
          
  }
  else {
    // TIC standard --------------------------------------------------------
    // Recup contrat TEMPO
    if (strcmp(etiq, "NTARF") == 0) {
      Serial.print(F("NTARF:"));
      Serial.println(val);
      if (strncmp(val, "BBR", 3) == 0) {
        tarif = TARIF_TEMPO;
        Serial.println(F("S TARIF_TEMPO"));
      }
      else {
        tarif = TARIF_AUTRE;
        Serial.println(F("S TARIF_AUTRE"));
      }
    }

    // Recup HC/HP
    heure=HEURE_TOUTE;
    if (strcmp(etiq, "PTEC") == 0) {
      Serial.print(F("PTEC:"));
      Serial.println(val);
      if (strncmp(val, "TH", 2) == 0) {
        heure=HEURE_PLEINE;
        Serial.println(F("HEURE_PLEINE"));
      }
      else if (strcmp(val, "HC") == 0) {
        heure=HEURE_CREUSE;
        Serial.println(F("HEURE_CREUSE"));
      } 
    }
    
    // Recup couleur jour
    if (strcmp(etiq, "STGE") == 0) {
        Serial.print(F("STGE:"));
        Serial.println(val);
        unsigned long long_stge = atol(val);
        long_stge = (long_stge >> 24) && B11;
        jour = (int)long_stge;
        Serial.print(F("Jour: "));
        Serial.println(jour);
      }
  }
  
}


// ---------------------------------------------------------------- 
// setup
// ---------------------------------------------------------------- 
void setup() {

  pinMode(LED_ROUGE, OUTPUT);
  pinMode(LED_BLANC, OUTPUT);
  pinMode(LED_BLEU, OUTPUT);
  pinMode(LED_EAU, OUTPUT);
  pinMode(LED_CHAUF, OUTPUT);
  pinMode(LED_TIC, OUTPUT);
  pinMode(RELAIS_EAU, OUTPUT);
  pinMode(RELAIS_CHAUF, OUTPUT);

  pinMode(SW_0, INPUT_PULLUP);
  pinMode(SW_1, INPUT_PULLUP);
  pinMode(SW_2, INPUT_PULLUP);
  pinMode(SW_3, INPUT_PULLUP);
  pinMode(SW_4, INPUT_PULLUP);
  
  sequence_test_led();

  // init interface série suivant mode TIC
  mode_tic = init_speed_TIC();

  Serial.println(F("   __|              _/           _ )  |"));
  Serial.println(F("   _| |  |   ` \\    -_)   -_)    _ \\  |   -_)  |  |   -_)"));
  Serial.println(F("  _| \\_,_| _|_|_| \\___| \\___|   ___/ _| \\___| \\_,_| \\___|"));
  Serial.print(F("                                             "));
  Serial.println(VERSION);

   // init interface TIC
  tinfo.init(mode_tic);

}

// ---------------------------------------------------------------- 
// loop
// ---------------------------------------------------------------- 
void loop() {
  byte val_chauf, val_eau;
  uint32_t currentTime = millis();

  if (Serial.available()) tinfo.process(Serial.read());

  led_chauf.Update();
  led_eau.Update();

  if (currentTime - lastTime_maj > TEMPS_MAJ) {
    // commande relais suivant programmation -------
    lecture_val_switch(&val_chauf, &val_eau);
    // chauf ------
    if (recup_val_relais_chauf(val_chauf)) {
      digitalWrite(RELAIS_CHAUF, HIGH);
      if (heure == HEURE_PLEINE) {
        led_chauf.Blink(600, 600).Forever();
        Serial.println(F("led_chauf.blink"));
      }
      else {
        Serial.println(F("led_chauf.On"));
        led_chauf.On();
      }
    }
    else {
      digitalWrite(RELAIS_CHAUF, LOW);
      Serial.println(F("led_chauf.Off"));
      led_chauf.Off();
    }
    // eau -----
    if (recup_val_relais_eau(val_eau)) {
      digitalWrite(RELAIS_EAU, HIGH);
      if (heure == HEURE_PLEINE) {
        led_eau.Blink(600, 600).Forever();
        Serial.println(F("led_eau.blink"));
      }
      else {
        led_eau.On();
        Serial.println(F("led_eau.On"));
      }
    }
    else {
      digitalWrite(RELAIS_EAU, LOW);
      Serial.println(F("led_eau.Off"));
      led_eau.Off();
    }

    // commande led couleur jour
    switch (jour) {
      case JOUR_BLEU:
        digitalWrite(LED_BLEU, HIGH);
        digitalWrite(LED_BLANC, LOW);
        digitalWrite(LED_ROUGE, LOW);
        break;

      case JOUR_BLANC:
        digitalWrite(LED_BLEU, LOW);
        digitalWrite(LED_BLANC, HIGH);
        digitalWrite(LED_ROUGE, LOW);
        break;

      case JOUR_ROUGE:
        digitalWrite(LED_BLEU, LOW);
        digitalWrite(LED_BLANC, LOW);
        digitalWrite(LED_ROUGE, HIGH);
        break;
    }
    lastTime_maj = currentTime;
  }
}
