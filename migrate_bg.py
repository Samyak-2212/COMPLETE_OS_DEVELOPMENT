import os

def migrate_color(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    # Replace hardcoded navy blue with FB_DEFAULT_BG
    # We match both 0x1A1A2E and 0x001A1A2E
    new_content = content.replace('0x1A1A2E', 'FB_DEFAULT_BG')
    new_content = new_content.replace('0x001A1A2E', 'FB_DEFAULT_BG')

    if new_content != content:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(new_content)
        print(f"Migrated {filepath}")

def main():
    src_dir = 'kernel/src'
    for root, dirs, files in os.walk(src_dir):
        for file in files:
            if file.endswith('.c') or file.endswith('.h'):
                # Skip the files where we defined the macros to avoid circularity or re-replacement issues
                if file in ['framebuffer.h', 'terminal.h']:
                    continue
                migrate_color(os.path.join(root, file))

if __name__ == '__main__':
    main()
