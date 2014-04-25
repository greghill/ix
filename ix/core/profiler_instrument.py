#!/usr/bin/env python

import argparse
import re
import sys

MAGICS = {
  'profiler_stack_size_shift': 4,
  'profiler_stack_funcid': 0,
  'profiler_stack_start': 8,
  'profiler_funcdata_size': 16,
  'profiler_funcdata_count': 0,
  'profiler_funcdata_duration': 8,
  'funcid': '{funcid}',
}

PROLOGUE = '''
movq profiler_stack_pointer, %r11
shlq ${profiler_stack_size_shift}, %r11
push %rax
push %rdx
rdtsc
movq ${funcid}, profiler_stack+{profiler_stack_funcid}(%r11)
movq %rax, profiler_stack+{profiler_stack_start}(%r11)
movq %rdx, profiler_stack+{profiler_stack_start}+4(%r11)
addl $1, profiler_stack_pointer
pop %rdx
pop %rax
'''.format(**MAGICS)

EPILOGUE = '''
subl $1, profiler_stack_pointer
movq profiler_stack_pointer, %r11
shlq ${profiler_stack_size_shift}, %r11
push %rax
push %rdx
rdtsc
shlq $32,%rdx
orq %rax,%rdx
subq profiler_stack+{profiler_stack_start}(%r11),%rdx
lock addl $1, profiler_funcdata+{profiler_funcdata_size}*{funcid}+{profiler_funcdata_count}
lock addq %rdx, profiler_funcdata+{profiler_funcdata_size}*{funcid}+{profiler_funcdata_duration}
movq profiler_stack+{profiler_stack_funcid}(%r11), %r11
cmp ${funcid}, %r11
jz 1f
int $3
1:
pop %rdx
pop %rax
ret
'''.format(**MAGICS)

class Processor():
  def __init__(self):
    self.functions = []
    self.funcid = -1

  def function_entry(self, i, input, output):
    output.append(input[i])
    output.append(input[i+1])
    args = {
      'funcid': self.funcid
    }
    output.append(PROLOGUE.format(**args))
    return i+2

  def function_exit(self, i, input, output):
    args = {
      'funcid': self.funcid
    }
    output.append(EPILOGUE.format(**args))
    return i+1

  def process(self, input):
    output = []
    i = 0
    while i < len(input):
      line = input[i]
      m = re.match(r'\t\.type\t([^,]*), @function\n', line)
      if m is not None:
        self.functions.append(m.group(1))
        self.funcid += 1
        i = self.function_entry(i, input, output)
        continue
      m = re.match(r'\tjmp\t([^.].*)\n', line)
      if m is not None:
        dest = m.group(1)
        if '.L' not in dest:
          output.append('call %s\n' % dest)
          i = self.function_exit(i, input, output)
          continue
      m = re.match(r'\tret\n', line)
      if m is not None:
        i = self.function_exit(i, input, output)
        continue
      output.append(line)
      i += 1

    return output

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--output', '-o', required=True, help='output header file for funcinfo initialization')
  parser.add_argument('input_files', metavar='input', nargs='+', help='input assembler files')
  args = parser.parse_args()

  processor = Processor()

  for filename in args.input_files:
    f = open(filename, 'r')
    input = f.readlines()
    f.close()

    output = processor.process(input)

    f = open(filename, 'w')
    f.writelines(output)
    f.close()

  f = open(args.output, 'w')
  for function in processor.functions:
    f.write('{.funcname = "%s"},\n' % function)
  f.close()

if __name__ == '__main__':
  main()
