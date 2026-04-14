import os
import re
import fnmatch

# --- Configuration ---
ROOT_DIR = os.getcwd()
SRC_DIR = os.path.join(ROOT_DIR, "kernel", "src")
TASK_PATH = os.path.join(ROOT_DIR, "knowledge_items", "nexusos-task-tracker", "artifacts", "task.md")
REPORT_PATH = os.path.join(ROOT_DIR, "knowledge_items", "nexusos-progress-report", "artifacts", "progress_report.md")
DEBUGGER_DIR = os.path.join(ROOT_DIR, "debugger")
GITIGNORE_PATH = os.path.join(ROOT_DIR, ".gitignore")

def get_ignore_rules():
    rules = []
    if os.path.exists(GITIGNORE_PATH):
        with open(GITIGNORE_PATH, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                rules.append(line)
    return rules

def is_ignored(rel_path, rules):
    rel_path = rel_path.replace('\\', '/')
    for rule in rules:
        if rule.endswith('/'):
            if rel_path.startswith(rule) or f"/{rule}" in rel_path:
                return True
        if fnmatch.fnmatch(rel_path, rule) or fnmatch.fnmatch(os.path.basename(rel_path), rule):
            return True
    return False

def count_source_files():
    count = 0
    rules = get_ignore_rules()
    for root, dirs, files in os.walk(SRC_DIR):
        for file in files:
            full_path = os.path.join(root, file)
            rel_path = os.path.relpath(full_path, ROOT_DIR)
            if file.endswith(('.c', '.h', '.asm')):
                if not is_ignored(rel_path, rules):
                    count += 1
    
    if os.path.exists(DEBUGGER_DIR):
        for root, dirs, files in os.walk(DEBUGGER_DIR):
            for file in files:
                full_path = os.path.join(root, file)
                rel_path = os.path.relpath(full_path, ROOT_DIR)
                if file.endswith(('.c', '.h', '.asm')):
                    if not is_ignored(rel_path, rules):
                        count += 1
    return count

def check_docs():
    print("--- NexusOS Documentation Audit ---")
    
    # 1. File Count
    actual_count = count_source_files()
    documented_count = 0
    
    if os.path.exists(REPORT_PATH):
        with open(REPORT_PATH, 'r', encoding='utf-8') as f:
            content = f.read()
            match = re.search(r'Files created[:\*]*\s*(\d+)', content, re.IGNORECASE)
            if match:
                documented_count = int(match.group(1))
    
    print(f"Source Files (kernel/src + debugger):")
    print(f"  Actual: {actual_count}")
    print(f"  Documented: {documented_count}")
    
    if actual_count != documented_count:
        print(f"  [!] DESYNC: File count mismatch. Actual: {actual_count}, Documented: {documented_count}")
    else:
        print(f"  [OK] File count matches.")

    # 2. Phase Status
    if os.path.exists(TASK_PATH):
        with open(TASK_PATH, 'r', encoding='utf-8') as f:
            content = f.read()
            # Check for the Phase 3 heading with emoji or checkmark
            if re.search(r'## Phase 3:.*[✅|complete]', content, re.IGNORECASE):
                print("Phase 3 Status: [x] COMPLETE (task.md)")
            else:
                print("Phase 3 Status: [ ] PENDING (task.md)")
    
    print("-----------------------------------")

if __name__ == "__main__":
    check_docs()
