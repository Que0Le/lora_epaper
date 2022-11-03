#include <SPI.h>
#include <LoRa.h>
#include <SPI.h>

// define the pins used by the transceiver module
#define ss 5
#define rst 14
#define dio0 2

/*  */
#include "epd2in13b_V3.h"
#include <miniz.h>

#define COLORED 0
#define UNCOLORED 1


#include <EEPROM.h>
#define EEPROM_SIZE 1

#define BUFF_LEN_2_13B 2756 //212*104/8
unsigned char *BUFF_BLACK_IMAGE; 
unsigned char *BUFF_RED_IMAGE;
unsigned char *BUFF_COMPRESSED_BLACK_IMAGE; 
unsigned char *BUFF_COMPRESSED_RED_IMAGE; 

int counter = 0;
int resetCount = 0;

void setup_eeprom()
{
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(0, 0);
    EEPROM.commit();
}

struct packet_struct {
    unsigned char host_code;
    unsigned char flag;
    unsigned char data_type;    // 0->black image, 1->red image
    unsigned int total_length;  // total length of file to transmitt
    unsigned int index_start;   // where does it start (0->total_length-1)
    unsigned char data_length;  // length of this fragment
    char data[100];
};

struct control_struct {
    unsigned char host_code;
    unsigned char flag;
    unsigned char is_encrypted;
    unsigned char is_compressed;
    unsigned char message_type;    // 
    unsigned char request_status;  // 0: accepted and transmitting, 1: deny, 2: error, 3: done
    unsigned char message[30];     // 
};

packet_struct *pkt;
control_struct *ctrl;
#define DATA_TYPE_BLACK_IMAGE 0
#define DATA_TYPE_RED_IMAGE 1


void setup()
{
    delay(1000);
    Serial.begin(115200);
    while (!Serial)
        ;
    setup_eeprom();

    // Init mem
    pkt = (packet_struct *)malloc(sizeof(packet_struct));
    ctrl = (control_struct *)malloc(sizeof(control_struct));

    if ((BUFF_BLACK_IMAGE = (unsigned char *)malloc(BUFF_LEN_2_13B)) == NULL) {
        printf("Failed alloc BUFF_BLACK_IMAGE...\r\n");
        while (1);
    }
    if ((BUFF_RED_IMAGE = (unsigned char *)malloc(BUFF_LEN_2_13B)) == NULL) {
        printf("Failed alloc BUFF_RED_IMAGE...\r\n");
        while (1);
    }
    for (int i=0;i<BUFF_LEN_2_13B; i++) {
        BUFF_RED_IMAGE[i] = 0;
        BUFF_BLACK_IMAGE[i] = 0;
    }
    Serial.println("Allocated mem for back and red buffs");

    /* Init loram */
    LoRa.setPins(ss, rst, dio0);
    while (!LoRa.begin(866E6))
    {
        Serial.println(".");
        delay(500);
    }
    LoRa.setSyncWord(0xF3);    // ranges from 0-0xFF
    Serial.println("LoRa Initializing OK!");

}

int rq_sent = 0;
int request_status = -1;
char buffer[256];
unsigned int written_bytes[2] = {0, 0};
unsigned int buff_comp_len_s[2] = {0, 0};

void loop()
{
    if (rq_sent==0 && request_status==-1) {
        // Send request
        Serial.println("sending request");
        LoRa.beginPacket();
        LoRa.print("{\"msg_type\":\"rqst_img\",\"dsp_type\":\"2in13b_V3\",\"board\":\"esp32\",\"uniq_id\":\"nbr000\"}");
        LoRa.print(counter);
        LoRa.endPacket();
        Serial.println("request sent");
        rq_sent = 1;
    }
    // Handle message
    int packetSize = LoRa.parsePacket();
    if (packetSize)
    {
        // read packet
        while (LoRa.available())
        {
            int nbr_byte = LoRa.readBytes(buffer, 256);
            // Serial.print("nbr_byte: ");
            if (nbr_byte==sizeof(control_struct)) { // control packet
                memcpy(ctrl, buffer, sizeof(control_struct));
                unsigned int temp = 0;
                memcpy(&temp, ctrl->message, 4);
                buff_comp_len_s[0] += temp; // somehow assign with "=" didn't work!
                memcpy(&temp, ctrl->message+4, 4);
                buff_comp_len_s[1] += temp;
                request_status = ctrl->request_status;
                Serial.printf("Got control packet ... request_status: %d\n", request_status);
                if (request_status == 1) {
                    if ((BUFF_COMPRESSED_BLACK_IMAGE = (unsigned char *)malloc(buff_comp_len_s[0])) == NULL) {
                        printf("Failed alloc BUFF_COMPRESSED_BLACK_IMAGE...\r\n");
                        while (1);
                    }
                    if ((BUFF_COMPRESSED_RED_IMAGE = (unsigned char *)malloc(buff_comp_len_s[1])) == NULL) {
                        printf("Failed alloc BUFF_COMPRESSED_RED_IMAGE...\r\n");
                        while (1);
                    }
                    Serial.printf(
                        "Allocated mem for COMPRESSED back (%d) and red (%d) buffs\n", 
                        buff_comp_len_s[0], buff_comp_len_s[1]
                    );
                }
            } else /* if (nbr_byte==sizeof(packet_struct)) */ {
                memcpy(pkt, buffer, sizeof(packet_struct));
                Serial.printf("buff_compressed_lengths[%d]=%d\n", pkt->data_type, buff_comp_len_s[pkt->data_type]);
                Serial.printf("... received data: index_start: %d, data_length: %d\n", pkt->index_start, pkt->data_length);
                // Make sure we don't exceed the buffer
                if (pkt->data_type>1 || pkt->index_start>=buff_comp_len_s[pkt->data_type])
                    break;
                // Calculate the correct bytes to copy. Can't always trust server!
                size_t bytes_to_copy = (pkt->index_start + pkt->data_length) >= buff_comp_len_s[pkt->data_type]
                    ? buff_comp_len_s[pkt->data_type] - pkt->index_start
                    : pkt->data_length;
                // Copy to compressed buffer
                if (pkt->data_type==0) {
                    memcpy(BUFF_COMPRESSED_BLACK_IMAGE + pkt->index_start, pkt->data, bytes_to_copy);
                } else if (pkt->data_type==1) {
                    memcpy(BUFF_COMPRESSED_RED_IMAGE + pkt->index_start, pkt->data, bytes_to_copy);
                }
                written_bytes[pkt->data_type] += bytes_to_copy;
            }
        }
    }

    // Done
    if (rq_sent==1 && request_status==3) {
        int uncomp_success = 1;
        Serial.printf("Receiving done. Start decompressing data ...\n");
        // Serial.printf("buff_compressed_lengths: %d %d\n", buff_comp_len_s[0], buff_comp_len_s[1]);
        Serial.printf("\nwritten_bytes COMPRESSSED BLACK: %d, RED: %d\n", written_bytes[0], written_bytes[1]);

        for (int d_type=0; d_type<2; d_type++) {
            uLong uncomp_len = BUFF_LEN_2_13B;
            uint total_succeeded = 0;
            uint step = 0;
            int cmp_status = 0;
            if (d_type==0) {
                cmp_status = uncompress(
                    BUFF_BLACK_IMAGE, &uncomp_len, 
                    (const unsigned char *)BUFF_COMPRESSED_BLACK_IMAGE, (uLong)buff_comp_len_s[d_type]
                );
            } else {
                cmp_status = uncompress(
                    BUFF_RED_IMAGE, &uncomp_len, 
                    (const unsigned char *)BUFF_COMPRESSED_RED_IMAGE, (uLong)buff_comp_len_s[d_type]
                );
            }
            total_succeeded += (cmp_status == Z_OK);
            if (cmp_status != Z_OK) {
                Serial.printf("uncompress failed!\n");
                uncomp_success = 0;
            } else {
                Serial.printf("Decompressed from %u to %u bytes\n", (mz_uint32)buff_comp_len_s[d_type], (mz_uint32)uncomp_len);
            }
        }

        // Serial.println("BUFF_BLACK_IMAGE content: ");
        // for (int i=0; i<BUFF_LEN_2_13B; i++) {
        //     Serial.printf("%c", BUFF_BLACK_IMAGE[i]);
        // }
        // Serial.print("\n#####################################\n");
        // Serial.println("BUFF_RED_IMAGE content: ");
        // for (int i=0; i<BUFF_LEN_2_13B; i++) {
        //     Serial.printf("%c", BUFF_RED_IMAGE[i]);
        // }

        rq_sent = 0;
        if (uncomp_success) {
            Serial.print("\nwritten_bytes BLACK: ");
            Serial.println(written_bytes[0]);
            Serial.print("written_bytes RED: ");
            Serial.println(written_bytes[1]);
            // Print
            // Start e-paper
            Epd epd;
            Serial.println("\nInit eink ...");
            if (epd.Init() != 0)
            {
                Serial.print("e-Paper init failed");
                return;
            }
            Serial.println("Inited eink ...");
            epd.ClearFrame();    // clears the SRAM of the e-paper display
            epd.DisplayFrame(BUFF_BLACK_IMAGE, BUFF_RED_IMAGE);
            Serial.println("E paper done!");
            epd.Sleep();    // Deep sleep
            delay(500000);
            Serial.println("Restarting ...!");
            ESP.restart();
        }
        // delay(10000);
    }


/* 
    // try to parse packet
    int packetSize = LoRa.parsePacket();
    if (packetSize)
    {
        // received a packet
        Serial.print("Received packet '");

        // read packet
        while (LoRa.available())
        {
            String LoRaData = LoRa.readString();
            Serial.print(LoRaData);
        }

        // print RSSI of packet
        Serial.print("' with RSSI ");
        Serial.println(LoRa.packetRssi());

        // Start e-paper
        Epd epd;
        Serial.println("Init eink ...");
        if (epd.Init() != 0)
        {
            Serial.print("e-Paper init failed");
            return;
        }
        Serial.println("Inited eink ...");
        epd.ClearFrame();    // clears the SRAM of the e-paper display
        // epd.DisplayFrame(IMAGE_BLACK, IMAGE_RED);
        Serial.println("E paper done!");
        epd.Sleep();    // Deep sleep
        Serial.println("E paper slept!");
        // Update status
        int oldStatus = EEPROM.read(0);
        EEPROM.write(0, oldStatus+1);
        EEPROM.commit();

        resetCount++;
        ESP.restart();
    } else {
        if (counter % 5000 == 0)
        {
            int oldStatus = EEPROM.read(0);
            Serial.print("oldStatus:  ");
            Serial.print(oldStatus);
            Serial.print(" No packet: ");
            Serial.println(counter / 1000);
        }
    }
     */
}