#include <Arduino.h>

// Первый (возможно, единственный) униполяр на ногах 2, 3, 4 и 5
//#define UNIPOLAR_1_WIRE_1 2
//#define UNIPOLAR_1_WIRE_2 4
//#define UNIPOLAR_1_WIRE_3 3
//#define UNIPOLAR_1_WIRE_4 5

// @todo implement
// Второй униполяр на ногах 2, 3, 4 и 5
//#define UNIPOLAR_2_WIRE_1 2
//#define UNIPOLAR_2_WIRE_2 4
//#define UNIPOLAR_2_WIRE_3 3
//#define UNIPOLAR_2_WIRE_4 5

// Один биполярный шаговик @todo implement
// Два биполярных шаговика @todo implement

// Отладка, если нужна
//#define DEBUG

#ifdef UNIPOLAR_1_WIRE_1
	#include <AccelStepper.h>

	// 4080 шагов на оборот. 0.75 оборота. 10% заступ для срабатывания концевика
	// @todo перевести в полушаг, он мощнее
	#define DISTANCE 4080 * 0.75 * 1.1
#endif

// Первый DC-мотор на ногах 2 и 3
#define DC_1_WIRE_1 2
#define DC_1_WIRE_2 3

// Второй DC-мотор на ногах 7 и 8
#define DC_2_WIRE_1 7
#define DC_2_WIRE_2 8

// концевики первого мотора на ногах 4 и 5
#define SWITCH_1_OPEN  5
#define SWITCH_1_CLOSE 4

// концевики второго мотора на ногах 9 и 10 (если есть второй мотор)
#define SWITCH_2_OPEN  9
#define SWITCH_2_CLOSE 10

// 20 секунд таймаут открытия / закрытия
#define TIMEOUT 25

// 2 секунды задержка реакции на команду второго мотора @todo implement
//#define GAP 2

// Реверс мотора 1 и 2, если нужен
#define REVERSE_MOTOR_1
#define REVERSE_MOTOR_2

/*
	СТАТУС представляет собой одну/две буквы и новую строку. Каждая буква передаёт состояние одного мотора:
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

#ifdef UNIPOLAR_1_WIRE_1
	AccelStepper motor1(4, UNIPOLAR_1_WIRE_1, UNIPOLAR_1_WIRE_2, UNIPOLAR_1_WIRE_3, UNIPOLAR_1_WIRE_4);
#endif
#ifdef UNIPOLAR_1_WIRE_2
	AccelStepper motor2(4, UNIPOLAR_2_WIRE_1, UNIPOLAR_2_WIRE_2, UNIPOLAR_2_WIRE_3, UNIPOLAR_2_WIRE_4);
#endif


void setup() {
	noInterrupts();

	#ifdef UNIPOLAR_1_WIRE_1
		pinMode(UNIPOLAR_1_WIRE_1, OUTPUT);
		pinMode(UNIPOLAR_1_WIRE_2, OUTPUT);
		pinMode(UNIPOLAR_1_WIRE_3, OUTPUT);
		pinMode(UNIPOLAR_1_WIRE_4, OUTPUT);

		motor1.setMaxSpeed(250.0);
		motor1.setAcceleration(50.0);
	#endif
	#ifdef UNIPOLAR_2_WIRE_1
		pinMode(UNIPOLAR_2_WIRE_1, OUTPUT);
		pinMode(UNIPOLAR_2_WIRE_2, OUTPUT);
		pinMode(UNIPOLAR_2_WIRE_3, OUTPUT);
		pinMode(UNIPOLAR_2_WIRE_4, OUTPUT);

		motor2.setMaxSpeed(250.0);
		motor2.setAcceleration(50.0);
	#endif

	#ifdef DC_1_WIRE_1
		pinMode(DC_1_WIRE_1, OUTPUT);
		pinMode(DC_1_WIRE_2, OUTPUT);
	#endif
	#ifdef DC_2_WIRE_1
		pinMode(DC_2_WIRE_1, OUTPUT);
		pinMode(DC_2_WIRE_2, OUTPUT);
	#endif

	pinMode(SWITCH_1_OPEN,  INPUT_PULLUP);
	pinMode(SWITCH_1_CLOSE, INPUT_PULLUP);

	#ifdef SWITCH_2_OPEN
		pinMode(SWITCH_2_OPEN,  INPUT_PULLUP);
		pinMode(SWITCH_2_CLOSE, INPUT_PULLUP);
	#endif

	Serial.begin(9600);

	TCCR1A = 0;
	TCCR1B = 0;

	TCNT1 = 31250; //34286;   // preload timer 65536-16MHz/256/2Hz
	TCCR1B |= (1 << CS12);    // 256 prescaler
	TIMSK1 |= (1 << TOIE1);   // enable timer overflow interrupt

	interrupts();             // enable all interrupts
}

byte cmd1 = NULL; // команда первому мотору
byte cmd2 = NULL; // команда второму мотору
byte status1 = 'U';
byte status2 = 'U';
byte timeout, gap;


void stop_motors(short motor = 0)
{
	if (motor == 0 || motor == 1) {
		#ifdef UNIPOLAR_1_WIRE_1
			motor1.setAcceleration(9999.0);
			motor1.stop();
		#endif

		#ifdef DC_1_WIRE_1
			digitalWrite(DC_1_WIRE_1, LOW);
			digitalWrite(DC_1_WIRE_2, LOW);
		#endif
	}

	if (motor == 0 || motor == 2) {
		#ifdef UNIPOLAR_2_WIRE_1
			motor2.setAcceleration(9999.0);
			motor2.stop();
		#endif

		#ifdef DC_2_WIRE_1
			digitalWrite(DC_2_WIRE_1, LOW);
			digitalWrite(DC_2_WIRE_2, LOW);
		#endif
	}
}


// раз в секунду вывожу текущий статус
ISR(TIMER1_OVF_vect) {
	TCNT1 = 31250; //34286;   // preload timer 65536-16MHz/256/2Hz

	#ifdef DEBUG
		Serial.print("status1=");
		Serial.write(status1);
		Serial.print(" status2=");
		Serial.write(status2);
		Serial.print(" cmd1=");
		Serial.write(cmd1);
		Serial.print(" cmd2=");
		Serial.write(cmd2);
		Serial.print(" timeout=");
		Serial.print(timeout);
		Serial.print(" open1=");
		Serial.print(digitalRead(SWITCH_1_OPEN));
		Serial.print(" close1=");
		Serial.print(digitalRead(SWITCH_1_CLOSE));
		Serial.print(" open2=");
		Serial.print(digitalRead(SWITCH_2_OPEN));
		Serial.print(" close2=");
		Serial.println(digitalRead(SWITCH_2_CLOSE));
		// ...
	#else
		Serial.write(status1);
		#ifdef DC_2_WIRE_1 || UNIPOLAR_2_WIRE_1
			Serial.write(status2);
		#endif
		Serial.println();
	#endif

	switch (cmd1) {
		case 'O':
		case 'C':
			if (timeout >= TIMEOUT) {
				#ifdef DEBUG
					Serial.println("got timeout 1");
				#endif

				stop_motors(1);

				status1 = 'E';
				cmd1    = NULL;
			}
			break;
	}

	#ifdef DC_2_WIRE_1 || UNIPOLAR_2_WIRE_1
		switch (cmd2) {
			case 'O':
			case 'C':
				if (timeout >= TIMEOUT) {
					#ifdef DEBUG
						Serial.println("got timeout 2");
					#endif

					stop_motors(2);

					status2 = 'E';
					cmd2    = NULL;
				}
				break;
		}
	#endif

	timeout++;
}


void loop() {
	noInterrupts();

	if (Serial.available() > 0) {
		switch (Serial.read()) {
			case 'O':
				#ifdef UNIPOLAR_1_WIRE_1
					motor1.setAcceleration(50.0);
					#ifdef REVERSE_MOTOR_1
						motor1.moveTo(motor1.currentPosition() - DISTANCE);
					#else
						motor1.moveTo(motor1.currentPosition() + DISTANCE);
					#endif
				#endif

				#ifdef DC_1_WIRE_1
					#ifdef REVERSE_MOTOR_1
						digitalWrite(DC_1_WIRE_1, LOW);
						digitalWrite(DC_1_WIRE_2, HIGH);
					#else
						digitalWrite(DC_1_WIRE_1, HIGH);
						digitalWrite(DC_1_WIRE_2, LOW);
					#endif
				#endif

				#ifdef UNIPOLAR_2_WIRE_1
					motor2.setAcceleration(50.0);
					#ifdef REVERSE_MOTOR_2
						motor1.moveTo(motor2.currentPosition() - DISTANCE);
					#else
						motor1.moveTo(motor2.currentPosition() + DISTANCE);
					#endif
				#endif

				#ifdef DC_2_WIRE_1
					#ifdef REVERSE_MOTOR_2
						digitalWrite(DC_2_WIRE_1, LOW);
						digitalWrite(DC_2_WIRE_2, HIGH);
					#else
						digitalWrite(DC_2_WIRE_1, HIGH);
						digitalWrite(DC_2_WIRE_2, LOW);
					#endif
				#endif

				cmd1 = 'O';
				#ifdef GAP
					cmd2 = 'G';
				#else
					cmd2 = 'O';
				#endif

				status1 = 'O';
				status2 = 'O';
				timeout = 0;
				gap     = 0;
				delay(300);
				break;

			case 'C':
				#ifdef UNIPOLAR_1_WIRE_1
					motor1.setAcceleration(50.0);
					#ifdef REVERSE_MOTOR_1
						motor1.moveTo(motor1.currentPosition() + DISTANCE);
					#else
						motor1.moveTo(motor1.currentPosition() - DISTANCE);
					#endif
				#endif

				#ifdef DC_1_WIRE_1
					#ifdef REVERSE_MOTOR_1
						digitalWrite(DC_1_WIRE_1, HIGH);
						digitalWrite(DC_1_WIRE_2, LOW);
					#else
						digitalWrite(DC_1_WIRE_1, LOW);
						digitalWrite(DC_1_WIRE_2, HIGH);
					#endif
				#endif

				#ifdef UNIPOLAR_2_WIRE_1
					motor2.setAcceleration(50.0);
					#ifdef REVERSE_MOTOR_2
						motor1.moveTo(motor2.currentPosition() + DISTANCE);
					#else
						motor1.moveTo(motor2.currentPosition() - DISTANCE);
					#endif
				#endif

				#ifdef DC_2_WIRE_1
					#ifdef REVERSE_MOTOR_2
						digitalWrite(DC_2_WIRE_1, HIGH);
						digitalWrite(DC_2_WIRE_2, LOW);
					#else
						digitalWrite(DC_2_WIRE_1, LOW);
						digitalWrite(DC_2_WIRE_2, HIGH);
					#endif
				#endif

				cmd1 = 'C';
				#ifdef GAP
					cmd2 = 'G';
				#else
					cmd2 = 'C';
				#endif

				status1 = 'C';
				status2 = 'C';
				timeout = 0;
				gap     = 0;
				delay(300);
				break;

			// case 'A': // имею в виду
			default:
				stop_motors();

				cmd1    = NULL;
				cmd2    = NULL;
				status1 = 'U';
				status2 = 'U';
		}
	}

	switch (cmd1) {
		case 'O':
			if (digitalRead(SWITCH_1_OPEN) == 0) {
				#ifdef DEBUG
					Serial.println("got open sw 1");
				#endif

				#ifdef UNIPOLAR_1_WIRE_1
					motor1.setAcceleration(9999.0);
					motor1.stop();
				#endif

				#ifdef DC_1_WIRE_1
					digitalWrite(DC_1_WIRE_1, LOW);
					digitalWrite(DC_1_WIRE_2, LOW);
				#endif

				status1 = 'o';
				cmd1    = NULL;
			}
			break;

		case 'C':
			if (digitalRead(SWITCH_1_CLOSE) == 0) {
				#ifdef DEBUG
					Serial.println("got close sw 1");
				#endif

				#ifdef UNIPOLAR_1_WIRE_1
					motor1.setAcceleration(9999.0);
					motor1.stop();
				#endif

				#ifdef DC_1_WIRE_1
					digitalWrite(DC_1_WIRE_1, LOW);
					digitalWrite(DC_1_WIRE_2, LOW);
				#endif

				status1 = 'c';
				cmd1    = NULL;
			}
			break;

		default:
			#ifdef UNIPOLAR_1_WIRE_1
				digitalWrite(UNIPOLAR_1_WIRE_1, LOW);
				digitalWrite(UNIPOLAR_1_WIRE_2, LOW);
				digitalWrite(UNIPOLAR_1_WIRE_3, LOW);
				digitalWrite(UNIPOLAR_1_WIRE_4, LOW);
			#endif

			#ifdef DC_1_WIRE_1
				digitalWrite(DC_1_WIRE_1, LOW);
				digitalWrite(DC_1_WIRE_2, LOW);
			#endif
	}

	#ifdef DC_2_WIRE_1 || UNIPOLAR_2_WIRE_1
		switch (cmd2) {
			case 'O':
				if (digitalRead(SWITCH_2_OPEN) == 0) {
					#ifdef DEBUG
						Serial.println("got open sw 2");
					#endif

					#ifdef UNIPOLAR_2_WIRE_1
						motor2.setAcceleration(9999.0);
						motor2.stop();
					#endif

					#ifdef DC_2_WIRE_1
						digitalWrite(DC_2_WIRE_1, LOW);
						digitalWrite(DC_2_WIRE_2, LOW);
					#endif

					status2 = 'o';
					cmd2    = NULL;
				}
				break;

			case 'C':
				if (digitalRead(SWITCH_2_CLOSE) == 0) {
					#ifdef DEBUG
						Serial.println("got close sw 2");
					#endif

					#ifdef UNIPOLAR_2_WIRE_1
						motor2.setAcceleration(9999.0);
						motor2.stop();
					#endif

					#ifdef DC_2_WIRE_1
						digitalWrite(DC_2_WIRE_1, LOW);
						digitalWrite(DC_2_WIRE_2, LOW);
					#endif

					status2 = 'c';
					cmd2    = NULL;
				}
				break;

			default:
				#ifdef UNIPOLAR_2_WIRE_1
					digitalWrite(UNIPOLAR_2_WIRE_1, LOW);
					digitalWrite(UNIPOLAR_2_WIRE_2, LOW);
					digitalWrite(UNIPOLAR_2_WIRE_3, LOW);
					digitalWrite(UNIPOLAR_2_WIRE_4, LOW);
				#endif

				#ifdef DC_2_WIRE_1
					digitalWrite(DC_2_WIRE_1, LOW);
					digitalWrite(DC_2_WIRE_2, LOW);
				#endif
		}
	#endif

	#ifdef UNIPOLAR_1_WIRE_1
		motor1.run();
	#endif

	#ifdef UNIPOLAR_2_WIRE_1
		motor2.run();
	#endif

	interrupts();
}
