import re
s = open('sources/Application/Utils/HelpLegend.h', encoding='utf-8').read()
cur = None
for line in s.splitlines():
    m = re.search(r'case (I_CMD_\w+)', line)
    if m:
        cur = m.group(1)
    m = re.search(r'result\[(\d)\]\.assign\("(.*)"\);', line)
    if m and len(m.group(2)) > 20:
        print(cur, m.group(1), len(m.group(2)), repr(m.group(2)))
