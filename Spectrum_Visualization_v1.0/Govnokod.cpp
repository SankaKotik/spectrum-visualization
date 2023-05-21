#define _CRT_SECURE_NO_WARNINGS
#pragma comment(lib, "shlwapi.lib")

#include <stdint.h>
#include <windows.h>
#include <fstream>
//#include <math.h>
#include <Shlwapi.h>
//#include <iostream>
#include <float.h>
//#include <complex.h>
//#define PI (double) 3.14159265358979323846264338327950288
#define PI (double) 3.1415926535897932384626433832795028841971693993751058209749445923078164062862089986280348253421170679821480865132823066470938446095505822317253594081284811174502841027019385211055596446229489549303819644288109756659334461284756482337867831652712019091456485669234603486104543266482133936072602491412737245870066063155881748815209209628292540917153643678925903600113305305488204665213841469519415116094330572703657595919530921861173819326117931051185480744623799627495673518857527248912279381830119491
#define Shift32s (int64_t) 0x0000000080000000	// Переменная для преобразования int64_t в int32_t (делением, эквивалент >>32)
#define Shift24s (int64_t) 0x0000008000000000	// Переменная для преобразования int64_t в int24_t (делением, эквивалент >>40)
#define Shift16s (int64_t) 0x0000800000000000	// Переменная для преобразования int64_t в int16_t (делением, эквивалент >>48)
#define Shift8s (int64_t) 0x0080000000000000	// Переменная для преобразования int64_t в int8_t (делением, эквивалент >>56)
#define INT24_MAX 8388607i32


// get current time in ms
__int64 microtime() {
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);
	__int64 t = ((__int64)ft.dwHighDateTime << 32) + ft.dwLowDateTime;
	return t /= 10000;
}

/* fix working directory changed by drag-n-drop */
void fix_working_dir() {
	TCHAR buffer[MAX_PATH];
	GetModuleFileName(NULL, buffer, sizeof(buffer));
	PathRemoveFileSpec(buffer);
	SetCurrentDirectory(buffer);
}

int main(int argc, char ** argv) {
	__int64 t1 = microtime();

	/* take file name as argument */
	char filename[MAX_PATH];
	if (argc == 2) {
		fix_working_dir();
		sprintf(filename, "%s", argv[1]);
	}
	else {
		sprintf(filename, "out.wav");
	}

	/* открыть файл-источник на чтение */
	printf("Loading file...\n%s\n\n", filename);
	FILE * in = fopen(filename, "rb");
	if (in == NULL) { printf("Can't open file!\n"); Sleep(3000); return 0; }
	fseek(in, 0, SEEK_END);
	uint64_t size = ftell(in); //получить размер файла
	fseek(in, 0, SEEK_SET); //установить указатель на начало файла
	char * TMP = new char[size]; //выделить память под буффер
	fread(TMP, 1, size, in); //прочесть файл целиком в озу
	fclose(in);	 //Закрыть файл-источник

	// Проверка файла на совместимость
	if (size<22) { printf("File not supported! File must be in WAVE format (8, 16, 24 or 32 bit; stereo).\n"); Sleep(3000); return 0; }
	if ((TMP[0] != 'R') || (TMP[1] != 'I') || (TMP[2] != 'F') || (TMP[3] != 'F') || (TMP[8] != 'W') || (TMP[9] != 'A') || (TMP[10] != 'V') || (TMP[11] != 'E') || (TMP[12] != 'f') || (TMP[13] != 'm') || (TMP[14] != 't') || (TMP[15] != ' ') || (((TMP[20] & 0xFF) | ((TMP[21] & 0xFF) << 8)) != 1)) { printf("File not supported! File must be in WAVE format (8, 16, 24 or 32 bit; stereo).\n"); Sleep(3000); return 0; }
	uint32_t subchunk1Size = ((TMP[16] & 0xFF) | ((TMP[17] & 0xFF) << 8) | ((TMP[18] & 0xFF) << 16) | ((TMP[19] & 0xFF) << 24)) + 20; //Смещение в заголовке, байт
	if (size<(subchunk1Size + 7)) { printf("File is corrupted! Error offset header file.\n"); Sleep(3000); return 0; }
	if ((TMP[subchunk1Size + 0] != 'd') || (TMP[subchunk1Size + 1] != 'a') || (TMP[subchunk1Size + 2] != 't') || (TMP[subchunk1Size + 3] != 'a')) { printf("File is corrupted! Error in the file header.\n"); Sleep(3000); return 0; }
	uint32_t data_size = (TMP[subchunk1Size + 4] & 0xFF) | ((TMP[subchunk1Size + 5] & 0xFF) << 8) | ((TMP[subchunk1Size + 6] & 0xFF) << 16) | ((TMP[subchunk1Size + 7] & 0xFF) << 24); //Количество байт в области данных
	uint32_t data_offset = subchunk1Size + 8; //Размер заголовка файла
	if (size<(data_offset + data_size)) { printf("File is corrupted! Error in the data size.\n"); Sleep(3000); return 0; }
	uint32_t numChannels = (TMP[22] & 0xFF) | ((TMP[23] & 0xFF) << 8); //Количество каналов
	if (numChannels != 2) { printf("File not supported! Number of channels must be equal two.\n"); Sleep(3000); return 0; }
	uint32_t sampleRate = (TMP[24] & 0xFF) | ((TMP[25] & 0xFF) << 8) | ((TMP[26] & 0xFF) << 16) | ((TMP[27] & 0xFF) << 24); //Частота дискретизации
	uint32_t bitsPerSample = (TMP[34] & 0xFF) | ((TMP[35] & 0xFF) << 8); //Глубина (разрядность бит)
	if ((bitsPerSample != 8) && (bitsPerSample != 16) && (bitsPerSample != 24) && (bitsPerSample != 32)) { printf("File not supported! Bit resolution must be equal 8, 16, 24 or 32 bit.\n"); Sleep(3000); return 0; }
	printf("Sample Rate - %uHz\nSample resolution - %u\nNumber of Channels - %u\nNumber of samples - %u\n", sampleRate, bitsPerSample, numChannels, (data_size / numChannels * 8 / bitsPerSample));

	//Выделение дорожек в отдельные float массивы
	uint32_t sample_quantity = data_size/(bitsPerSample/8)/numChannels; //Количество семплов в одном канале
	uint32_t frame_quantity = sample_quantity/120; //Количество фреймов в одном канале
	float * RAWL = new float[sample_quantity];
	float * RAWR = new float[sample_quantity];
	float * Area = new float[frame_quantity*240*4]; //Буфер пространственного кодирования
	for(uint32_t i=0; i<frame_quantity*240*4; i++){Area[i]=0;}//Очистка буфера
	//memset(Area, 0, frame_quantity*240*4*sizeof(float));
	for (uint32_t D = data_offset; D<(data_size + data_offset); D += bitsPerSample / 8 * numChannels) {
		if (bitsPerSample == 8) { //Чтение семпла из буфера и нормальзация по уровню +-1
			RAWL[(D-data_offset)/(bitsPerSample/8)/numChannels] = (float)((TMP[D + 0] & 0xFF) << 24)/(float)2147483648.0; //L
			RAWR[(D-data_offset)/(bitsPerSample/8)/numChannels] = (float)((TMP[D + 1] & 0xFF) << 24)/(float)2147483648.0; //R
		}
		else if (bitsPerSample == 16) {
			RAWL[(D-data_offset)/(bitsPerSample/8)/numChannels] = (float)(((TMP[D + 1] & 0xFF) << 24) | ((TMP[D + 0] & 0xFF) << 16))/(float)2147483648.0; //L
			RAWR[(D-data_offset)/(bitsPerSample/8)/numChannels] = (float)(((TMP[D + 3] & 0xFF) << 24) | ((TMP[D + 2] & 0xFF) << 16))/(float)2147483648.0; //R
		}
		else if (bitsPerSample == 24) {
			RAWL[(D-data_offset)/(bitsPerSample/8)/numChannels] = (float)(((TMP[D + 2] & 0xFF) << 24) | ((TMP[D + 1] & 0xFF) << 16) | ((TMP[D + 0] & 0xFF) << 8))/(float)2147483648.0; //L
			RAWR[(D-data_offset)/(bitsPerSample/8)/numChannels] = (float)(((TMP[D + 5] & 0xFF) << 24) | ((TMP[D + 4] & 0xFF) << 16) | ((TMP[D + 3] & 0xFF) << 8))/(float)2147483648.0; //R
		}
		else {
			RAWL[(D-data_offset)/(bitsPerSample/8)/numChannels] = (float)(((TMP[D + 3] & 0xFF) << 24) | ((TMP[D + 2] & 0xFF) << 16) | ((TMP[D + 1] & 0xFF) << 8) | (TMP[D + 0] & 0xFF))/(float)2147483648.0; //L
			RAWR[(D-data_offset)/(bitsPerSample/8)/numChannels] = (float)(((TMP[D + 7] & 0xFF) << 24) | ((TMP[D + 6] & 0xFF) << 16) | ((TMP[D + 5] & 0xFF) << 8) | (TMP[D + 4] & 0xFF))/(float)2147483648.0; //R
		}
	}
	delete[]TMP;

	//ГЛАВНЫЙ ЦИКЛ ОБРАБОТКИ ДАННЫХ ==================================================================
	/* открыть файлы на запись */
	printf("\nOpen files...\n");
	FILE * out_s = fopen("spectrum.raw", "wb");
	while (out_s == NULL){
		out_s = fopen("spectrum.raw", "wb");
		if (out_s == NULL) { printf("Can't open spectrum.raw for writing!\n"); Sleep(3000);}
	}
	FILE * out_b = fopen("background.raw", "wb");
	while (out_b == NULL){
		out_b = fopen("background.raw", "wb");
		if (out_b == NULL) { printf("Can't open background.raw for writing!\n"); Sleep(3000);}
	}

	printf("\nApplying filter...\n");
	//Заполнение пространственного буфера и нахождение максимума амплитуды в спектре (для нормализации)
	float Freq_MAX = 0;
	for(uint32_t frame_num=0; frame_num<frame_quantity; frame_num++){
		float T_FreqMAX = 4.0/(float)frame_quantity; //Постоянная времени сглаживающего пиковые значения фильтра (для нормализации)
		for(uint32_t freq_num=0; freq_num<120; freq_num++){
			float Lin = frame_num*120+freq_num<sample_quantity ? RAWL[frame_num*120+freq_num] : 0.0; //0...1
			float Rin = frame_num*120+freq_num<sample_quantity ? RAWR[frame_num*120+freq_num] : 0.0; //0...1
			float Ain = Rin+Lin;
			float Din = Ain>0 ? (Rin-Lin)/Ain : 0; //-1...+1 - расположение сигнала на оси X
			if(Ain>Freq_MAX){Freq_MAX += (Ain-Freq_MAX)*T_FreqMAX;} //Максимальное значение амплитуды (для нормализации)
			//Распределение градиента цвета по спектру: Красный-Зелёный-Синий-Пурпурный
			float R = freq_num<48 ? (float)(48-freq_num)/48.0 : freq_num>96 ? (float)(freq_num-96)/48.0 : 0; //0...1
			float G = freq_num<48 ? (float)freq_num/48.0 : freq_num<96 ? (float)(96-freq_num)/48.0 : 0; //0...1
			float B = freq_num<49 ? 0 : freq_num>96 ? (float)(144-freq_num)/48.0 : (float)(freq_num-48)/48.0; //0...1
			//Вычисление координаты на оси X
			int32_t Pos = (int32_t)(119.0+120.0*Din);
			Pos = Pos<0 ? 0 : Pos>239 ? 239 : Pos;
			//Запись пиксела
			Area[frame_num*240*4+Pos*4+0] += R*Ain;//R
			Area[frame_num*240*4+Pos*4+1] += G*Ain;//G
			Area[frame_num*240*4+Pos*4+2] += B*Ain;//B
		}
	}
	//printf("Freq_MAX = %f\n", Freq_MAX*0.5);
	//Сглаживание пространственного буфера
	#define coeff_quantity 127 //Количество коэффициентов фильтра Гаусса
	float FIR_coefficients[coeff_quantity];//Синтез коэффициентов фильтра
		for(int lp=0; lp<coeff_quantity; lp++){
		float X = sinf((float)(lp+1)*PI/(float)(coeff_quantity+1));
		X = powf(X, 4.0);
		FIR_coefficients[lp] = X;
		//printf("FIR %u = %f\n", lp, X);
	}
	for(uint32_t frame_num=0; frame_num<frame_quantity; frame_num++){
		float Buf[240][4];
		for(uint32_t x=0; x<240; x++){//Сглаживание строки
			float R=0, G=0, B=0;
			for(uint32_t lp=0; lp<coeff_quantity; lp++){
				int32_t shift = ((int32_t)(x+lp)-coeff_quantity/2);
				if((shift<240)&&(shift>=0)){
				R += FIR_coefficients[lp]*Area[frame_num*240*4+shift*4+0];
				G += FIR_coefficients[lp]*Area[frame_num*240*4+shift*4+1];
				B += FIR_coefficients[lp]*Area[frame_num*240*4+shift*4+2];
				}
			}
			Buf[x][0] = R;
			Buf[x][1] = G;
			Buf[x][2] = B;
		}
		for(uint32_t x=0; x<240; x++){//Запись сглаженной строки
			Area[frame_num*240*4+x*4+0] = Buf[x][0];
			Area[frame_num*240*4+x*4+1] = Buf[x][1];
			Area[frame_num*240*4+x*4+2] = Buf[x][2];
		}
	}
	//Нахождение максимума в пространственном буфере (для нормализации)
	float Area_MAX = 0;
	for(uint32_t frame_num=0; frame_num<frame_quantity; frame_num++){
		for(uint32_t x=0; x<240; x++){
			if(Area[frame_num*240*4+x*4+0]>Area_MAX){Area_MAX = Area[frame_num*240*4+x*4+0];}
			if(Area[frame_num*240*4+x*4+1]>Area_MAX){Area_MAX = Area[frame_num*240*4+x*4+1];}
			if(Area[frame_num*240*4+x*4+2]>Area_MAX){Area_MAX = Area[frame_num*240*4+x*4+2];}
		}
	}
	//printf("Area_MAX = %f\n", Area_MAX*0.5);
	//Обработка кадра
	for(uint32_t frame_num=0; frame_num<frame_quantity; frame_num++){
		uint8_t Buf_s[135][240][4]; //Массив под кадр спектра
		uint8_t Buf_b[135][240][4]; //Массив под кадр фона
		size = sizeof(Buf_s);
		for(uint32_t freq_num=0; freq_num<120; freq_num++){ //Рендер кадра
			float Lin = frame_num*120+freq_num<sample_quantity ? RAWL[frame_num*120+freq_num] : 0.0;
			float Rin = frame_num*120+freq_num<sample_quantity ? RAWR[frame_num*120+freq_num] : 0.0;
			float Ain = (Lin+Rin)/Freq_MAX;
			Ain = Ain<0 ? 0 : Ain>1 ? 1 : Ain;
			Ain = powf(Ain, 0.707); //Логарифмизация высоты столбиков
			//Распределение градиента цвета по спектру: Красный-Зелёный-Синий-Пурпурный
			float R = freq_num<48 ? (float)(48-freq_num)/48.0 : freq_num>96 ? (float)(freq_num-96)/48.0 : 0; //0...1
			float G = freq_num<48 ? (float)freq_num/48.0 : freq_num<96 ? (float)(96-freq_num)/48.0 : 0; //0...1
			float B = freq_num<49 ? 0 : freq_num>96 ? (float)(144-freq_num)/48.0 : (float)(freq_num-48)/48.0; //0...1
			//Гамма-коррекция и нормализация RGB пространства
			float r = 255.0*powf(R, 1.0/2.2);
			float g = 255.0*powf(G, 1.0/2.2);
			float b = 255.0*powf(B, 1.0/2.2);
			//Подготовка пикселей для заполнения фона с пространственного буфера
			uint32_t x = freq_num*2;
			float Ra0 = Area[frame_num*240*4+x*4+0]/Area_MAX;
			float Ga0 = Area[frame_num*240*4+x*4+1]/Area_MAX;
			float Ba0 = Area[frame_num*240*4+x*4+2]/Area_MAX;
			float Ra1 = Area[frame_num*240*4+x*4+4]/Area_MAX;
			float Ga1 = Area[frame_num*240*4+x*4+5]/Area_MAX;
			float Ba1 = Area[frame_num*240*4+x*4+6]/Area_MAX;
			float alpfa_0 = Ra0;
			alpfa_0 = Ga0>alpfa_0 ? Ga0 : alpfa_0;
			alpfa_0 = Ba0>alpfa_0 ? Ba0 : alpfa_0;
			if(alpfa_0>0){
				Ra0 /= alpfa_0;
				Ga0 /= alpfa_0;
				Ba0 /= alpfa_0;
			}
			float alpfa_1 = Ra1;
			alpfa_1 = Ga1>alpfa_1 ? Ga1 : alpfa_1;
			alpfa_1 = Ba1>alpfa_1 ? Ba1 : alpfa_1;
			if(alpfa_1>0){
				Ra1 /= alpfa_1;
				Ga1 /= alpfa_1;
				Ba1 /= alpfa_1;
			}
			float ra0 = 255.0*powf(Ra0, 1.0/2.2);
			float ga0 = 255.0*powf(Ga0, 1.0/2.2);
			float ba0 = 255.0*powf(Ba0, 1.0/2.2);
			alpfa_0 = 255.0*powf(alpfa_0, 1.0/2.2);
			float ra1 = 255.0*powf(Ra1, 1.0/2.2);
			float ga1 = 255.0*powf(Ga1, 1.0/2.2);
			float ba1 = 255.0*powf(Ba1, 1.0/2.2);
			alpfa_1 = 255.0*powf(alpfa_1, 1.0/2.2);
			//Отрисовка столбика
			for(int32_t y=134; y>=0; y--){
				//Запись пикселей в кадр спектра
				Buf_s[y][x][0] = (uint8_t)r;
				Buf_s[y][x+1][0] = (uint8_t)r;
				Buf_s[y][x][1] = (uint8_t)g;
				Buf_s[y][x+1][1] = (uint8_t)g;
				Buf_s[y][x][2] = (uint8_t)b;
				Buf_s[y][x+1][2] = (uint8_t)b;
				Buf_s[y][x][3] = (Ain*135.0)>(135-y) ? 255 : (Ain*135.0)>(134-y) ? (uint8_t)(((float)(134-y)-Ain*135.0)*-255.0) : 0;
				//Buf_s[y][x][3] = (Ain*135.0)>(135-y) ? 255 : (Ain*135.0)>(134-y) ? (uint8_t)(powf(((float)(134-y)-Ain*135.0), 1.0/2.2)*-255.0) : 0;
				Buf_s[y][x+1][3] = 0;
				//Запись пикселей в кадр фона
				Buf_b[y][x][0] = (uint8_t)ra0;
				Buf_b[y][x+1][0] = (uint8_t)ra1;
				Buf_b[y][x][1] = (uint8_t)ga0;
				Buf_b[y][x+1][1] = (uint8_t)ga1;
				Buf_b[y][x][2] = (uint8_t)ba0;
				Buf_b[y][x+1][2] = (uint8_t)ba1;
				Buf_b[y][x][3] = (uint8_t)alpfa_0;
				Buf_b[y][x+1][3] = (uint8_t)alpfa_1;
			}
		}
		fwrite(Buf_s, 1, size, out_s); //Запись кадра в файл спектра
		fwrite(Buf_b, 1, size, out_b); //Запись кадра в файл фона
	}
	delete[]RAWL;
	delete[]RAWR;
	delete[]Area;
	fclose(out_s);
	fclose(out_b);
	printf("Done!\r\n");

	__int64 t2 = microtime() - t1;
	printf("Execution time: %I64d ms\r\n", t2);
	Sleep(2*1000);
	return 0;
}