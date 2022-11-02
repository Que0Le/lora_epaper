import struct
from typing import List

from LoRaRF import SX127x



def init_lora(
    busId: int = 0, csId: int = 0, 
    resetPin: int = 22, irqPin: int = -1, txenPin: int = -1, rxenPin: int = -1,
    freq_mhz: int = 866,
    txPower: int = 17,
    spreadingFactor: int = 7, bandwidth_khz: int = 125, codeRate: int = 5,
    syncWord: int = 0xF3

):
    busId = 0; csId = 0
    resetPin = 22; irqPin = -1; txenPin = -1; rxenPin = -1
    LoRa = SX127x()
    print("Begin LoRa radio")
    if not LoRa.begin(busId, csId, resetPin, irqPin, txenPin, rxenPin) :
        raise Exception("Something wrong, can't begin LoRa radio")
    
    if freq_mhz not in (433, 866, 915):
        raise Exception(f"freq_mhz={freq_mhz} not supported (433, 866, 915 MHz)!")
    print(f"Set frequency to {freq_mhz} Mhz")
    LoRa.setFrequency(freq_mhz*1000000)

    # Set TX power, this function will set PA config with optimal setting for requested TX power
    print(f"Set TX power to +{txPower} dBm")
    LoRa.setTxPower(txPower, LoRa.TX_POWER_PA_BOOST) # TX power +xx dBm using PA boost pin

    # Configure modulation parameter including spreading factor (SF), bandwidth (BW), and coding rate (CR)
    # Receiver must have same SF and BW setting with transmitter to be able to receive LoRa packet
    print(f"Set modulation parameters:\n\tSpreading factor = {spreadingFactor}\n\tBandwidth = {bandwidth_khz} kHz\n\tCoding rate = {codeRate}")
    LoRa.setSpreadingFactor(spreadingFactor)
    LoRa.setBandwidth(bandwidth_khz*1000)
    LoRa.setCodeRate(codeRate)

    # Configure packet parameter including header type, preamble length, payload length, and CRC type
    # The explicit packet includes header contain CR, number of byte, and CRC type
    # Receiver can receive packet with different CR and packet parameters in explicit header mode
    # LoRa.setHeaderType(LoRa.HEADER_EXPLICIT)                        # Explicit header mode
    # LoRa.setPreambleLength(12)                                      # Set preamble length to 12
    # LoRa.setPayloadLength(15)                                       # Initialize payloadLength to 15
    # LoRa.setCrcEnable(True)                                         # Set CRC enable

    # Set syncronize word for public network (0x34)
    print(f"Set syncronize word to {hex(syncWord)}")
    LoRa.setSyncWord(syncWord)

    return LoRa


def send_a_struct(
    LoRa: SX127x, struct_data: List, format_characters: str, endpacket_timeout: int = 0
):
    send_message = struct.pack(format_characters, *struct_data)
    LoRa.beginPacket()
    LoRa.put(send_message)
    LoRa.endPacket(endpacket_timeout)
    LoRa.wait()