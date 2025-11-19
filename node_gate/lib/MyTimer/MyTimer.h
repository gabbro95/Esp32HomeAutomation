/*
 * MyTimer.h - Dichiarazione della classe MyTimer
 *
 * Questa libreria fornisce un semplice timer non bloccante.
 *
 * Utilizzo:
 * #include "MyTimer.h"
 * MyTimer t1 = MyTimer();
 *
 * t1.set(1000); // Imposta il timer a 1000ms (1 secondo)
 *
 * if (t1.check()) {
 * // Il tempo è scaduto
 * t1.set(1000); // Riavvia il timer
 * }
*/

#ifndef mytimer_h
#define mytimer_h

// Dichiarazione della classe MyTimer
class MyTimer {
  private:
  int tempo; // Durata del timer in millisecondi
  unsigned long t1; // Tempo di inizio del timer
  int attivo; // Flag per indicare se il timer è attivo (1) o inattivo (0)

  public:
  // Costruttore della classe
  MyTimer();

  // Metodo per impostare il timer con una durata 'n' in ms
  void set(int n);

  // Metodo per controllare se il timer è scaduto
  // Restituisce 1 se scaduto e si resetta, 0 altrimenti
  int check();

  // Metodo per controllare se il timer è attivo (correzione)
  // Restituisce 1 se attivo, 0 altrimenti
  int isSet();

  // Metodo per resettare il timer
  void resetSet();
};

#endif
