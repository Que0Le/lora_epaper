from lora_helper import *



def send_abort_request_msg(session_id: int, msg: str=""):
    """ Abort the current request with msg """
    pass


def send_deny_request_msg(session_id: int = 0, msg: str=""):
    """ Abort the current request with msg """
    pass


def send_shard(shard_id: int, chunk_id: int, session_id: int):
    pass


def check_false_session_and_return_msg(
    session_id: int, current_session: int, msg: str = ""
) -> bool:
    """ Return True and msg to client if seesion not correct """
    if session_id!=current_session:
        send_abort_request_msg(msg=msg, session_id=0)
        return True
    return False