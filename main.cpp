//Test of cheap 13.56 Mhz RFID-RC522 module from eBay
//This code is based on Martin Olejar's MFRC522 library. Minimal changes
//Adapted for Nucleo STM32 F401RE. Should work on other Nucleos too

//Connect as follows:
//RFID pins        ->  Nucleo header CN5 (Arduino-compatible header)
//----------------------------------------
//RFID IRQ=pin5    ->   Not used. Leave open
//RFID MISO=pin4   ->   Nucleo SPI_MISO=PA_6=D12
//RFID MOSI=pin3   ->   Nucleo SPI_MOSI=PA_7=D11
//RFID SCK=pin2    ->   Nucleo SPI_SCK =PA_5=D13
//RFID SDA=pin1    ->   Nucleo SPI_CS  =PB_6=D10
//RFID RST=pin7    ->   Nucleo         =PA_9=D8
//3.3V and Gnd to the respective pins

#include "mbed.h"
#include "MFRC522.h"
#include "MQTTEthernet.h"
#include "MQTTClient.h"
#include "TextLCD.h"
#include <sstream>
#include <string>
#define ECHO_SERVER_PORT   7
#define SPI_MOSI    D11
#define SPI_MISO    D12
#define SPI_SCLK    D13
#define SPI_CS      D10

// WIZWiki-W7500 Pin for MFRC522 reset(pick another D pin if you need D8)
#define MF_RESET    D9

TextLCD lcd(D2, D3, D4, D5, D6, D7); // rs, e, d4-d7

Serial out(USBTX,USBRX);
int arrivedcount = 0;
char RFIDTagMessage[10];

void messageArrived(MQTT::MessageData& md)
{
    MQTT::Message &message = md.message;
    printf("Message arrived: qos %d, retained %d, dup %d, packetid %d\n", message.qos, message.retained, message.dup, message.id);
    printf("Payload %.*s\n", message.payloadlen, (char*)message.payload);
    ++arrivedcount;
}

void baud(int baudrate)
{
    Serial s(USBTX, USBRX);
    s.baud(baudrate);
}


MFRC522    RfChip (SPI_MOSI, SPI_MISO, SPI_SCLK, SPI_CS, MF_RESET);        // Init MFRC522 card

DigitalOut LedGreen(D0);
DigitalOut LedRed(D1);
DigitalOut Buzzer(D4);
char* store_buf;

int main(void)
{
    printf("starting...\n");

    // Init. RC522 Chip
    RfChip.PCD_Init();
    MQTTEthernet ipstack = MQTTEthernet();


    while (true) {
       // printf("starting LCD initi ..\n");

        lcd.locate(0,0);
        lcd.printf("Attendance\n");
        lcd.locate(0,1);
        lcd.printf(" System\n");
        // Look for new cards
        if ( ! RfChip.PICC_IsNewCardPresent()) {
            wait_ms(50);
            continue;
        }

        // Select one of the cards
        if ( ! RfChip.PICC_ReadCardSerial()) {
            wait_ms(50);
            continue;
        }

        lcd.cls();
        wait_ms(50);

        //Size

        printf("Size: %d \n",RfChip.uid.size);
        printf("\n\r");
        // Print Card UID
        printf("Card UID: ");
        for (uint8_t i = 0; i < RfChip.uid.size; i++) {
            printf(" %X", RfChip.uid.uidByte[i]);
            store_buf+= sprintf(store_buf,"%X",RfChip.uid.uidByte[i]);
        }
        printf("\n\r");
        store_buf=RFIDTagMessage;
        printf("Card ID: %s",store_buf);
 
         if(store_buf[0]!='\0') {


            MQTT::Client<MQTTEthernet, Countdown> client = MQTT::Client<MQTTEthernet, Countdown>(ipstack);

            char* hostname = "iot.eclipse.org";
            int port = 1883;

            int rc = ipstack.connect(hostname, port);

            out.printf("rc from TCP connect is %d\n", rc);

            if (rc != 0)
                out.printf("rc from TCP connect is %d\n", rc);

            char MQTTClientID[30];

            MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
            data.MQTTVersion = 3;
            sprintf(MQTTClientID,"WIZwiki-W7500-client-%d",rand()%1000);
            data.clientID.cstring = MQTTClientID;
            /*   data.username.cstring = "testuser";
               data.password.cstring = "testpassword"; */

            if ((rc = client.connect(data)) != 0)
                printf("rc from MQTT connect is %d\n", rc);

            MQTT::Message message;

            message.qos = MQTT::QOS0;
            message.retained = false;
            message.dup = false;

            message.payload = (void*)RFIDTagMessage;
            message.payloadlen = strlen(RFIDTagMessage);

            rc = client.publish("my/attendance", message);
            printf("Rc result: % d ",rc);
            if(rc!=0) {
            lcd.printf(" FAILED..! TRY AGAIN!\n");
                lcd.locate(4,0);
                lcd.printf("FAILED.!\n");
                lcd.locate(2,1);
                lcd.printf("TRY AGAIN..\n");

                LedRed=1;

                LedGreen = 0;
                wait_ms(3000);
                LedRed=0;

                lcd.cls();
                // NVIC_SystemReset();
            } else {
                lcd.printf(" USER VERIFIED!\n");
                LedGreen = 1;
                wait_ms(2000);
                LedGreen = 0;
                lcd.cls();
            }
        }



        // Print Card type
        uint8_t piccType = RfChip.PICC_GetType(RfChip.uid.sak);
        printf("PICC Type: %s \n\r", RfChip.PICC_GetTypeName(piccType));
        wait_ms(50);

    }
}
