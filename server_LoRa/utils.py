


def get_bmp_raw_data_from_file(filepath: str, include_header: bool = False):
    with open(filepath, mode='rb') as file: # b is important -> binary
        fileContent = file.read()
    if include_header:
        return fileContent
    return fileContent[62:]