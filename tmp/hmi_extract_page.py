import hashlib
import struct
import sys
from pathlib import Path


ENTRY = struct.Struct("<16sIII")


def main() -> None:
    source_path = Path(sys.argv[1])
    record_index = int(sys.argv[2])
    expected_page = sys.argv[3]
    output_path = Path(sys.argv[4])

    with source_path.open("rb") as source:
        count = struct.unpack("<I", source.read(4))[0]
        if not 0 <= record_index < count:
            raise ValueError(f"Record {record_index} is outside 0..{count - 1}")
        source.seek(4 + record_index * ENTRY.size)
        raw_name, offset, length, flags = ENTRY.unpack(source.read(ENTRY.size))
        source.seek(offset)
        payload = source.read(length)

    if len(payload) != length:
        raise ValueError("Page payload is truncated")
    internal_length = struct.unpack_from("<I", payload, 4)[0]
    if internal_length != length:
        raise ValueError(
            f"Internal page length {internal_length} does not match directory length {length}"
        )
    page_name = payload[24:40].split(b"\0", 1)[0].decode("ascii")
    if page_name != expected_page:
        raise ValueError(f"Expected page {expected_page!r}, found {page_name!r}")
    if output_path.exists():
        raise FileExistsError(f"Refusing to overwrite {output_path}")

    output_path.write_bytes(payload)
    digest = hashlib.sha256(payload).hexdigest().upper()
    print(
        f"record={record_index} raw_name={raw_name.hex()} offset=0x{offset:08X} "
        f"length={length} flags=0x{flags:08X} page={page_name} sha256={digest}"
    )


if __name__ == "__main__":
    main()
