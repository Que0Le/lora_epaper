# This python file defines specific instructions to produce picture data for different boards
# and displays.
import zlib, random

from utils import *
from config import config as cfg
from typing import Tuple

SUPPORTED_DISPLAYS = ["2in13b_V3", "7in5_V2"]

def get_corrected_and_rotated_bmp_for_waveshare_epp_2in13b_v2(
    bmp_raw_data,
    total_bytes_per_line: int = 28, used_bytes_per_line: int = 27,
    used_bits_in_last_byte: int = 4, nbr_8th_row: int = 13
):
    """ 
    Convert standard bitmap data (without header) to byte array in waveshare's epaper format.
    The default parameters are for 2.13b epp, 212*104
    @param: used_bytes_per_line rounded up value, ie 26.5 bytes -> 27
    Normal Bitmap 1-bit photos use 1 bit to represent 1 pixel, start from the 
    lower left to right for the first line, and start over from left to right till the end.
    -----------------------
    | ----------------->  |
    | ----------------->  |
    | ----------------->  |
    | HERE START ------>  |
    -----------------------
      !1 byte !
    Epaper from waveshare uses different order:
    - 1 bit, from Lower left to upper left.
    - Next column again from lower left to upper left.
    -----------------------
    | /|\ /|\             | -----
    |  |   |              | byte
    |  |   |              | 1
    | HERE |  .......     | -----
    -----------------------
    """
    mask_bits = [0b10000000, 0b01000000, 0b00100000, 0b00010000, 0b00001000, 0b00000100, 0b00000010, 0b00000001]
    corrected_rotated_bmp = []
    for column_th in range(0, used_bytes_per_line):
        bits_to_take = 8 if column_th!=(used_bytes_per_line-1) else used_bits_in_last_byte
        for bit_th in range(0, bits_to_take):   # bit_th of the output
            for eight_row_th in range(0, nbr_8th_row): #8-rows
                bits = []
                output_byte = 0
                for row_th_of_8 in range(0, 8):  #row_th in vertical
                    bits.append(
                        (mask_bits[bit_th] & bmp_raw_data[(eight_row_th*8+row_th_of_8)*total_bytes_per_line+column_th]) << bit_th
                    )
                # Store result of current byte
                for i in range(0, 8):
                    output_byte = output_byte | (bits[i] >> i)
                corrected_rotated_bmp.append(output_byte)
    return corrected_rotated_bmp


def flip_and_rotate_bmp_raw_7in5_v2(bmp_raw_data, R: int = 480, C: int = 100):
    """
    R number of rows, C number of colume devided by 8. 
    Default value for R and C of 7in5_v2 epp.

    # for (int r=0; r<int(R/2); r++) {
    #     for (int col=0; col<C; col++) {
    #         unsigned char temp = buff_48k[r*C+col + BMP_HEADER_LEN];
    #         buff_48k[r*C+col + BMP_HEADER_LEN] = buff_48k[(R-r-1)*C+col + BMP_HEADER_LEN];
    #         buff_48k[(R-r-1)*C+col + BMP_HEADER_LEN] = temp;
    #     }
    # }        
    """
    bmp_corrected = list(bmp_raw_data)
    for r in range(0, int(R/2)):
        for col in range(0, C):
            temp_byte = bmp_corrected[r*C+col]
            bmp_corrected[r*C+col] = bmp_corrected[(R-r-1)*C+col]
            bmp_corrected[(R-r-1)*C+col] = temp_byte

    return bmp_corrected


def get_data_for_request(board_type: str, display_type: str) -> Tuple[list, int, int, int]:
    """ 
    Read picture data, convert in appropriate format for `display_type` and `board_type`
    @retun Tupple(chunks[], size of chunks[], total bytes, compressed bytes)
    """
    chunks = []
    total_length = 0
    compressed_length = 0
    if display_type=="2in13b_V3":
        # Prepare data
        bmp_black_corrected: list = get_corrected_and_rotated_bmp_for_waveshare_epp_2in13b_v2(
            get_bmp_raw_data_from_file("images/den.bmp"))
        bmp_red_corrected: list = get_corrected_and_rotated_bmp_for_waveshare_epp_2in13b_v2(
            get_bmp_raw_data_from_file("images/do.bmp"))
        total_length = len(bmp_black_corrected) + len(bmp_red_corrected)
        pic_data = [
            zlib.compress(bytes(bmp_black_corrected)), zlib.compress(bytes(bmp_red_corrected))
        ]
        compressed_length = len(pic_data[0]) + len(pic_data[1])
        # Chunk 0: black image, Chunk 1: red
        for chunk_th in range(0, 2):   
            shards = [
                pic_data[chunk_th][i:i+cfg.MAX_CHAR_LORA_PAYLOAD] for i in range(0, len(pic_data[chunk_th]), cfg.MAX_CHAR_LORA_PAYLOAD)
            ]
            chunks.append(shards)

    elif display_type=="7in5_V2":
        # Prepare data
        # filepath_to_send = random.choice(list_file_with_filter("images", "800x480_1bit", "bmp"))
        filepath_to_send = "./images/800x480_1bit_apache.bmp"
        bmp_raw_data = get_bmp_raw_data_from_file(filepath=filepath_to_send)
        bmp_black_corrected: list = flip_and_rotate_bmp_raw_7in5_v2(bmp_raw_data)
        total_length = len(bmp_black_corrected)
        pic_data = [zlib.compress(bytes(bmp_black_corrected))]
        compressed_length = len(pic_data[0])
        # print(f"filepath_to_send: {filepath_to_send}, compressed to: {len(pic_data[0])} bytes" )

        for chunk_th in range(0, 1):
            shards = [
                pic_data[chunk_th][i:i+cfg.MAX_CHAR_LORA_PAYLOAD] for i in range(0, len(pic_data[chunk_th]), cfg.MAX_CHAR_LORA_PAYLOAD)
            ]
            chunks.append(shards)

    chunk_sizes = []
    for chunk in chunks:
        s = 0
        for shard in chunk:
            s = s + len(shard)
        chunk_sizes.append(s)
    return chunks, chunk_sizes, total_length, compressed_length
