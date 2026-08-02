import json
import re
import os

log_path = r'C:\Users\asus\.gemini\antigravity-ide\brain\4e902772-265c-40fe-8bf3-b0b29f4220d3\.system_generated\logs\transcript.jsonl'
files = {}

with open(log_path, 'r', encoding='utf-8') as f:
    for line in f:
        try:
            step = json.loads(line)
            if 'tool_calls' in step:
                for tc in step['tool_calls']:
                    # Extract from write_to_file
                    if tc.get('function', {}).get('name') == 'default_api:write_to_file':
                        try:
                            args_str = tc['function']['arguments']
                            args = json.loads(args_str)
                            path = args.get('TargetFile', '')
                            if path.endswith('.tex'):
                                files[path] = args.get('CodeContent', '')
                        except:
                            pass
            
            # Also try to extract from view_file outputs if they are in the log
            if step.get('type') == 'TOOL_RESPONSE':
                content = step.get('content', '')
                if 'The following code has been modified to include a line number' in content:
                    lines = content.split('\n')
                    file_path = None
                    for l in lines:
                        if l.startswith('File Path: `file:///'):
                            file_path = l.split('`file:///')[1].split('`')[0]
                            break
                    if file_path and file_path.endswith('.tex'):
                        code_lines = []
                        for l in lines:
                            m = re.match(r'^\d+:\s(.*)$', l)
                            if m:
                                code_lines.append(m.group(1))
                        files[file_path] = '\n'.join(code_lines)
        except:
            pass

print('Found files:', len(files))
for k, v in files.items():
    if len(v) > 0:
        base = os.path.basename(k)
        print('Recovered:', base, len(v))
        with open(base, 'w', encoding='utf-8') as out:
            out.write(v.replace('[!ht]', '[H]'))
