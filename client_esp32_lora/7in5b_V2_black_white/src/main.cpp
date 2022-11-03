#include <SPI.h>
#include <LoRa.h>
#include <SPI.h>

// define the pins used by the transceiver module
#define ss 5
#define rst 14
#define dio0 2

/*  */
#include <epd7in5_V2.h>
#include <miniz.h>

#define COLORED 0
#define UNCOLORED 1


#include <EEPROM.h>
#define EEPROM_SIZE 1

#define BUFF_LEN_7IN5 48000 //212*104/8
unsigned char *BUFF_BLACK_IMAGE; 
unsigned char *BUFF_COMPRESSED_BLACK_IMAGE; 

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

    log_d("Total heap: %d", ESP.getHeapSize());
    log_d("Free heap: %d", ESP.getFreeHeap());
    log_d("Total PSRAM: %d", ESP.getPsramSize());
    log_d("Free PSRAM: %d", ESP.getFreePsram());

    // Init mem
    pkt = (packet_struct *)malloc(sizeof(packet_struct));
    ctrl = (control_struct *)malloc(sizeof(control_struct));

    if ((BUFF_BLACK_IMAGE = (unsigned char *)malloc(BUFF_LEN_7IN5)) == NULL) {
        printf("Failed alloc BUFF_BLACK_IMAGE...\r\n");
        while (1);
    }
    for (int i=0;i<BUFF_LEN_7IN5; i++) {
        BUFF_BLACK_IMAGE[i] = 0;
    }
    Serial.println("Allocated mem for back buff");

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
unsigned int written_bytes = 0;
unsigned int buff_comp_len = 0;

void loop()
{
    if (rq_sent==0 && request_status==-1) {
        // Send request
        Serial.println("sending request");
        LoRa.beginPacket();
        LoRa.print("{\"msg_type\":\"rqst_img\",\"dsp_type\":\"7in5_V2\",\"board\":\"esp32\",\"uniq_id\":\"nbr000\"}");
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
                buff_comp_len += temp; // TODO: somehow assign with "=" didn't work!
                request_status = ctrl->request_status;
                Serial.printf("Got control packet ... request_status: %d\n", request_status);
                if (request_status == 1) {
                    if ((BUFF_COMPRESSED_BLACK_IMAGE = (unsigned char *)malloc(buff_comp_len)) == NULL) {
                        printf("Failed alloc BUFF_COMPRESSED_BLACK_IMAGE...\r\n");
                        while (1);
                    }
                    Serial.printf("Allocated mem for COMPRESSED back (%d) buff\n", buff_comp_len);
                }
            } else /* if (nbr_byte==sizeof(packet_struct)) */ {
                memcpy(pkt, buffer, sizeof(packet_struct));
                Serial.printf("... received data: index_start: %d, data_length: %d\n", pkt->index_start, pkt->data_length);
                // Make sure we don't exceed the buffer
                if (pkt->data_type>1 || pkt->index_start>=buff_comp_len)
                    break;
                // Calculate the correct bytes to copy. Can't always trust server!
                size_t bytes_to_copy = (pkt->index_start + pkt->data_length) >= buff_comp_len
                    ? buff_comp_len - pkt->index_start
                    : pkt->data_length;
                // Copy to compressed buffer
                memcpy(BUFF_COMPRESSED_BLACK_IMAGE + pkt->index_start, pkt->data, bytes_to_copy);
                written_bytes += bytes_to_copy;
            }
        }
    }

    // Done
    if (rq_sent==1 && request_status==3) {
        int uncomp_success = 1;
        Serial.printf("Receiving done. Start decompressing data ...\n");
        Serial.printf("\nwritten_bytes COMPRESSSED BLACK: %d of %d\n", written_bytes, buff_comp_len);

        uLong uncomp_len = BUFF_LEN_7IN5;
        uint total_succeeded = 0;
        uint step = 0;
        int cmp_status = 0;
        cmp_status = uncompress(
            BUFF_BLACK_IMAGE, &uncomp_len, 
            (const unsigned char *)BUFF_COMPRESSED_BLACK_IMAGE, (uLong)buff_comp_len
        );

        total_succeeded += (cmp_status == Z_OK);
        if (cmp_status != Z_OK) {
            Serial.printf("uncompress failed!\n");
            uncomp_success = 0;
        } else {
            Serial.printf("Decompressed from %u to %u bytes\n", (mz_uint32)buff_comp_len, (mz_uint32)uncomp_len);
        }

        rq_sent = 0;
        if (uncomp_success) {
            Serial.print("\nwritten_bytes BLACK: ");
            Serial.println(written_bytes);
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
            epd.Clear();    // clears the SRAM of the e-paper display
            epd.DisplayFrame(BUFF_BLACK_IMAGE);
            Serial.println("E paper done!");
            epd.Sleep();    // Deep sleep
            delay(500000);
            Serial.println("Restarting ...!");
            ESP.restart();
        }
    }
}