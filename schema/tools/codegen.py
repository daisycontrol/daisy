class CodeGen:
    indent_level = 0
    file_string = ""
    indent = '    '
    newline = '\r\n'

    def add_line(self, line):
        self.file_string += self.indent * self.indent_level + line + self.newline
    def increase_indent(self):
        self.indent_level += 1
    def decrease_indent(self):
        self.indent_level -= 1
    
class Indent:
    def __init__(self, gen, bracket_open = None, bracket_close = None) -> None:
        self.gen = gen
        self.bracket_open = bracket_open
        self.bracket_close = bracket_close

    def __enter__(self):
        if(self.bracket_open):
            self.gen.add_line(self.bracket_open)
        self.gen.increase_indent()

    def __exit__(self, exc_type, exc_value, traceback):
        self.gen.decrease_indent()
        if(self.bracket_close):
            self.gen.add_line(self.bracket_close)