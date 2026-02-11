#!/usr/bin/env python3
import argparse
import pathlib
import struct


def main() -> int:
    parser = argparse.ArgumentParser(description="Convert SPIR-V binary to C++ header")
    parser.add_argument("--input", required=True, help="Input .spv file")
    parser.add_argument("--output", required=True, help="Output .h file")
    parser.add_argument(
        "--symbol", default="kPathtraceVulkanSpv", help="C++ array symbol name"
    )
    args = parser.parse_args()

    input_path = pathlib.Path(args.input)
    output_path = pathlib.Path(args.output)

    data = input_path.read_bytes()
    if len(data) % 4 != 0:
        raise ValueError("SPIR-V byte length must be a multiple of 4")

    words = [struct.unpack_from("<I", data, i)[0] for i in range(0, len(data), 4)]

    lines = [
        "#ifndef VULKANPATHTRACERSPV_H",
        "#define VULKANPATHTRACERSPV_H",
        "",
        "#include <cstddef>",
        "#include <cstdint>",
        "",
        f"static const uint32_t {args.symbol}[] = {{",
    ]

    for i, word in enumerate(words):
        prefix = "    " if i % 8 == 0 else ""
        lines.append(f"{prefix}0x{word:08x},")

    lines.extend(
        [
            "};",
            "",
            f"static const size_t {args.symbol}Size = sizeof({args.symbol});",
            "",
            "#endif",
            "",
        ]
    )

    output_path.write_text("\n".join(lines), encoding="ascii")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
