from LoRaRF import SX127x

from lora_helper import *



def send_abort_request_msg(session_id: int, msg: str=""):
    """ Abort the current request with msg """
    pass


def send_deny_request_msg(session_id: int = 0, msg: str=""):
    """ Abort the current request with msg """
    pass


def send_shard(shard_id: int, chunk_id: int, session_id: int):
    pass


def send_accept_img_request(
    LoRa: SX127x, chunk_sizes: list(int),
    host_code: int, flag: int, session_id: int,  
    is_encrypted: int,is_compressed: int = 1, 
    request_status = 1
):
    """ Send approvement to client with chunk size ... """
    # message encodes how much space each chunk is
    sizes: list = []
    for cs in chunk_sizes:
        sizes = sizes + list(struct.pack("I", cs))
    chunk_sizes_bytes = bytes(sizes)
    
    packet_struct = [
        host_code, flag, session_id, is_encrypted, 
        is_compressed, request_status, chunk_sizes_bytes
    ]
    send_a_struct(
        LoRa=LoRa, struct_data=packet_struct, 
        format_characters="BBBBBB30s", endpacket_timeout=250
    )
    return 1


def check_false_session_and_return_msg(
    session_id: int, current_session: int, msg: str = ""
) -> bool:
    """ Return True and msg to client if seesion not correct """
    if session_id!=current_session:
        send_abort_request_msg(msg=msg, session_id=0)
        return True
    return False