#include <SPI.h>
#include <LoRa.h>
#include <SPI.h>

#include "helpers.h"
// define the pins used by the transceiver module
#define ss 5
#define rst 14
#define dio0 2

/*  */
#include <epd2in13b_V3.h>
#include <miniz.h>


#include <EEPROM.h>
#define EEPROM_SIZE 1

#define BUFF_LEN_2IN13 2756     //  212*104/8
#define HOST_CODE 123           // Host ID
#define THIS_CLIENT_ID 1
#define THIS_CLIENT_CHUNKS_NBR 2    // in how many chunks is the data split 

unsigned char **BUFF_IMAGES; 
unsigned char **BUFF_COMPRESSED_IMAGES; 

int counter = 0;
int resetCount = 0;

void setup_eeprom()
{
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(0, 0);
    EEPROM.commit();
}

struct packet_struct {
    unsigned short int host_code;
    unsigned char flag;
    unsigned short int client_id;    // 0->black image, 1->red image
    unsigned short int session_id;  // total length of file to transmitt
    unsigned char chunk_id;   // where does it start (0->total_length-1)
    unsigned short int shard_id;  // length of this fragment
    unsigned char shard_length;
    char data[100];
};

struct control_struct {
    unsigned short int host_code;
    unsigned char flag;
    unsigned short int client_id;
    unsigned short int session_id;
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

    Serial.printf("Sizeof(control_struct)=%d Sizeof(packet_struct)\n", sizeof(control_struct), sizeof(packet_struct));

    // Init mem
    pkt = (packet_struct *)malloc(sizeof(packet_struct));
    ctrl = (control_struct *)malloc(sizeof(control_struct));
    // allocate 2-d array mem: https://www.geeksforgeeks.org/dynamically-allocate-2d-array-c/
    BUFF_COMPRESSED_IMAGES = (unsigned char**)malloc(2 * sizeof(unsigned char*));
    BUFF_IMAGES = (unsigned char**)malloc(2 * sizeof(unsigned char*));
    for (int i = 0; i < 2; i++) {
        BUFF_IMAGES[i] = (unsigned char *)malloc(BUFF_LEN_2IN13 * sizeof(unsigned char));
        if (!BUFF_IMAGES[i]) {
            Serial.printf("Failed alloc mem for BUFF_IMAGES, i=%d\n", i);
        }
    }
    // if ((BUFF_BLACK_IMAGE = (unsigned char *)malloc(BUFF_LEN_7IN5)) == NULL) {
    //     printf("Failed alloc BUFF_BLACK_IMAGE...\r\n");
    //     while (1);
    // }
    for (int i=0;i<2; i++) {
        for (int j=0;j<BUFF_LEN_2IN13; j++) {
            BUFF_IMAGES[i][j] = 0;//0b10101010;
            // BUFF_IMAGES[0][i] = rand() % (127 + 1 - 0) + 0;
        }
    }
    // Serial.printf("%d %d %d %d %d\n", BUFF_IMAGES[0][0], BUFF_IMAGES[0][100], BUFF_IMAGES[0][1000], BUFF_IMAGES[0][67], BUFF_IMAGES[0][9999]);
    // Serial.printf("Allocated mem for BUFF_IMAGES, length=%d\n", BUFF_LEN_7IN5);
    // Epd epd;
    // Serial.println("\nInit eink ...");
    // if (epd.Init() != 0)
    // {
    //     Serial.print("e-Paper init failed");
    //     return;
    // }
    // Serial.println("Inited eink ...");
    // epd.Clear(); // clears the SRAM of the e-paper display
    // epd.DisplayFrame(BUFF_IMAGES[0]);
    // Serial.println("E paper done!");
    // epd.Sleep(); // Deep sleep
    // delay(5000);
    // Serial.println("Restarting ...!");
    // ESP.restart();

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
unsigned char shard_max_length = 0;
unsigned char nbr_chunks = 0;
unsigned int nbr_shards[2] = {0, 0};
unsigned int chunk_sizes_bytes[2] = {0, 0};
unsigned int session_id = 0;
int current_chunk = -1;
int current_shard = -1;
int shard_rqst_sent = 0;

void loop()
{
    if (img_rqst_sent==-1) {
        // Send request
        LoRa.beginPacket();
        LoRa.print("{\"msg_type\":\"img_rqst\",\"hid\":123,\"cid\":1,\"sid\":0,\"dt\":\"2in13b_V3\",\"b\":\"esp32\"}}");
        LoRa.endPacket();
        Serial.println("Requested new image.");
        img_rqst_sent = 1;
    }

    if (shard_rqst_sent == 0 && img_rqst_sent == 1 && flag == 1 && session_id > 0) {
        // if (current_chunk >= THIS_CLIENT_CHUNKS_NBR && current_shard >= nbr_shards[current_chunk]) {
        //     // Done. TODO: send done message
        //     // TODO: clean up
        // }
        // We Send request for the next chunk
        LoRa.beginPacket();
        LoRa.printf("{\"msg_type\":\"shard_rqst\",\"hid\":%d,\"cid\":%d,\"sid\":%d,\"chid\":%d,\"shid\":%d}}",
                    123, THIS_CLIENT_ID, session_id, current_chunk, current_shard);
        LoRa.endPacket();
        Serial.printf("Requested chunk=%d shard=%d\n", current_chunk, current_shard); 
        shard_rqst_sent = 1;
    }

    // Handle message
    int packetSize = LoRa.parsePacket();
    if (packetSize)
    {
        Serial.printf("Waiting for packet ...\n");
        int nbr_byte = 0;
        while (LoRa.available())
        {
            nbr_byte = LoRa.readBytes(buffer, 256);
        }
        Serial.printf("... captured %d bytes ... \n", nbr_byte);
        if (nbr_byte == sizeof(control_struct)) {
            memcpy(ctrl, buffer, sizeof(control_struct));
            Serial.printf("...  ctrl, request_status=%d client_id=%d shard_max_length=%d \n",
                          ctrl->request_status, ctrl->client_id, ctrl->shard_max_length);
            // Check if this msg is for us
            if (ctrl->client_id != THIS_CLIENT_ID || ctrl->host_code != HOST_CODE || ctrl->flag != 1) {
                return;
            }
            if (ctrl->request_status == 1) {
                // Server accepts.
                Serial.printf("Server accepts\n");
                // Extract approvement data
                unsigned int chunk_sizes_byte = 0;
                memcpy(&chunk_sizes_byte, ctrl->message, 4);                
                chunk_sizes_bytes[0] = chunk_sizes_byte;
                memcpy(&chunk_sizes_byte, ctrl->message + 4, 4);
                chunk_sizes_bytes[1] = chunk_sizes_byte;
                Serial.printf("chunk_sizes_byte=%d %d\n", chunk_sizes_bytes[0], chunk_sizes_bytes[1]);
                // Get number of shards
                for (int i=0; i<THIS_CLIENT_CHUNKS_NBR; i++) {
                    if (chunk_sizes_bytes[i] % ctrl->shard_max_length > 0) {
                        nbr_shards[i] = chunk_sizes_bytes[i] / ctrl->shard_max_length + 1;
                    } else {
                        nbr_shards[i] = chunk_sizes_bytes[i] / ctrl->shard_max_length;
                    }
                }
                Serial.printf("nbr_shards[0]=%d nbr_shards[1]=%d\n", nbr_shards[0], nbr_shards[1]);
                request_status = ctrl->request_status;
                flag = ctrl->flag;
                session_id = ctrl->session_id;
                // Allocate memory for the compressed data
                for (int i = 0; i < THIS_CLIENT_CHUNKS_NBR; i++) {
                    BUFF_COMPRESSED_IMAGES[i] = (unsigned char *)malloc(chunk_sizes_bytes[i] * sizeof(unsigned char));
                    if (!BUFF_COMPRESSED_IMAGES[i]) {
                        Serial.printf("Failed alloc %d bytes for BUFF_COMPRESSED_IMAGES, i=%d\n", chunk_sizes_bytes[i], i);
                    }
                    Serial.printf("Allocated %d bytes for BUFF_COMPRESSED_IMAGES, i=%d\n", chunk_sizes_bytes[i], i);
                }
                current_chunk = current_shard = 0;
                delay(1000);
                return;
            } else if (ctrl->request_status == 2) {
                Serial.println("Server denied the image request.!");
                delay(1000);
                // TODO: cleanup
                reset_vars(&img_rqst_sent, &flag, &request_status, &written_bytes, &shard_max_length,
                           &nbr_chunks, &session_id, &current_chunk, &current_shard);
                // reset_buffs();
                return;
            } else if (ctrl->request_status == 3) {
                Serial.println("Server aborted the image request.!");
                reset_vars(&img_rqst_sent, &flag, &request_status, &written_bytes, &shard_max_length,
                           &nbr_chunks, &session_id, &current_chunk, &current_shard);
                // reset_buffs();
                return;
            }
        //} else if (nbr_byte==sizeof(packet_struct)) {     // somehow size of pkt is not catched here
        } else {     // somehow size of pkt is not catched here
            memcpy(pkt, buffer, sizeof(packet_struct));
            Serial.printf("... received data: chunk_id: %d, shard_id: %d\n", pkt->chunk_id, pkt->shard_id);
            // Make sure we don't exceed the buffer
            if (pkt->chunk_id >= THIS_CLIENT_CHUNKS_NBR || pkt->shard_id>=nbr_shards[pkt->chunk_id])
                return;
            Serial.printf("... nbr_shards[pkt->chunk_id]: %d\n", nbr_shards[pkt->chunk_id]);
            // Calculate the correct bytes to copy. Can't always trust server!
            size_t bytes_to_copy = (pkt->shard_id*ctrl->shard_max_length + pkt->shard_length) > chunk_sizes_bytes[current_chunk]
                ? chunk_sizes_bytes[current_chunk] - pkt->shard_id*ctrl->shard_max_length
                : pkt->shard_length;
            Serial.printf("... bytes_to_copy: %d location chunk=%d shard=%d\n", bytes_to_copy, pkt->chunk_id, pkt->shard_id);
            memcpy(&BUFF_COMPRESSED_IMAGES[pkt->chunk_id][pkt->shard_id*ctrl->shard_max_length], pkt->data, bytes_to_copy);
            written_bytes += bytes_to_copy;
            if (current_shard == nbr_shards[current_chunk] - 1) {
                if (current_chunk < THIS_CLIENT_CHUNKS_NBR -1) {
                    current_chunk++;
                    current_shard = 0;
                } else {
                    // TODO: send done signal
                    LoRa.beginPacket();
                    LoRa.printf("{\"msg_type\":\"rqst_done\",\"hid\":%d,\"cid\":%d,\"sid\":%d}}",
                                123, THIS_CLIENT_ID, session_id);
                    LoRa.endPacket();
                    Serial.printf("Sent Done message to server\n"); 
                    // TODO: clean up
                    Serial.printf("All chunks and shards have been requested and received! written_bytes: %d\n", written_bytes);
                    // TODO: uncompress data
                    // TODO: show image
                    Serial.printf("Receiving done. Start decompressing data ...\n");
                    // Serial.printf("\nwritten_bytes COMPRESSSED BLACK: %d of %d\n", written_bytes, buff_comp_len);
                    int uncomp_success = 1;
                    uLong uncomp_len = BUFF_LEN_2IN13;
                    uint total_succeeded = 0;
                    uint step = 0;
                    int cmp_status = 0;
                    for (int chunk_th=0; chunk_th<THIS_CLIENT_CHUNKS_NBR; chunk_th++) {
                        cmp_status = uncompress(BUFF_IMAGES[chunk_th], &uncomp_len,
                            (const unsigned char *)BUFF_COMPRESSED_IMAGES[chunk_th], (uLong)chunk_sizes_bytes[chunk_th]);

                        total_succeeded += (cmp_status == Z_OK);
                        if (cmp_status != Z_OK) {
                            Serial.printf("uncompress failed!\n");
                            uncomp_success = 0;
                        }
                        else {
                            Serial.printf("Decompressed from %u to %u bytes\n",
                                          (mz_uint32)chunk_sizes_bytes[chunk_th], (mz_uint32)uncomp_len);
                        }
                    }

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
                        epd.ClearFrame(); // clears the SRAM of the e-paper display
                        epd.DisplayFrame(BUFF_IMAGES[0], BUFF_IMAGES[1]);
                        Serial.println("E paper done!");
                        epd.Sleep(); // Deep sleep
                        delay(5000);
                        Serial.println("Restarting ...!");
                        ESP.restart();
                    }
                    /// 
                    reset_vars(&img_rqst_sent, &flag, &request_status, &written_bytes, &shard_max_length,
                               &nbr_chunks, &session_id, &current_chunk, &current_shard);
                }
            } else {
                current_shard++;
            }
            shard_rqst_sent = 0;
            delay(1000);
        }
    }
}