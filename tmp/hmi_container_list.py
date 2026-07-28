import struct
import sys


ENTRY = struct.Struct("<16sIII")


def decode_name(raw: bytes) -> str:
    data = raw.split(b"\0", 1)[0]
    return "".join(chr(byte) if 32 <= byte < 127 else f"\\x{byte:02x}" for byte in data)


def main() -> None:
    with open(sys.argv[1], "rb") as source:
        count_data = source.read(4)
        if len(count_data) != 4:
            raise ValueError("Missing HMI resource count")
        count = struct.unpack("<I", count_data)[0]
        print(f"count={count}")
        for index in range(count):
            data = source.read(ENTRY.size)
            if len(data) != ENTRY.size:
                raise ValueError(f"Truncated resource record {index}")
            raw_name, offset, length, flags = ENTRY.unpack(data)
            name = decode_name(raw_name)
            print(
                f"{index:02d} name={name!r} offset=0x{offset:08x} "
                f"length={length} flags=0x{flags:08x}"
            )


if __name__ == "__main__":
    main()
