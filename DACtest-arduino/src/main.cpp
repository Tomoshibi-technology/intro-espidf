#include <Arduino.h>
#include <driver/i2s.h>

#define PARAM_SAMPLING_FREQUENCY  (44100)
#define NUM_BUFFER_LEN    (1024)
#define PIN_DACOUT (25)

uint8_t gmuiBuffer[NUM_BUFFER_LEN];

const uint8_t cui_DC_OFFSET = 127;//fixed value
const float cf_FREQ_ORIGN = 55.0;

const uint8_t cuiBPM = 80; //beats per minute

const uint8_t cui_AMPLITUDE = 100;//max 127
const uint8_t cuiNumNote = 14; //number of note
const uint8_t cmuiMusLen[]={4,4,4,4,4,4,2,  4,4,4,4,4,4,2 };
const uint8_t cmuiMusNote[]={3, 3, 10 , 10 , 12 ,12, 10    ,8, 8, 7,7,5,5,3};
//   0 1  2 3 4  5 6  7 8 9  10 11 12  13  14
//   a a# b c c# d d# e f f# g  g#  a  a#  b
const uint8_t cuiMusOctave = 3; 

//const uint8_t cuiToneNumber = 0; //0:sin, 1:saw, 2:triangle, 3:rectangle8, 4:table
#define TONE_NUMBER (2) //0:sin, 1:saw, 2:triangle, 3:rectangle8, 4:table

#define NUM_LENGTH_WAVE_TABLE (50)
const float  cf_PARAM_TABLE_ORIGIN_FREQ =(880.0);
const float  cf_PARAM_TABLE_INDEX_COEF = ((float)PARAM_SAMPLING_FREQUENCY/880.0);
//const char cmcTableWave[] = "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF01";
const char cmcTableWave[] =   "89ABCCCCCCDDDDDDEEDCBA9876543211112222333333334567";
const uint8_t cui_VAL_WAVE_TABLE_HIGHT = (16);
const uint8_t cui_VAL_WAVE_TABLE_DC = (cui_VAL_WAVE_TABLE_HIGHT>>1);
const float cf_VAL_WAVE_TABLE_HIGHT_INV = 1/(float)cui_VAL_WAVE_TABLE_HIGHT;
uint8_t gmuiTableWave[NUM_LENGTH_WAVE_TABLE];

const float cf_ENVELOPE_DECAY_INV = ((1/(float)PARAM_SAMPLING_FREQUENCY/2));

float gmfItn[cuiNumNote]; // inversed cycle of current pitch
uint32_t gmuiTn[cuiNumNote];//cycle of current pitch
//float gfMusNotesLen = (float)PARAM_SAMPLING_FREQUENCY*60.0*4 /(float)cuiBPM /(float)guiMusLen+0.5; 
float gmfMusNotesLen[cuiNumNote];

uint32_t guiPosNoteCurrent = 0;//index for Current Note
uint32_t guiXt = 0; //counter for ocsillator
uint32_t guiXn = 0; //counter for note
uint32_t guiXe = 0; //counter for envelope


void setup_i2s()
{
  i2s_config_t i2s_config = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN),
    .sample_rate          = PARAM_SAMPLING_FREQUENCY,
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S_MSB,
    .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count        = 2,
    .dma_buf_len          = NUM_BUFFER_LEN,
    .use_apll             = false,
    .tx_desc_auto_clear   = true,
    .fixed_mclk           = 0,
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, NULL);               // 25, 26
  //i2s_set_dac_mode(I2S_DAC_CHANNEL_BOTH_EN);  // 25, 26
  //i2s_set_dac_mode(I2S_DAC_CHANNEL_RIGHT_EN); // 25
  //i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN ); // 26
  i2s_zero_dma_buffer(I2S_NUM_0);
}//end setup_i2s()

float sg_envelope(){
  float ftmp = 1.0;
  ftmp -= cf_ENVELOPE_DECAY_INV*guiXe;
  ftmp *= (float)(ftmp>0);
  return ftmp;
}

int16_t sg_sin(){
  float ftmp = sin(2*PI*gmfItn[guiPosNoteCurrent]*(float)guiXt);
  int16_t iRes = (int16_t)((float)cui_AMPLITUDE * ftmp * sg_envelope() );
  return iRes;
}//end sg_sin()

int16_t sg_saw(){
  int16_t iRes=0;
  float ftmp =  (gmfItn[guiPosNoteCurrent]*(float)guiXt) ;
  iRes = (int16_t)((float)cui_AMPLITUDE * (2 * ftmp -1) * sg_envelope()); 
  return iRes;
}//end sg_saw()


int16_t sg_triangle(){
  int16_t iRes=0;
  float ftmp = 0;
  if( guiXt > (gmuiTn[guiPosNoteCurrent]>>1) ){
    ftmp =  2-(gmfItn[guiPosNoteCurrent]*2 * (float)guiXt );
  }
  else{
    ftmp =  (gmfItn[guiPosNoteCurrent]*2 * (float)guiXt);
  }
  iRes = (int16_t)((float)cui_AMPLITUDE * (2 * ftmp -1)* sg_envelope()); 
  return iRes;
}//end sg_triangle()

int16_t sg_rectangle8(){
  int16_t iRes=0;
  float ftmp = 0;
  if( guiXt < (gmuiTn[guiPosNoteCurrent]>>3) ){
    ftmp =  -1;
  }
  if( guiXt < (gmuiTn[guiPosNoteCurrent]>>4) ){
    ftmp =  1;
  }
  iRes = (int16_t)((float)cui_AMPLITUDE * 1 * ftmp * sg_envelope() ); 
  return iRes;
}//end sg_rectangle8()


int16_t sg_waveTable(){
  int16_t iRes=0;
  float ftmp = 0;
  uint32_t uiInx = (uint32_t)((float)guiXt *gmfItn[guiPosNoteCurrent]*cf_PARAM_TABLE_INDEX_COEF);
  ftmp = (float)((int32_t)gmuiTableWave[uiInx] - (int32_t)cui_VAL_WAVE_TABLE_DC)* cf_VAL_WAVE_TABLE_HIGHT_INV ;
  iRes = (int16_t)((float)cui_AMPLITUDE * 1 * ftmp * sg_envelope() ); 
  return iRes;
}//end sg_waveTable()


uint8_t sg_main(){
  int16_t iRes = 0;

  switch (TONE_NUMBER) {
    case 1:
      iRes = sg_saw();
      break;
    case 2:
      iRes = sg_triangle();
      break;
    case 3:
      iRes = sg_rectangle8();
      break;
    case 4:
      iRes = sg_waveTable();
      break;
    default:
      iRes = sg_sin();
      break;
  }
  //iRes = sg_sin();
  //iRes = sg_saw();
  //iRes = sg_triangle();
  //iRes = sg_rectangle8();
  //iRes = sg_waveTable();

  iRes += cui_DC_OFFSET;

  guiXt++;
  guiXn++;
  guiXe++;

  // reset counter for current pitch
  if( guiXt >= gmuiTn[guiPosNoteCurrent]){
    guiXt = 0;
  }

  // reset counter for current note
  if( guiXn >= gmfMusNotesLen[guiPosNoteCurrent]){
    guiXn = 0;//for note
    guiXe = 0;//for envelope
    guiPosNoteCurrent +=1;    
    if(guiPosNoteCurrent >= cuiNumNote){
      guiPosNoteCurrent = 0; //loop music
    }//end if
  }//end if

  return (uint8_t)iRes;
}//end sg_main()


#define VAL_SECONDS_IN_MINUTE (60.0)
#define VAL_BASE_BEAT_NOTE_LENGTH (4.0)
#define VAL_ROUNDUP_NUMBER (0.5)
#define VAL_OCTAVE_DIVIDE_NUMBER (12.0)

void  setup_calc_music_note(){
  for(uint32_t k =0; k < cuiNumNote; k++){
    gmfMusNotesLen[k] = (float)PARAM_SAMPLING_FREQUENCY*VAL_SECONDS_IN_MINUTE*VAL_BASE_BEAT_NOTE_LENGTH /(float)cuiBPM /(float)cmuiMusLen[k]+VAL_ROUNDUP_NUMBER;
    gmfItn[k] = pow(2 ,((float)cmuiMusNote[k])/VAL_OCTAVE_DIVIDE_NUMBER+(float)cuiMusOctave) * cf_FREQ_ORIGN/(float)PARAM_SAMPLING_FREQUENCY;
    gmuiTn[k] = (1.0/gmfItn[k]);//floor
  }
}//end setup_calc_music_note()

int hex2int(char y){
  uint8_t x = (uint8_t)y;
  if( x>=48 && x<=57) return ( x-48);//0-9
  if( x>=65 && x<=70) return ( x-55);//A-F
  if( x>=97 && x<=102) return ( x-87);//a-f
  return (0);
}

void setup_table_wave(){
  for(uint32_t k =0; k < NUM_LENGTH_WAVE_TABLE; k++){
    gmuiTableWave[k] = hex2int(cmcTableWave[k]);
    //Serial.println(gmuiTableWave[k]);
  }
}//end setup_table_wave()

void setup_zero_clear_buffer(){
  for (uint32_t k = 0; k < NUM_BUFFER_LEN; k++) gmuiBuffer[k] = 0;
}//end setup_zeroClear_buffer()

void setup() {
  Serial.begin(115200);
  // pinMode(33, INPUT); 
}

long long t = 0;
void loop() {
	int hoge[] = {3,5,7,8,10,12,14,15};
	int music = 
	(abs((t*150.0*int(pow(2,hoge[(t>>14)%8]/12)/8000.0*256.0))%256-127)*(255.0/128) 
	+ ((t >> 10) & 3) * t % 50 
	)%256;

	dacWrite(25,music);
	t+=1;
	// delay(1);

	// Serial.print(analogRead(33));
	// Serial.print(" ");
	// Serial.print(j*(38&j>>10) + j*(j>>10));
	// Serial.println(" ");
}//end loop()














