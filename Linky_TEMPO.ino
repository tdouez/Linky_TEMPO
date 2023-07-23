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
// 2023/03/13 - FB V1.0.2 - Correction sur programmation CHAUF/EAU 
// 2023/03/19 - FB V1.0.3 - Correction sur lecture switch
// 2023/03/23 - FB V1.0.4 - Correction sur inversion sorties relais eau/chauf. Merci à Patrick F. pour son aide.
// 2023/04/21 - FB V1.0.5 - Correction sur la detection du mode TIC standard. Merci à Gilbert P. pour son aide.
// 2023/06/16 - FB V1.1.0 - Ajout visualisation couleur lendemain
// 2023/07/06 - FB V1.2.0 - Inversion ordre relais, les relais seront moins sollicités. Modif affichage detection des modes TIC, clignotement des leds rouges ou led bleu en plus de la led verte. 
// 2023/07/20 - FB V1.2.1 - Ajout traces debug. Inversion relais eau.
// 2023/07/22 - FB V1.2.2 - Correction détection passage HP/HC. Merci à Christophe D. pour son aide
//--------------------------------------------------------------------

#include <Arduino.h>
#include "LibTeleinfoLite.h"
#include <jled.h>

#define VERSION   "v1.2.2"

//#define FORCE_MODE_TIC		TINFO_MODE_HISTORIQUE
//#define FORCE_MODE_TIC		TINFO_MODE_STANDARD

#define DEBUG_TEMPO
//#define DEBUG_TEST
//#define DEBUG_TIC

#define LED_ROUGE 8
#define LED_BLANC 9
#define LED_BLEU  10
#define LED_EAU   2
#define LED_CHAUF 3 
#define LED_TIC   7

#define RELAIS_EAU   13
#define RELAIS_CHAUF 12 

#define SW_0   A0
#define SW_1   A1
#define SW_2   A2
#define SW_3   A3
#define SW_4   A4

#define TARIF_AUTRE 0
#define TARIF_TEMPO 1

#define HEURE_TOUTE    0
#define HEURE_CREUSE   1
#define HEURE_PLEINE   2

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


const unsigned long TEMPS_MAJ  =  10000; // 10s,  Minimum time between send (in milliseconds). 
unsigned long lastTime_maj = 0;
uint8_t tarif = TARIF_AUTRE;
uint8_t jour = JOUR_SANS;
uint8_t heure = HEURE_TOUTE;
uint8_t demain = JOUR_SANS;

auto led_chauf = JLed(LED_CHAUF); 
auto led_eau = JLed(LED_EAU);
auto led_bleu = JLed(LED_BLEU);
auto led_blanc = JLed(LED_BLANC);
auto led_rouge = JLed(LED_ROUGE);

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
  delay(300);
  Serial.println(F("LED blacnhe"));
  digitalWrite(LED_BLANC, HIGH);
  delay(300);
  Serial.println(F("LED bleue"));
  digitalWrite(LED_BLEU, HIGH);
  delay(300);
  Serial.println(F("LED TIC"));
  digitalWrite(LED_TIC, HIGH);
  delay(300);
  Serial.println(F("LED EAU"));
  digitalWrite(LED_EAU, HIGH);
  delay(300);
  Serial.println(F("LED CHAUF"));
  digitalWrite(LED_CHAUF, HIGH);
  delay(300);
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
    if (i<3) bitWrite(*val_chauf, i, !digitalRead(SW_4-i));
      else bitWrite(*val_eau, i-3, !digitalRead(SW_4-i));
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

  #ifdef DEBUG_TEMPO
    Serial.print(F("recup_val_relais_chauf "));
    Serial.print(val_CHAUF);
    Serial.print(",");
    Serial.print(jour);
    Serial.print(",");
    Serial.print(heure);
    Serial.println(".");
  #endif

  switch (val_CHAUF) {
    case CHAUF_0:
      Serial.println("CHAUF_0:1");
      rc=1;
      break;
      
    case CHAUF_1:
      Serial.print("CHAUF_1:");
      if (jour == JOUR_BLEU || jour == JOUR_BLANC || (jour == JOUR_ROUGE && heure == HEURE_CREUSE)) {
        Serial.println("1");
        rc=1;
      }
      else Serial.println("0");
      break;

    case CHAUF_2:
      Serial.print("CHAUF_2:");
      if (jour == JOUR_BLEU || jour == JOUR_BLANC) {
        Serial.println("1");
        rc=1;
      }
      else Serial.println("0");
      break;

    case CHAUF_3:
      Serial.print("CHAUF_3:");
      if (jour == JOUR_BLEU || (jour == JOUR_BLANC && heure == HEURE_CREUSE)) {
        Serial.println("1");
        rc=1;
      }
      else Serial.println("0");
      break;

    case CHAUF_4:
      Serial.print("CHAUF_4:");
      if (jour == JOUR_BLEU) {
        Serial.println("1");
        rc=1;
      }
      else Serial.println("0");
      break;

    case CHAUF_5:
      Serial.print("CHAUF_5:");
      if (jour == JOUR_BLEU && heure == HEURE_CREUSE) {
        Serial.println("1");
        rc=1;
      }
      else Serial.println("0");
      break;

    case CHAUF_6:
      Serial.println("CHAUF_6:0");
      rc=0;
      break;

    case CHAUF_C:
      Serial.print("CHAUF_C:");
      if ((jour == JOUR_BLEU || jour == JOUR_BLANC || jour == JOUR_ROUGE) && heure == HEURE_CREUSE) {
        Serial.println("1");
        rc=1;
      }
      else Serial.println("0");
      break;

    default:
      Serial.println("CHAUF_X:0");
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

  #ifdef DEBUG_TEMPO
    Serial.print(F("recup_val_relais_eau "));
    Serial.print(val_eau);
    Serial.print(",");
    Serial.print(jour);
    Serial.print(",");
    Serial.print(heure);
    Serial.println(".");
  #endif

  switch (val_eau) {

    case EAU_0:
      Serial.println("EAU_0:1");
      rc=1;
      break;

    case EAU_1:
      Serial.print("EAU_1:");
      if ((jour == JOUR_BLEU && heure == HEURE_CREUSE) || (jour == JOUR_BLANC && heure == HEURE_CREUSE) || (jour == JOUR_ROUGE && heure == HEURE_CREUSE)) {
        Serial.println("1"); 
        rc=1;
      }
      else Serial.println("0");
      break;
      
    case EAU_2:
      Serial.print("EAU_2:");
      if (jour == JOUR_BLEU || (jour == JOUR_BLANC && heure == HEURE_CREUSE) || (jour == JOUR_ROUGE && heure == HEURE_CREUSE)) {
        Serial.println("1");
        rc=1;
      }
      else Serial.println("0");
      break;

    case EAU_3:
      Serial.print("EAU_3:");
      if (jour == JOUR_BLEU || jour == JOUR_BLANC || (jour == JOUR_ROUGE && heure == HEURE_CREUSE)) {
        Serial.println("1");
        rc=1;
      }
      else Serial.println("0");
      break;

    default:
      Serial.println("EAU_X:0");
      break;
  }

  return rc;
}

// ---------------------------------------------------------------- 
// change_etat_led_teleinfo
// ---------------------------------------------------------------- 
void change_etat_led(uint8_t led)
{
  static uint8_t led_state;

  led_state = !led_state;
  digitalWrite(led, led_state);

}

// ---------------------------------------------------------------- 
// clignote_led
// ---------------------------------------------------------------- 
void clignote_led(uint8_t led, uint8_t nbr, int16_t delais)
{
int led_state=0;

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
int nbc_etiq=0;
int nbc_val=0;
_Mode_e mode;

  digitalWrite(LED_TIC, HIGH);
  digitalWrite(LED_BLANC, HIGH);
  
  // Test en mode historique
  // Recherche des éléments de début, milieu et fin de trame (0x0A, 0x20, 0x20, 0x0D)
  Serial.begin(1200); // mode historique
  Serial.println(F("Recherche mod TIC"));

  while (!flag_timeout && !flag_found_speed) {
    if (Serial.available()>0) {
      char in = (char)Serial.read() & 127;  // seulement sur 7 bits
      change_etat_led(LED_TIC);
	  
	  #ifdef DEBUG_TIC
		Serial.print(in, HEX);
		Serial.println(".");
	  #endif
      // début trame
      if (in == 0x0A) {
        step = 1;
        nbc_etiq = -1;
        nbc_val = 0;
        #ifdef DEBUG_TIC
          Serial.println(F("Deb 0x0A"));
        #endif
      }
	  
      // premier milieu de trame, étiquette
      if (step == 1) {
        if (in == 0x20) {
          #ifdef DEBUG_TIC
            Serial.print(F("Etq 0x20:"));
            Serial.println(nbc_etiq);
          #endif
          if (nbc_etiq > 3 && nbc_etiq < 10) step = 2;
            else step = 0;
        }
        else nbc_etiq++; // recupère nombre caractères de l'étiquette
      }
      else {
        // deuxième milieu de trame, valeur
        if (step == 2) {
          if (in == 0x20) {
            #ifdef DEBUG_TIC
              Serial.print(F("Val 0x20:"));
              Serial.println(nbc_val);
            #endif
            if (nbc_val > 0 && nbc_val < 13) step = 3;
              else step = 0;
          }
          else nbc_val++; // recupère nombre caractères de la valeur
        }
      }

      // fin trame
      if (step == 3 && in == 0x0D) {
        #ifdef DEBUG_TIC
          Serial.println(F("Fin 0x0D"));
        #endif
        flag_found_speed = true;
        step = 0;
      }
    }
    if (currentTime + 10000 <  millis()) {
      flag_timeout = true; // 10s de timeout
    }
  }

  digitalWrite(LED_BLANC, LOW);

  if (flag_timeout == true && flag_found_speed == false) { // trame avec vistesse histo non trouvée donc passage en mode standard
     mode = TINFO_MODE_STANDARD;
     Serial.end();
     Serial.begin(9600); // mode standard
     Serial.println(F(">> TIC mode standard <<"));
     clignote_led(LED_BLEU, 6, 300);
  }
  else {
    mode = TINFO_MODE_HISTORIQUE;
    Serial.println(F(">> TIC mode historique <<"));
    clignote_led(LED_ROUGE, 6, 300);
  }
  
  digitalWrite(LED_TIC, LOW);

  return mode;
}

// ---------------------------------------------------------------- 
// send_teleinfo
// ---------------------------------------------------------------- 
void send_teleinfo(char *etiq, char *val)
{
   
  change_etat_led(LED_TIC);

#ifdef DEBUG_TEMPO
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
	
	// Recup couleur lendemain
    if (strcmp(etiq, "DEMAIN") == 0) {
      Serial.print(F("DEMAIN:"));
      Serial.println(val);
	    demain=JOUR_SANS;

  	  if (strcmp(val, "BLEU") == 0) {
    		demain=JOUR_BLEU;
    		Serial.println(F("bleu"));
  	  }
  	  if (strcmp(val, "BLANC") == 0) {
    		demain=JOUR_BLANC;
    		Serial.println(F("blanc"));
  	  }
  	  if (strcmp(val, "ROUGE") == 0) {
    		demain=JOUR_ROUGE;
    		Serial.println(F("rouge"));
  	  }
	}
          
  }
  else {
    // TIC standard --------------------------------------------------------
    // Recup contrat TEMPO
    if (strcmp(etiq, "NGTF") == 0) {
      Serial.print(F("NGTF:"));
      Serial.println(val);
      if (strstr(val, "TEMPO") != NULL) {
        tarif = TARIF_TEMPO;
        Serial.println(F("S TARIF_TEMPO"));
      }
      else {
        tarif = TARIF_AUTRE;
        Serial.println(F("S TARIF_AUTRE"));
      }
    }

    // Recup HC/HP
    if (strcmp(etiq, "LTARF") == 0) {
      Serial.print(F("LTARF:"));
      Serial.println(val);
      heure=HEURE_TOUTE;

      if (strstr(val, "HP") != NULL) {
        heure=HEURE_PLEINE;
        Serial.println(F("HEURE_PLEINE"));
      }
      else if (strstr(val, "HC") != NULL) {
        heure=HEURE_CREUSE;
        Serial.println(F("HEURE_CREUSE"));
      } 
    }
    
    // Recup couleur jour
    if (strcmp(etiq, "STGE") == 0) {
      Serial.print(F("STGE:"));
      Serial.println(val);
  
      // couleur jour courant
      unsigned long long_stge = strtol(val, NULL, 16);
      long_stge = (long_stge >> 24) & B11;
      jour = (int)long_stge;
      Serial.print(F("Jour: "));
      Serial.print(jour);
  
      // couleur jour lendemain
      long_stge = strtol(val, NULL, 16);
      long_stge = (long_stge >> 26) & B11;
      demain = (int)long_stge;
      Serial.print(F(", Demain: "));
      Serial.println(jour);
     }
  }
}

// ---------------------------------------------------------------- 
// test_couleur_jour
// ---------------------------------------------------------------- 
void test_couleur_jour() {

  Serial.println("---- Chauffage -----");
  for (int chauf=CHAUF_0; chauf <= CHAUF_C; chauf++) {
    Serial.print("CHAUF");
    Serial.print(chauf);
    Serial.print(" ");
    for (jour = JOUR_BLEU; jour <= JOUR_ROUGE; jour++) {
      for (heure = HEURE_CREUSE; heure <= HEURE_PLEINE; heure++) {
        Serial.print(recup_val_relais_chauf(chauf));
        Serial.print(" ");
      }
    }
    Serial.println("");
  }

  Serial.println("---- Eau -----");
  for (int eau=EAU_0; eau <= EAU_3; eau++) {
    Serial.print("EAU");
    Serial.print(eau);
    Serial.print(" ");
    for (jour = JOUR_BLEU; jour <= JOUR_ROUGE; jour++) {
      for (heure = HEURE_CREUSE; heure <= HEURE_PLEINE; heure++) {
        Serial.print(recup_val_relais_eau(eau));
        Serial.print(" ");
      }
    }
    Serial.println("");
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
  #ifdef FORCE_MODE_TIC
	mode_tic = FORCE_MODE_TIC;
	if (mode_tic == TINFO_MODE_HISTORIQUE) {
		Serial.begin(1200);
		Serial.print(F(">> FORCE TIC mode historique <<"));
	}
	else {
		Serial.begin(9600);
		Serial.print(F(">> FORCE TIC mode standard <<"));
	}
  #else
	mode_tic = init_speed_TIC();
  #endif

  Serial.println(F("   __|              _/           _ )  |"));
  Serial.println(F("   _| |  |   ` \\    -_)   -_)    _ \\  |   -_)  |  |   -_)"));
  Serial.println(F("  _| \\_,_| _|_|_| \\___| \\___|   ___/ _| \\___| \\_,_| \\___|"));
  Serial.print(F("                                             "));
  Serial.println(VERSION);

  //test_couleur_jour();

   // init interface TIC
  tinfo.init(mode_tic);

  #ifdef DEBUG_TEST
  jour = JOUR_SANS;
  tarif = TARIF_TEMPO;
  demain = JOUR_SANS;
  Serial.println(F("!! ------ Mode Test --------- !!"));
  Serial.print("> Jour : ");
  Serial.println(jour);
  Serial.print("> Demain : ");
  Serial.println(demain);
  Serial.println("----------------------------");
  #endif

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
  led_bleu.Update();
  led_blanc.Update();
  led_rouge.Update();


  if (currentTime - lastTime_maj > TEMPS_MAJ) {

    #ifdef DEBUG_TEST
      // Test jour/tarif
      demain++;
      if (demain > JOUR_ROUGE) {
        demain = JOUR_SANS;
        jour++;
      }
      if (jour > JOUR_ROUGE) {
        jour = JOUR_SANS;
      }
      Serial.print("> Jour : ");
      Serial.println(jour);
      Serial.print("> Demain : ");
      Serial.println(demain);
      Serial.println("----------------------------");
    #endif

    // commande relais suivant programmation -------
    lecture_val_switch(&val_chauf, &val_eau);

    // chauf ------
    if (recup_val_relais_chauf(val_chauf) == 1) {
      digitalWrite(RELAIS_CHAUF, LOW);
	    Serial.println(F("RELAIS_CHAUF.LOW"));

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
	    Serial.println(F("RELAIS_CHAUF.HIGH"));
      digitalWrite(RELAIS_CHAUF, HIGH);
      Serial.println(F("led_chauf.Off"));
      led_chauf.Off();
    }
    
    // eau -----
    if (recup_val_relais_eau(val_eau) == 1) {
	    Serial.println(F("RELAIS_EAU.HIGH"));
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
	    Serial.println(F("RELAIS_EAU.LOW"));
      digitalWrite(RELAIS_EAU, LOW);
      Serial.println(F("led_eau.Off"));
      led_eau.Off();
    }

    // commande led couleur demain
    if (jour != JOUR_SANS) {
      switch (demain) {
        case JOUR_BLEU:
          led_bleu.Blink(600, 600).Forever();
          break;
        case JOUR_BLANC:
          led_blanc.Blink(600, 600).Forever();
          break;
        case JOUR_ROUGE:
          led_rouge.Blink(600, 600).Forever();
          break;
      }

      // commande led couleur jour
      switch (jour) {
        case JOUR_BLEU:
          if (demain != JOUR_BLEU) led_bleu.On();
          if (demain != JOUR_BLANC) led_blanc.Off();
          if (demain != JOUR_ROUGE) led_rouge.Off();
          break;

        case JOUR_BLANC:
          if (demain != JOUR_BLEU) led_bleu.Off();
          if (demain != JOUR_BLANC) led_blanc.On();
          if (demain != JOUR_ROUGE) led_rouge.Off();
          break;

        case JOUR_ROUGE:
          if (demain != JOUR_BLEU) led_bleu.Off();
          if (demain != JOUR_BLANC) led_blanc.Off();
          if (demain != JOUR_ROUGE) led_rouge.On();
          break;
      }
	  }
    else {
      led_bleu.Off();
      led_blanc.Off();
      led_rouge.Off();
    }

    lastTime_maj = currentTime;
  }
}
