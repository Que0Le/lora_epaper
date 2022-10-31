import io

from PIL import Image, ImageDraw, ImageFont

from typing import List
# import datetime as dt
import textwrap

""" Return byte buffer if output=='__buffer', else export file 'output' @"""


def create_bitmap_from_word(word: str = "this is a test", output="temp.bmp"):
    out = Image.new("1", (212, 104), 255)
    fmb60 = ImageFont.truetype("FreeMonoBold.ttf", 20)
    d = ImageDraw.Draw(out)

    d.text((5, 5), word, font=fmb60, fill=0)
    d.point((0, 103), fill=0)
    d.point((0, 102))
    out.save("output.bmp")
    # out.show()

# create_bitmap_from_word()


def buff_to_pic_debug(bmp_data: List):
    out = Image.new("1", (212, 104), 255)
    d = ImageDraw.Draw(out)
    mask_bits = [0b10000000, 0b01000000, 0b00100000, 0b00010000,
                 0b00001000, 0b00000100, 0b00000010, 0b00000001]
    current_bit_th = 8
    current_byte = 0
    for column in range(0, 212):
        for row in range(103, -1, -1):
            if current_bit_th == 8:
                current_byte = bmp_data.pop(0)
                current_bit_th = 0
            d.point((column, row), fill=(
                (current_byte & mask_bits[current_bit_th]) >> (7-current_bit_th)))
            current_bit_th += 1
    out.save("pic_test.bmp")


def get_corrected_and_rotated_bmp(bmp_raw_data, R: int = 212, C: int = 13):
    """"""
    mask_bits = [0b10000000, 0b01000000, 0b00100000, 0b00010000,
                 0b00001000, 0b00000100, 0b00000010, 0b00000001]
    corrected_rotated_bmp = []
    for column_th in range(0, 27):
        bits_to_take = 8 if column_th != 26 else 4
        for bit_th in range(0, bits_to_take):   # bit_th of the output
            for eight_row_th in range(0, 13):  # 8-rows
                bits = []
                output_byte = 0
                for row_th_of_8 in range(0, 8):  # row_th in vertical
                    bits.append((mask_bits[bit_th] & bmp_raw_data[(eight_row_th*8+row_th_of_8)*28+column_th]) << bit_th)
                    if column_th <2 and bit_th == 0:
                        print((eight_row_th*8+row_th_of_8)*28+column_th)
                # Store result of current byte
                for i in range(0, 8):
                    output_byte = output_byte | (bits[i] >> i)
                # bits = []
                corrected_rotated_bmp.append(output_byte)
    return corrected_rotated_bmp


def get_bmp_raw_data_from_file(filepath: str, include_header: bool = False):
    with open(filepath, mode='rb') as file:  # b is important -> binary
        fileContent = file.read()
    if include_header:
        return fileContent
    return fileContent[62:]


buff_to_pic_debug(get_corrected_and_rotated_bmp(
    get_bmp_raw_data_from_file("output.bmp")))
# buff_to_pic_debug([0]*1 + [255]*12 + [0]*5 + [255]*8 + [0]*13  + [255]*2717)

# buff_to_pic_debug(get_corrected_and_rotated_bmp([0]*20 +  [1]*20 + [0]*10 + [255]*2706))
