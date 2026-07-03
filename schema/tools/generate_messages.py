import tomllib
from schema.tools.codegen import CodeGen, Indent
import argparse

c_types = {
    'u8': 'uint8_t',
    'i8':  'int8_t',
    'u16': 'uint16_t',
    'i16': 'int16_t',
    'u32': 'uint32_t',
    'i32': 'int32_t',
}

py_types = {
    'u8': 'int',
    'i8':  'int',
    'u16': 'int',
    'i16': 'int',
    'u32': 'int',
    'i32': 'int',
}

py_struct_types = {
    'u8': 'B',
    'i8':  'b',
    'u16': 'H',
    'i16': 'h',
    'u32': 'I',
    'i32': 'i',
}

type_lengths = {
    'u8': 1,
    'i8':  1,
    'u16': 2,
    'i16': 2,
    'u32': 4,
    'i32': 4,
}

max_can_message_length = 64
can_message_lengths = [1,2,3,4,5,6,7,8,12,16,20,24,32,48,64]

def write_message_c(message, gen: CodeGen):
    name = message["name"]
    gen.add_line(f'struct {name}')

    with Indent(gen, "{", "};"):
        for field in message["fields"]:
            gen.add_line(f'{c_types[field["type"]]} {field["name"]};')

        gen.add_line(f"static std::vector<uint8_t> encode(const struct {name}& f)")
        with Indent(gen, "{", "}"):
            gen.add_line("std::vector<uint8_t> buf;")
            for field in message["fields"]:
                gen.add_line(f'ipc::put_{field["type"]}(buf, f.{field["name"]});') 

            gen.add_line("return buf;")

        gen.add_line(f'static {message["name"]} decode(const uint8_t* data, size_t length)')
        with Indent(gen, "{", "}"):
            gen.add_line("ipc::Reader r{data, length};")
            gen.add_line(f'{message["name"]} frame{{}};')

            for field in message["fields"]:
                gen.add_line(f'frame.{field["name"]} = r.{field["type"]}();') 

            gen.add_line("return frame;")
    return gen

def write_message_py(message, gen : CodeGen):
    gen.add_line("@dataclass")
    gen.add_line(f'class {message["name"]}:')
    with Indent(gen):
        for field in message["fields"]:
            gen.add_line(f'{field["name"]}: {py_types[field["type"]]}') 

        gen.add_line("")
        gen.add_line(f'_FMT = "<{ "".join([ py_struct_types[f["type"]] for f in message["fields"] ]) }"')

        gen.add_line("@classmethod")
        gen.add_line(f'def decode(cls, data: bytes) -> "{message["name"]}":')
        with Indent(gen):
            gen.add_line(f'{ ",".join([ f["name"] for f in message["fields"] ]) } = struct.unpack(cls._FMT, data[: struct.calcsize(cls._FMT)])')
            gen.add_line(f'return cls({ ",".join([ f["name"] for f in message["fields"] ]) })')
        
        gen.add_line("")
        gen.add_line("@staticmethod")
        gen.add_line(f'def encode(obj: "{message["name"]}"):')
        with Indent(gen):
            gen.add_line(f'return struct.pack({message["name"]}._FMT, ')
            with Indent(gen):
                [ gen.add_line(f'obj.{f["name"]},') for f in message["fields"] ]
                gen.add_line(")")

def validate_message(message):
    length = 0
    for field in message['fields']:
        length += type_lengths[field['type']]

    if((length + 1) > max_can_message_length):
        raise Exception(f"Message length {length} for message {message['name']} is longer than the maximum allowed: {max_can_message_length}")
    if((length + 1) not in can_message_lengths):
        raise Exception(f"Message length {length} for message {message['name']} is not a standard canfd length and will be padded")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()

    parser.add_argument("--lang", dest = "lang")
    parser.add_argument("--schema", dest = "schema")
    parser.add_argument("--out-dir", dest = "out")

    args = parser.parse_args()

    with open(args.schema, "rb") as f:
        message_structure = tomllib.load(f)

    if(args.lang == "python"):
        gen_py = CodeGen()
        gen_py.add_line("from dataclasses import dataclass")
        gen_py.add_line("import struct")
        gen_py.add_line("")

        for message in message_structure['message']:
            validate_message(message)
            write_message_py(message, gen_py)

        with open(args.out, "w") as f:
            f.write(gen_py.file_string)

    elif(args.lang == "cpp"):
        gen_c = CodeGen()
        gen_c.add_line("#pragma once")
        gen_c.add_line("#include <cstdint>")
        gen_c.add_line("#include <vector>")
        gen_c.add_line('#include "common/ipc/ipc.h"') # TODO: Fix this static path?
        gen_c.add_line("")

        for message in message_structure['message']:
            validate_message(message)
            write_message_c(message, gen_c)

        with open(args.out, "w") as f:
            f.write(gen_c.file_string)

