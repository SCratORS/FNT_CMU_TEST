#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
	#if (SSD1306_LCDHEIGHT != 32)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif
#define OLED_RESET 12					//Пин для сброса, по факту нахер не нужен в текущем конфиге, но либа просит. 12 пин на схеме в воздухе
Adafruit_SSD1306 display(OLED_RESET);

#define LOG_OUT 1	// use the log output function
#define FHT_N 128	// set to 128 point fht
#define FNT_C FHT_N / 2
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#include <FHT.h>
#define LowChL		0					//Диапазоны частот
#define LowChH		5					
#define MidChL		6
#define MidChH		24
#define HigChL		25
#define HigChH		64					//Диапазоны частот
#define AUDIO_IN	A0 					//Сюда подключен аудио вход
#define ZERO_DETECT	2					//Сюда подключен 220 из оптопары, для определения нуля в сети
#define ENCODER_A	4					//Энкодер, канал А
#define ENCODER_B	7					//Энкодер, канал Б
#define ENCODER_SW	8					//Энкодер, кнопка
//Настройки меню, значения типов, некоторые ид менюшек
#define T_MENU		0x01
#define T_VALUE		0x02
#define T_BACK		0xFF
#define VT_NUMBER	0x00
#define VT_MENU		0x00
#define VT_LIST		0x01
#define M_ID_INP	0x0B
#define M_ID_PWM	0x0C
//Настройки событий энкодера, значаения типов
#define KEY_NONE	0x00
#define KEY_PREV	0x01
#define KEY_NEXT	0x02
#define KEY_SELECT	0x03
uint8_t uiKeyCodeFirst	= KEY_NONE;
uint8_t uiKeyCodeSecond	= KEY_NONE;
uint8_t uiKeyCode		= KEY_NONE;
uint8_t last_key_code	= KEY_NONE;

const char AUTO[]   = "AUTO";				//Просто текс, который отображается на главном экране, объявлен тут, что бы в оперативке место не занимать
const char SMOOTH[] = "SMOOTH";       //Просто текс, который отображается на главном экране, объявлен тут, что бы в оперативке место не занимать
//---------------------Дисплей и меню----------------------------------
bool	menu_redraw_required = false;	//Признак того, что нужно перерисовать дисплей
#define	SCREEN_LINES	3   			//Количество строк на экране
#define	MENU_ITEM_COUNT	13   			//Обшее количество строк меню
struct	item {							//Тип структуры меню
	char 	*caption[1];				//Заголовок сделан таким образом, что бы не надо было байты считать, а тип string дофига памяти занимает
	uint8_t id;
	uint8_t type;
	uint8_t parent;
	uint8_t pos;
	uint8_t child;
	uint8_t valueType;
	char 	*valueData[3];				//Аналогичная фигня
	uint8_t minValue;
	uint8_t maxValue;
};
uint8_t page[SCREEN_LINES];				//Грубо говоря - это ID менюшек на видимой части экрана. 
const item mainMenu[MENU_ITEM_COUNT] = {
  /*
    id -	id менюшки уникально для каждого
    type -	T_MENU - есть подменю, T_VALUE - значение, T_BACK - назад в предменю
    par -	id родительского меню T_MENU т.е. кому принадлежит
    pos -	позиция элемента в меню, нужно для корректной работы скрола
    chi -	id родительского меню, в которое выходим. применимол только для T_BACK, 255 - выход в главный экран
    v_tp -	тип значений, VT_MENU - для входа в подменю, VT_LIST - Список текстовых значений, VT_NUMBER - просто число
    v_dt -	массив текстовых значений для VT_LIST, всего доступно 3 позиции, если надо больше, меняй struct item char *valueData[3 <- вот это значение];
    min -	минимальное значение,  применимо для VT_LIST и VT_NUMBER
    max -	максимальное значение,  применимо для VT_LIST и VT_NUMBER
     
    P.S.	Не пытайся хитрить, структура меню жесткая, минимальные и максимальные значения привязаны к типам, т.е. если Значение будет больше 255,
     		то будет выход за пределы типа byte, в лучшем случае получишь 0. В структуре VT_LIST, не пытайся делать значение больше чем количество
     		элементов, получишь глюк, т.к. если у тебя указано только 2 значения в листе - напр. Да и Нет, то максимальное будет 1, а минималка всегда 0,
     		попытка указания максимума больше - будет глюк. Также в словах в листе используй не более 3 букв,
     		т.к. в менюшке выделено расстояние для 3 знаков
  */
  //caption,            id,   type,     par,  pos,  chi,  v_tp,     v_dt    min   max
  {{"Channel levels"},    1,  T_MENU,   0,    0,    0,    VT_MENU,   {},    0,    0},
    {{"Peak down"},       2,  T_VALUE,  1,    0,    0,    VT_LIST,   {"OFF", "ON"}, 0, 1},
    {{"General level"},   3,  T_VALUE,  1,    1,    0,    VT_NUMBER, {},    0,    255},
    {{"Low"},             4,  T_VALUE,  1,    2,    0,    VT_NUMBER, {},    0,    255},
    {{"Middle"},          5,  T_VALUE,  1,    3,    0,    VT_NUMBER, {},    0,    255},
    {{"High"},            6,  T_VALUE,  1,    4,    0,    VT_NUMBER, {},    0,    255},
    {{"Average freq"},    7,  T_VALUE,  1,    5,    0,    VT_NUMBER, {},    0,    255},
    {{"Direct freq"},     8,  T_VALUE,  1,    6,    0,    VT_NUMBER, {},    0,    255},
    {{"Direct vol"},      9,  T_VALUE,  1,    7,    0,    VT_NUMBER, {},    0,    255},
    {{"Back"},            10, T_BACK,   1,    8,    0,    VT_MENU,   {},    0,    0},
  {{"Input Mode"},  M_ID_INP, T_VALUE,  0,    1,    0,    VT_LIST,   {"FFT", "COM", "LED"}, 0, 2},
  {{"Smooth Mode"}, M_ID_PWM, T_VALUE,  0,    2,    0,    VT_LIST,   {"OFF", "ON"}, 0, 1},
  {{"Exit menu"},         13, T_BACK,   0,    3,    255,  VT_MENU,   {},    0,    0}
};
uint8_t menuValues[MENU_ITEM_COUNT] = {0, 1, 255,100,60,60,200,128,255,0,0,0,0}; //Значения переменных меню
uint8_t parentId;		//Какому меню принадлежит субМеню
uint8_t selectId;		//Текущее меню - глобальная позиция в массиве
uint8_t scroll;			//Скрол
uint8_t maxScroll;		//Максимальное значение скрола
uint8_t cursorPos;		//Текущее положение курсора
uint8_t offsetPages;	//Смещение страниц меню
bool    value_change;	//Признак редактирования значения меню
//---------------------Дисплей и меню----------------------------------

uint16_t maxValueFHT[FNT_C] = {504, 340, 181, 162, 153,
111, 130, 111, 140, 127, 119, 101, 120, 93, 72, 81, 80,
70, 66, 63, 49, 49, 52, 57, 54, 68, 88, 60, 42, 53, 51,
58, 44, 51, 39, 46, 52, 64, 49, 47, 51, 45, 46, 48, 63,
63, 38, 48, 55, 44, 41, 46, 33, 46, 41, 34, 49, 60, 44,
49, 35, 42, 38, 44};//Массив максимальных значений анализатора

//Настройки ламп
uint8_t 			step_down		= 20;					//Скорость затухания ламп
uint8_t 			backLight		= 0;					//Значение канала ОПГ
uint8_t 			pwmValue[6];							//Массив значений для ШИМ
const uint8_t 		pwmLeds[] 		= {3,5,6,9,10,11};		//Пины Ламп
volatile uint8_t 	pwmCounter;								//Счетчик для AC ШИМа
bool				pwmEnable;								//Флаг разрешения АС ШИМа

//Переменные для работы
uint16_t			generalVolume;							//Общая громкость
uint16_t			generalMaxVolume;						//Максимальная громкость
uint16_t 			peak;									//Ограничитель семплов, расчитыватся от выбраного значение General Level
int8_t        aniRight = 6;
int8_t        aniLeft  = -1;
bool          smoothPPSACh;
uint16_t       skeepFramesRight;
uint16_t       skeepFramesLeft;

void setup() {
	Serial.begin(500000);
	sbi(ADCSRA, ADPS2); cbi(ADCSRA, ADPS1); sbi(ADCSRA, ADPS0); //ускорение DAC
	maxMenuCount();												//Посчитаем максимальное количество менюшек в текущем меню
	TIMSK2 = 0x00;												//Отключаем лишние прерывания на 2 таймере
	TCCR2A = 0x02;												//Режим сброса при совпадении, работает только на А канале
	TCCR2B = 0x02;												//Делитель на 8
	OCR2A  = 0x4F;												//Счетчик будет считать до 79 (80 тактов). 
																//16000000 / 8 = 2000000, 1/2000000 = 0,5мкс * 80 = 40 мкс * 250 (макс знач. ШИМ) = 10мс.

																//В розетке 50 Гц, значит период 20мс, за это время синус проходит 2 раза через 0, значит
																//каждые 10мс значение = 0, что вызывает прерывание детектора нуля.
																//т.е. 10мс - это период для ШИМа, за который надо успеть досчитать до максимума в 250.  
	TIMSK2 |= (1 << OCIE2A);									//Разрешить прерывание по таймеру 2 канал А - Для ШИМ
	TIMSK0 |= (1 << OCIE0A);									//Разрешить прерывание по таймеру 0 канал А	- Для Энкодера
	attachInterrupt(0, detectZeroInterrupt, RISING);			//Прерывание по детектору нуля, на повышение напруги.
	analogReference(INTERNAL);									//Опорка для DAC
	pinMode(ENCODER_SW, INPUT);
	pinMode(ENCODER_A,  INPUT);
	pinMode(ENCODER_B,  INPUT);
	pinMode(AUDIO_IN,   INPUT);
	pinMode(ZERO_DETECT,INPUT);
	for (uint8_t i = 0; i < 6; i++) pinMode(pwmLeds[i], OUTPUT);
	parentId  = 255;											//По дефолту - главный экран, а не меню
	menu_redraw_required = true;								//Дисплей надо отрисовать
	peak = map(menuValues[2], 0x00, 0xFF, 0x00, 0x4FF);			//Устанавливаем ограничитель семплов мапим его от General Level, 0-255 -> 0-1280, делаем больше 1024, чтоб был запас по срезу
	display.begin(SSD1306_SWITCHCAPVCC, 0x3C);					//Инициализация дисплея
	display.clearDisplay();  									//Очистка дисплея
	display.setRotation(2);  									//Ориентация дисплея
  
}
ISR (TIMER2_COMPA_vect) { //AC PWM
	if (pwmEnable) {
		for (uint8_t i = 0; i < 6; i++) if (pwmCounter == (0xFF - pwmValue[i]) && pwmValue[i] != 0x00) digitalWrite(pwmLeds[i],HIGH);
		pwmCounter ++;
	}
}
ISR (TIMER0_COMPA_vect) { //Encoder lisner
	uiStep();
	encoderHandler();
}

void detectZeroInterrupt() {
	pwmEnable = false;
	pwmCounter = 0x00;
	for (uint8_t i = 0; i < 6; i++) digitalWrite(pwmLeds[i],LOW);
	pwmEnable = true;
}

void loop() {
	switch (menuValues[10]) {
		case 0: calculation();
				analyzeAudio();
		break;
		case 1: comLed();
		break;
		case 2: staticLed();
		break;
	}
	if (menu_redraw_required) draw();
}

/* Обработка энкодера, определение кода события */
void uiStep(void) {
	uiKeyCodeSecond = uiKeyCodeFirst;
	if (digitalRead(ENCODER_A)) digitalRead(ENCODER_B) ? uiKeyCodeFirst = KEY_NEXT : uiKeyCodeFirst = KEY_PREV;
	else (digitalRead(ENCODER_SW) ) ? uiKeyCodeFirst = KEY_SELECT : uiKeyCodeFirst = KEY_NONE;
	(uiKeyCodeSecond == KEY_NONE) ? uiKeyCode = uiKeyCodeFirst :  uiKeyCode = KEY_NONE;
}

void calculation() {
    if (menuValues[M_ID_PWM - 1])  {//Если включена плавность, то делаем плавное затухание ламп
  	for (uint8_t i = 0; i < 6; i++) {
  		if (i == 3 && smoothPPSACh) {           //Если лампа ППСАЧ, то она пущай затухаем медленнее, для эпичности
  			if (pwmValue[i] > (step_down / 5) + backLight) {
  			      pwmValue[i] -= (step_down / 5);
  			    } else {
  			      pwmValue[i] = backLight;
  			      smoothPPSACh = false;
  			    }
  		} else {
  			pwmValue[i] > step_down + backLight ? pwmValue[i] -= step_down : pwmValue[i] = backLight;
  		} //Затухание ламп
  	}
  } else {
    for (uint8_t i = 0; i < 6; i++) if (pwmValue[i] < backLight) pwmValue[i] = backLight;
  }
  if (backLight < (step_down / 5) + 0x50/* <- 0x50 - Это максимальный уровень яркости фона*/) backLight += (step_down / 5);//И постоянно пытаемся плавно набрать яркость фоновой подсветки, но когда играет музыка, то ППГ будет её постояно сбрасывать в 0
	peak = map(menuValues[2], 0x00, 0xFF, 0x00, 0x4FF); //Мапим ПИК generalLevel до значений 0-1279, это запас, на случай если входной уровень будет очень высокий, хотя по факту работает эта штука хреново. 
}

void staticLed() {
	for (uint8_t i = 0; i < 6; i++ ) pwmValue[i] = menuValues[i+3];
}

void comLed() {
	if (Serial.available() > 0) {
		uint8_t incomingByte = Serial.read();
		for (uint8_t i = 0; i < 6; i++) if ((incomingByte >> i) & 0x01) pwmValue[i] = 0xFF;
	}
}

void encoderSwitchPress() {
	if (parentId == 255) {parentId = 0;maxMenuCount();} else { //Если нажали из главного экрана - попали в меню
		switch (mainMenu[selectId - 1].type) {
			case T_MENU: parentId = mainMenu[selectId - 1].id;
				maxMenuCount();
			break;
			case T_BACK: parentId = mainMenu[selectId - 1].child;
				maxMenuCount();
			break;
			case T_VALUE: value_change = !value_change;
			break;
		}
	}
}

void encoderRotateLeft() {
	if (parentId == 255) {
		if (menuValues[M_ID_INP - 1] == 0 && menuValues[2] > 0) menuValues[2]--;
	} else {
		if (value_change) {
			if (menuValues[selectId - 1] > mainMenu[selectId - 1].minValue) menuValues[selectId - 1]--;
		} else {
			if (cursorPos > 0) {
				cursorPos--;
				if (scroll == 0) {
					if (offsetPages > 0) offsetPages--;
				} else scroll--;
			}  
		}
	}
}

void encoderRotateRight() {
	if (parentId == 255) {
		if (menuValues[M_ID_INP - 1] == 0 && menuValues[2] < mainMenu[2].maxValue) menuValues[2]++;
	} else {
		if (value_change) {
			if (menuValues[selectId - 1] < mainMenu[selectId - 1].maxValue) menuValues[selectId - 1]++;
		} else {
			if (cursorPos < maxScroll - 1) {
				cursorPos++;
				if (scroll == SCREEN_LINES - 1) {
					if (offsetPages < maxScroll - SCREEN_LINES) offsetPages++;
				} else scroll++; 
			}  
		}
	}
}

void encoderHandler() {
	if ( uiKeyCode != KEY_NONE && last_key_code == uiKeyCode ) return;
	last_key_code = uiKeyCode;
	switch ( uiKeyCode ) {
		case KEY_SELECT:	encoderSwitchPress();
		break;
		case KEY_PREV:		encoderRotateLeft();
		break;
		case KEY_NEXT:		encoderRotateRight();
		break;
	}
	menu_redraw_required = true;
}

void maxMenuCount(){
	scroll    = 0;
	maxScroll = 0;
	offsetPages = 0;
	cursorPos   = 0;
	for (uint8_t i = 0; i < MENU_ITEM_COUNT; i++) if (mainMenu[i].parent == parentId) maxScroll++;
}

void draw() {
	display.clearDisplay();
	parentId == 255 ? drawMainScreen() : drawMenu();
	display.display();
	menu_redraw_required = false;
}

void drawMainScreen() {
	switch (menuValues[10]) {
		case 0: mainFhtScreen();
		break;
	}
	printScreenConfig();
}

void drawHLine(byte X, byte Y, byte L) {
	display.drawLine(X, Y, X+L, Y, WHITE);
}
void drawVLine(byte X, byte Y, byte L) {
	display.drawLine(X, Y-1, X, Y+L, WHITE);
}

void mainFhtScreen() {
	display.setTextColor(WHITE);
	display.setCursor(3, 1);
	display.print(mainMenu[2].caption[0]);
	display.setCursor(3, 11);
	display.print(menuValues[2]);
	drawHLine(24, 15, 128);
	for (uint8_t i = 0; i < 9; i++) drawVLine(i * 13 + 23, 14, 3);
	uint8_t gainL = map (menuValues[2], 0, 0xFF, 0, 0x66);
	drawVLine(gainL + 23, 13, 5);
	drawVLine(gainL + 24, 12, 7);
	drawVLine(gainL + 25, 13, 5);
}


void printScreenConfig() {
	display.writeFillRect(0,  23, 24, 9, WHITE);
	display.setTextColor(BLACK);
	display.setCursor(3, 24);
	display.print(mainMenu[M_ID_INP - 1].valueData[menuValues[M_ID_INP - 1]]);
  
  display.setTextColor(WHITE);
  if (menuValues[M_ID_PWM - 1]) {
    display.writeFillRect(25, 23, 42, 9, WHITE);
    display.setTextColor(BLACK);
  }
  display.setCursor(28, 24);
  display.print(SMOOTH);
  
  display.setTextColor(WHITE);  
	if (menuValues[1]) {
		display.writeFillRect(68, 23, 30 , 9, WHITE);
		display.setTextColor(BLACK);
	}
  display.setCursor(71, 24);
  display.print(AUTO);
}

void drawMenu() {
	uint8_t countPageItem = 0;
	for (uint8_t i = 0; i < MENU_ITEM_COUNT; i++) {
		if (mainMenu[i].parent == parentId) {
			if (mainMenu[i].pos >= offsetPages && countPageItem < SCREEN_LINES) {
				page[countPageItem] = i;
				countPageItem++;
			}
		}
	}
	for (uint8_t i = 0; i < countPageItem; i++) { 
		if (mainMenu[page[i]].pos == cursorPos) {
			selectId = mainMenu[page[i]].id;
			value_change ? display.writeFillRect(104, i * 10 , 24, 11, WHITE) : display.writeFillRect(0, i * 10, 128, 11, WHITE);
			display.setTextColor(BLACK);
		} else display.setTextColor(WHITE);
		if (mainMenu[page[i]].type == T_VALUE) {
			display.setCursor(107, i * 10 + 2);
			(mainMenu[page[i]].valueType == VT_LIST) ? display.print(mainMenu[page[i]].valueData[menuValues[page[i]]]) : display.print(menuValues[page[i]]);
		}
		if (value_change) display.setTextColor(WHITE);
		display.setCursor(3, i * 10 + 2);
		display.print(mainMenu[page[i]].caption[0]);
	}
}

void analyzeAudio() {
  uint16_t sample;
	for (uint8_t i = 0 ; i < FHT_N ; i++) {
		sample = analogRead(AUDIO_IN);
    	fht_input[i] = sample; // put real data into bins
	}
  	generalVolume = map(sample, 0x00, peak, 0x00, 0x4FF); //Мапим уровень по ПИКу
	if (generalMaxVolume < generalVolume) generalMaxVolume = generalVolume;	//Общий Максимальный уровень
	fht_window();  // window the data for better frequency response
	fht_reorder(); // reorder the data before doing the fht
	fht_run();     // process the data in the fht
	fht_mag_log(); // take the output of the fht
	fht_led_flash_analyse();
}

void peakDownFHT(uint8_t i){
 if (menuValues[1] && maxValueFHT[i] > 0x02 && fht_log_out[i] < 0x05) maxValueFHT[i]-- ; //PeakDownFHT
 if (menuValues[1] && maxValueFHT[i]-0xF <= fht_log_out[i]) maxValueFHT[i]++ ; //PeakDownFHT
}
/*
void animationRight(){
  if (aniRight > 5) return; //Если меньше нуля то просто выходим
  skeepFramesRight++;
  if (skeepFramesRight == 0x3FF) { 
    pwmValue[aniRight] = 0xFF;
    aniRight++; 
    skeepFramesRight = 0;
  }
}

void animationLeft(){
  if (aniLeft < 0) return; //Если меньше нуля то просто выходим 
  skeepFramesLeft++;
  if (skeepFramesLeft == 0x3FF) { 
    pwmValue[aniLeft] = 0xFF;
    aniLeft--; 
    skeepFramesLeft = 0;
  }
}
*/
void fht_led_flash_analyse() {
	uint8_t value;
	uint8_t aver[5];
	for (uint8_t i = 0; i < FNT_C; i++) {
		fht_log_out[i] = abs(fht_log_out[i]);//Берем абсолютное значение - модуль числа,  чтобы отрицательных значений не было
		if (fht_log_out[i] > maxValueFHT[i])  maxValueFHT[i] = fht_log_out[i]; //если значение больше максимального, то запишем его как максимальное,
		fht_log_out[i] = map(fht_log_out[i], 0x00, maxValueFHT[i], 0x00, 0xFF);//Промапим значение по максимальному, к знначениям 0-FF
		uint8_t ndx = 4;

		if (i >= LowChL && i <= LowChH) ndx = 0;//НЧ Фильтр
		if (i >= MidChL && i <= MidChH) ndx = 1;//СЧ Фильтр
		if (i >= HigChL && i <= HigChH) ndx = 2;//ВЧ Фильтр

      	value = map(fht_log_out[i], 0x00, menuValues[ndx+3], 0x00, 0xFF);//Промапим значение по границе lowLevel к значениям 0-FF
  //    if (value == 0xFF && aniRight > 5) {aniRight = 0;skeepFramesRight = 0x3FE;} //Если значение максимальное, то запускаем бегущий огонёк вправо
		if (menuValues[M_ID_PWM - 1])  {//Если включена плавность
        	if (value > pwmValue[ndx]) pwmValue[ndx] = value;//Если значение больше текущего на лампе, то передаем лампе новое значение
		} else (aver[ndx] == 0) ? aver[ndx]=value : aver[ndx] = (aver[ndx] + value) / 2;//Если плавность выключена, то запоминаем среднее значение полосы.
		//PeakDown FHT array
		peakDownFHT(i);	//Уменьшение максимальных значений.. ну типа для подстройки яркости относительно входного сигнала
	}

	//ППСАЧ Фильтр
  	value = map((((float)pwmValue[0] * 0.5) + pwmValue[1] + pwmValue[2] ) / 3, 0x00, 0xD4,    0x00, 0xFF);//Вычисляем среднее значение между половиной НЧ(что бы не перебивало), СЧ, ВЧ, а т.к. максимально среднее значение получится 212, то мапим по нему до 0-FF
  	if (menuValues[M_ID_PWM - 1]) { //Если включена плавность
    	if (value > menuValues[6]) {pwmValue[3] = value;smoothPPSACh=true;} //Если значение больше выбраного уровня, то передаем лампе значение
  	} else  pwmValue[3] = value;//Если плавность выключена, то просто даем на лампу то что получилось
	
	//ППГ Фильтр
	value = map(generalVolume, 0x00, generalMaxVolume, 0x00, 0xFF); //Промапим значение громкости по максимальному к значениям 0-FF
  	value = map(value,         0x00, menuValues[8],    0x00, 0xFF); //Промапим полученное значение по границе generalLevel к значениям 0-FF
//    if (value == 0xFF && aniLeft < 0) {aniLeft = 5;skeepFramesLeft = 0x3FE;} //Если значение максимальное, то запускаем бегущий огонёк влево
	if (menuValues[M_ID_PWM - 1]) {//Если включена плавность
		if (value > pwmValue[5]) pwmValue[5] = value;//Если значение больше текущего на лампе, то передаем лампе новое значение
	} else {//Если плавность выключена, то передаем лампе громкости текущее значение, и за одно остальным лампам значения средних показателей полосы
    	pwmValue[0] = aver[0];//НЧ
    	pwmValue[1] = aver[1];//СЧ
    	pwmValue[2] = aver[2];//ВЧ
    	pwmValue[4] = aver[4];//ППЧ
		pwmValue[5] = value;  //ППГ
	}
	if (value > 0x16) backLight = 0x00; //Если ППГ больше 20% то выключаем фон (ОПГ)
	if (menuValues[1] && generalMaxVolume > 0x02 && value < 0x05) generalMaxVolume-- ; //PeakDown Volume
/*
if (!menuValues[M_ID_PWM - 1]) {
  animationRight();
  animationLeft();
} 
*/
}
