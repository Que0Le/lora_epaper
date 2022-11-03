# This python file defines specific instructions to produce picture data for different boards
# and display.

SUPPORTED_DISPLAYS = ["2in13b_V3", "7in5_V2"]

def get_corrected_and_rotated_bmp_for_waveshare_epp(
    bmp_raw_data,
    total_bytes_per_line: int = 28, used_bytes_per_line: int = 27,
    used_bits_in_last_byte: int = 4
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
            for eight_row_th in range(0, 13): #8-rows
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


# def