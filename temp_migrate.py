import os
import re

MAP = {
    'ls': ('"[path]"', '"-l   Use a long listing format\\n  -a   Do not ignore entries starting with ."'),
    'cd': ('"<path>"', '""'),
    'pwd': ('""', '""'),
    'cat': ('"<file>"', '""'),
    'echo': ('"[text...]"', '""'),
    'mkdir': ('"<dir>"', '""'),
    'rmdir': ('"<dir>"', '""'),
    'rm': ('"<file>"', '""'),
    'cp': ('"<src> <dst>"', '""'),
    'mv': ('"<src> <dst>"', '""'),
    'touch': ('"<file>"', '""'),
    'mount': ('"<dev> <path>"', '""'),
    'umount': ('"<path>"', '""'),
    'df': ('""', '""'),
    'help': ('"[command]"', '""'),
    'clear': ('""', '""'),
    'shutdown': ('""', '""'),
    'reboot': ('""', '""'),
    'dmesg': ('""', '""'),
    'uname': ('""', '""'),
    'free': ('""', '""'),
    'colortest': ('""', '""'),
    'uptime': ('""', '""'),
    'lspci': ('""', '""'),
    'ps': ('""', '""'),
    'kill': ('"<pid>"', '""'),
}

def split_macro_args(s):
    args = []
    current = []
    in_quotes = False
    escape = False
    for c in s:
        if escape:
            current.append(c)
            escape = False
            continue
        if c == '\\':
            escape = True
            current.append(c)
            continue
        if c == '"':
            in_quotes = not in_quotes
            current.append(c)
            continue
        if c == ',' and not in_quotes:
            args.append("".join(current).strip())
            current = []
            continue
        current.append(c)
    if current:
        args.append("".join(current).strip())
    return args

def migrate_file(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    pattern = re.compile(r'(REGISTER_SHELL_COMMAND(?:_EXT)?\s*\()(.*?)\)(\s*;)', re.DOTALL)
    
    def replacer(match):
        prefix = match.group(1)
        args_str = match.group(2)
        suffix = match.group(3)
        
        args = split_macro_args(args_str)
        cmd_name = args[0]
        
        # Only migrate if it has 5 or 6 args (legacy)
        if len(args) not in (5, 6):
            return match.group(0)
            
        if cmd_name not in MAP:
            print(f"WARNING: Unknown command {cmd_name} in {filepath}")
            return match.group(0)
            
        args_fmt, opts_fmt = MAP[cmd_name]
        
        new_args = [args[0], args_fmt, opts_fmt] + args[1:]
        
        return prefix + ", ".join(new_args) + ")" + suffix

    new_content = pattern.sub(replacer, content)
    
    if new_content != content:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(new_content)
        print(f"Migrated {filepath}")

def main():
    cmds_dir = 'kernel/src/shell/cmds'
    for root, dirs, files in os.walk(cmds_dir):
        for file in files:
            if file.endswith('.c'):
                migrate_file(os.path.join(root, file))

if __name__ == '__main__':
    main()
