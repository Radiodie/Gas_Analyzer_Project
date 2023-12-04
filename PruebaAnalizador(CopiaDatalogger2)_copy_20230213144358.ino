#include <BotonSimple.h>

#include <Adafruit_GFX.h>     // Core graphics library
#include <Adafruit_ST7735.h>  // Hardware-specific library for ST7735
#include <SPI.h>

#include "RTClib.h"
#include "FS.h"
#include "SD.h"

#include <esp_wifi.h>
#include <WiFiClientSecure.h>
#include "Base64.h"

#define TFT_CS        2
#define TFT_RST       15
#define TFT_DC        4
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
RTC_DS3231 rtc;

BotonSimple boton1(32);
BotonSimple boton2(33);
BotonSimple boton3(25);
BotonSimple boton4(26);   
unsigned int accionado = 0;
const char* caracScreen;

#define CELESTE 0x8eba
#define GRIS    0xffff
#define BLANCO  0x05d6

String namefile = "";

uint8_t dutycycle = 163;                    //Voltaje entregado a la bomba en un rango de 0 - 255

unsigned long int tconc[4] = {0};                         //tiempo entre muestreos de cada parámetro escogido
int cont2[4]= {0};
int muestras = 60;     //Segundos que detecta cada muestra
float suma[4];
float prom[4]= {0};
float sumascreen[4];
int cont2screen[4] ={0};
unsigned long int tconcscreen[4] = {0};

//--------------------------------------

String screen[5] = {"", "", "", ""};  //cadenas de caracteres de cada renglon en pantalla

short tab = 0;   //encargado de pasar de una pantalla a otra
short casi = 0;  //cambio entre casillas de ej: fecha:año->mes u hora:horas->minutos
unsigned short nventana = 1;
String mainmenu[7] = { "ANALIZADOR DE GAS", "INICIAR", "CAMBIAR # EST.", "CAMBIAR FECHA", "CAMBIAR HORA", "SELEC. PARAMETROS","CAMBIAR # PROYEC."};
unsigned short numenu = 0;          //numero de pantalla del menu
String param[4] = {"O3 ","NO2",
                   "SO2","CO "};      //Parametro de concentración escogido
unsigned short est = 0;               //numero de estacion
unsigned short newest = 0;            //nuevo numero de estación
unsigned short date[3] = {1,1,0};     //fecha ej: 20/07/22: date[0]=20,date[1]=07,date[2]=22
unsigned short newdate[3] = {1,1,0};  //nueva fecha ej: 20/07/22
unsigned short timea[2] = { 0 };      //hora ej: 13:07: time[0]=13,time[1]=07
unsigned short newtime[2] = { 0 };    //nueva hora
uint8_t plantyp[2] = {0,0};    //plan anterior de proyecto plannum[0] es CA o AIR y plannum[1] es el numero de proyecto
uint8_t plannum[2] = {0,0};    //plan nuevo de proyecto plantyp[0] es CA o AIR y plantyp[1] es el numero de proyecto 
bool pplan = 0;
String DHparpadeo[3] = {""};                            //variable donde guarda el stirng del tiempo y hace parpadear la variable a modificar
bool parpadeo = true;                  //booleano que decide el intervalo de parpadeo de la variable de tiempo a modificar
String saved[3] = { "" };           //pantalla de guardado saved[0]=desde, saved[1]=hasta, saved[2]= bool para saber que es de saved de grabando
unsigned long int tsaved = 0;           //guardar tiempo en que se quita pantalla de guardado
String zero[4];                                         //Cero que irá antes de la fecha y hora
unsigned short casiparam = 0;       //casilla de parametro
bool dispparam[4] = {0};    //banderas de parametros escogidos para mostrar
bool newdispparam[5] = {0};

unsigned short dimenconc[5][2] = {40,97,65,78,65,38,17,78,17,38};    //coordenadas para colocar cuadros de concentraciones
bool resalt = false;
bool aviso = false;               //booleano para cambiar entre avisos que se muestren en pantalla
bool avisos[2] = {0};           //para activar o desactivar qué aviso se mostraran ej: concentación, estación, etc

bool setupwifi = true;        //iniciar wifi
bool enviar = true;      //ACTIVAR SUBIDA DE DATOS A NUBE TRUE = PARAR DE SUBIR, FALSE = SUBIR 
uint8_t estatus = 0;      //Comunica el estado del wifi entre los dos núcleos
TaskHandle_t wifi;            //Activar 2do nucleo para activar wifi
unsigned long int notif;      //notificacion de archivo subido
bool notifsubida = false;
uint8_t estrato = 20;                     //avisa que debe actualizarse el simbolo de wifi
bool fixread = false;
String DataCSV;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  tft.initR(INITR_BLACKTAB);
  tft.fillScreen(CELESTE);
  tft.fillRect(0, 0, 128, 8, BLANCO);
  tft.setTextColor(ST77XX_BLACK);
  tft.setTextWrap(true);
  tft.cp437(true);
  tft.setTextSize(1);
  inicializarSens();
  DateTime OLA = rtc.now();

  xTaskCreatePinnedToCore(
    loop_wifi, /* Funcion de la wifi */
    "wifi", /* Nombre de la tarea */
    10000,  /* Tamaño de la pila */
    NULL,  /* Parametros de entrada */
    0,  /* Prioridad de la tarea */
    &wifi,  /* objeto TaskHandle_t. */
    0); /* Nucleo donde se correra */

  //COMPROBACION SI HUBO APAGADO BRUSCO
  File file = SD.open("/NoEliminar/Registros.txt");
  if (file) {
    String contents = "";
    while (file.available())contents += char(file.read()); //Guarda contenido de bloc de notas
    file.close();

    //unsigned uint16_t lastLineStart = contents.lastIndexOf('\n') + 1;  // guarda posicion del ultimo archivo
    String lastFileName = contents.substring(contents.lastIndexOf('\n') + 1);   //guarda el nombre

    if(lastFileName.substring(50) == "Grabando"){           //ubica posicion del ultimo estado del dispositivo
      nventana = 2;
      namefile = lastFileName.substring(0,31);
      saved[0] = lastFileName.substring(37,47);

	    char pastconc[5]; 
	    strcpy(pastconc, (lastFileName.substring(32,36)).c_str()); 

      for(uint8_t i=0;i<4;++i){
        Serial.print(pastconc[i]);
        if(pastconc[i] == '1'){
          dispparam[i] = 1;
        }
        else{dispparam[i] = 0;
        }  
      }          
    }
  }
  else{Serial.println("Error al abrir el archivo");}
}

String screenhere[5];

void loop() {

  boton1.actualizar(); boton2.actualizar(); boton3.actualizar(); boton4.actualizar();
 
  if ( boton1.leer() == 3){
    accionado = 1;
  }else if( boton2.leer() == 3){
    accionado = 2;
  }else if( boton3.leer() == 3){
    accionado = 3;
  }else if( boton4.leer() == 3){
    accionado = 4;
  }
  EstadoWifi();
  movescreen(accionado,rtc.now());
  Serial.println(nventana);
  Serial.print("\t");
  Serial.print(numenu);
  if(fixread == true){
  DataCSV = readFile(SD, namefile.c_str());
  fixread = false;
  }  

  actscreen();

  if(notifsubida == true){
    notifsubida = false;
    tft.fillRect(5,120,125,10,GRIS);
    tft.setTextColor(ST77XX_BLACK);
    tft.setCursor(5,120);
    tft.print("(!!) Archivo subido.");
  }else if(millis() - notif > 8000){
    tft.fillRect(5,120,125,10,CELESTE);
  }

  for(uint8_t i=0;i < 5 ; ++i){

    if(screen[i] != screenhere[i] || resalt == true){
      uint8_t renglon[5] = {40,80,100,140,0};
      if(i == 4){
        tft.setTextColor(BLANCO);
      }else{
        tft.setTextColor(CELESTE);
      }
      drawCentreString(screenhere[i].c_str(),128/2,renglon[i]);

      tft.setTextColor(ST77XX_BLACK);
      drawCentreString(screen[i].c_str(),128/2,renglon[i]);
      screenhere[i] = screen[i].c_str();
      
      if (i == 0 && resalt == true){
        resalt = true;
      }else{
        resalt = false;
      }
    }
  }
  accionado = 0;
}

void movescreen(unsigned int button, DateTime OLA) {
  tab = 0;
  date[0] = OLA.day(); date[1] = OLA.month(); date[2] = OLA.year();
  timea[0] = OLA.hour(); timea[1] = OLA.minute();
  
  //nventana = _nventana;
  switch (button) {
    case 1: case 2:
      if (button == 1){
        if (nventana == 2){
          //deleteFile(SD, namefile.c_str());
          nventana = 10;                  //vuelve a ventana INICIO
          casi = 0;
        }else{
          tab++;
        }
      }else{
        tab--;
      }
      if (nventana == 1 || nventana == 4 || nventana == 5 || nventana == 7) {  //para poder volver a Menu inicio si se tarda cierto tiempo
        tsaved = millis();
        parpadeo = true;
      } else if (nventana == 6){
        tft.drawRect(dimenconc[casiparam][0], dimenconc[casiparam][1], 45, 12, CELESTE);
        Serial.print(newdispparam[1]); Serial.print("\t"); Serial.print(newdispparam[2]); Serial.print("\t"); Serial.print(newdispparam[3]); Serial.print("\t"); Serial.print(newdispparam[4]); Serial.print("\t"); Serial.println(casiparam); 
      }
      break;

    case 3:
      switch (nventana) {
        case 1:
          nventana = numenu + 1;
          
          switch(numenu){ 
            case 1:
              //enviar == false;
              if(est == 0){
                avisos[1] = 1;
              }else{avisos[1] = 0;}

              for (uint8_t k = 0; k < 4 ; k++){
                if (dispparam[k] == 0){
                  avisos[0] = 1;
                }else{avisos[0] = 0;}
              }
              if (avisos[0] == 1 || avisos[1] == 1){
                nventana = 9;
                aviso = true; 
                tsaved = millis();
              }
              else{        
                  char dateBuffer[24];
                  sprintf(dateBuffer,"MEDICION%04u%02u%02u_%02u%02u%02u_%02u",OLA.year(),OLA.month(),OLA.day(),OLA.hour(),OLA.minute(),OLA.second(),est); //para colocar cero entre 0 y 9
                  namefile = "/"+String(dateBuffer) + ".csv";

                  writeFile(SD, namefile.c_str(), "FECHA ; HORA ; TEMP.(C°);");
                  for (uint8_t i = 0 ; i < 4; i++){
                    if (dispparam[i] == 1){
                      tconc[i] = millis();
                      tconcscreen[i] = millis();
                      Serial.print(tconc[i]);
                      Serial.print(" ");
                      appendFile(SD, namefile.c_str(), (param[i] + " ").c_str() );
                      if(i == 3){
                        appendFile(SD, namefile.c_str(), "(PPM);");
                      }else{
                        appendFile(SD, namefile.c_str(), "(PPB);");
                      }
                      appendFile(SD, namefile.c_str(), "Voltaje(V);");
                    }
                  }
                  // AGREGAR NOMBRE PROYECTO 
                  char *a;
                  String planes;
                  if(plantyp[0] == 0){
                    planes = "AIR";
                  }else{planes = "CA ";}
                  sprintf(a,"%03u",plannum[0]);
                  planes = planes + "-" + String(date[2]) + "-" + String(a);
                  appendFile(SD, namefile.c_str(), ("PROYECTO: PLAN " + planes).c_str());
                  appendFile(SD, namefile.c_str(), ("ESTACION N.: " + (String)est).c_str());                  

                  saved[0] = OLA.timestamp(DateTime::TIMESTAMP_DATE);
                  File file = SD.open("/NoEliminar/Registros.txt");
                  String nombretxt = namefile + " " + dispparam[0]+dispparam[1]+dispparam[2]+dispparam[3]+" " + String(OLA.timestamp(DateTime::TIMESTAMP_DATE));
                  if (!file) {                                                      //comprueba si existe la carpeta
                    createDir(SD,"/NoEliminar");
                    file.close();
                    writeFile(SD, "/NoEliminar/Registros.txt",(nombretxt+ " = Grabando").c_str());
                  }else{
                    file.close();
                    appendFile(SD, "/NoEliminar/Registros.txt",("\n"+nombretxt + " = Grabando").c_str());
                  }                            
                }
              tsaved = millis();              
            break;

            case 3: 
            newdate[0]=date[0];newdate[1]=date[1];newdate[2]=date[2];           //iguala la fecha a cambiar a la actual
            tsaved = millis();                                                  //reinicia para el parpadeo            
            break;

            case 4:
            newtime[0] = timea[0];newtime[1] = timea[1];newtime[2] = timea[2];        //cambia la hora a cambiar ala actual
            tsaved = millis();                                                        //reinicia para el parpadeo   
            break;

            case 5:
            tft.fillRect(0, 78, 128, 12, CELESTE);
            casiparam = 4;
            resalt = true;
            for(uint8_t i = 0; i < 4 ; ++i){
              newdispparam[i+1] = dispparam[i];
            }
            break;

            case 6:
            pplan = 0;
            plannum[1] = plannum[0]; plantyp[1] = plantyp[0];       
            break;
          }

        case 2:
          Serial.print("??????????????????????????????????????????????");
          nventana = 8;
          break;

        case 3:
          est = newest;
          nventana = 9;
          tsaved = millis();
          break;

        case 4:
          tsaved = millis();
          parpadeo=true;                            //para que apenas presione ok, no parpadee
          if (casi == 2) {
            nventana = 9;
            date[0]=newdate[0];date[1]=newdate[1];date[2]=newdate[2];
            rtc.adjust(DateTime(date[2], date[1], date[0], timea[0], timea[1]));
            tsaved = millis();
            casi = 0;
          } else {
            casi++;
          }
          break;

        case 5:
          tsaved = millis(); 
          parpadeo=true;                            //para que apenas presione ok, no parpadee
          if (casi == 1) {
            nventana = 9;
            timea[0]=newtime[0];timea[1]=newtime[1];timea[2]=newtime[2];
            rtc.adjust(DateTime(date[2], date[1], date[0], timea[0], timea[1]));
            casi = 0;
            tsaved = millis();
          } else {
            casi++;
          }
          break;

        case 6:
          if (casiparam == 0){
            nventana = 9;
            for(uint8_t i = 0; i < 4 ; ++i){
              dispparam[i] = newdispparam[i+1];
            }
            tft.fillScreen(CELESTE);
            estrato = 20;
            tft.fillRect(0, 0, 128, 8, BLANCO);
            tsaved = millis();
          }else{
            resalt = true;
            if (newdispparam[casiparam] == 0){
              newdispparam[casiparam] = 1;
            } else{
              newdispparam[casiparam] = 0;
            }
          }
          break;

        case 7:
        if (casi == 1) {
          nventana = 9;
          plannum[0] = plannum[1] ; plantyp[0] = plantyp[1];
          casi = 0;
          tsaved = millis();
        }else {
          casi++;
        }

        case 8:
          nventana = 2;
          for (uint8_t i = 0 ; i < 4; i++){
            if (dispparam[i] == 1){
              tconc[i] = millis();
              tconcscreen[i] = millis();
            }
          }
          break;

        case 10:
        if(casi == 0){              //distingue si es cancelar o terminar grabacion
          nventana = 1;
          deleteFile(SD, namefile.c_str());     
               
        } else{                    //cambia a ventana Guardado con >>De:>> y >>A:>>      
            nventana = 9;
            saved[1] = OLA.timestamp(DateTime::TIMESTAMP_DATE);
            saved[2] = "1";
          }

          File file = SD.open("/NoEliminar/Registros.txt", FILE_READ);
          String contents = "";
          while (file.available()) {
            contents += char(file.read());
          }
          file.close();
          if(casi == 0){
            contents.replace("Grabando", "Cancelado");         // Reemplaza la palabra "grabando" con "completo"
          }else{
            contents.replace("Grabando", "Completo");         // Reemplaza la palabra "grabando" con "completo"
            casi = 0;
          }
          writeFile(SD, "/NoEliminar/Registros.txt",contents.c_str());

          //    ultscreen = nventana;      se cambió por numenu   
          tsaved = millis();
          for(uint8_t i = 0 ; i<4; ++i){
            prom[i] = 0;
          }
          screen[0] = "";screen[1] = "";screen[2] = "";screen[3] = "";
          digitalWrite(12, LOW);
          //enviar = true;
          break;
      }
    break;

    case 4:
      screen[0] = "";screen[1] = "";screen[2] = "";screen[3] = "";
      tft.fillScreen(CELESTE);
      estrato = 20;
      tft.fillRect(0, 0, 128, 8, BLANCO);

      switch (nventana){
        case 2: case 8:
          nventana = 10;
          casi = 1;
        break;

        case 3: case 4: case 5: case 6: case 7:
          nventana = 1;
          casi = 0;
          newest = 0;
          newtime[0]=0;newtime[1]=0;
          newdate[0]=1;newdate[1]=1;newdate[2]=0;
          plantyp[1] = 0; plannum[1] = 0;

          tsaved = millis();
        break;

        case 9:
        Serial.print("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
          nventana = 2;
          tsaved = millis();
        break;
      }

    break;
  }
} 

bool salto = true;                       //escribir fecha en el SD y salto de línea 
void actscreen() {
  DateTime OLA = rtc.now();
  unsigned short tabtime[5] = { 31, 12, 99, 23, 59 };  //vector que contiene cada rango de numeros de las fechas                              //cadena de caracteres donde tendra la informacion de la pantalla a devolver
  
  screen[0] = " ";
  screen[3] = " ";
  screen[4] = OLA.timestamp(DateTime::TIMESTAMP_TIME) + "   " + OLA.timestamp(DateTime::TIMESTAMP_DATE);

  if(nventana == 4 || nventana == 5){
    for(int i=0; i<4; ++i){
      if(i != 3){
        if (newdate[i] >= 0 && newdate[i] <= 9){              //colocar zero en los numeros del 0 a 9
          zero[i] = "0";
        }else{
          zero[i] = "";
        }
      }else{
        if (newtime[1] >= 0 && newtime[1] <= 9){
          zero[i] = "0";
        }else{
          zero[i] = "";
        }
      }
    }
  }
  switch (nventana) {
    case 1:
      //Hay varios de estos if en los siguientes case con el objetivo de establecer un rango de valores a las variables
      if(tab == -1 || tab == 1){
        numenu = rangonum(numenu,1,6);        //rango de numeros de pantalla principal
      }
      if (numenu != 0){
        screen[3] = " ^ | v | Ok |    ";
      }
      screen[1] = mainmenu[numenu];
      if (millis() - tsaved > 7990 && millis() - tsaved < 8010) {  //TIEMPO QUE TARDA EN VOLVER ATRÁS POR INACTIVIDAD
        numenu = 0;
      }

      break;

    case 2:
    {
      digitalWrite(27, LOW);
      digitalWrite(12, HIGH);
      ledcWrite(0, dutycycle); 
      screen[1] = "";
      screen[2] = "";
      screen[0] = "GRABANDO:";
      for (uint8_t i = 0 ; i < 4;++i){
      //prueba = millis();
        if (dispparam[i] == 1){
          //Serial.print(i);
          screen[1] = screen[1] + " " + param[i];
          screen[2] = screen[2] + " " + (String)promedioDato(i);
        }else{}
      }      
      salto = true; 
      screen[3] = "<- |   |STOP| END";
    }
      break;

    case 3:
      newest = rangonum(newest,0,99);

      screen[0] = "Actual: " + String(est);
      screen[1] = "Nuevo:  " + String(newest);
      screen[3] = " ^ | v | Ok | Esc ";
      break;

    case 4:
      {
        unsigned int iniciotime = 1;  //inicio de tiempo; dia:1-31, mes:1-12, ano:0-99
        if (casi == 2) {
          iniciotime = 0;
        }
        newdate[casi] = rangonum(newdate[casi],iniciotime,tabtime[casi]);      
        if(parpadeo == true){
          for (short k = 0; k<3 ; k++){
            DHparpadeo[k] = zero[k] + String(newdate[k]);
          }
        }
        parpadeador();

        screen[0] = "Actual: " + String(date[0]) + "/" + String(date[1]) + "/" + String(date[2]);
        screen[1] = "Nuevo:  " + DHparpadeo[0] + "/"+ DHparpadeo[1] + "/" + DHparpadeo[2];
        screen[3] = " ^ | v | Ok | Esc ";
      }
      break;

    case 5:                           //inicio de hora; horas:0-23, minutos:0-59
      newtime[casi] = rangonum(newtime[casi],0,tabtime[casi+3]);
      if(parpadeo == true){
        DHparpadeo[0] = "";
        if (newtime[0]< 10){          //arregla el error de casilla que se corre cuando es menor que 0
          DHparpadeo[0] = " ";
        }
        DHparpadeo[0] = DHparpadeo[0] + String(newtime[0]); DHparpadeo[1] = zero[3] + String(newtime[1]);  
        //sprintf(DHparpadeo[1],"%02u",) ||||||||||PEDIENTE PARA ELIMINAR OPERACION DE ZERO
      }
      parpadeador();
      screen[0] = "Actual: " + String(timea[0]) + ":" + String(timea[1]);
      screen[1] = "Nuevo:  " + DHparpadeo[0] + ":" + DHparpadeo[1];
      screen[3] = " ^ | v | Ok | Esc ";
      break;

    case 6:
      casiparam = rangonum(casiparam,0,4);

      if(resalt == true){
        for(uint8_t i = 1; i<5; ++i){
          if(newdispparam[i] == 1){
            resalt = true;
            tft.fillRect(dimenconc[i][0], dimenconc[i][1], 45, 12, GRIS);
          }else{
            tft.fillRect(dimenconc[i][0], dimenconc[i][1], 45, 12, CELESTE);
          }
        }
      }
      tft.drawRect(dimenconc[casiparam][0], dimenconc[casiparam][1], 45, 12, ST77XX_BLACK);

      screen[0] = "CO      NO2";
      screen[1] = "SO2     O3 ";
      screen[2] = "Aceptar";
      screen[3] = " ^ | v | Ok | Esc ";
      break;

    case 7:             //cambios de proyecto: CA o AIR y 0-200
    {
      uint8_t rangplan[2] = {1,200};
      String planes[2];
      char *a;
      plantyp[casi] = rangonum(plantyp[casi],0,rangplan[casi]);

      for(uint8_t t = int(pplan);t<2;++t){
        planes[t] = "";
        if(parpadeo == true && t == 1){
          break;
        }
        if(plantyp[t] == 0){
          planes[t] = "AIR";
        }else{planes[t] = "CA ";}
        sprintf(a,"%03u",plannum[t]);
        planes[t] = planes[t] + "-" + String(date[2]) + "-" + String(a);
      }
      pplan=1;
      parpadeador();
      screen[0] = "Actual: " + planes[0];               //MODIFICAR EN BOTONES
      screen[1] = "Nuevo:  " + planes[1]; 
      screen[3] = " ^ | v | Ok | Esc ";
    }

    case 8:
      screen[0] = "GRABAC. PARADA";
      screen[3] = "        |CONT| END";
      break;

    case 9:
      if (aviso == false){
        screen[0] = "GUARDADO!";

        if (saved[2] == "1") {
          screen[1] = "De:    " + String(saved[0]);
          screen[2] = "Hasta: " + String(saved[1]);
        }
      }else{
        screen[0] = "Falta configurar:";
        screen[1] = "";
        if(avisos[0]==1)screen[1] = "Parametros";
        if(avisos[1]==1)screen[2] = "N. Estacion";
        
      }          
      if (millis() - tsaved > 1990 && millis() - tsaved < 2010) {
        nventana = 1;
        saved[2] = "0";
        tsaved = millis();
        screen[0] = "";screen[1] = "";screen[2] = "";
        aviso = false;
      }
      break;

    case 10:
    digitalWrite(12, HIGH);
    digitalWrite(27, LOW);
    ledcWrite(0, dutycycle); 
    screen[1] = "Seguro que desea";
    screen[3] = "       | Si | No ";
    if(casi == 0){
      screen[2] = "cancelar monitoreo?";
    }else{
      screen[2] = "finalizar monitoreo?";      
    }
    
    
  }     
}

void inicializarSens(){
  //CONFIGURACION BOMBA
  pinMode(12, OUTPUT);
  pinMode(27, OUTPUT);
  pinMode(14, OUTPUT);
  ledcSetup(0, 30000, 8);     //configuracion PWM para bomba  ledcSetup(pwmChannel, freq, resolution)
  ledcAttachPin(14, 0);       //configutacion bomba PIN de control y Channel

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    drawCentreString("Couldn't find RTC",128/2,100);
    Serial.flush();
    while (1) delay(10);
  }

  if(!SD.begin()){
    Serial.println("Card Mount Failed");
    drawCentreString("Card Mount Failed",128/2,100);
    return;
  }
      tft.setTextColor(CELESTE);
    drawCentreString("Card Mount Failed",128/2,100);
    tft.setTextColor(ST77XX_BLACK);

  if(SD.cardType() == CARD_NONE){
    Serial.println("Sin tarjeta SD insertada");
    //tft.setTextColor(ST77XX_BLACK);
    //drawCentreString("Sin tarjeta SD insertada",128/2,100);
    return;
  }

}

void EscribirDateSD(){
    DateTime OLA = rtc.now();
    appendFile(SD, namefile.c_str(), "\n");
    appendFile(SD, namefile.c_str(), OLA.timestamp(DateTime::TIMESTAMP_DATE).c_str());
    appendFile(SD, namefile.c_str(), ";");
    appendFile(SD, namefile.c_str(), OLA.timestamp(DateTime::TIMESTAMP_TIME).c_str());
    appendFile(SD, namefile.c_str(), ";");
    char temp[10];
    appendFile(SD, namefile.c_str(), dtostrf(rtc.getTemperature(),1,2,temp));
    appendFile(SD, namefile.c_str(), ";");

    Serial.print("\n");
    Serial.print(OLA.timestamp(DateTime::TIMESTAMP_DATE).c_str());
    Serial.print("\t");
    Serial.print(OLA.timestamp(DateTime::TIMESTAMP_TIME).c_str());
    Serial.print("\t");
    enviar = false;
}

void EscribirConcSD(float concentr){
    DateTime OLA = rtc.now();
    
    float conform;
    float enppm = concentr*(310/0.95);
    if(concentr <= 0.19){                      //este rango es el que más toma el sensor, por lo que se hace este ajuste en 0 a 10ppm
      conform = concentr * 10/0.19;
    }else{
      conform = ((pow(enppm,3))*3.804920e-07)+((pow(enppm,2))*-0.001316)+((enppm)*1.325062)+(16.35514);     //esta es la fórmula de regresion polinomial
    }

      appendFile(SD, namefile.c_str(), String(conform).c_str());
      appendFile(SD, namefile.c_str(), ";");
      appendFile(SD, namefile.c_str(), String(concentr).c_str());
      appendFile(SD, namefile.c_str(), ";");
      //Serial.print(String(concentr*(250.0/1.076)).c_str());
      //Serial.print("\t");
}


float promedioDato(uint8_t ncon){
  float conval = 0;                         //valor de  concentración en
  float ScalV = 3.3/4096.0;
  float valB = 0.0;
  uint8_t ARed[4] = {39,36,34,35};            //pines de entrada de voltaje analógico analizador

  valB = analogRead(ARed[ncon]); 
  conval = ScalV * valB; 

  cont2[ncon]++;
  suma[ncon] = conval + suma[ncon];

  
  if(millis() - tconc[ncon] >= ((muestras*1000))){

    float escribirconc;
    if(salto == true){                  //escribe la fecha en el SD una sola vez cada que hay muestreo de datos
      EscribirDateSD();
      salto = false;
    }
    escribirconc = suma[ncon]/cont2[ncon];
    if(ncon == 3){
      //prom[ncon] = pow((4.4922e-5)*prom[ncon],3) + pow((-1.71668e-2)*prom[ncon],2) + 2.621091*prom[ncon] + (6,39488e-14);
    }    

    EscribirConcSD(escribirconc);
    suma[ncon]=0;
    cont2[ncon]=0;
    tconc[ncon] = millis();
    Serial.println("---Fin escritura datos---");
  }else{}

  //IMPRIMIR EN PANTALLA MÁS RAPIDO
  cont2screen[ncon]++;
  sumascreen[ncon] = conval + sumascreen[ncon];
  if(millis() - tconcscreen[ncon] >= 5000){ 
    float conform;
    prom[ncon] = sumascreen[ncon]/cont2screen[ncon];
    float enppm = prom[ncon]*(310/0.95);
    if(prom[ncon] <= 0.19){                      //este rango es el que más toma el sensor, por lo que se hace este ajuste en 0 a 10ppm
      conform = prom[ncon] * 10/0.19;
    }else{
      conform = ((pow(enppm,3))*3.804920e-07)+((pow(enppm,2))*-0.001316)+((enppm)*1.325062)+(16.35514);     //esta es la fórmula de regresion polinomial
    }
    prom[ncon] = conform;
    sumascreen[ncon]=0;
    cont2screen[ncon]=0;
    tconcscreen[ncon] = millis();
  }else{}

  return prom[ncon];
}

unsigned short rangonum(int casilla, int min, int max){         //Funcion encargada de establecer un rango de numeros en la variable que desee
  if (casilla + tab < min) {
      casilla = max;
    } else if (casilla + tab > max) {
      casilla = min;
    } else {
      casilla += tab;
    }
  return casilla;
}

void drawCentreString(const char *buf, int x, int y){
    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds(buf, 0, y, &x1, &y1, &w, &h); //calc width of new string
    tft.setCursor(x - w / 2, y);
    tft.print(buf);
}

void parpadeador(){                                       //funcion para el parpadeo del tiempo cuando se quiere modificar
  if (millis() - tsaved > 490 && millis() - tsaved < 510) {
    if (parpadeo == true){
      DHparpadeo[casi] = "  ";
      parpadeo = false;
    } else{
      parpadeo = true;
    }
    tsaved = millis();
  }
}

void writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }
    if(file.print(message)){
        Serial.println("File written");
    } else {
        Serial.println("Write failed");
    }
    file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Appending to file: %s\n", path);

    File file = fs.open(path, FILE_APPEND);
    if(!file){
        Serial.println("Failed to open file for appending");
        return;
    }
    if(file.print(message)){
        Serial.println("Message appended");
    } else {
        Serial.println("Append failed");
    }
    file.close();
}

void deleteFile(fs::FS &fs, const char * path){
    Serial.printf("Deleting file: %s\n", path);
    if(fs.remove(path)){
        Serial.println("File deleted");
    } else {
        Serial.println("Delete failed");
    }
}

void createDir(fs::FS &fs, const char * path){
    Serial.printf("Creating Dir: %s\n", path);
    if(fs.mkdir(path)){
        Serial.println("Dir created");
    } else {
        Serial.println("mkdir failed");
    }
}

//------------------------------WIFI---------------------------------
bool gettime = true;                //activa solo una vez, que cambie la hora y fecha del programa
void loop_wifi(void * pvParameters)
{
  while(1){
  if(setupwifi == true){
  WiFi.mode(WIFI_STA);
  esp_wifi_set_ps(WIFI_PS_NONE);
  setupwifi = false;
  }/*
    const char* ssid     = "AP1_SIAMINGPISO1";   //your network SSID 2174467277
    const char* password = "Siam2022*";   //your network password
    */
    const char* ssid     = "Se vende nevera";   //your network SSID
    const char* password = "nacho123";  //your network password
    
    Serial.println("");
    Serial.print("Connecting to ");
    Serial.println(ssid); 
    WiFi.begin(ssid, password); 

    unsigned long won = millis();
    while (WiFi.status() != WL_CONNECTED) {
      estatus = 0;
      //COLOCAR SIMBOLO DE PUNTOS EN WIFI PARA SABER QUE ESTÁ CARGANDO
      if (millis() - won > 30000){
        WiFi.begin(ssid, password); 
        won = millis();
        estatus = 1;
        //COLOCAR SIMBOLO DE CONEXION FALLIDA
        delay(5000);
        Serial.println("Reconnecting again");
        }
    }
    //COLOCAR SIMBOLO DE CONEXION ESTABLECIDO
    Serial.println("");
    Serial.println("STAIP address: ");
    Serial.println(WiFi.localIP());
    Serial.println("");
    while (WiFi.status() == WL_CONNECTED){     
      if (estatus != 3)estatus = 2;

      if(enviar == false)saveCapturedImage();             
      if(gettime == true && nventana == 1)changeTime();        
    }    
  }
}

void changeTime(){
  configTime(-18000,0,"pool.ntp.org");        //configuraciones para obtener hora de internet (#GMT,#segdia,sitio de extraccion hora)
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
  Serial.println("Failed to obtain time");
  estatus = 3;
  return;
  }else{Serial.println("Success obtain time");}
  estatus = 2;
  gettime = false;
  char ano[5];
  char mes[3];
  char dia[3];
  char hora[3];
  char minuto[3];
  strftime(ano,5, "%Y", &timeinfo);  
  strftime(mes,3, "%m", &timeinfo);  
  strftime(dia,3, "%e", &timeinfo);  
  strftime(hora,3, "%H", &timeinfo);  
  strftime(minuto,3, "%M", &timeinfo);  
  rtc.adjust(DateTime(atoi(ano), atoi(mes), atoi(dia), atoi(hora), atoi(minuto)));

}

String readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\n", path);
  File file = fs.open(path);
  String csvData;
  Serial.print("Read from file: ");
  while(file.available()){
    csvData += char(file.read());
  } 
  file.close();
  fs.open(path).close();

  return csvData;
}

String urlencode(String str)
{
    String encodedString="";
    char c;
    char code0;
    char code1;
    char code2;
    for (int i =0; i < str.length(); i++){
      c=str.charAt(i);
      if (c == ' '){
        encodedString+= '+';
      } else if (isalnum(c)){
        encodedString+=c;
      } else{
        code1=(c & 0xf)+'0';
        if ((c & 0xf) >9){
            code1=(c & 0xf) - 10 + 'A';
        }
        c=(c>>4)&0xf;
        code0=c+'0';
        if (c > 9){
            code0=c - 10 + 'A';
        }
        code2='\0';
        encodedString+='%';
        encodedString+=code0;
        encodedString+=code1;
      }
      yield();
    }
    return encodedString;
}

void saveCapturedImage() {
  String myScript = "/macros/s/AKfycbxzki1ulGv7w5zjZvSSMth-MA8zu6CSuWCBOvfh5AhxMfu-TJOYCVyP4spOZtvf5HIlQQ/exec";
  String myFilename = "filename=DATOS.csv";
  String mimeType = "&mimetype=text/csv";
  String myImage = "&data=";  
  const char* myDomain = "script.google.com";
  //String myScript = "/macros/s/AKfycbzobTBXBv7kdQ49tjLb4x15JTqhqg01cqLOpTJEuMo2wmwErF9WbwV0-vXMzHLpzOlk/exec";    //Replace with your own url
  short waitingTime = 6000; //Wait 30 seconds to google response.
  
  Serial.println("Connect to " + String(myDomain));
  WiFiClientSecure client;
  client.setInsecure();
  if (client.connect(myDomain, 443)) {
    Serial.println("Connection successful");
    
    fixread = true;
    while(fixread == true){Serial.print(fixread);}
    /*
    SD.open(namefile.c_str(), FILE_WRITE).close();
    SD.open(namefile.c_str(), FILE_APPEND).close();
    String DataCSV = readFile(SD, namefile.c_str());
    fixread = false;*/
    Serial.print(DataCSV);
    char *input = (char *)DataCSV.c_str();
    char output[base64_enc_len(3)];
    String imageFile = "";
    for (int i=0;i<DataCSV.length();i++) {
      base64_encode(output, (input++), 3);
      if (i%3==0) imageFile += urlencode(String(output));
    }
    String Data = myFilename+mimeType+myImage;
    
    Serial.println("Sending a file to Google Drive.");
    
    client.println("POST " + myScript + " HTTP/1.1");
    client.println("Host: " + String(myDomain));
    client.println("Content-Length: " + String(Data.length()+imageFile.length()));
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.println();
    
    client.print(Data);
    int Index;
    for (Index = 0; Index < imageFile.length(); Index = Index+1000) {
      client.print(imageFile.substring(Index, Index+1000));
    }
    
    Serial.println("Waiting for response.");
    unsigned long int StartTime=millis();
    while (!client.available()) {
      Serial.print(".");
      //delay(100);
      if ((StartTime+waitingTime) < millis()) {
        Serial.println();
        Serial.println("No response.");
        enviar = false;
        return;
      }
    }
    Serial.println();   
    while (client.available()) {
      Serial.print(char(client.read()));
      enviar = true;
      notifsubida = true;
      notif = millis();
    }  
  } else {         
    Serial.println("Connected to " + String(myDomain) + " failed.");
      enviar = false;
      return;
  }
  client.stop();
}

void EstadoWifi(){
  if(estrato != estatus){
  switch(estatus){
    case 0:
      tft.fillRect(10,20,30,10,CELESTE);
      tft.setCursor(10, 20);
      tft.setTextColor(ST77XX_BLACK); 
      tft.write(31);    tft.print("...");
      break;
    
    case 1:
      tft.fillRect(10,20,30,10,CELESTE);
      tft.setCursor(10, 20);
      tft.setTextColor(ST77XX_BLACK);
      tft.write(31);tft.print("X");
      break;
      
    case 2:
      tft.fillRect(10,20,30,10,CELESTE);
      tft.setTextColor(ST77XX_BLACK);
      tft.setCursor(10, 20);
      tft.write(31);  tft.write(251); 
      break;

    case 3:
      tft.fillRect(10,20,30,10,CELESTE);
      tft.setCursor(10, 20);
      tft.setTextColor(ST77XX_BLACK); 
      tft.write(31);    tft.print("!"); 
      break;
  }
  estrato = estatus;
  }
}