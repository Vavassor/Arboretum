#!/usr/bin/env python3

# Unicode multi-stage table builder
# (c) Peter Kankowski, 2008
# Released under the zlib/libpng license (http://www.opensource.org/licenses/zlib-license.php)
# 
# 2018 - Modified by Andrew Dawson to produce binary files and unicode data
# other than General Category.

import argparse
import os.path
import requests


class TableBuilder:
    """Build the table, including compacting the identical blocks and choosing
    the smallest possible item size.
    """

    def __init__(self, block_size):
        # Dictionary for finding identical blocks
        self.blocks = {}
        # Stage 1 table contains block numbers (indices into stage 2 table)
        self.stage1 = [] 
        # Stage 2 table contains the blocks with property values
        self.stage2 = []
        self.block_size = block_size
    
    def add_block(self, block, count = 1):
        assert(len(block) == self.block_size)
        
        # If there is such block in the stage2 table, use it
        tblock = tuple(block)
        start = self.blocks.get(tblock)
        if start is None:
            # Allocate a new block
            start = len(self.stage2) // self.block_size
            self.stage2 += block
            self.blocks[tblock] = start
        
        # Add 'count' blocks with the same values
        self.stage1 += [start] * count
    
    def __get_type_size(self, seq):
        type_size = [("uint8_t", 1), ("uint16_t", 2), ("uint32_t", 4),
                     ("int8_t", 1), ("int16_t", 2), ("int32_t", 4)]
        limits = [(0, 255), (0, 65535), (0, 4294967295),
                  (-128, 127), (-32768, 32767), (-2147483648, 2147483647)]
        minval = min(seq)
        maxval = max(seq)
        for num, (minlimit, maxlimit) in enumerate(limits):
            if minlimit <= minval and maxval <= maxlimit:
                return type_size[num]
        else:
            raise OverflowError("Too large to fit into C types")

    def output_stage1_binary(self, filepath):
        stage1_type, stage1_size = self.__get_type_size(self.stage1)
        with open(filepath, "wb") as file:
            file.write(bytearray(self.stage1))

    def output_stage2_binary(self, filepath):
        stage2_type, stage2_size = self.__get_type_size(self.stage2)
        with open(filepath, "wb") as file:
            file.write(bytearray(self.stage2))
    
    def output_cpp_file(self, output_path, prop_name, mood_print):
        stage1_type, stage1_size = self.__get_type_size(self.stage1)
        stage2_type, stage2_size = self.__get_type_size(self.stage2)
        
        filename = prop_name + ".cpp"
        path = os.path.join(output_path, filename)
        with open(path, "w") as file:
            total_size = len(self.stage1) * stage1_size \
                    + len(self.stage2) * stage2_size
            header = "// {:s} table, {:d} bytes\n".format(prop_name, total_size)
            file.write(header)
            
            file.write("static const {:s} {:s}_stage1[] = {{\n".format(
                    stage1_type, prop_name))
            for i, block in enumerate(self.stage1):
                file.write("{:2d},".format(block))
                if i % 16 == 15:
                    block_base = (i - 15) * self.block_size
                    file.write("// U+{:04X}\n".format(block_base))
            file.write("}};\n\n")
            
            file.write("static const {:s} {:s}_stage2[] = {{\n".format(
                    stage2_type, prop_name))
            for i, val in enumerate(self.stage2):
                if i % self.block_size == 0:
                    file.write("\n// block {:d}\n".format(i // self.block_size))
                file.write("{:2d},".format(val))
                if i % 16 == 15:
                    file.write("\n")
            file.write("}};\n")
            
            func = """
{t} get_{p}(uint32_t ch)
{{
    const uint32_t BLOCK_SIZE = {size};
    ASSERT(ch < 0x110000);
    uint32_t block_offset = {p}_stage1[ch / BLOCK_SIZE] * BLOCK_SIZE;
    return {p}_stage2[block_offset + ch % BLOCK_SIZE];
}}
"""
            file.write(func.format(t=stage2_type, p=prop_name,
                    size=self.block_size))

        abs_output_path = os.path.abspath(output_path)
        mood_print("Saved {} to {}.".format(filename, abs_output_path))


class UnicodeDataExtractor:
    """Read UnicodeData.txt file"""

    def __init__(self, block_size, add_block_func):
        self.block_size = block_size
        self.add_block_func = add_block_func
        self.block = []
        self.next_char = 0
        self.def_val = 18 # 'Cn' (General_Category=Unassigned)
        self.span_val = self.def_val
    
    def __decode(self, chardata):
        categories = {
            'Lu':0, 'Ll':1, 'Lt':2, 'Lm':3, 'Lo':4,
            'Mn':5, 'Me':6, 'Mc':7,
            'Nd':8, 'Nl':9, 'No':10,
            'Zs':11, 'Zl':12, 'Zp':13,
            'Cc':14, 'Cf':15, 'Co':16, 'Cs':17, 'Cn':18,
            'Pd':19, 'Ps':20, 'Pe':21, 'Pc':22, 'Po':23, 'Pi':24, 'Pf':25,
            'Sm':26, 'Sc':27, 'Sk':28, 'So':29,
        }
        return categories[chardata[2]]
    
    def __add_char(self, val):
        # Add to the block while it's not filled
        self.block.append(val)
        if len(self.block) == self.block_size:
            self.add_block_func(self.block)
            self.block = []
    
    def __add_chars(self, val, count):
        n = min(self.block_size - len(self.block), count)
        for i in range(n):
            self.block.append(val)
        if len(self.block) == self.block_size:
            self.add_block_func(self.block)
            self.block = []
        
        # Add a lot of blocks in a long span (optimized)
        if (count - n) // self.block_size != 0:
            block = [val] * self.block_size
            self.add_block_func(block, (count - n) // self.block_size)
        
        # Add the remaining chars
        for i in range((count - n) % self.block_size):
            self.block.append(val)
    
    def add_line(self, line):
        chardata = line.split(";")
        char = int(chardata[0], 16)
        # Add unassigned characters or the preceding span
        if char > self.next_char:
            self.__add_chars(self.span_val, char - self.next_char)
        val = self.__decode(chardata)
        self.__add_char(val)
        
        # Special mode for character spans
        self.span_val = self.def_val if chardata[1][-8:] != ", First>" else val
        self.next_char = char + 1
    
    def finish(self):
        for i in range(self.next_char, 0x110000):
            self.__add_char(self.span_val)


class ExtractorInfo:
    def __init__(self, value_map, default_value, block_size):
        self.value_map = value_map
        self.default_value = default_value
        self.block_size = block_size


class ValueRange:
    def __init__(self, interval, value):
        self.interval = interval
        self.value = value


class BreakExtractor:
    """Read GraphemeClusterBreak.txt, LineBreak.txt, and WordBreakText.txt."""

    def __init__(self, add_block_func, info):
        self.block_size = info.block_size
        self.add_block_func = add_block_func
        self.block = []
        self.default_value = info.default_value
        self.value_map = info.value_map
        self.next_char = 0
        self.value_ranges = []
    
    def __decode(self, value_name):
        return self.value_map[value_name]

    @staticmethod
    def __get_interval(interval_or_codepoint):
        dotdot = interval_or_codepoint.find("..")
        if dotdot != -1:
            interval = interval_or_codepoint
            first = interval[:dotdot]
            second = interval[dotdot+2:]
            return [int(first, 16), int(second, 16)]
        else:
            codepoint = interval_or_codepoint
            return [int(codepoint, 16)]

    def __add_char(self, value):
        self.block.append(value)
        if len(self.block) == self.block_size:
            self.add_block_func(self.block)
            self.block = []

    def __add_chars(self, val, count):
        n = min(self.block_size - len(self.block), count)
        for i in range(n):
            self.block.append(val)
        if len(self.block) == self.block_size:
            self.add_block_func(self.block)
            self.block = []
        
        # Add a lot of blocks in a long span (optimized)
        if (count - n) // self.block_size != 0:
            block = [val] * self.block_size
            self.add_block_func(block, (count - n) // self.block_size)
        
        # Add the remaining chars
        for i in range((count - n) % self.block_size):
            self.block.append(val)

    def add_line(self, line):
        pound = line.find("#")
        if pound != -1:
            semicolon = line.find(";", 0, pound)
            if semicolon != -1:
                interval_or_codepoint = line[:semicolon].strip()
                interval = BreakExtractor.__get_interval(interval_or_codepoint)
                value_name = line[semicolon+1:pound].strip()
                value = self.__decode(value_name)
                value_range = ValueRange(interval, value)
                self.value_ranges.append(value_range)
                
    def finish(self):
        self.value_ranges.sort(key=lambda r: r.interval[0])
        for value_range in self.value_ranges:
            interval = value_range.interval
            value = value_range.value
            if interval[0] > self.next_char:
                self.__add_chars(self.default_value, interval[0] - self.next_char)
            if len(interval) == 1:
                self.__add_char(value)
                self.next_char = interval[0] + 1
            else:
                self.__add_chars(value, (interval[1] + 1) - interval[0])
                self.next_char = interval[1] + 1
        for i in range(self.next_char, 0x110000):
            self.__add_char(self.default_value)


def generate_table(data_url, property_name, extractor_info, output_path,
            output_cpp, mood_print):
    block_size = extractor_info.block_size
    builder = TableBuilder(block_size)
    if property_name is "category":
        extractor = UnicodeDataExtractor(block_size, builder.add_block)
    else:
        extractor = BreakExtractor(builder.add_block, extractor_info)
    mood_print("Loading {}… ".format(data_url), end="", flush=True)
    response = requests.get(data_url)
    mood_print("Completed.")
    lines = response.text.splitlines()
    for line in lines:
        extractor.add_line(line)
    extractor.finish()
    if output_cpp:
        builder.output_cpp_file(output_path, property_name, mood_print)
    else:
        stage1_name = property_name + "_stage1.bin"
        stage1_path = os.path.join(output_path, stage1_name)
        stage2_name = property_name + "_stage2.bin"
        stage2_path = os.path.join(output_path, stage2_name)
        abs_output_path = os.path.abspath(output_path)
        builder.output_stage1_binary(stage1_path)
        mood_print("Saved {} to {}.".format(stage1_name, abs_output_path))
        builder.output_stage2_binary(stage2_path)
        mood_print("Saved {} to {}.".format(stage2_name, abs_output_path))


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

    
arguments = parse_arguments()
output_path = arguments.output
output_cpp = arguments.cpp
quiet = arguments.quiet

mood_print = print if not quiet else lambda *a, **k: None

grapheme_cluster_breaks = {
    "Other":0, "CR":1, "LF":2, "Control":3,
    "Extend":4, "ZWJ":5, "Regional_Indicator":6, "Prepend":7,
    "SpacingMark":8, "L":9, "V":10, "T":11, "LV":12, "LVT":13,
    "E_Base":14, "E_Modifier":15, "Glue_After_Zwj":16,
    "E_Base_GAZ":17,
}
default_value = 0 # 'XX' (Grapheme_Cluster_Break=Unknown)
extractor_info = ExtractorInfo(grapheme_cluster_breaks, default_value, 256)
url = "https://www.unicode.org/Public/10.0.0/ucd/auxiliary/GraphemeBreakProperty.txt"
generate_table(url, "grapheme_cluster_break", extractor_info, output_path,
        output_cpp, mood_print)

line_breaks = {
    "AI":0, "AL":1, "B2":2, "BA":3, "BB":4, "BK":5, "CB":6, "CJ":7,
    "CL":8, "CM":9, "CP":10, "CR":11, "EB":12, "EM":13, "EX":14,
    "GL":15, "H2":16, "H3":17, "HL":18, "HY":19, "ID":20, "IN":21,
    "IS":22, "JL":23, "JT":24, "JV":25, "LF":26, "NL":27, "NS":28,
    "NU":29, "OP":30, "PO":31, "PR":32, "QU":33, "RI":34, "SA":35,
    "SG":36, "SP":37, "SY":38, "WJ":39, "XX":40, "ZW":41, "ZWJ":42,
}
default_value = 40 # 'XX' (Line_Break=Unassigned)
extractor_info = ExtractorInfo(line_breaks, default_value, 128)
url = "https://www.unicode.org/Public/10.0.0/ucd/LineBreak.txt"
generate_table(url, "line_break", extractor_info, output_path, output_cpp,
        mood_print)

word_breaks = {
    "Other":0, "CR":1, "LF":2, "Newline":3, "Extend":4, "ZWJ":5,
    "Regional_Indicator":6, "Format":7, "Katakana":8,
    "Hebrew_Letter":9, "ALetter":10, "Single_Quote":11,
    "Double_Quote":12, "MidNumLet":13, "MidLetter":14, "MidNum":15,
    "Numeric":16, "ExtendNumLet":17, "E_Base":18, "E_Modifier":19,
    "Glue_After_Zwj":20, "E_Base_GAZ":21,
}
default_value = 0 # 'XX' (Word_Break=Other)
extractor_info = ExtractorInfo(word_breaks, default_value, 256)
url = "https://www.unicode.org/Public/10.0.0/ucd/auxiliary/WordBreakProperty.txt"
generate_table(url, "word_break", extractor_info, output_path, output_cpp,
        mood_print)

