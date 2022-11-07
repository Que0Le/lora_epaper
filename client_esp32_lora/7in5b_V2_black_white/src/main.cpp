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


#include <EEPROM.h>
#define EEPROM_SIZE 1

#define BUFF_LEN_7IN5 48000     //  800*480/8
#define HOST_CODE 123
#define THIS_CLIENT_ID 2

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
    unsigned int host_code;
    unsigned char flag;
    unsigned int client_id;    // 0->black image, 1->red image
    unsigned int session_id;  // total length of file to transmitt
    unsigned char chunk_id;   // where does it start (0->total_length-1)
    unsigned int shard_id;  // length of this fragment
    unsigned char shard_length;
    char data[100];
};

struct control_struct {
    unsigned int host_code;
    unsigned char flag;
    unsigned int client_id;
    unsigned int session_id;
    unsigned char is_encrypted;
    unsigned char is_compressed;
    unsigned char request_status;  // 1: accepted and transmitting, 2: deny, 3: abort, 4: done
    unsigned char shard_max_length;
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

int img_rqst_sent = -1;
int flag = -1;
int request_status = -1;
char buffer[256];
unsigned int written_bytes = 0;
// unsigned int buff_comp_len = 0;
unsigned char total_chunks = 0;
unsigned int total_shards[1] = {0};
unsigned int session_id = 0;
int current_chunk = -1;
int current_shard = -1;

void loop()
{
    if (img_rqst_sent==0) {
        // Send request
        LoRa.beginPacket();
        LoRa.print("{\"msg_type\":\"img_rqst\",\"hid\":\"123\",\"cid\":1,\"sid\":0,\"dsp_type\":\"7in5_V2\",\"board\":\"esp32\"}");
        // LoRa.print(counter);
        LoRa.endPacket();
        Serial.println("Requested new image.");
        img_rqst_sent = 1;
    }

    if (img_rqst_sent == 1 && flag == 1 && session_id > 0) {
        // Send request for the next chunk
        LoRa.beginPacket();
        LoRa.print("{\"msg_type\":\"shard_rqst\",\"hid\":\"123\",\"cid\":1,\"sid\":0,\"dsp_type\":\"7in5_V2\",\"board\":\"esp32\"}");
        // LoRa.print(counter);
        LoRa.endPacket();
        Serial.println("Requested new image."); 
    }

    // Handle message
    int packetSize = LoRa.parsePacket();
    if (packetSize)
    {
        int nbr_byte = 0;
        while (LoRa.available())
        {
            nbr_byte = LoRa.readBytes(buffer, 256);
        }
        if (nbr_byte == sizeof(control_struct)) {
            memcpy(ctrl, buffer, sizeof(control_struct));
            // Check if this msg is for us
            if (ctrl->client_id != THIS_CLIENT_ID || ctrl->host_code != HOST_CODE || ctrl->flag != 1) {
                return;
            }
            if (ctrl->request_status == 1) {
                // Server accepts.

                // Extract approvement data
                unsigned int chunk_sizes_bytes = 0;
                memcpy(&chunk_sizes_bytes, ctrl->message, 4);
                total_chunks = 1;
                // Get number of shards
                if (chunk_sizes_bytes % ctrl->shard_max_length > 0) {
                    total_shards[0] = chunk_sizes_bytes % ctrl->shard_max_length + 1;
                } else {
                    total_shards[0] = chunk_sizes_bytes % ctrl->shard_max_length;
                }
                request_status = ctrl->request_status;
                flag = ctrl->flag;
                session_id = ctrl->session_id;
                return;
            } else if (ctrl->request_status == 2) {
                Serial.println("Server denied the image request.!");
                delay(1000);
                return;
            } else if (ctrl->request_status == 3) {
                Serial.println("Server aborted the image request.!");
                img_rqst_sent = 1;
                request_status = -1;
                written_bytes = 0;
                current_chunk =-1;
                current_shard =-1;
                total_shards[0] = 0;
                session_id = 0;
                flag = -1;
                free(BUFF_COMPRESSED_BLACK_IMAGE);
                return;
            }
        } else /* if (nbr_byte==sizeof(packet_struct)) */ {     // somehow size of pkt is not catched here


        }
    }



    // if (packetSize)
    // {
    //     // read packet
    //     while (LoRa.available())
    //     {
    //         int nbr_byte = LoRa.readBytes(buffer, 256);
    //         // Serial.print("nbr_byte: ");
    //         if (nbr_byte==sizeof(control_struct)) { // control packet
    //             memcpy(ctrl, buffer, sizeof(control_struct));
    //             unsigned int temp = 0;
    //             memcpy(&temp, ctrl->message, 4);
    //             buff_comp_len += temp; // TODO: somehow assign with "=" didn't work!
    //             request_status = ctrl->request_status;
    //             Serial.printf("Got control packet ... request_status: %d\n", request_status);
    //             if (request_status == 1) {
    //                 if ((BUFF_COMPRESSED_BLACK_IMAGE = (unsigned char *)malloc(buff_comp_len)) == NULL) {
    //                     printf("Failed alloc BUFF_COMPRESSED_BLACK_IMAGE...\r\n");
    //                     while (1);
    //                 }
    //                 Serial.printf("Allocated mem for COMPRESSED back (%d) buff\n", buff_comp_len);
    //             }
    //         } else /* if (nbr_byte==sizeof(packet_struct)) */ {
    //             memcpy(pkt, buffer, sizeof(packet_struct));
    //             Serial.printf("... received data: index_start: %d, data_length: %d\n", pkt->index_start, pkt->data_length);
    //             // Make sure we don't exceed the buffer
    //             if (pkt->data_type>1 || pkt->index_start>=buff_comp_len)
    //                 break;
    //             // Calculate the correct bytes to copy. Can't always trust server!
    //             size_t bytes_to_copy = (pkt->index_start + pkt->data_length) >= buff_comp_len
    //                 ? buff_comp_len - pkt->index_start
    //                 : pkt->data_length;
    //             // Copy to compressed buffer
    //             memcpy(BUFF_COMPRESSED_BLACK_IMAGE + pkt->index_start, pkt->data, bytes_to_copy);
    //             written_bytes += bytes_to_copy;
    //         }
    //     }
    // }

    // // Done
    // if (rq_sent==1 && request_status==3) {
    //     int uncomp_success = 1;
    //     Serial.printf("Receiving done. Start decompressing data ...\n");
    //     Serial.printf("\nwritten_bytes COMPRESSSED BLACK: %d of %d\n", written_bytes, buff_comp_len);

    //     uLong uncomp_len = BUFF_LEN_7IN5;
    //     uint total_succeeded = 0;
    //     uint step = 0;
    //     int cmp_status = 0;
    //     cmp_status = uncompress(
    //         BUFF_BLACK_IMAGE, &uncomp_len, 
    //         (const unsigned char *)BUFF_COMPRESSED_BLACK_IMAGE, (uLong)buff_comp_len
    //     );

    //     total_succeeded += (cmp_status == Z_OK);
    //     if (cmp_status != Z_OK) {
    //         Serial.printf("uncompress failed!\n");
    //         uncomp_success = 0;
    //     } else {
    //         Serial.printf("Decompressed from %u to %u bytes\n", (mz_uint32)buff_comp_len, (mz_uint32)uncomp_len);
    //     }

    //     rq_sent = 0;
    //     if (uncomp_success) {
    //         Serial.print("\nwritten_bytes BLACK: ");
    //         Serial.println(written_bytes);
    //         // Print
    //         // Start e-paper
    //         Epd epd;
    //         Serial.println("\nInit eink ...");
    //         if (epd.Init() != 0)
    //         {
    //             Serial.print("e-Paper init failed");
    //             return;
    //         }
    //         Serial.println("Inited eink ...");
    //         epd.Clear();    // clears the SRAM of the e-paper display
    //         epd.DisplayFrame(BUFF_BLACK_IMAGE);
    //         Serial.println("E paper done!");
    //         epd.Sleep();    // Deep sleep
    //         delay(5000);
    //         Serial.println("Restarting ...!");
    //         ESP.restart();
    //     }
    // }
}