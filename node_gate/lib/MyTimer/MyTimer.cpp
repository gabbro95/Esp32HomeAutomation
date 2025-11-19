/*
 * MyTimer.cpp - Implementazione della classe MyTimer
 *
 * Questo file contiene l'implementazione dei metodi della classe MyTimer.
 * Include Arduino.h per accedere a funzioni come millis().
*/

#include <Arduino.h> // Necessario per millis() e altre funzioni Arduino
#include "MyTimer.h" // Include la dichiarazione della classe MyTimer

// Costruttore della classe MyTimer
// Inizializza il timer come inattivo
MyTimer::MyTimer() {
  attivo = 0;
}

// Metodo per impostare il timer
// n: durata del timer in millisecondi
void MyTimer::set(int n){
  tempo = n; // Imposta la durata
  t1 = millis(); // Registra il tempo di inizio corrente
  attivo = 1; // Attiva il timer
}

// Metodo per controllare se il timer è scaduto
// Restituisce 1 se il tempo impostato è trascorso, 0 altrimenti
int MyTimer::check(){
  int ret = 0; // Valore di ritorno predefinito (non scaduto)
  if (attivo == 1) { // Se il timer è attivo
    unsigned long dt = millis() - t1; // Calcola il tempo trascorso
    if (dt >= tempo) { // Se il tempo trascorso è maggiore o uguale alla durata impostata
      ret = 1; // Il timer è scaduto
      attivo = 0; // Disattiva il timer (si resetta automaticamente dopo la scadenza)
    }
  } else {
    ret = 0; // Il timer non è attivo, quindi non può scadere
  }
  return ret; // Restituisce lo stato aggiornato
}

// Implementazione del metodo isSet() (correzione)
// Restituisce 1 se il timer è attualmente in esecuzione, 0 altrimenti
int MyTimer::isSet() {
  return attivo;
}

// Implementazione del metodo resetSet()
void MyTimer::resetSet() {
   attivo = 0;
}
