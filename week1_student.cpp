#include <stdio.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <time.h>
#include <math.h>
#include <sys/time.h>
#include <stdint.h>
#include <signal.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <curses.h>

//gcc -o week1 week_1.cpp -lwiringPi -lncurses -lm

#define frequency 25000000.0
#define CONFIG           0x1A
#define SMPLRT_DIV       0x19
#define GYRO_CONFIG      0x1B
#define ACCEL_CONFIG     0x1C
#define ACCEL_CONFIG2    0x1D
#define USER_CTRL        0x6A  // Bit 7 enable DMP, bit 3 reset DMP
#define PWR_MGMT_1       0x6B // Device defaults to the SLEEP mode
#define PWR_MGMT_2       0x6C
// #define M_PI             acos(-1.0) // PI for radian to degree conversion 


// Safety Limits
#define GYRO_LIM	300.0 	// Gyro should not exceed 300 degrees/sec
#define PITCH_ANG	45.0	// Pitch should not exceed (+/-) 45 degrees
#define ROLL_ANG	45.0	// Roll should not exceed (+/-) 45 degrees

enum Ascale {
  AFS_2G = 0,
  AFS_4G,
  AFS_8G,
  AFS_16G
};
 
enum Gscale {
  GFS_250DPS = 0,
  GFS_500DPS,
  GFS_1000DPS,
  GFS_2000DPS
};
 
int setup_imu();
void calibrate_imu();      
void read_imu();    
void update_filter();

//global variables

struct Keyboard {
  char key_press;
  int heartbeat;
  int version;
};
Keyboard* shared_memory; 
int run_program=1;
int imu;
float x_gyro_calibration=0;
float y_gyro_calibration=0;
float z_gyro_calibration=0;
float roll_calibration=0;
float pitch_calibration=0;
float accel_z_calibration=0;
float imu_data[6]; //gyro xyz, accel xyz
long time_curr;
long time_prev;
struct timespec te;
float yaw=0;
float pitch_angle=0;
float roll_angle=0;
float Roll=0;
float Pitch=0;
 
int main (int argc, char *argv[])
{

    setup_imu();
    calibrate_imu();

    //in main before while(1) loop add...
    setup_keyboard();
    signal(SIGINT, &trap);

    //to refresh values from shared memory first 
    Keyboard keyboard=*shared_memory;
    
    while(run_program==1)
    {
      read_imu();      
      update_filter();   
      safety_check();
     
    }
      
  
}

void safety_check()
{
// Use the limits and catch keyboard events to check if we need to stop the machine
// GYRO_LIM, PITCH_ANG, ROLL_ANG
// Also turn off if space is pressed, keyboard times out, or we catch CTRL+C (as configured above in main())
	if(shared_memory->key_press != null && shared_memory->key_press == " "){
		run_program=0;
	}
	
	if(Roll > ROLL_ANG || ROLL < -ROLL_ANG){
		run_program=0;
	}
	
	if(Pitch > PITCH_ANG || Pitch < -PITCH_ANG){
		run_program=0;
	}
	
	
// xyz gyro values read from imu_data[0..2]
	if(imu_data[0] > GYRO_LIM || imu_data[1] > GYRO_LIM || imu_data[2] > GYRO_LIM){
		run_program=0;
	}
	
	
}


void calibrate_imu()
{
  x_gyro_calibration = 0; // set all values to 0 initially
  y_gyro_calibration = 0;
  z_gyro_calibration = 0;
  roll_calibration = 0;
  pitch_calibration = 0;
  accel_z_calibration = 0;
  float ax = 0;
  float az = 0;
  float ay = 0;
  
  int i;
  for (i = 1; i <= 1000; i++) { // averaging over 1000 calculations for initial calculation
    
    int address = 67; // x gyro value address
    
    // high and low
    int vh = wiringPiI2CReadReg8(imu, address);
    int vl = wiringPiI2CReadReg8(imu, address+1);
    
    
    // two's complement
    int vw=(((vh<<8)&0xff00)|(vl&0x00ff))&0xffff;
    if(vw>0x8000)
    {
      vw=vw ^ 0xffff;
      vw=-vw-1;
    }
    
    x_gyro_calibration += (vw * 500.0 / 32768.0) / 1000.0; // normalized to dps + averaging
    
    address = 69; // y gyro value address
       
    // high and low
    vh = wiringPiI2CReadReg8(imu, address);
    vl = wiringPiI2CReadReg8(imu, address+1);
    
    
    // two's complement
    vw=(((vh<<8)&0xff00)|(vl&0x00ff))&0xffff;
    if(vw>0x8000)
    {
      vw=vw ^ 0xffff;
      vw=-vw-1;
    }
    
    y_gyro_calibration += (vw * 500.0 / 32768.0) / 1000.0; // normalized to dps + averaging
    
    address = 71; // z gyro value address
    
        
    // high and low
    vh = wiringPiI2CReadReg8(imu, address);
    vl = wiringPiI2CReadReg8(imu, address+1);
    
    
    // two's complement
    vw=(((vh<<8)&0xff00)|(vl&0x00ff))&0xffff;
    if(vw>0x8000)
    {
      vw=vw ^ 0xffff;
      vw=-vw-1;
    }
    
    z_gyro_calibration += (vw * 500.0 / 32768.0) / 1000.0; // normalized to dps + averaging
    
    
    // getting x accel and y accel values to calculate roll and pitch

    address = 59; // x accel value location
    
    // high and low
    vh = wiringPiI2CReadReg8(imu, address);
    vl = wiringPiI2CReadReg8(imu, address+1);
    
    
    // two's complement
    vw=(((vh<<8)&0xff00)|(vl&0x00ff))&0xffff;
    if(vw>0x8000)
    {
      vw=vw ^ 0xffff;
      vw=-vw-1;
    }
    
    ax = vw;  // setting ax value
    
    address = 61; // y accel value location
    
    // high and low
    vh = wiringPiI2CReadReg8(imu, address);
    vl = wiringPiI2CReadReg8(imu, address+1);
    
    
    // two's complement
    vw=(((vh<<8)&0xff00)|(vl&0x00ff))&0xffff;
    if(vw>0x8000)
    {
      vw=vw ^ 0xffff;
      vw=-vw-1;
    }
    
    ay = vw;  // setting ay value
        
    address = 63; // z accel value location
       
    // high and low
    vh = wiringPiI2CReadReg8(imu, address);
    vl = wiringPiI2CReadReg8(imu, address+1);
    
    
    // two's complement
    vw=(((vh<<8)&0xff00)|(vl&0x00ff))&0xffff;
    if(vw>0x8000)
    {
      vw=vw ^ 0xffff;
      vw=-vw-1;
    }
    
    
    accel_z_calibration += (vw * 2.0 / 32768.0) / 1000.0; // normalized to dps + averaging

    az = vw;  // setting az value
     

    // roll and pitch calculations

    roll_calibration += (atan2(ay, -az)*180.0/M_PI)/1000.0;

	  pitch_calibration += (atan2(ax, -az)*180.0/M_PI)/1000.0;

  }
  
  printf("calibration complete, %f %f %f %f %f %f\n\r",x_gyro_calibration,y_gyro_calibration,z_gyro_calibration,roll_calibration,pitch_calibration,accel_z_calibration);


}

void read_imu()
{
  int address=59; // address for accel x value 
  float ax=0;
  float az=0;
  float ay=0; 
  float roll = 0;
  float pitch = 0;
  int vh,vl;
  
  //read in data
  vh=wiringPiI2CReadReg8(imu,address);
  vl=wiringPiI2CReadReg8(imu,address+1);
  //convert 2 complement
  int vw=(((vh<<8)&0xff00)|(vl&0x00ff))&0xffff;
  if(vw>0x8000)
  {
    vw=vw ^ 0xffff;
    vw=-vw-1;
  }          
  imu_data[3] = (vw * 2.0 / 32768.0); //convert vw from raw values to g's (normalize by 2/32768.0)
  
  
  address=61; // address value for accel y value
  vh=wiringPiI2CReadReg8(imu,address);
  vl=wiringPiI2CReadReg8(imu,address+1);
  vw=(((vh<<8)&0xff00)|(vl&0x00ff))&0xffff;
  if(vw>0x8000)
  {
    vw=vw ^ 0xffff;
    vw=-vw-1;
  }          
  imu_data[4] = (vw * 2.0 / 32768.0); //convert vw from raw values to g's
  
  
  address=63; //address value for accel z value;
  vh=wiringPiI2CReadReg8(imu,address);
  vl=wiringPiI2CReadReg8(imu,address+1);
  vw=(((vh<<8)&0xff00)|(vl&0x00ff))&0xffff;
  if(vw>0x8000)
  {
    vw=vw ^ 0xffff;
    vw=-vw-1;
  }          
  imu_data[5] = (vw * 2.0 / 32768.0); //convert vw from raw values to g's
  
  
  address=67; // gyro x value;
  vh=wiringPiI2CReadReg8(imu,address);
  vl=wiringPiI2CReadReg8(imu,address+1);
  vw=(((vh<<8)&0xff00)|(vl&0x00ff))&0xffff;
  if(vw>0x8000)
  {
    vw=vw ^ 0xffff;
    vw=-vw-1;
  }          
  imu_data[0]= -x_gyro_calibration + (vw * 500.0 / 32768.0); //// convert vw from raw values to degrees/second (normalize by 500/32768.0)
  
  address=69;//todo: set addres value for gyro y value;
  vh=wiringPiI2CReadReg8(imu,address);
  vl=wiringPiI2CReadReg8(imu,address+1);
  vw=(((vh<<8)&0xff00)|(vl&0x00ff))&0xffff;
  if(vw>0x8000)
  {
    vw=vw ^ 0xffff;
    vw=-vw-1;
  }          
  imu_data[1]= -y_gyro_calibration + (vw * 500.0 / 32768.0); //// convert vw from raw values to degrees/second
  
  address=71;//// gyro z value;
  vh=wiringPiI2CReadReg8(imu,address);
  vl=wiringPiI2CReadReg8(imu,address+1);
  vw=(((vh<<8)&0xff00)|(vl&0x00ff))&0xffff;
  if(vw>0x8000)
  {
    vw=vw ^ 0xffff;
    vw=-vw-1;
  }          
  imu_data[2]= -z_gyro_calibration + (vw * 500.0 / 32768.0);////todo: convert vw from raw values to degrees/second
  
  
  az = imu_data[5];  // pulling out the values to display
  ay = imu_data[4];
  ax = imu_data[3];
  
  // roll and pitch calculations

  roll = -roll_calibration + (atan2(imu_data[4], -imu_data[5])*180.0/M_PI);
  
  pitch = -pitch_calibration + (atan2(imu_data[3], -imu_data[5])*180.0/M_PI); 

  //printf("Gyros: (%10.5f %10.5f %10.5f), Roll:  %10.5f, Pitch: %10.5f\n\r",imu_data[0], imu_data[1], imu_data[2], roll, pitch);
  printf()
 


}

void update_filter()
{

  //get current time in nanoseconds
  timespec_get(&te,TIME_UTC);
  time_curr=te.tv_nsec;
  //compute time since last execution
  float imu_diff=time_curr-time_prev;           
  
  //check for rollover
  if(imu_diff<=0)
  {
    imu_diff+=1000000000;
  }
  //convert to seconds
  imu_diff=imu_diff/1000000000;
  time_prev=time_curr;
  
  //comp. filter for roll, pitch here:
  int address=59; // address for accel x value 
  float ax=0;
  float az=0;
  float ay=0;
  float roll_accel=0;
  float pitch_accel=0;
  int vh,vl;
  
  //read in data
  vh=wiringPiI2CReadReg8(imu,address);
  vl=wiringPiI2CReadReg8(imu,address+1);
  //convert 2 complement
  int vw=(((vh<<8)&0xff00)|(vl&0x00ff))&0xffff;
  if(vw>0x8000)
  {
    vw=vw ^ 0xffff;
    vw=-vw-1;
  }          
  ax = (vw * 2.0 / 32768.0); //convert vw from raw values to g's (normalize by 2/32768.0)
  
  
  address=61; // address value for accel y value
  vh=wiringPiI2CReadReg8(imu,address);
  vl=wiringPiI2CReadReg8(imu,address+1);
  vw=(((vh<<8)&0xff00)|(vl&0x00ff))&0xffff;
  if(vw>0x8000)
  {
    vw=vw ^ 0xffff;
    vw=-vw-1;
  }          
  ay = (vw * 2.0 / 32768.0); //convert vw from raw values to g's
  
  
  address=63; //address value for accel z value;
  vh=wiringPiI2CReadReg8(imu,address);
  vl=wiringPiI2CReadReg8(imu,address+1);
  vw=(((vh<<8)&0xff00)|(vl&0x00ff))&0xffff;
  if(vw>0x8000)
  {
    vw=vw ^ 0xffff;
    vw=-vw-1;
  }          
  az = (vw * 2.0 / 32768.0); //convert vw from raw values to g's

  // roll_accel is the angle.
  roll_accel = -roll_calibration + (atan2(ay, -az)*180.0/M_PI);
  pitch_accel = -pitch_calibration + (atan2(ax, -az)*180.0/M_PI);

  address=69;//todo: set addres value for gyro y value;
  vh=wiringPiI2CReadReg8(imu,address);
  vl=wiringPiI2CReadReg8(imu,address+1);
  vw=(((vh<<8)&0xff00)|(vl&0x00ff))&0xffff;
  if(vw>0x8000)
  {
    vw=vw ^ 0xffff;
    vw=-vw-1;
  }          
  float roll_gyro;
  roll_gyro = -y_gyro_calibration + (vw * 500.0 / 32768.0); //// convert vw from raw values to degrees/second

  float roll_gyro_delta;
  roll_gyro_delta = roll_gyro * imu_diff;
  
  address=67; // gyro x value;
  vh=wiringPiI2CReadReg8(imu,address);
  vl=wiringPiI2CReadReg8(imu,address+1);
  vw=(((vh<<8)&0xff00)|(vl&0x00ff))&0xffff;
  if(vw>0x8000)
  {
    vw=vw ^ 0xffff;
    vw=-vw-1;
  }     
  float pitch_gyro;     
  pitch_gyro = -x_gyro_calibration + (vw * 500.0 / 32768.0); //// convert vw from raw values to degrees/second (normalize by 500/32768.0)
  
  float pitch_gyro_delta;
  pitch_gyro_delta = pitch_gyro * imu_diff;

  float a;  // tunes roll
  a = 0.02;
  Roll = roll_accel * a + (1-a) * (roll_gyro_delta * Roll);

  float b;  // tunes pitch
  b = 0.02;
  Pitch = pitch_accel * b + (1-b) * (pitch_gyro_delta * Pitch);


  // Print graph values for roll - checkpoint 2
  printf("%10.5f, %10.5f, %10.5f", Roll, roll_accel, roll_gyro);
}

//function to add
void setup_keyboard()
{

  int segment_id;   
  struct shmid_ds shmbuffer; 
  int segment_size; 
  const int shared_segment_size = 0x6400; 
  int smhkey=33222;
  
  /* Allocate a shared memory segment.  */ 
  segment_id = shmget (smhkey, shared_segment_size,IPC_CREAT | 0666); 
  /* Attach the shared memory segment.  */ 
  shared_memory = (Keyboard*) shmat (segment_id, 0, 0); 
  printf ("shared memory attached at address %p\n", shared_memory); 
  /* Determine the segment's size. */ 
  shmctl (segment_id, IPC_STAT, &shmbuffer); 
  segment_size  =               shmbuffer.shm_segsz; 
  printf ("segment size: %d\n", segment_size); 
  /* Write a string to the shared memory segment.  */ 
  //sprintf (shared_memory, "test!!!!."); 

}


//when cntrl+c pressed, kill motors

void trap(int signal)

{

   
 
   printf("ending program\n\r");

   run_program=0;
}


int setup_imu()
{
  wiringPiSetup ();
  
  
  //setup imu on I2C
  imu=wiringPiI2CSetup (0x68) ; //accel/gyro address
  
  if(imu==-1)
  {
    printf("-----cant connect to I2C device %d --------\n",imu);
    return -1;
  }
  else
  {
  
    printf("connected to i2c device %d\n",imu);
    printf("imu who am i is %d \n",wiringPiI2CReadReg8(imu,0x75));
    
    uint8_t Ascale = AFS_2G;     // AFS_2G, AFS_4G, AFS_8G, AFS_16G
    uint8_t Gscale = GFS_500DPS; // GFS_250DPS, GFS_500DPS, GFS_1000DPS, GFS_2000DPS
    
    
    //init imu
    wiringPiI2CWriteReg8(imu,PWR_MGMT_1, 0x00);
    printf("                    \n\r");
    wiringPiI2CWriteReg8(imu,PWR_MGMT_1, 0x01);
    wiringPiI2CWriteReg8(imu, CONFIG, 0x00);  
    wiringPiI2CWriteReg8(imu, SMPLRT_DIV, 0x00); //0x04        
    int c=wiringPiI2CReadReg8(imu,  GYRO_CONFIG);
    wiringPiI2CWriteReg8(imu,  GYRO_CONFIG, c & ~0xE0);
    wiringPiI2CWriteReg8(imu, GYRO_CONFIG, c & ~0x18);
    wiringPiI2CWriteReg8(imu, GYRO_CONFIG, c | Gscale << 3);       
    c=wiringPiI2CReadReg8(imu, ACCEL_CONFIG);
    wiringPiI2CWriteReg8(imu,  ACCEL_CONFIG, c & ~0xE0); // Clear self-test bits [7:5] 
    wiringPiI2CWriteReg8(imu,  ACCEL_CONFIG, c & ~0x18); // Clear AFS bits [4:3]
    wiringPiI2CWriteReg8(imu,  ACCEL_CONFIG, c | Ascale << 3);      
    c=wiringPiI2CReadReg8(imu, ACCEL_CONFIG2);         
    wiringPiI2CWriteReg8(imu,  ACCEL_CONFIG2, c & ~0x0F); //
    wiringPiI2CWriteReg8(imu,  ACCEL_CONFIG2,  c | 0x00);
  }
  return 0;
}


