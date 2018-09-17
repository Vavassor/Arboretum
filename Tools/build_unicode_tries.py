#!/usr/bin/env python3

import argparse
import array
import sys
import requests


atr_byteorder = "little"
atr_signature = "ARBOTRIE"
atr_version = 0
chunk_id_data = "DATA"
chunk_id_format = "FORM"
chunk_id_index = "INDX"


class AtrWriter:
    def __init__(self, trie):
        self.trie = trie

    def save_file(self, path):
        file_data = bytearray()
        self._write_file_header_after_checksum(file_data)
        self._write_format_chunk(file_data)
        self._write_index_chunk(file_data)
        self._write_data_chunk(file_data)
        with open(path, "wb") as file:
            file.write(atr_signature.encode("utf-8"))
            checksum = self._compute_checksum(file_data)
            file.write(checksum.to_bytes(4, byteorder=atr_byteorder))
            file.write(file_data)

    def _write_data_chunk(self, file_data):
        file_data.extend(chunk_id_data.encode("utf-8"))
        data_array = array.array("I", self.trie.data)
        data = array_to_endian_bytes(data_array, atr_byteorder)
        file_data.extend(len(data).to_bytes(4, byteorder=atr_byteorder))
        file_data.extend(data)

    def _write_file_header_after_checksum(self, file_data):
        file_data.extend(atr_version.to_bytes(2, byteorder=atr_byteorder))

    def _write_format_chunk(self, file_data):
        file_data.extend(chunk_id_format.encode("utf-8"))
        default_value = self.trie.default_value.to_bytes(4, byteorder=atr_byteorder)
        high_end = self._make_codepoint(self.trie.high_end)
        chunk_size = len(high_end) + len(default_value)
        file_data.extend(chunk_size.to_bytes(4, byteorder=atr_byteorder))
        file_data.extend(default_value)
        file_data.extend(high_end)

    def _write_index_chunk(self, file_data):
        file_data.extend(chunk_id_index.encode("utf-8"))
        indices_array = array.array("H", self.trie.indices)
        indices = array_to_endian_bytes(indices_array, atr_byteorder)
        file_data.extend(len(indices).to_bytes(4, byteorder=atr_byteorder))
        file_data.extend(indices)

    @staticmethod
    def _compute_checksum(data):
        builder = ChecksumBuilder()
        return builder.checksum(data, 0xffffffff)

    @staticmethod
    def _make_codepoint(string):
        codepoint = string.encode("utf-32")[4:]
        if sys.byteorder != atr_byteorder:
            codepoint.reverse()
        return codepoint


class BreakReader:
    """Read GraphemeClusterBreak.txt, LineBreak.txt, and WordBreakText.txt."""

    def __init__(self, default_value, value_map):
        self.default_value = default_value
        self.value_map = value_map
        self.value_ranges = []

    def add_line(self, line):
        pound = line.find("#")
        if pound != -1:
            semicolon = line.find(";", 0, pound)
            if semicolon != -1:
                interval_or_codepoint = line[:semicolon].strip()
                interval = self._get_interval(interval_or_codepoint)
                value_name = line[semicolon+1:pound].strip()
                value = self._decode(value_name)
                value_range = ValueRange(interval, value)
                self.value_ranges.append(value_range)

    def add_lines(self, lines):
        for line in lines:
            self.add_line(line)

    def get_value_ranges(self):
        return sorted(self.value_ranges, key=lambda r: r.interval[0])

    def _decode(self, value_name):
        return self.value_map[value_name]

    @staticmethod
    def _get_interval(interval_or_codepoint):
        dotdot = interval_or_codepoint.find("..")
        if dotdot != -1:
            interval = interval_or_codepoint
            first = interval[:dotdot]
            second = interval[dotdot+2:]
            return [int(first, 16), int(second, 16)]
        else:
            codepoint = interval_or_codepoint
            return [int(codepoint, 16)]


class ChecksumBuilder:
    def __init__(self):
        self.table = self._create_table()

    def checksum(self, data, crc):
        crc ^= 0xffffffff
        for k in data:
            crc = (crc >> 8) ^ self.table[(crc & 0xff) ^ k]
        return crc ^ 0xffffffff

    @staticmethod
    def _create_table():
        a = []
        for i in range(256):
            k = i
            for j in range(8):
                if k & 1:
                    k ^= 0x1db710640
                k >>= 1
            a.append(k)
        return a


class Source:
    """A description of the ranges for one property to be added to a Trie."""

    def __init__(self, bits, value_ranges):
        self.bits = bits
        self.value_ranges = value_ranges


class Trie:
    def __init__(self):
        self.indices = [5]
        self.data = [17]
        self.high_end = "K"
        self.default_value = 0


class TrieBuilder:
    def __init__(self, sources):
        self.sources = sources

    def build(self):
        return Trie()


class ValueRange:
    def __init__(self, interval, value):
        self.interval = interval
        self.value = value


def array_to_endian_bytes(a, byteorder):
    if byteorder != sys.byteorder:
        a.byteswap()
    return a.tobytes()

def generate_grapheme_cluster_break(mood_print):
    bits = 5
    default_value = 0 # 'XX' (Grapheme_Cluster_Break=Unknown)
    url = "https://www.unicode.org/Public/10.0.0/ucd/auxiliary/"
            "GraphemeBreakProperty.txt"
    values = {
        "Other":0, "CR":1, "LF":2, "Control":3, "Extend":4, "ZWJ":5,
        "Regional_Indicator":6, "Prepend":7, "SpacingMark":8, "L":9, "V":10,
        "T":11, "LV":12, "LVT":13, "E_Base":14, "E_Modifier":15,
        "Glue_After_Zwj":16, "E_Base_GAZ":17,
    }
    return generate_source(url, bits, default_value, values, mood_print)

def generate_line_break(mood_print):
    bits = 6
    default_value = 40 # 'XX' (Line_Break=Unassigned)
    url = "https://www.unicode.org/Public/10.0.0/ucd/LineBreak.txt"
    values = {
        "AI":0, "AL":1, "B2":2, "BA":3, "BB":4, "BK":5, "CB":6, "CJ":7,
        "CL":8, "CM":9, "CP":10, "CR":11, "EB":12, "EM":13, "EX":14,
        "GL":15, "H2":16, "H3":17, "HL":18, "HY":19, "ID":20, "IN":21,
        "IS":22, "JL":23, "JT":24, "JV":25, "LF":26, "NL":27, "NS":28,
        "NU":29, "OP":30, "PO":31, "PR":32, "QU":33, "RI":34, "SA":35,
        "SG":36, "SP":37, "SY":38, "WJ":39, "XX":40, "ZW":41, "ZWJ":42,
    }
    return generate_source(url, bits, default_value, values, mood_print)

def generate_source(url, bits, default_value, value_map, mood_print):
    mood_print("Loading {}… ".format(url), end="", flush=True)
    response = requests.get(url)
    mood_print("Completed.")
    lines = response.text.splitlines()
    reader = BreakReader(default_value, value_map)
    reader.add_lines(lines)
    return Source(bits, reader.get_value_ranges())

def generate_word_break(mood_print):
    bits = 5
    default_value = 0 # 'XX' (Word_Break=Other)
    url = "https://www.unicode.org/Public/10.0.0/ucd/auxiliary/"
            "WordBreakProperty.txt"
    value_map = {
        "Other":0, "CR":1, "LF":2, "Newline":3, "Extend":4, "ZWJ":5,
        "Regional_Indicator":6, "Format":7, "Katakana":8,
        "Hebrew_Letter":9, "ALetter":10, "Single_Quote":11,
        "Double_Quote":12, "MidNumLet":13, "MidLetter":14, "MidNum":15,
        "Numeric":16, "ExtendNumLet":17, "E_Base":18, "E_Modifier":19,
        "Glue_After_Zwj":20, "E_Base_GAZ":21,
    }
    return generate_source(url, bits, default_value, values, mood_print)

def main():
    arguments = parse_arguments()
    output_path = arguments.output
    output_cpp = arguments.cpp
    quiet = arguments.quiet

    mood_print = print if not quiet else lambda *a, **k: None

    sources = []
    sources.append(generate_grapheme_cluster_break(mood_print))
    sources.append(generate_line_break(mood_print))
    sources.append(generate_word_break(mood_print))
    
    builder = TrieBuilder(sources)
    trie = builder.build()
    
    writer = AtrWriter(trie)
    writer.save_file("test.atr")

def parse_arguments():
    description = "Generates two-stage tables for looking up Unicode " \
            "codepoint properties."
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument("--cpp", action="store_true",
            help="save C++ .cpp files instead of .bin files", dest="cpp")
    parser.add_argument("-o", "--output", default=".",
            help="path to put the table files", dest="output")
    parser.add_argument("-q", "--quiet", action="store_true",
            help="don't print anything", dest="quiet")
    return parser.parse_args()


main()

