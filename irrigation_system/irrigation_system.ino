#include <Wire.h> // Библиотека для I2C
#include <LiquidCrystal_I2C.h> // Подключение библиотеки для дисплея 1602

const int sensorPin1 = A0;  // Пин для первого датчика влажности
const int sensorPin2 = A1;  // Пин для второго датчика влажности
const int pumpPin = 2;       // Пин для насоса 
const int thresholdHigh = 600; // Порог включения насоса
const int maxSensorValue = 900;
const int minSensorValue = 100;

LiquidCrystal_I2C lcd(0x27, 16, 2);

bool pumpOn = false; // Состояние насоса
bool sensor1Valid = true;
bool sensor2Valid = true;
unsigned long pumpStartTime = 0; // Время включения насоса
unsigned long pumpCooldownStartTime = 0; // Время начала паузы после выключения
unsigned long lastUpdateTime = 0; // Время последнего обновления
const unsigned long updateInterval = 10000; // Интервал обновления измерений
const unsigned long wateringInterval = 9000;
bool pumpCooldown = false; // Флаг состояния блокировки насоса
bool firstRun = true; // Флаг для отслеживания первого запуска


void setup() {
  Serial.begin(9600);
  pinMode(pumpPin, OUTPUT);
  lcd.init();
  lcd.backlight();
}

void loop() {
  unsigned long currentMillis = millis();

  // Обновление показаний датчиков по таймеру
  if (currentMillis - lastUpdateTime >= updateInterval) {
    lastUpdateTime = currentMillis;

    // Считывание значений с датчиков
    int sensorValue1 = analogRead(sensorPin1);
    int sensorValue2 = analogRead(sensorPin2);

    // Вывод значений датчиков на первой строке дисплея
    if (!pumpOn) { // Дисплей включён только, если насос не работает
      lcd.setCursor(0, 0);
      lcd.print("S1:");
      lcd.print(sensorValue1);
      lcd.print(" S2:");
      lcd.print(sensorValue2);
      lcd.print("  ");
      Serial.println("Sensor 1: ");
      Serial.println(sensorValue1);
      Serial.println("Sensor 2: ");
      Serial.println(sensorValue2);


    }

    // Проверка значений и определение состояния датчиков
    checkSensors(sensorValue1, sensorValue2);

    // Управление насосом в зависимости от состояния
    managePump(sensorValue1, sensorValue2);
  }

  // Управление насосом при включенном состоянии (автоматическое выключение через n секунд)
  if (pumpOn && (millis() - pumpStartTime >= wateringInterval)) {
    digitalWrite(pumpPin, HIGH); // Выключение насоса 
    Serial.println("Pump off");
    pumpOn = false;
    pumpCooldown = true; // Устанавливаем блокировку
    pumpCooldownStartTime = millis(); // Запоминаем время начала блокировки
    lcd.backlight(); // Включение подсветки дисплея
    updateDisplayStatus(); // Обновляем статус на дисплее
  }

  // Снятие блокировки насоса через n секунд
  if (pumpCooldown && (millis() - pumpCooldownStartTime >= 10000)) {
    pumpCooldown = false; // Снимаем блокировку
  }
}

void checkSensors(int sensorValue1, int sensorValue2) {
  sensor1Valid = (sensorValue1 <= maxSensorValue && sensorValue1 >= minSensorValue);
  sensor2Valid = (sensorValue2 <= maxSensorValue && sensorValue2 >= minSensorValue);

  // Вывод ошибок на вторую строку дисплея, если насос не работает
  if (!pumpOn) {
    if (!sensor1Valid && !sensor2Valid) {
      lcd.setCursor(0, 1);
      lcd.print("Err: Both Sensrs");
    } else if (!sensor1Valid) {
      lcd.setCursor(0, 1);
      lcd.print("Err: Sensor 1   ");
    } else if (!sensor2Valid) {
      lcd.setCursor(0, 1);
      lcd.print("Err: Sensor 2   ");
    } else {
      // Если ошибок нет и насос не работает, показываем "Стабильно"
      lcd.setCursor(0, 1);
      lcd.print("All Stable      ");
    }
  }
}

void managePump(int sensorValue1, int sensorValue2) {
  // Если оба датчика находятся в ошибочном состоянии, отключаем насос и выходим
  if (!sensor1Valid && !sensor2Valid) {
    digitalWrite(pumpPin, HIGH); // Отключение насоса
    pumpOn = false;
    pumpCooldown = false; // Сбрасываем блокировку насоса
    return; 
  }

  // Если один из датчиков невалиден, управление насосом будет производиться только по валидному датчику
  if (!sensor1Valid && sensor2Valid) {
    controlPump(sensorValue2);
  } else if (sensor1Valid && !sensor2Valid) {
    controlPump(sensorValue1);
  } else if (sensor1Valid && sensor2Valid) {
    int avgValue = (sensorValue1 + sensorValue2) / 2;
    controlPump(avgValue);
  } else {
    // В любом другом случае насос отключается
    digitalWrite(pumpPin, HIGH);
    pumpOn = false;
  }
}

void controlPump(int sensorValue) {
  if (firstRun) {
    firstRun = false; // После первого запуска сбрасываем флаг
    pumpOn = false; 
    digitalWrite(pumpPin, HIGH); 
    return; 
  }

  // Включение насоса при превышении порога и отсутствии блокировки
  if (sensorValue > thresholdHigh && !pumpOn && !pumpCooldown) {
    digitalWrite(pumpPin, LOW); // Включение насоса 
    Serial.println("Watering is on");
    pumpOn = true;
    pumpStartTime = millis(); // Запоминаем время включения
    //lcd.noBacklight(); // Отключение подсветки дисплея
  }

  // Выключение насоса через определённое время или по другому условию
  if (pumpOn && (millis() - pumpStartTime >= wateringInterval)) { 
    digitalWrite(pumpPin, HIGH); // Выключение насоса 
    Serial.println("Watering is off");
    pumpOn = false;
    pumpCooldown = true; // Устанавливаем блокировку
    pumpCooldownStartTime = millis(); // Запоминаем время начала блокировки
    //lcd.backlight(); // Включение подсветки дисплея при выключении насоса
  }
}


void updateDisplayStatus() {
  // Обновление статуса на дисплее после выключения насоса
  if (!sensor1Valid && !sensor2Valid) {
    lcd.setCursor(0, 1);
    lcd.print("Err: Both Sensrs");
  } else if (!sensor1Valid) {
    lcd.setCursor(0, 1);
    lcd.print("Err: Sensor 1   ");
  } else if (!sensor2Valid) {
    lcd.setCursor(0, 1);
    lcd.print("Err: Sensor 2   ");
  } else {
    lcd.setCursor(0, 1);
    lcd.print("All Stable      ");
  }
}
