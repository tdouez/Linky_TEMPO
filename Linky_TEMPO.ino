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
//--------------------------------------------------------------------

#include <Arduino.h>
#include <LibTeleinfo.h>
#include <jled.h>

#define VERSION   "v1.0.0 alpha"

#define LED_ROUGE 2
#define LED_BLANC 3
#define LED_BLEU  4
#define LED_EAU   5 
#define LED_CHAUF 6 
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

#define EAU_1   B000
#define EAU_2   B001
#define EAU_3   B010

const unsigned long TEMPS_MAJ  =  15000; // 10s,  Minimum time between send (in milliseconds). 
unsigned long lastTime_maj = 0;
unsigned int tarif = TARIF_AUTRE;
unsigned int jour = JOUR_SANS;
unsigned int heure = HEURE_TOUTE;

auto led_chauf = JLed(LED_CHAUF); 
auto led_eau = JLed(LED_EAU);

byte val_switch=0;
_Mode_e mode_tic;
TInfo tinfo;



// ---------------------------------------------------------------- 
// lecture_val_switch
// ----------------------------------------------------------------
void lecture_val_switch(byte *val_chauf, byte *val_eau)
{

  for (int i=0; i<5; i++) {
    if (i<3) bitWrite(*val_chauf, i, !digitalRead(i+SW_0));
      else bitWrite(*val_eau, i, !digitalRead(i+SW_0));
  }
}

// ---------------------------------------------------------------- 
// recup_val_relais_chauf
// ----------------------------------------------------------------
byte recup_val_relais_chauf(byte val_CHAUF)
{
byte rc=0;

  switch (val_CHAUF) {
    case CHAUF_0:
      rc=1;
      break;
      
    case CHAUF_1:
      if (jour == JOUR_BLEU || jour == JOUR_BLANC || (jour == JOUR_ROUGE && heure == HEURE_CREUSE)) {
        rc=1;
      }
      break;

    case CHAUF_2:
      if (jour != JOUR_ROUGE) {
        rc=1;
      }
      break;

    case CHAUF_3:
      if (jour == JOUR_BLEU || (jour = JOUR_BLANC && heure == HEURE_CREUSE)) {
        rc=1;
      }
      break;

    case CHAUF_4:
      if (jour == JOUR_BLEU) {
        rc=1;
      }
      break;

    case CHAUF_5:
      if (jour == JOUR_BLEU && heure == HEURE_CREUSE) {
        rc=1;
      }
      break;

    case CHAUF_6:
      rc=0;
      break;

    case CHAUF_C:
      if (heure == HEURE_CREUSE) {
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
    case EAU_1:
      if (heure == HEURE_CREUSE) {
        rc=1;
      }
      break;
      
    case EAU_2:
      if (jour == JOUR_BLEU || (jour == JOUR_BLANC && heure == HEURE_CREUSE) || (jour == JOUR_ROUGE && heure == HEURE_CREUSE)) {
        rc=1;
      }
      break;

    case EAU_3:
      if (jour == JOUR_BLEU || jour == JOUR_BLANC || (jour == JOUR_ROUGE && heure == HEURE_CREUSE)) {
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
     Serial.println(F("TIC mode standard"));
     clignote_led(LED_TIC, 3, 500);
  }
  else {
    mode = TINFO_MODE_HISTORIQUE;
    Serial.println(F("TIC mode historique"));
    clignote_led(LED_TIC, 5, 500);
  }
  
  digitalWrite(LED_TIC, LOW);

  return mode;
}

/* ======================================================================
Function: DataCallback 
Purpose : callback when we detected new or modified data received
Input   : linked list pointer on the concerned data
          current flags value
Output  : - 
Comments: -
====================================================================== */
void DataCallback(ValueList * me, uint8_t  flags)
{
   
  change_etat_led_teleinfo();

  if (mode_tic == TINFO_MODE_HISTORIQUE) {
    // TIC historique --------------------------------------------------------
    // Recup contrat TEMPO
    if (strstr(me->name, "OPTARIF") == 0) {
      if (strstr(me->value, "BBR") == 0) tarif = TARIF_TEMPO;
        else tarif = TARIF_AUTRE;
    }

    // Recup HC/HP et couleur jour
    if (strstr(me->name, "PTEC") == 0) {
      if (tarif == TARIF_TEMPO) {
        jour=JOUR_SANS;
        heure=HEURE_TOUTE;
        
        if (strstr(me->value, "HCJB") == 0) {
          jour=JOUR_BLEU;
          heure=HEURE_CREUSE;
        }
        if (strstr(me->value, "HCJW") == 0) {
          jour=JOUR_BLANC;
          heure=HEURE_CREUSE;
        }
        if (strstr(me->value, "HCJR") == 0) {
          jour=JOUR_ROUGE;
          heure=HEURE_CREUSE;
        }
        if (strstr(me->value, "HPJB") == 0) {
          jour=JOUR_BLEU;
          heure=HEURE_PLEINE;
        }
        if (strstr(me->value, "HPJW") == 0) {
          jour=JOUR_BLANC;
          heure=HEURE_PLEINE;
        }
        if (strstr(me->value, "HPJR") == 0) {
          jour=JOUR_ROUGE;
          heure=HEURE_PLEINE;
        }
      }
    }
          
  }
  else {
    // TIC standard --------------------------------------------------------
    // Recup contrat TEMPO
    if (strstr(me->name, "NGTF") == 0) {
      if (strstr(me->value, "BBR") == 0) tarif = TARIF_TEMPO;
        else tarif = TARIF_AUTRE;
    }

    // Recup HC/HP
    heure=HEURE_TOUTE;
    if (strstr(me->name, "NTARF") == 0) {
      if (strstr(me->value, "1") == 0) heure=HEURE_PLEINE;
        else if (strstr(me->value, "2") == 0) heure=HEURE_CREUSE;
    }
    
    // Recup couleur jour
    if (strstr(me->name, "STGE") == 0) {
        unsigned long long_stge = atol(me->value);
        long_stge = (long_stge >> 24) && B11;
        jour = (int)long_stge;
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

  digitalWrite(LED_ROUGE, HIGH);
  digitalWrite(LED_BLANC, HIGH);
  digitalWrite(LED_BLEU, HIGH);
  digitalWrite(LED_TIC, LOW);
  digitalWrite(RELAIS_EAU, LOW);
  digitalWrite(RELAIS_CHAUF, LOW);
  led_chauf.Off();
  led_eau.Off();

  // init interface série suivant mode TIC
  mode_tic = init_speed_TIC();

  Serial.println(F("   __|              _/           _ )  |"));
  Serial.println(F("   _| |  |   ` \\    -_)   -_)    _ \\  |   -_)  |  |   -_)"));
  Serial.println(F("  _| \\_,_| _|_|_| \\___| \\___|   ___/ _| \\___| \\_,_| \\___|"));
  Serial.print(F("                                             "));
  Serial.println(VERSION);

   // init interface TIC
  tinfo.init(mode_tic);
  tinfo.attachData(DataCallback);

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
    
    if (recup_val_relais_chauf(val_chauf)) {
      digitalWrite(RELAIS_CHAUF, HIGH);
      if (heure == HEURE_PLEINE) led_chauf.Blink(600, 600).Forever();
        else led_chauf.On();
    }
    else {
      digitalWrite(RELAIS_CHAUF, LOW);
      led_chauf.Off();
    }
    
    if (recup_val_relais_eau(val_eau)) {
      digitalWrite(RELAIS_EAU, HIGH);
      if (heure == HEURE_PLEINE) led_eau.Blink(600, 600).Forever();
        else led_eau.On();
    }
    else {
      digitalWrite(RELAIS_EAU, LOW);
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