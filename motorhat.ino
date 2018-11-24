#include <AccelStepper.h>

// униполяр на ногах 2, 3, 4 и 5
#define STEP1 2
#define STEP2 4
#define STEP3 3
#define STEP4 5

//#define DEBUG

// концевики на ногах 6 и 7
#define OPEN_SW 7
#define CLOSE_SW 6

// 4080 шагов на оборот. 0.75 оборота. 10% заступ для срабатывания концевика
// @todo перевести в полушаг, он мощнее
#define DISTANCE 4080 * 0.75 * 1.1

// 10 секунд таймаут
#define TIMEOUT 20

/*
  СТАТУС:
  o = открыто
  c = закрыто
  C = движется к закрытию
  O = движется к открытию
  E = сбой (обычно, таймаут)
  U = не определено (двигалась куда-то, получила отмену. Или сразу после старта) @todo eeprom?

  КОМАНДА:
  O = открыть
  С = закрыть
  A = отмена
*/

AccelStepper motor(4, STEP1, STEP2, STEP3, STEP4);


void setup() {
  noInterrupts();

  motor.setMaxSpeed(250.0);
  motor.setAcceleration(50.0);

  pinMode(OPEN_SW, INPUT_PULLUP);
  pinMode(CLOSE_SW, INPUT_PULLUP);

  Serial.begin(9600);

  TCCR1A = 0;
  TCCR1B = 0;

  TCNT1 = 31250; //34286;   // preload timer 65536-16MHz/256/2Hz
  TCCR1B |= (1 << CS12);    // 256 prescaler
  TIMSK1 |= (1 << TOIE1);   // enable timer overflow interrupt

  interrupts();             // enable all interrupts
}

byte cmd = NULL;
byte status = 'U';
byte timeout;

// раз в секунду вывожу текущий статус
ISR(TIMER1_OVF_vect) {
  TCNT1 = 31250; //34286;   // preload timer 65536-16MHz/256/2Hz

#ifdef DEBUG
  Serial.write("status=");
  Serial.write(status);
  Serial.print(" timeout=");
  Serial.print(timeout);
  Serial.print(" open=");
  Serial.print(digitalRead(OPEN_SW));
  Serial.print(" close=");
  Serial.println(digitalRead(CLOSE_SW));
#else
  Serial.write(status);
#endif

  switch (cmd) {
    case 'O':
    case 'C':
      if (timeout++ >= TIMEOUT) {
#ifdef DEBUG
        Serial.println("got timeout");
#endif
        motor.setAcceleration(9999.0);
        motor.stop();
        status = 'E';
        cmd    = NULL;
      }
      break;
  }

}

void loop() {

  noInterrupts();

  if (Serial.available() > 0) {
    byte input = Serial.read();

    switch (input) {
      case 'O':
        motor.setAcceleration(50.0);
        motor.moveTo(motor.currentPosition() - DISTANCE);
        cmd    = 'O';
        status = 'O';
        timeout = 0;
        break;

      case 'C':
        motor.setAcceleration(50.0);
        motor.moveTo(motor.currentPosition() + DISTANCE);
        cmd    = 'C';
        status = 'C';
        timeout = 0;
        break;

      // case 'A': // имею в виду
      default:
        motor.setAcceleration(9999.0);
        motor.stop();
        cmd    = NULL;
        status = 'U';
    }
  }

  //Serial.print("open=");
  //Serial.println(digitalRead(OPEN_SW));
  //Serial.print("close=");
  //Serial.println(digitalRead(CLOSE_SW));
  switch (cmd) {
    case 'O':
      if (digitalRead(OPEN_SW) == 1) {   // обычно 0, но там микрик сдох :)
#ifdef DEBUG
        Serial.println("got open sw");
#endif
        motor.setAcceleration(9999.0);
        motor.stop();
        status = 'o';
        cmd    = NULL;
      }
      break;

    case 'C':
      if (digitalRead(CLOSE_SW) == 0) {
#ifdef DEBUG
        Serial.println("got close sw");
#endif
        motor.setAcceleration(9999.0);
        motor.stop();
        status = 'c';
        cmd    = NULL;
      }
      break;

    default:
      digitalWrite(STEP1, 0);
      digitalWrite(STEP2, 0);
      digitalWrite(STEP3, 0);
      digitalWrite(STEP4, 0);
  }

  motor.run();

  interrupts();
}
